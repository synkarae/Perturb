/*
   Copyright 2015 Robert C. Taylor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/


#ifndef __PERTURB_INCLUDE_ACTOR__H__
#define __PERTURB_INCLUDE_ACTOR__H__

#include <Theron/Theron.h> 
#include <typeinfo>       
#include <typeindex>      
#include <unordered_map>  
#include <stdexcept>
#include <deque>
#include <algorithm>
#include <App.h>
#include <perturb.h>
#include <ActorAddLink.h>
#include <ActorInputMessage.h>
#include <ActorRemoveLink.h>
#include <ActorSyncReplyMessage.h>
#include <ActorSyncRequestMessage.h>
#include <functional>



namespace Perturb {

class Actor : public Theron::Actor 
{
	private:
	Perturb::Address message_from_address_; 
	int message_input_id_;
	int message_token_;

	std::unordered_map<size_t, std::unordered_map<int,  std::function<void(const void *)> > > input_map_;
	std::unordered_map<size_t, std::unordered_map<int, std::deque<std::pair<int, Perturb::Address> > > > output_map_;
	int token_ = 0;
	bool token_check_ = true;    

  public:
	explicit Actor(Perturb::App& App);
	
  protected:
	Perturb::App& app_;
	
  virtual bool ToggleTokenCheck(); 
  
  int get_message_token();
  int get_message_input_id();
  
  virtual void doWork() {};
  virtual void Reset() {};
  Perturb::Address& get_message_from_address();
  
  void AddLinkMessageHandler(const Perturb::ActorAddLink& msg, const Theron::Address from);
  void RemoveLinkMessageHandler(const Perturb::ActorRemoveLink& msg, const Theron::Address from);
  virtual void TokenChanged(int Token) {};
  void SyncMessageHandler(const Perturb::ActorSyncRequestMessage& msg, const Theron::Address from);

  template <typename T, typename FunType>
  bool AddInputHandler(FunType * const container, void (FunType::*callback)(const T&), int InputID)
  {
		static std::type_index Type = typeid(T);
    /*Insert Function into correct map by type hash code*/
    std::unordered_map<int, std::function<void(const void *)> >& map = input_map_[Type.hash_code()];
    
    /*This lambda function uses closure to enable pretty callbacks*/
    map[InputID] = [container, callback] (const void * a) -> void 
		{
			T& value = *(T*)(a);
			(container->*callback)(value);
		};

    /*Register Handler for this type if not already done*/
    if(IsHandlerRegistered(this, &Perturb::Actor::InputMessageHandler<T>) != true)
      return RegisterHandler(this, &Perturb::Actor::InputMessageHandler<T>);

    return true;
  }

  template <typename T>
  bool RemoveInputHandler(int InputID)
  {
    static std::type_index Type = typeid(T);
    try {
      std::unordered_map<int, std::function<void(const void *)> >& map = this->input_map_.at(Type.hash_code());
      map.erase(InputID); 
      return true;
    } catch (const std::out_of_range& oor) {return false; }
  }

  template <typename T>
  void InputMessageHandler(const Perturb::ActorInputMessage<T>& msg,  const Theron::Address from)
  {
    static std::type_index Type = typeid(T);
    if(msg.Token != this->token_ && this->token_check_ == true)
      return ;
    try {
      std::unordered_map<int,  std::function<void(const void *)> >& map = this->input_map_.at(Type.hash_code());
      std::function<void(const void *)> F = map.at(msg.InputID);
      this->message_from_address_ = from;
      this->message_token_ = msg.Token;
      this->message_input_id_ = msg.InputID;
      F((const void *)&msg.Value);
    } catch (const std::out_of_range& oor) {}
      this->doWork();
  }

  template <typename T>
  bool AddOutput(int OutputID)
  {
    static std::type_index Type = typeid(T);
    std::unordered_map<int, std::deque<std::pair<int, Perturb::Address> > >& map = this->output_map_[Type.hash_code()];
    std::deque<std::pair<int, Perturb::Address> > que;
    map[OutputID] = que;

    return true;
  }

  template <typename T>    
  bool RemoveOutput(int OutputID)
  {
    static std::type_index Type = typeid(T);
    try {
      std::unordered_map<int, std::deque<std::pair<int, Perturb::Address> > >& map = this->output_map_.at(Type.hash_code());
      map.erase(OutputID);
    } catch (const std::out_of_range& oor) {}

    return true;
  }

  template <typename T>
  bool WriteToOutput(const T& value, int OutputID)
  {
    return this->WriteToOutput<T>(value, OutputID, token_);
  }

  template <typename T>
  bool WriteToOutput(const T& value, int OutputID, int Token)
  {
    static std::type_index Type = typeid(T);
    try {
      std::unordered_map<int, std::deque<std::pair<int, Perturb::Address> > >& map = this->output_map_.at(Type.hash_code());
      std::deque<std::pair<int, Perturb::Address> >& list = map.at(OutputID);
      std::for_each(list.begin(), list.end(), [&] (const std::pair<int, Perturb::Address>& a) -> void 
        {
          Perturb::ActorInputMessage<T> msg;
					msg.InputID = a.first;
					msg.Value = value;
					msg.Token = Token;
          this->Send(msg, a.second);
        });
    } catch (const std::out_of_range& oor) {return false;}
    return true;   
  }
};



};
#endif

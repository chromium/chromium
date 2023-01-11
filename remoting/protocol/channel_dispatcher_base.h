// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_
#define REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/message_pipe.h"

namespace remoting {

class CompoundBuffer;

namespace protocol {

class MessageChannelFactory;

// Base class for channel message dispatchers. It's responsible for
// creating the named channel. Derived dispatchers then dispatch
// incoming messages on this channel as well as send outgoing
// messages.
class ChannelDispatcherBase : public MessagePipe::EventHandler {
 public:
  class EventHandler {
   public:
    EventHandler() {}
    virtual ~EventHandler() {}

    // Called after the channel is initialized.
    virtual void OnChannelInitialized(
        ChannelDispatcherBase* channel_dispatcher) = 0;

    // Called after the channel is closed.
    virtual void OnChannelClosed(ChannelDispatcherBase* channel_dispatcher) = 0;
  };

  ChannelDispatcherBase(const ChannelDispatcherBase&) = delete;
  ChannelDispatcherBase& operator=(const ChannelDispatcherBase&) = delete;

  ~ChannelDispatcherBase() override;

  // Creates and connects the channel using |channel_factory|.
  void Init(MessageChannelFactory* channel_factory,
            EventHandler* event_handler);

  // Initializes the channel using |message_pipe| that's already connected.
  void Init(std::unique_ptr<MessagePipe> message_pipe,
            EventHandler* event_handler);

  const std::string& channel_name() { return channel_name_; }

  // Returns true if the channel is currently connected.
  bool is_connected() { return is_connected_; }

 protected:
  explicit ChannelDispatcherBase(const std::string& channel_name);

  MessagePipe* message_pipe() { return message_pipe_.get(); }

  // Child classes must override this method to handle incoming messages.
  virtual void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) = 0;

 private:
  void OnChannelReady(std::unique_ptr<MessagePipe> message_pipe);

  // MessagePipe::EventHandler interface.
  void OnMessagePipeOpen() override;
  void OnMessageReceived(std::unique_ptr<CompoundBuffer> message) override;
  void OnMessagePipeClosed() override;

  std::string channel_name_;
  raw_ptr<MessageChannelFactory> channel_factory_ = nullptr;
  raw_ptr<EventHandler> event_handler_ = nullptr;
  bool is_connected_ = false;

  std::unique_ptr<MessagePipe> message_pipe_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_

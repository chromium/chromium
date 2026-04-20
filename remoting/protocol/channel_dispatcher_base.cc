// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_dispatcher_base.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_pipe.h"

namespace remoting::protocol {

ChannelDispatcherBase::ChannelDispatcherBase(const std::string& channel_name)
    : channel_name_(channel_name) {}

ChannelDispatcherBase::~ChannelDispatcherBase() = default;

void ChannelDispatcherBase::Init(std::unique_ptr<MessagePipe> message_pipe,
                                 EventHandler* event_handler) {
  event_handler_ = event_handler;
  OnChannelReady(std::move(message_pipe));
}

void ChannelDispatcherBase::OnChannelReady(
    std::unique_ptr<MessagePipe> message_pipe) {
  message_pipe_ = std::move(message_pipe);
  message_pipe_->Start(this);
}

void ChannelDispatcherBase::OnMessagePipeOpen() {
  DCHECK(!is_connected_);
  is_connected_ = true;
  event_handler_->OnChannelInitialized(this);
}

void ChannelDispatcherBase::OnMessageReceived(
    std::unique_ptr<CompoundBuffer> message) {
  OnIncomingMessage(std::move(message));
}

void ChannelDispatcherBase::OnMessagePipeClosed() {
  is_connected_ = false;
  message_pipe_.reset();
  event_handler_->OnChannelClosed(this);
}

}  // namespace remoting::protocol

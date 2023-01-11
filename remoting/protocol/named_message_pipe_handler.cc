// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/named_message_pipe_handler.h"

#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "remoting/base/compound_buffer.h"

namespace remoting::protocol {

NamedMessagePipeHandler::NamedMessagePipeHandler(
    const std::string& name,
    std::unique_ptr<MessagePipe> pipe)
    : name_(name), pipe_(std::move(pipe)) {
  DCHECK(pipe_);
  pipe_->Start(this);
}

NamedMessagePipeHandler::~NamedMessagePipeHandler() = default;

void NamedMessagePipeHandler::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (connected()) {
    OnDisconnecting();
    is_connected_ = false;
  }
  delete this;
}

void NamedMessagePipeHandler::Send(const google::protobuf::MessageLite& message,
                                   base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(connected());
  pipe_->Send(const_cast<google::protobuf::MessageLite*>(&message),
              std::move(done));
}

void NamedMessagePipeHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {}

void NamedMessagePipeHandler::OnConnected() {}

void NamedMessagePipeHandler::OnDisconnecting() {}

void NamedMessagePipeHandler::OnMessagePipeOpen() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_connected_);
  is_connected_ = true;
  OnConnected();
}

void NamedMessagePipeHandler::OnMessageReceived(
    std::unique_ptr<CompoundBuffer> message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OnIncomingMessage(std::move(message));
}

void NamedMessagePipeHandler::OnMessagePipeClosed() {
  Close();
}

}  // namespace remoting::protocol

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_message_pipe_wrapper.h"

#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/fake_message_pipe.h"

namespace remoting {
namespace protocol {

FakeMessagePipeWrapper::FakeMessagePipeWrapper(FakeMessagePipe* pipe)
    : pipe_(pipe) {
  DCHECK(pipe_);
}

FakeMessagePipeWrapper::~FakeMessagePipeWrapper() = default;

void FakeMessagePipeWrapper::Start(EventHandler* event_handler) {
  pipe_->Start(event_handler);
}

void FakeMessagePipeWrapper::Send(google::protobuf::MessageLite* message,
                                  base::OnceClosure done) {
  pipe_->Send(message, std::move(done));
}

void FakeMessagePipeWrapper::Receive(std::unique_ptr<CompoundBuffer> message) {
  pipe_->Receive(std::move(message));
}

void FakeMessagePipeWrapper::OpenPipe() {
  pipe_->OpenPipe();
}

void FakeMessagePipeWrapper::ClosePipe() {
  pipe_->ClosePipe();
}

}  // namespace protocol
}  // namespace remoting

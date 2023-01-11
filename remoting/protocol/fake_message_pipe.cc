// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_message_pipe.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting::protocol {

FakeMessagePipe::FakeMessagePipe(bool asynchronous)
    : asynchronous_(asynchronous) {}

FakeMessagePipe::~FakeMessagePipe() = default;

std::unique_ptr<FakeMessagePipeWrapper> FakeMessagePipe::Wrap() {
  auto wrapper = std::make_unique<FakeMessagePipeWrapper>(this);
  wrappers_.push_back(wrapper->GetWeakPtr());
  return wrapper;
}

void FakeMessagePipe::Start(EventHandler* event_handler) {
  ASSERT_TRUE(event_handler_ == nullptr);
  ASSERT_TRUE(event_handler != nullptr);
  event_handler_ = event_handler;
}

void FakeMessagePipe::Send(google::protobuf::MessageLite* message,
                           base::OnceClosure done) {
  if (asynchronous_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FakeMessagePipe* me, google::protobuf::MessageLite* message,
               base::OnceClosure done) {
              me->SendImpl(message, std::move(done));
            },
            base::Unretained(this), base::Unretained(message),
            std::move(done)));
    return;
  }
  SendImpl(message, std::move(done));
}

void FakeMessagePipe::Receive(std::unique_ptr<CompoundBuffer> message) {
  if (asynchronous_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FakeMessagePipe* me, std::unique_ptr<CompoundBuffer> message) {
              me->ReceiveImpl(std::move(message));
            },
            base::Unretained(this), std::move(message)));
    return;
  }

  ReceiveImpl(std::move(message));
}

void FakeMessagePipe::ReceiveProtobufMessage(
    const google::protobuf::MessageLite& message) {
  auto buffer = std::make_unique<CompoundBuffer>();
  std::string data = message.SerializeAsString();
  buffer->AppendCopyOf(data.data(), data.size());
  Receive(std::move(buffer));
}

void FakeMessagePipe::OpenPipe() {
  if (asynchronous_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce([](FakeMessagePipe* me) { me->OpenPipeImpl(); },
                       base::Unretained(this)));
    return;
  }

  OpenPipeImpl();
}

void FakeMessagePipe::ClosePipe() {
  if (asynchronous_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce([](FakeMessagePipe* me) { me->ClosePipeImpl(); },
                       base::Unretained(this)));
    return;
  }

  ClosePipeImpl();
}

bool FakeMessagePipe::HasWrappers() const {
  auto* wrappers =
      const_cast<std::vector<base::WeakPtr<FakeMessagePipeWrapper>>*>(
          &wrappers_);
  wrappers->erase(std::remove_if(wrappers->begin(), wrappers->end(),
                                 [](const auto& weak_ptr) {
                                   return weak_ptr.get() == nullptr;
                                 }),
                  wrappers->end());
  return !wrappers->empty();
}

void FakeMessagePipe::SendImpl(google::protobuf::MessageLite* message,
                               base::OnceClosure done) {
  ASSERT_TRUE(pipe_opened_);

  std::string message_string;
  message->SerializeToString(&message_string);
  sent_messages_.push(message_string);

  if (done) {
    std::move(done).Run();
  }
}

void FakeMessagePipe::ReceiveImpl(std::unique_ptr<CompoundBuffer> message) {
  ASSERT_TRUE(pipe_opened_);
  ASSERT_TRUE(event_handler_ != nullptr);
  if (message) {
    message->Lock();
  }
  event_handler_->OnMessageReceived(std::move(message));
}

void FakeMessagePipe::OpenPipeImpl() {
  ASSERT_FALSE(pipe_opened_);
  ASSERT_TRUE(event_handler_ != nullptr);
  pipe_opened_ = true;
  event_handler_->OnMessagePipeOpen();
}

void FakeMessagePipe::ClosePipeImpl() {
  ASSERT_TRUE(pipe_opened_);
  ASSERT_TRUE(event_handler_ != nullptr);
  pipe_opened_ = false;
  event_handler_->OnMessagePipeClosed();
}

}  // namespace remoting::protocol

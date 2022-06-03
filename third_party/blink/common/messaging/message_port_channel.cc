// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/message_port_channel.h"

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace blink {

class MessagePortChannel::State : public base::RefCountedThreadSafe<State> {
 public:
  State();
  explicit State(MessagePortDescriptor handle);

  MessagePortDescriptor TakeHandle();

  const MessagePortDescriptor& handle() const { return handle_; }

 private:
  friend class base::RefCountedThreadSafe<State>;
  ~State();

  // Guards access to the fields below.
  base::Lock lock_;

  MessagePortDescriptor handle_;
};

MessagePortChannel::~MessagePortChannel() = default;

MessagePortChannel::MessagePortChannel() : state_(new State()) {}

MessagePortChannel::MessagePortChannel(const MessagePortChannel& other) =
    default;

MessagePortChannel& MessagePortChannel::operator=(
    const MessagePortChannel& other) {
  state_ = other.state_;
  return *this;
}

MessagePortChannel::MessagePortChannel(MessagePortDescriptor handle)
    : state_(new State(std::move(handle))) {}

const MessagePortDescriptor& MessagePortChannel::GetHandle() const {
  return state_->handle();
}

MessagePortDescriptor MessagePortChannel::ReleaseHandle() const {
  return state_->TakeHandle();
}

// static
std::vector<MessagePortDescriptor> MessagePortChannel::ReleaseHandles(
    const std::vector<MessagePortChannel>& ports) {
  std::vector<MessagePortDescriptor> handles(ports.size());
  for (size_t i = 0; i < ports.size(); ++i)
    handles[i] = ports[i].ReleaseHandle();
  return handles;
}

// static
std::vector<MessagePortChannel> MessagePortChannel::CreateFromHandles(
    std::vector<MessagePortDescriptor> handles) {
  std::vector<MessagePortChannel> ports(handles.size());
  for (size_t i = 0; i < handles.size(); ++i)
    ports[i] = MessagePortChannel(std::move(handles[i]));
  return ports;
}

MessagePortChannel::State::State() = default;

MessagePortChannel::State::State(MessagePortDescriptor handle)
    : handle_(std::move(handle)) {}

MessagePortDescriptor MessagePortChannel::State::TakeHandle() {
  base::AutoLock lock(lock_);
  return std::move(handle_);
}

MessagePortChannel::State::~State() = default;

}  // namespace blink

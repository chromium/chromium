// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/message_port_channel.h"

#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
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
    const MessagePortChannel& other) = default;

MessagePortChannel::MessagePortChannel(MessagePortChannel&& other) noexcept =
    default;

MessagePortChannel& MessagePortChannel::operator=(
    MessagePortChannel&& other) noexcept = default;

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
  return base::ToVector(ports, &MessagePortChannel::ReleaseHandle);
}

// static
std::vector<MessagePortChannel> MessagePortChannel::CreateFromHandles(
    std::vector<MessagePortDescriptor> handles) {
  return base::ToVector(handles, [](MessagePortDescriptor& handle) {
    return MessagePortChannel(std::move(handle));
  });
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

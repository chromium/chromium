// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements `MojoResponderWrapper` and `ResponderHolder` for thread-safe C++
// IPC response delivery.

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/mojo_responder_wrapper.h"

#include <utility>

namespace mojo::rust::bindings {

// Concrete target object held inside `base::SequenceBound<ResponderHolder>`.
// Accepts incoming message wrappers from Rust, converts them into C++
// `mojo::Message` instances, and calls `MessageReceiverWithStatus::Accept` on
// the bound sequence.
class MojoResponderWrapper::ResponderHolder {
 public:
  explicit ResponderHolder(
      std::unique_ptr<mojo::MessageReceiverWithStatus> responder)
      : responder_(std::move(responder)) {}

  void Accept(
      std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper> msg_wrapper) {
    if (!responder_ || !msg_wrapper) {
      return;
    }
    mojo::ScopedMessageHandle handle = msg_wrapper->take_handle();
    mojo::Message message = mojo::Message::CreateFromMessageHandle(&handle);
    std::ignore = responder_->Accept(&message);
  }

 private:
  std::unique_ptr<mojo::MessageReceiverWithStatus> responder_;
};

MojoResponderWrapper::MojoResponderWrapper(
    std::unique_ptr<mojo::MessageReceiverWithStatus> responder,
    scoped_refptr<base::SequencedTaskRunner> runner)
    : responder_(runner, std::move(responder)) {}

MojoResponderWrapper::~MojoResponderWrapper() = default;

bool MojoResponderWrapper::Accept(
    std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper> message_wrapper)
    const {
  if (!message_wrapper) {
    return false;
  }
  responder_.AsyncCall(&ResponderHolder::Accept)
      .WithArgs(std::move(message_wrapper));
  return true;
}

}  // namespace mojo::rust::bindings

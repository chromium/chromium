// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/thread_safe_forwarder_base.h"

#include <utility>

namespace mojo {
namespace internal {

ThreadSafeForwarderBase::ThreadSafeForwarderBase(
    scoped_refptr<ThreadSafeProxy> proxy)
    : proxy_(std::move(proxy)) {}

ThreadSafeForwarderBase::~ThreadSafeForwarderBase() = default;

bool ThreadSafeForwarderBase::PrefersSerializedMessages() {
  // NOTE: This means SharedRemote etc will ignore lazy serialization hints and
  // will always eagerly serialize messages.
  return true;
}

bool ThreadSafeForwarderBase::Accept(Message* message) {
  proxy_->SendMessage(*message);
  return true;
}

bool ThreadSafeForwarderBase::AcceptWithResponder(
    Message* message,
    std::unique_ptr<MessageReceiver> responder) {
  proxy_->SendMessageWithResponder(*message, std::move(responder));
  return true;
}

}  // namespace internal
}  // namespace mojo

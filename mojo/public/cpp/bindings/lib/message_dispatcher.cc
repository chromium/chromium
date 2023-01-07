// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message_dispatcher.h"

#include <algorithm>

#include "base/check.h"

namespace mojo {

MessageDispatcher::MessageDispatcher(MessageReceiver* sink) : sink_(sink) {}

MessageDispatcher::MessageDispatcher(MessageDispatcher&& other) {
  *this = std::move(other);
}

MessageDispatcher& MessageDispatcher::operator=(MessageDispatcher&& other) {
  std::swap(sink_, other.sink_);
  validator_ = std::move(other.validator_);
  filter_ = std::move(other.filter_);
  return *this;
}

MessageDispatcher::~MessageDispatcher() {}

void MessageDispatcher::SetSink(MessageReceiver* sink) {
  DCHECK(!sink_);
  sink_ = sink;
}

bool MessageDispatcher::Accept(Message* message) {
  internal::MessageDispatchContext dispatch_context(message);

  DCHECK(sink_);
  if (validator_) {
    if (!validator_->Accept(message))
      return false;
  }

  if (!filter_)
    return sink_->Accept(message);

  base::WeakPtr<MessageDispatcher> weak_self = weak_factory_.GetWeakPtr();
  if (!filter_->WillDispatch(message))
    return false;
  bool result = sink_->Accept(message);
  if (!weak_self)
    return result;
  filter_->DidDispatchOrReject(message, result);
  return result;
}

void MessageDispatcher::SetValidator(
    std::unique_ptr<MessageReceiver> validator) {
  DCHECK(!validator_);
  validator_ = std::move(validator);
}

void MessageDispatcher::SetFilter(std::unique_ptr<MessageFilter> filter) {
  DCHECK(!filter_);
  filter_ = std::move(filter);
}

}  // namespace mojo

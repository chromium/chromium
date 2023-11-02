// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/message_queue.h"

#include "base/check.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace test {

MessageQueue::MessageQueue() {
}

MessageQueue::~MessageQueue() {
}

bool MessageQueue::IsEmpty() const {
  return queue_.empty();
}

void MessageQueue::Push(Message* message) {
  queue_.emplace(std::move(*message));
}

void MessageQueue::Pop(Message* message) {
  DCHECK(!queue_.empty());
  *message = std::move(queue_.front());
  Pop();
}

void MessageQueue::Pop() {
  DCHECK(!queue_.empty());
  queue_.pop();
}

}  // namespace test
}  // namespace mojo

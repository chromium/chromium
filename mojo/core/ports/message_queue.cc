// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/message_queue.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/core/ports/message_filter.h"

namespace mojo {
namespace core {
namespace ports {

// Used by std::{push,pop}_heap functions
inline bool operator<(const std::unique_ptr<UserMessageEvent>& a,
                      const std::unique_ptr<UserMessageEvent>& b) {
  return a->sequence_num() > b->sequence_num();
}

MessageQueue::MessageQueue() : MessageQueue(kInitialSequenceNum) {}

MessageQueue::MessageQueue(uint64_t next_sequence_num)
    : next_sequence_num_(next_sequence_num) {
  // The message queue is blocked waiting for a message with sequence number
  // equal to |next_sequence_num|.
}

MessageQueue::~MessageQueue() {
#if DCHECK_IS_ON()
  size_t num_leaked_ports = 0;
  for (const auto& message : heap_)
    num_leaked_ports += message->num_ports();
  DVLOG_IF(1, num_leaked_ports > 0)
      << "Leaking " << num_leaked_ports << " ports in unreceived messages";
#endif
}

bool MessageQueue::HasNextMessage() const {
  return !heap_.empty() && heap_[0]->sequence_num() == next_sequence_num_;
}

void MessageQueue::GetNextMessage(std::unique_ptr<UserMessageEvent>* message,
                                  MessageFilter* filter) {
  if (!HasNextMessage() || (filter && !filter->Match(*heap_[0]))) {
    message->reset();
    return;
  }

  std::pop_heap(heap_.begin(), heap_.end());
  *message = std::move(heap_.back());
  total_queued_bytes_ -= (*message)->GetSizeIfSerialized();
  heap_.pop_back();

  // We keep the capacity of |heap_| in check so that a large batch of incoming
  // messages doesn't permanently wreck available memory. The choice of interval
  // here is somewhat arbitrary.
  constexpr size_t kHeapMinimumShrinkSize = 16;
  constexpr size_t kHeapShrinkInterval = 512;
  if (UNLIKELY(heap_.size() > kHeapMinimumShrinkSize &&
               heap_.size() % kHeapShrinkInterval == 0)) {
    heap_.shrink_to_fit();
  }

  next_sequence_num_++;
}

void MessageQueue::AcceptMessage(std::unique_ptr<UserMessageEvent> message,
                                 bool* has_next_message) {
  // TODO: Handle sequence number roll-over.

  total_queued_bytes_ += message->GetSizeIfSerialized();
  heap_.emplace_back(std::move(message));
  std::push_heap(heap_.begin(), heap_.end());

  if (!signalable_) {
    *has_next_message = false;
  } else {
    *has_next_message = (heap_[0]->sequence_num() == next_sequence_num_);
  }
}

void MessageQueue::TakeAllMessages(
    std::vector<std::unique_ptr<UserMessageEvent>>* messages) {
  *messages = std::move(heap_);
  total_queued_bytes_ = 0;
}

}  // namespace ports
}  // namespace core
}  // namespace mojo

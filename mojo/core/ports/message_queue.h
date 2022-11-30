// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_MESSAGE_QUEUE_H_
#define MOJO_CORE_PORTS_MESSAGE_QUEUE_H_

#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "mojo/core/ports/event.h"

namespace mojo {
namespace core {
namespace ports {

constexpr uint64_t kInitialSequenceNum = 1;
constexpr uint64_t kInvalidSequenceNum = std::numeric_limits<uint64_t>::max();

class MessageFilter;

// An incoming message queue for a port. MessageQueue keeps track of the highest
// known sequence number and can indicate whether the next sequential message is
// available. Thus the queue enforces message ordering for the consumer without
// enforcing it for the producer (see AcceptMessage() below.)
class COMPONENT_EXPORT(MOJO_CORE_PORTS) MessageQueue {
 public:
  explicit MessageQueue();
  explicit MessageQueue(uint64_t next_sequence_num);

  MessageQueue(const MessageQueue&) = delete;
  MessageQueue& operator=(const MessageQueue&) = delete;

  ~MessageQueue();

  void set_signalable(bool value) { signalable_ = value; }

  uint64_t next_sequence_num() const { return next_sequence_num_; }

  bool HasNextMessage() const;

  // Gives ownership of the message. If |filter| is non-null, the next message
  // will only be retrieved if the filter successfully matches it.
  // Need to call |MessageProcessed| after processing is finished.
  void GetNextMessage(std::unique_ptr<UserMessageEvent>* message,
                      MessageFilter* filter);

  // Mark the message from |GetNextMessage| as processed.
  void MessageProcessed();

  // Takes ownership of the message. Note: Messages are ordered, so while we
  // have added a message to the queue, we may still be waiting on a message
  // ahead of this one before we can let any of the messages be returned by
  // GetNextMessage.
  //
  // Furthermore, once has_next_message is set to true, it will remain false
  // until GetNextMessage is called enough times to return a null message.
  // In other words, has_next_message acts like an edge trigger.
  //
  void AcceptMessage(std::unique_ptr<UserMessageEvent> message,
                     bool* has_next_message);

  // Takes all messages from this queue. Used to safely destroy queued messages
  // without holding any Port lock.
  void TakeAllMessages(
      std::vector<std::unique_ptr<UserMessageEvent>>* messages);

  // The number of messages queued here, regardless of whether the next expected
  // message has arrived yet.
  size_t queued_message_count() const { return heap_.size(); }

  // The aggregate memory size in bytes of all messages queued here, regardless
  // of whether the next expected message has arrived yet.
  size_t queued_num_bytes() const { return total_queued_bytes_; }

 private:
  std::vector<std::unique_ptr<UserMessageEvent>> heap_;
  uint64_t next_sequence_num_;
  bool signalable_ = true;
  size_t total_queued_bytes_ = 0;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_MESSAGE_QUEUE_H_

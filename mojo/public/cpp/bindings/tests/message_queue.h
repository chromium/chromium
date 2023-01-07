// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_MESSAGE_QUEUE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_MESSAGE_QUEUE_H_

#include "base/containers/queue.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace test {

// A queue for Message objects.
class MessageQueue {
 public:
  MessageQueue();

  MessageQueue(const MessageQueue&) = delete;
  MessageQueue& operator=(const MessageQueue&) = delete;

  ~MessageQueue();

  bool IsEmpty() const;

  // This method copies the message data and steals ownership of its handles.
  void Push(Message* message);

  // Removes the next message from the queue, copying its data and transferring
  // ownership of its handles to the given |message|.
  void Pop(Message* message);

  size_t size() const { return queue_.size(); }

 private:
  void Pop();

  base::queue<Message> queue_;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_MESSAGE_QUEUE_H_

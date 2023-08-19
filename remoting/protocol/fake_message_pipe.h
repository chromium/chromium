// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_MESSAGE_PIPE_H_
#define REMOTING_PROTOCOL_FAKE_MESSAGE_PIPE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/message_pipe.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting::protocol {

class FakeMessagePipeWrapper;

class FakeMessagePipe final : public MessagePipe {
 public:
  explicit FakeMessagePipe(bool asynchronous);
  ~FakeMessagePipe() override;

  // Creates an std::unique_ptr<FakeMessagePipeWrapper> instance to wrap |this|.
  // All operations will be forwarded to |this| except for the destructor.
  //
  // Most of the components take ownership of std::unique_ptr<MessagePipe>,
  // which makes the test cases hard to maintain the lifetime of a
  // FakeMessagePipe. So this function creates a "weak" unique_ptr of this
  // instance to let the test case decide the lifetime of a FakeMessagePipe.
  std::unique_ptr<FakeMessagePipeWrapper> Wrap();

  // MessagePipe implementation.
  void Start(EventHandler* event_handler) override;
  void Send(google::protobuf::MessageLite* message,
            base::OnceClosure done) override;

  // Forwards |message| to EventHandler.
  void Receive(std::unique_ptr<CompoundBuffer> message);

  // Similar to Receive(), but takes a protobuf and serializes it before sending
  // it to EventHandler.
  void ReceiveProtobufMessage(const google::protobuf::MessageLite& message);

  // Simulates the operation to open the pipe.
  void OpenPipe();

  // Simulates the operation to close the pipe.
  void ClosePipe();

  // Returns true if there is at least one valid wrapper that hasn't been
  // destroyed.
  bool HasWrappers() const;

  // Returns all messages sent using Send().
  const base::queue<std::string>& sent_messages() { return sent_messages_; }

  bool pipe_opened() const { return pipe_opened_; }

 private:
  void SendImpl(google::protobuf::MessageLite* message, base::OnceClosure done);
  void ReceiveImpl(std::unique_ptr<CompoundBuffer> message);
  void OpenPipeImpl();
  void ClosePipeImpl();

  const bool asynchronous_;
  bool pipe_opened_ = false;
  raw_ptr<EventHandler, AcrossTasksDanglingUntriaged> event_handler_ = nullptr;
  base::queue<std::string> sent_messages_;
  std::vector<base::WeakPtr<FakeMessagePipeWrapper>> wrappers_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FAKE_MESSAGE_PIPE_H_

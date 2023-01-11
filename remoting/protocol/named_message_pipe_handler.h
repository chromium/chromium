// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NAMED_MESSAGE_PIPE_HANDLER_H_
#define REMOTING_PROTOCOL_NAMED_MESSAGE_PIPE_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/message_pipe.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting {

class CompoundBuffer;

namespace protocol {

// A base class to handle data from a named MessagePipe. This class manages the
// lifetime itself: it deletes itself once the MessagePipe is closed or the
// derived instance actively calls Close() function.
class NamedMessagePipeHandler : public MessagePipe::EventHandler {
 protected:
  // The callers should create instances of derived classes instead of this
  // class. So hide the constructor.
  NamedMessagePipeHandler(const std::string& name,
                          std::unique_ptr<MessagePipe> pipe);

  ~NamedMessagePipeHandler() override;

  // Closes the channel and eventually destructs this instance. No operations
  // should be performed after executing this function.
  void Close();

  const std::string& pipe_name() const { return name_; }

  // Whether |pipe_| has been connected.
  bool connected() const { return is_connected_; }

  // Sends the message through the pipe. This function should only be called
  // once connected() returns true. See comments of
  // remoting::protocol::MessagePipe::Send() for details.
  void Send(const google::protobuf::MessageLite& message,
            base::OnceClosure done);

  // Derived classes can override these functions to receive data from the
  // connection or observe the connection state. These functions will not be
  // called unless |pipe_| has been opened.
  virtual void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message);
  virtual void OnConnected();
  virtual void OnDisconnecting();

 private:
  friend class base::DeleteHelper<NamedMessagePipeHandler>;

  // MessagePipe::EventHandler implementation.
  void OnMessagePipeOpen() override;
  void OnMessageReceived(std::unique_ptr<CompoundBuffer> message) override;
  void OnMessagePipeClosed() override;

  const std::string name_;
  std::unique_ptr<MessagePipe> pipe_;
  bool is_connected_ = false;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_NAMED_MESSAGE_PIPE_HANDLER_H_

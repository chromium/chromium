// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MESSAGE_PIPE_H_
#define REMOTING_PROTOCOL_MESSAGE_PIPE_H_

#include <memory>

#include "base/functional/callback_forward.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting {

class CompoundBuffer;

namespace protocol {

// Represents a bi-directional pipe that allows to send and receive messages.
class MessagePipe {
 public:
  class EventHandler {
   public:
    // Called when the channel is open.
    virtual void OnMessagePipeOpen() = 0;

    // Called when a message is received.
    virtual void OnMessageReceived(std::unique_ptr<CompoundBuffer> message) = 0;

    // Called when the channel is closed.
    virtual void OnMessagePipeClosed() = 0;

   protected:
    virtual ~EventHandler() {}
  };

  virtual ~MessagePipe() {}

  // Starts the channel. Must be called immediately after MessagePipe is
  // created. |event_handler| will be notified when state of the pipe changes or
  // when a message is received.
  virtual void Start(EventHandler* event_handler) = 0;

  // Sends a message. |done| is called when the message has been sent to the
  // client, but it doesn't mean that the client has received it. |done| is
  // never called if the message is never sent (e.g. if the pipe is destroyed
  // before the message is sent).
  // |message| is guaranteed to be used synchronously. It won't be referred
  // after this function returns.
  // TODO(zijiehe): the |message| should be const-ref.
  virtual void Send(google::protobuf::MessageLite* message,
                    base::OnceClosure done) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MESSAGE_PIPE_H_

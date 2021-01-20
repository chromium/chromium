// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_MESSAGE_PIPE_WRAPPER_H_
#define REMOTING_PROTOCOL_FAKE_MESSAGE_PIPE_WRAPPER_H_

#include <memory>

#include "remoting/protocol/message_pipe.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting {
namespace protocol {

class FakeMessagePipe;

// This class should not be used explicitly: use FakeMessagePipe::Wrap().
class FakeMessagePipeWrapper final : public MessagePipe {
 public:
  // |pipe| must outlive this instance.
  explicit FakeMessagePipeWrapper(FakeMessagePipe* pipe);
  ~FakeMessagePipeWrapper() override;

  // MessagePipe implementation.
  void Start(EventHandler* event_handler) override;
  void Send(google::protobuf::MessageLite* message,
            base::OnceClosure done) override;

  void Receive(std::unique_ptr<CompoundBuffer> message);
  void OpenPipe();
  void ClosePipe();

 private:
  FakeMessagePipe* const pipe_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_MESSAGE_PIPE_WRAPPER_H_

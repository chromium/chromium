// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STREAM_MESSAGE_PIPE_ADAPTER_H_
#define REMOTING_PROTOCOL_STREAM_MESSAGE_PIPE_ADAPTER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "remoting/protocol/message_channel_factory.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_reader.h"

namespace remoting {
class BufferedSocketWriter;

namespace protocol {

class P2PStreamSocket;
class StreamChannelFactory;

// MessagePipe implementation that sends and receives messages over a
// P2PStreamSocket.
class StreamMessagePipeAdapter : public MessagePipe {
 public:
  typedef base::OnceCallback<void(int)> ErrorCallback;

  StreamMessagePipeAdapter(std::unique_ptr<P2PStreamSocket> socket,
                           ErrorCallback error_callback);

  StreamMessagePipeAdapter(const StreamMessagePipeAdapter&) = delete;
  StreamMessagePipeAdapter& operator=(const StreamMessagePipeAdapter&) = delete;

  ~StreamMessagePipeAdapter() override;

  // MessagePipe interface.
  void Start(EventHandler* event_handler) override;
  void Send(google::protobuf::MessageLite* message,
            base::OnceClosure done) override;

 private:
  void CloseOnError(int error);

  raw_ptr<EventHandler, DanglingUntriaged> event_handler_ = nullptr;

  std::unique_ptr<P2PStreamSocket> socket_;
  ErrorCallback error_callback_;

  std::unique_ptr<MessageReader> reader_;
  std::unique_ptr<BufferedSocketWriter> writer_;
};

class StreamMessageChannelFactoryAdapter : public MessageChannelFactory {
 public:
  typedef base::RepeatingCallback<void(int)> ErrorCallback;

  StreamMessageChannelFactoryAdapter(
      StreamChannelFactory* stream_channel_factory,
      const ErrorCallback& error_callback);

  StreamMessageChannelFactoryAdapter(
      const StreamMessageChannelFactoryAdapter&) = delete;
  StreamMessageChannelFactoryAdapter& operator=(
      const StreamMessageChannelFactoryAdapter&) = delete;

  ~StreamMessageChannelFactoryAdapter() override;

  // MessageChannelFactory interface.
  void CreateChannel(const std::string& name,
                     ChannelCreatedCallback callback) override;
  void CancelChannelCreation(const std::string& name) override;

 private:
  void OnChannelCreated(ChannelCreatedCallback callback,
                        std::unique_ptr<P2PStreamSocket> socket);

  raw_ptr<StreamChannelFactory, DanglingUntriaged> stream_channel_factory_;
  ErrorCallback error_callback_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_STREAM_MESSAGE_PIPE_ADAPTER_H_

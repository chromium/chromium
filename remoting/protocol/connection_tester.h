// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TESTER_H_
#define REMOTING_PROTOCOL_CONNECTION_TESTER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "remoting/protocol/message_pipe.h"

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
}  // namespace net

namespace remoting {

class CompoundBuffer;
class VideoPacket;

namespace protocol {

class P2PStreamSocket;

// This class is used by unit tests to verify that a connection
// between two sockets works properly, i.e. data is delivered from one
// end to the other.
class StreamConnectionTester {
 public:
  StreamConnectionTester(P2PStreamSocket* client_socket,
                         P2PStreamSocket* host_socket,
                         int message_size,
                         int message_count);
  ~StreamConnectionTester();

  void Start(base::OnceClosure on_done);
  void CheckResults();

 protected:
  void Done();
  void InitBuffers();
  void DoWrite();
  void OnWritten(int result);
  void HandleWriteResult(int result);
  void DoRead();
  void OnRead(int result);
  void HandleReadResult(int result);

 private:
  raw_ptr<P2PStreamSocket> host_socket_;
  raw_ptr<P2PStreamSocket> client_socket_;
  int message_size_;
  int test_data_size_;
  base::OnceClosure on_done_;

  scoped_refptr<net::DrainableIOBuffer> output_buffer_;
  scoped_refptr<net::GrowableIOBuffer> input_buffer_;

  int write_errors_;
  int read_errors_;
};

class MessagePipeConnectionTester : public MessagePipe::EventHandler {
 public:
  MessagePipeConnectionTester(MessagePipe* host_pipe,
                              MessagePipe* client_pipe,
                              int message_size,
                              int message_count);
  ~MessagePipeConnectionTester() override;

  void RunAndCheckResults();

 protected:
  // MessagePipe::EventHandler interface.
  void OnMessagePipeOpen() override;
  void OnMessageReceived(std::unique_ptr<CompoundBuffer> message) override;
  void OnMessagePipeClosed() override;

 private:
  class MessageSender;

  base::RunLoop run_loop_;
  raw_ptr<MessagePipe> client_pipe_;

  std::unique_ptr<MessageSender> sender_;

  std::vector<std::unique_ptr<VideoPacket>> received_messages_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TESTER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_tester.h"

#include "base/bind.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/p2p_datagram_socket.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

StreamConnectionTester::StreamConnectionTester(P2PStreamSocket* client_socket,
                                               P2PStreamSocket* host_socket,
                                               int message_size,
                                               int message_count)
    : host_socket_(host_socket),
      client_socket_(client_socket),
      message_size_(message_size),
      test_data_size_(message_size * message_count),
      write_errors_(0),
      read_errors_(0) {}

StreamConnectionTester::~StreamConnectionTester() = default;

void StreamConnectionTester::Start(base::OnceClosure on_done) {
  on_done_ = std::move(on_done);
  InitBuffers();
  DoRead();
  DoWrite();
}

void StreamConnectionTester::CheckResults() {
  EXPECT_EQ(0, write_errors_);
  EXPECT_EQ(0, read_errors_);

  ASSERT_EQ(test_data_size_, input_buffer_->offset());

  output_buffer_->SetOffset(0);
  ASSERT_EQ(test_data_size_, output_buffer_->size());

  EXPECT_EQ(0, memcmp(output_buffer_->data(),
                      input_buffer_->StartOfBuffer(), test_data_size_));
}

void StreamConnectionTester::Done() {
  std::move(on_done_).Run();
}

void StreamConnectionTester::InitBuffers() {
  output_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::IOBuffer>(test_data_size_), test_data_size_);
  for (int i = 0; i < test_data_size_; ++i) {
    output_buffer_->data()[i] = static_cast<char>(i);
  }

  input_buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
}

void StreamConnectionTester::DoWrite() {
  int result = 1;
  while (result > 0) {
    if (output_buffer_->BytesRemaining() == 0)
      break;

    int bytes_to_write = std::min(output_buffer_->BytesRemaining(),
                                  message_size_);
    result =
        client_socket_->Write(output_buffer_.get(), bytes_to_write,
                              base::BindOnce(&StreamConnectionTester::OnWritten,
                                             base::Unretained(this)),
                              TRAFFIC_ANNOTATION_FOR_TESTS);
    HandleWriteResult(result);
  }
}

void StreamConnectionTester::OnWritten(int result) {
  HandleWriteResult(result);
  DoWrite();
}

void StreamConnectionTester::HandleWriteResult(int result) {
  if (result <= 0 && result != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Received error " << result << " when trying to write";
    write_errors_++;
    Done();
  } else if (result > 0) {
    output_buffer_->DidConsume(result);
  }
}

void StreamConnectionTester::DoRead() {
  int result = 1;
  while (result > 0) {
    input_buffer_->SetCapacity(input_buffer_->offset() + message_size_);
    result = host_socket_->Read(input_buffer_.get(), message_size_,
                                base::BindOnce(&StreamConnectionTester::OnRead,
                                               base::Unretained(this)));
    HandleReadResult(result);
  };
}

void StreamConnectionTester::OnRead(int result) {
  HandleReadResult(result);
  if (!on_done_.is_null())
    DoRead();  // Don't try to read again when we are done reading.
}

void StreamConnectionTester::HandleReadResult(int result) {
  if (result <= 0 && result != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Received error " << result << " when trying to read";
    read_errors_++;
    Done();
  } else if (result > 0) {
    // Allocate memory for the next read.
    input_buffer_->set_offset(input_buffer_->offset() + result);
    if (input_buffer_->offset() == test_data_size_)
      Done();
  }
}

class MessagePipeConnectionTester::MessageSender
    : public MessagePipe::EventHandler {
 public:
  MessageSender(MessagePipe* pipe, int message_size, int message_count)
      : pipe_(pipe),
        message_size_(message_size),
        message_count_(message_count) {}

  void Start() { pipe_->Start(this); }

  const std::vector<std::unique_ptr<VideoPacket>>& sent_messages() {
    return sent_messages_;
  }

  // MessagePipe::EventHandler interface.
  void OnMessagePipeOpen() override {
    for (int i = 0; i < message_count_; ++i) {
      std::unique_ptr<VideoPacket> message(new VideoPacket());
      message->mutable_data()->resize(message_size_);
      for (int p = 0; p < message_size_; ++p) {
        message->mutable_data()[0] = static_cast<char>(i + p);
      }
      pipe_->Send(message.get(), {});
      sent_messages_.push_back(std::move(message));
    }
  }
  void OnMessageReceived(std::unique_ptr<CompoundBuffer> message) override {
    NOTREACHED();
  }
  void OnMessagePipeClosed() override { NOTREACHED(); }

 private:
  MessagePipe* pipe_;
  int message_size_;
  int message_count_;

  std::vector<std::unique_ptr<VideoPacket>> sent_messages_;
};

MessagePipeConnectionTester::MessagePipeConnectionTester(
    MessagePipe* host_pipe,
    MessagePipe* client_pipe,
    int message_size,
    int message_count)
    : client_pipe_(client_pipe),
      sender_(new MessageSender(host_pipe, message_size, message_count)) {}

MessagePipeConnectionTester::~MessagePipeConnectionTester() = default;

void MessagePipeConnectionTester::RunAndCheckResults() {
  sender_->Start();
  client_pipe_->Start(this);

  run_loop_.Run();

  ASSERT_EQ(sender_->sent_messages().size(), received_messages_.size());
  for (size_t i = 0; i < sender_->sent_messages().size(); ++i) {
    EXPECT_TRUE(sender_->sent_messages()[i]->data() ==
                received_messages_[i]->data());
  }
}

void MessagePipeConnectionTester::OnMessagePipeOpen() {}

void MessagePipeConnectionTester::OnMessageReceived(
    std::unique_ptr<CompoundBuffer> message) {
  received_messages_.push_back(ParseMessage<VideoPacket>(message.get()));
  if (received_messages_.size() >= sender_->sent_messages().size()) {
    run_loop_.Quit();
  }
}

void MessagePipeConnectionTester::OnMessagePipeClosed() {
  run_loop_.Quit();
  FAIL();
}

}  // namespace protocol
}  // namespace remoting

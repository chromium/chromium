// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/socket.h"

#include <optional>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/test/fake_bluetooth_socket.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluetooth {

class SocketTest : public testing::Test {
 public:
  SocketTest() = default;
  ~SocketTest() override = default;
  SocketTest(const SocketTest&) = delete;
  SocketTest& operator=(const SocketTest&) = delete;

  void SetUp() override {
    mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
    ASSERT_EQ(
        MOJO_RESULT_OK,
        mojo::CreateDataPipe(/*options=*/nullptr, receive_pipe_producer_handle,
                             receive_pipe_consumer_handle));

    mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(/*options=*/nullptr,
                                                   send_pipe_producer_handle,
                                                   send_pipe_consumer_handle));

    receive_stream_ = std::move(receive_pipe_consumer_handle);
    send_stream_ = std::move(send_pipe_producer_handle);

    fake_bluetooth_socket_ =
        base::MakeRefCounted<device::FakeBluetoothSocket>();
    socket_ = std::make_unique<Socket>(fake_bluetooth_socket_,
                                       std::move(receive_pipe_producer_handle),
                                       std::move(send_pipe_consumer_handle));
  }

  void VerifyReceiveAndRead(const std::string& message, bool success) {
    EXPECT_FALSE(receive_stream_->QuerySignalsState().never_readable());

    // Socket should only have 1 outstanding invocation of
    // BluetoothSocket::Receive().
    EXPECT_TRUE(fake_bluetooth_socket_->HasReceiveArgs());
    auto receive_args = fake_bluetooth_socket_->TakeReceiveArgs();
    EXPECT_FALSE(fake_bluetooth_socket_->HasReceiveArgs());

    int max_buffer_size = std::get<0>(*receive_args);
    EXPECT_GT(max_buffer_size, 0);

    // Attempting to read from |receive_stream_| before the BluetoothSocket
    // provides received data should signal a MOJO_RESULT_SHOULD_WAIT result.
    size_t buffer_size = static_cast<size_t>(max_buffer_size);
    std::vector<uint8_t> buffer1(buffer_size, 0xff);
    EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
              receive_stream_->ReadData(MOJO_READ_DATA_FLAG_NONE, buffer1,
                                        buffer_size));

    if (success) {
      // Emulate a successful response from the remote device on the other side
      // of the BluetoothSocket. |receive_stream_| should then be ready to be
      // read from.
      auto success_callback = std::move(std::get<1>(*receive_args));
      std::move(success_callback)
          .Run(
              /*num_bytes_received=*/message.size(),
              /*io_buffer=*/base::MakeRefCounted<net::StringIOBuffer>(message));

      std::vector<char> buffer2(max_buffer_size);
      EXPECT_EQ(MOJO_RESULT_OK,
                receive_stream_->ReadData(
                    MOJO_READ_DATA_FLAG_NONE,
                    base::as_writable_byte_span(buffer2).first(buffer_size),
                    buffer_size));
      std::string_view received_string =
          base::as_string_view(base::as_byte_span(buffer2).first(buffer_size));
      EXPECT_EQ(message, received_string);
    } else {
      // Emulate an error in the stack. We should not be able to read from
      // |receive_stream_|.
      auto error_callback = std::move(std::get<2>(*receive_args));
      std::move(error_callback)
          .Run(device::BluetoothSocket::ErrorReason::kSystemError, "Error");
    }

    // Socket should only invoke BluetoothSocket::Receive() if it received
    // a success callback from the previous invocation.
    EXPECT_EQ(success, fake_bluetooth_socket_->HasReceiveArgs());

    // |receive_stream_| should only remain readable if Socket received a
    // success callback.
    EXPECT_EQ(success, !receive_stream_->QuerySignalsState().never_readable());
  }

  void WriteAndVerifySend(const std::string& message, bool success) {
    EXPECT_FALSE(send_stream_->QuerySignalsState().never_writable());

    // Verify that Socket has not attempted to invoke BluetoothSocket::Send(),
    // because no bytes have been written over |send_stream_| yet.
    EXPECT_FALSE(fake_bluetooth_socket_->HasSendArgs());

    size_t actually_written_bytes = 0;
    EXPECT_EQ(MOJO_RESULT_OK,
              send_stream_->WriteData(base::as_byte_span(message),
                                      MOJO_WRITE_DATA_FLAG_NONE,
                                      actually_written_bytes));
    EXPECT_EQ(message.size(), actually_written_bytes);

    // Allow Socket to be notified that it can now read |send_stream_|.
    base::RunLoop().RunUntilIdle();

    // Socket should have now attempted to send our |message| to the remote
    // device on the other side of the BluetoothSocket.
    EXPECT_TRUE(fake_bluetooth_socket_->HasSendArgs());
    auto send_args = fake_bluetooth_socket_->TakeSendArgs();

    int buffer_size = std::get<1>(*send_args);
    EXPECT_EQ(message.size(), static_cast<size_t>(buffer_size));

    char* buffer = std::get<0>(*send_args)->data();
    std::string sent_string(buffer, buffer_size);
    EXPECT_EQ(message, sent_string);

    if (success) {
      auto success_callback = std::move(std::get<2>(*send_args));
      std::move(success_callback).Run(/*num_bytes_sent=*/message.size());
    } else {
      auto error_callback = std::move(std::get<3>(*send_args));
      std::move(error_callback).Run(/*error_message=*/"Error");
    }

    // Never expect an outstanding invocation of BluetoothSocket::Send().
    EXPECT_FALSE(fake_bluetooth_socket_->HasSendArgs());

    // |send_stream_| should only remain writeable if Socket received a success
    // callback.
    EXPECT_EQ(success, !send_stream_->QuerySignalsState().never_writable());
  }

 protected:
  scoped_refptr<device::FakeBluetoothSocket> fake_bluetooth_socket_;
  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  std::unique_ptr<Socket> socket_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(SocketTest, TestOnDestroyCallsClose) {
  // When destroyed, |socket_| is expected to tear down its BluetoothSocket.
  socket_.reset();
  EXPECT_TRUE(fake_bluetooth_socket_->called_disconnect());
}

TEST_F(SocketTest, TestDisconnect) {
  socket_->Disconnect(base::DoNothing());
  EXPECT_TRUE(fake_bluetooth_socket_->called_disconnect());
}

TEST_F(SocketTest, TestReceive_Success) {
  VerifyReceiveAndRead("received_message", /*success=*/true);
}

TEST_F(SocketTest, TestReceive_Error) {
  VerifyReceiveAndRead("received_message", /*success=*/false);
}

TEST_F(SocketTest, TestSend_Success) {
  WriteAndVerifySend("sent_message", /*success=*/true);
}

TEST_F(SocketTest, TestSend_Error) {
  WriteAndVerifySend("sent_message", /*success=*/false);
}

TEST_F(SocketTest, TestReceiveAndSendMultiple) {
  VerifyReceiveAndRead("message_1", /*success=*/true);
  VerifyReceiveAndRead("message_2", /*success=*/true);
  WriteAndVerifySend("message_3", /*success=*/true);
  WriteAndVerifySend("message_4", /*success=*/true);
  VerifyReceiveAndRead("message_5", /*success=*/true);
  WriteAndVerifySend("message_6", /*success=*/true);
}

}  // namespace bluetooth

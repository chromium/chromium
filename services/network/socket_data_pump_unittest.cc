// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/socket_data_pump.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/mojo_socket_test_util.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/socket_factory.h"
#include "services/network/tcp_connected_socket.h"
#include "services/network/tcp_server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

// Test delegate to wait on network read/write errors.
class TestSocketDataPumpDelegate : public SocketDataPump::Delegate {
 public:
  TestSocketDataPumpDelegate() {}

  TestSocketDataPumpDelegate(const TestSocketDataPumpDelegate&) = delete;
  TestSocketDataPumpDelegate& operator=(const TestSocketDataPumpDelegate&) =
      delete;

  ~TestSocketDataPumpDelegate() {}

  // Waits for read error. Returns the error observed.
  int WaitForReadError() {
    read_loop_.Run();
    int error = read_error_;
    read_error_ = net::OK;
    return error;
  }

  // Waits for write error. Returns the error observed.
  int WaitForWriteError() {
    write_loop_.Run();
    int error = write_error_;
    write_error_ = net::OK;
    return error;
  }

  // Waits for shutdown.
  void WaitForShutdown() { shutdown_loop_.Run(); }

 private:
  void OnNetworkReadError(int error) override {
    read_error_ = error;
    read_loop_.Quit();
  }
  void OnNetworkWriteError(int error) override {
    write_error_ = error;
    write_loop_.Quit();
  }
  void OnShutdown() override { shutdown_loop_.Quit(); }

  int read_error_ = net::OK;
  int write_error_ = net::OK;
  base::RunLoop read_loop_;
  base::RunLoop write_loop_;
  base::RunLoop shutdown_loop_;
};

class SocketDataPumpTest : public testing::Test,
                           public ::testing::WithParamInterface<net::IoMode> {
 public:
  SocketDataPumpTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  SocketDataPumpTest(const SocketDataPumpTest&) = delete;
  SocketDataPumpTest& operator=(const SocketDataPumpTest&) = delete;

  ~SocketDataPumpTest() override {}

  // Initializes the test case with a socket data provider, which will be used
  // to populate the read/write data of the mock socket.
  void Init(net::StaticSocketDataProvider* data_provider) {
    mock_client_socket_factory_.AddSocketDataProvider(data_provider);
    mock_client_socket_factory_.set_enable_read_if_ready(true);

    mojo::ScopedDataPipeConsumerHandle send_consumer_handle;
    ASSERT_EQ(mojo::CreateDataPipe(nullptr, send_handle_, send_consumer_handle),
              MOJO_RESULT_OK);

    mojo::ScopedDataPipeProducerHandle receive_producer_handle;
    ASSERT_EQ(
        mojo::CreateDataPipe(nullptr, receive_producer_handle, receive_handle_),
        MOJO_RESULT_OK);

    socket_ = mock_client_socket_factory_.CreateTransportClientSocket(
        net::AddressList(), nullptr /*socket_performance_watcher*/,
        nullptr /*network_quality_estimator*/, nullptr /*netlog*/,
        net::NetLogSource());
    net::TestCompletionCallback callback;
    int result = socket_->Connect(callback.callback());
    if (result == net::ERR_IO_PENDING)
      result = callback.WaitForResult();
    EXPECT_EQ(net::OK, result);
    data_pump_ = std::make_unique<SocketDataPump>(
        socket_.get(), delegate(), std::move(receive_producer_handle),
        std::move(send_consumer_handle), TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // Reads |num_bytes| from |handle| or reads until an error occurs. Returns the
  // bytes read as a string.
  std::string Read(mojo::ScopedDataPipeConsumerHandle* handle,
                   size_t num_bytes) {
    std::string received_contents;
    while (received_contents.size() < num_bytes) {
      base::RunLoop().RunUntilIdle();
      std::string buffer(num_bytes, '\0');
      MojoResult result = handle->get().ReadData(
          MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
          num_bytes);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        continue;
      if (result != MOJO_RESULT_OK)
        return received_contents;
      received_contents.append(std::string_view(buffer).substr(0, num_bytes));
    }
    return received_contents;
  }

  TestSocketDataPumpDelegate* delegate() { return &test_delegate_; }

  mojo::ScopedDataPipeConsumerHandle receive_handle_;
  mojo::ScopedDataPipeProducerHandle send_handle_;

 private:
  base::test::TaskEnvironment task_environment_;
  net::MockClientSocketFactory mock_client_socket_factory_;
  TestSocketDataPumpDelegate test_delegate_;
  std::unique_ptr<net::StreamSocket> socket_;
  std::unique_ptr<SocketDataPump> data_pump_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SocketDataPumpTest,
                         testing::Values(net::SYNCHRONOUS, net::ASYNC));

TEST_P(SocketDataPumpTest, ReadAndWriteMultiple) {
  constexpr std::string_view kTestMsg = "abcdefghij";
  constexpr int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (const char& c : kTestMsg) {
      reads.emplace_back(mode, sequence_number++, base::byte_span_from_ref(c));
    }
    if (j == kNumIterations - 1) {
      reads.emplace_back(mode, net::OK, sequence_number++);
    }
    for (const char& c : kTestMsg) {
      writes.emplace_back(mode, sequence_number++, base::byte_span_from_ref(c));
    }
  }
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading `kTestMsg.size()` should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&receive_handle_, kTestMsg.size()));
    // Write multiple times.
    for (const char& c : kTestMsg) {
      size_t actually_written_bytes = 0;
      EXPECT_EQ(MOJO_RESULT_OK,
                send_handle_->WriteData(base::byte_span_from_ref(c),
                                        MOJO_WRITE_DATA_FLAG_NONE,
                                        actually_written_bytes));
      // Flush the 1 byte write.
      base::RunLoop().RunUntilIdle();
    }
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, PartialStreamSocketWrite) {
  constexpr std::string_view kTestMsg = "abcdefghij";
  constexpr int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (const char& c : kTestMsg) {
      reads.emplace_back(mode, sequence_number++, base::byte_span_from_ref(c));
    }
    if (j == kNumIterations - 1) {
      reads.emplace_back(mode, net::OK, sequence_number++);
    }
    for (const char& c : kTestMsg) {
      writes.emplace_back(mode, sequence_number++, base::byte_span_from_ref(c));
    }
  }
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading `kTestMsg.size()` should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&receive_handle_, kTestMsg.size()));
    // Write twice, each with kMsgSize/2 bytes which is bigger than the 1-byte
    // MockWrite(). This is to exercise that StreamSocket::Write() can do
    // partial write.
    auto [first_write, second_write] =
        base::as_byte_span(kTestMsg).split_at(kTestMsg.size() / 2);
    size_t actually_written_bytes = 0;
    EXPECT_EQ(MOJO_RESULT_OK,
              send_handle_->WriteData(first_write, MOJO_WRITE_DATA_FLAG_NONE,
                                      actually_written_bytes));
    EXPECT_EQ(kTestMsg.size() / 2, actually_written_bytes);
    // Flush the kMsgSize/2 byte write.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(MOJO_RESULT_OK,
              send_handle_->WriteData(second_write, MOJO_WRITE_DATA_FLAG_NONE,
                                      actually_written_bytes));
    EXPECT_EQ(kTestMsg.size() - first_write.size(), actually_written_bytes);
    // Flush the kMsgSize/2 byte write.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, ReadEof) {
  net::IoMode mode = GetParam();
  net::MockRead reads[] = {net::MockRead(mode, net::OK)};
  constexpr std::string_view kTestMsg = "hello!";
  net::MockWrite writes[] = {net::MockWrite(mode, 0, kTestMsg)};
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  EXPECT_EQ("", Read(&receive_handle_, 1));
  EXPECT_EQ(net::OK, delegate()->WaitForReadError());
  // Writes can proceed even though there is a read error.
  size_t actually_written_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            send_handle_->WriteData(base::as_byte_span(kTestMsg),
                                    MOJO_WRITE_DATA_FLAG_NONE,
                                    actually_written_bytes));
  EXPECT_EQ(kTestMsg.size(), actually_written_bytes);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, ReadError) {
  net::IoMode mode = GetParam();
  net::MockRead reads[] = {net::MockRead(mode, net::ERR_FAILED)};
  constexpr std::string_view kTestMsg = "hello!";
  net::MockWrite writes[] = {net::MockWrite(mode, 0, kTestMsg)};
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  EXPECT_EQ("", Read(&receive_handle_, 1));
  EXPECT_EQ(net::ERR_FAILED, delegate()->WaitForReadError());
  // Writes can proceed even though there is a read error.
  size_t actually_written_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            send_handle_->WriteData(base::as_byte_span(kTestMsg),
                                    MOJO_WRITE_DATA_FLAG_NONE,
                                    actually_written_bytes));
  EXPECT_EQ(kTestMsg.size(), actually_written_bytes);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, WriteEof) {
  net::IoMode mode = GetParam();
  constexpr std::string_view kTestMsg = "hello!";
  net::MockRead reads[] = {net::MockRead(mode, 0, kTestMsg),
                           net::MockRead(mode, net::OK)};
  net::MockWrite writes[] = {net::MockWrite(mode, net::OK)};
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  size_t actually_written_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            send_handle_->WriteData(base::as_byte_span(kTestMsg),
                                    MOJO_WRITE_DATA_FLAG_NONE,
                                    actually_written_bytes));
  EXPECT_EQ(kTestMsg.size(), actually_written_bytes);
  EXPECT_EQ(net::OK, delegate()->WaitForWriteError());
  // Reads can proceed even though there is a read error.
  EXPECT_EQ(kTestMsg, Read(&receive_handle_, kTestMsg.size()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, WriteError) {
  net::IoMode mode = GetParam();
  constexpr std::string_view kTestMsg = "hello!";
  net::MockRead reads[] = {net::MockRead(mode, 0, kTestMsg),
                           net::MockRead(mode, net::OK)};
  net::MockWrite writes[] = {net::MockWrite(mode, net::ERR_FAILED)};
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  size_t actually_written_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            send_handle_->WriteData(base::as_byte_span(kTestMsg),
                                    MOJO_WRITE_DATA_FLAG_NONE,
                                    actually_written_bytes));
  EXPECT_EQ(kTestMsg.size(), actually_written_bytes);
  EXPECT_EQ(net::ERR_FAILED, delegate()->WaitForWriteError());
  // Reads can proceed even though there is a read error.
  EXPECT_EQ(kTestMsg, Read(&receive_handle_, kTestMsg.size()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, PipesShutdown) {
  net::IoMode mode = GetParam();
  net::MockRead reads[] = {net::MockRead(mode, net::OK)};
  net::StaticSocketDataProvider data_provider(reads,
                                              base::span<net::MockWrite>());
  Init(&data_provider);
  send_handle_.reset();
  receive_handle_.reset();
  delegate()->WaitForShutdown();
}

namespace {

class PumpDestroyingDelegate : public SocketDataPump::Delegate {
 public:
  PumpDestroyingDelegate() = default;
  ~PumpDestroyingDelegate() = default;

  PumpDestroyingDelegate(const PumpDestroyingDelegate&) = delete;
  PumpDestroyingDelegate& operator=(const PumpDestroyingDelegate&) = delete;

  void set_pump(std::unique_ptr<SocketDataPump> pump) {
    pump_ = std::move(pump);
  }

  void set_run_on_shutdown(base::OnceClosure closure) {
    run_on_shutdown_ = std::move(closure);
  }

  // SocketDataPump::Delegate implementation:
  void OnNetworkReadError(int net_error) override {}
  void OnNetworkWriteError(int net_error) override {}
  void OnShutdown() override {
    pump_ = nullptr;
    if (run_on_shutdown_) {
      std::move(run_on_shutdown_).Run();
    }
  }

 private:
  std::unique_ptr<SocketDataPump> pump_;
  base::OnceClosure run_on_shutdown_;
};

// A dummy socket that behaves as if writes are blocked until the TakeWrite()
// method is explicitly called.
class BlockedStreamSocket : public net::StreamSocket {
 public:
  BlockedStreamSocket() = default;
  ~BlockedStreamSocket() override = default;

  void set_run_on_write(base::OnceClosure closure) {
    run_on_write_ = std::move(closure);
  }

  std::vector<uint8_t> TakeWrite() {
    size_t buf_len = pending_write_buf_len_;
    std::vector data(std::from_range, pending_write_buf_->first(buf_len));
    pending_write_buf_ = nullptr;
    pending_write_buf_len_ = 0;
    std::move(pending_write_callback_).Run(base::checked_cast<int>(buf_len));
    return data;
  }

  // net::StreamSocket implementation:
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    return net::ERR_IO_PENDING;
  }
  int ReadIfReady(net::IOBuffer* buf,
                  int buf_len,
                  net::CompletionOnceCallback callback) override {
    return net::ERR_IO_PENDING;
  }
  int CancelReadIfReady() override { return net::OK; }
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    pending_write_buf_ = buf;
    pending_write_buf_len_ = base::checked_cast<size_t>(buf_len);
    pending_write_callback_ = std::move(callback);
    if (run_on_write_) {
      std::move(run_on_write_).Run();
    }
    return net::ERR_IO_PENDING;
  }
  int SetReceiveBufferSize(int32_t size) override { return net::OK; }
  int SetSendBufferSize(int32_t size) override { return net::OK; }
  int Connect(net::CompletionOnceCallback callback) override { return net::OK; }
  void Disconnect() override {}
  bool IsConnected() const override { return true; }
  bool IsConnectedAndIdle() const override { return false; }
  int GetPeerAddress(net::IPEndPoint* address) const override {
    return net::OK;
  }
  int GetLocalAddress(net::IPEndPoint* address) const override {
    return net::OK;
  }
  const net::NetLogWithSource& NetLog() const override { return net_log_; }
  bool WasEverUsed() const override { return true; }
  net::NextProto GetNegotiatedProtocol() const override {
    return net::NextProto::kProtoUnknown;
  }
  bool GetSSLInfo(net::SSLInfo* ssl_info) override { return false; }
  int64_t GetTotalReceivedBytes() const override { return 0; }
  void ApplySocketTag(const net::SocketTag& tag) override {}

 private:
  scoped_refptr<net::IOBuffer> pending_write_buf_;
  size_t pending_write_buf_len_ = 0;
  net::CompletionOnceCallback pending_write_callback_;
  base::OnceClosure run_on_write_;
  net::NetLogWithSource net_log_;
};

}  // namespace

TEST(SocketDataPumpTest, ShutdownWhileBlockedOnWrite) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  BlockedStreamSocket socket;
  base::RunLoop wait_for_write;
  socket.set_run_on_write(wait_for_write.QuitClosure());

  mojo::ScopedDataPipeProducerHandle send_producer;
  mojo::ScopedDataPipeConsumerHandle send_consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, send_producer, send_consumer));

  mojo::ScopedDataPipeProducerHandle receive_producer;
  mojo::ScopedDataPipeConsumerHandle receive_consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(nullptr, receive_producer, receive_consumer));

  PumpDestroyingDelegate delegate;
  auto pump = std::make_unique<SocketDataPump>(
      &socket, &delegate, std::move(receive_producer), std::move(send_consumer),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  delegate.set_pump(std::move(pump));
  base::RunLoop wait_for_shutdown;
  delegate.set_run_on_shutdown(wait_for_shutdown.QuitClosure());

  // Write data to the send pipe.
  const std::string data = "secret";
  size_t actually_written = 0;
  ASSERT_EQ(MOJO_RESULT_OK, send_producer->WriteData(base::as_byte_span(data),
                                                     MOJO_WRITE_DATA_FLAG_NONE,
                                                     actually_written));
  EXPECT_EQ(actually_written, data.size());

  // Run until SocketDataPump reads from the pipe and calls socket.Write().
  wait_for_write.Run();

  // Trigger OnShutdown() by closing the receive consumer.
  // This will cause PumpDestroyingDelegate to destroy the SocketDataPump.
  receive_consumer.reset();
  wait_for_shutdown.Run();

  // Copy the data that was passed to Write().
  std::vector<uint8_t> written_data = socket.TakeWrite();
  EXPECT_EQ(data, base::as_string_view(written_data));
}

}  // namespace network

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/base/socket_stream.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

typedef std::vector<net::MockRead> ReadList;
typedef std::vector<net::MockWrite> WriteList;

const char kReadData[] = "read_data";
const int kReadDataSize = base::size(kReadData) - 1;
const char kReadData2[] = "read_alternate_data";
const int kReadData2Size = base::size(kReadData2) - 1;
const char kWriteData[] = "write_data";
const int kWriteDataSize = base::size(kWriteData) - 1;

class GCMSocketStreamTest : public testing::Test {
 public:
  GCMSocketStreamTest();
  ~GCMSocketStreamTest() override;

  // Build a socket with the expected reads and writes.
  void BuildSocket(const ReadList& read_list, const WriteList& write_list);

  // Pump the message loop until idle.
  void PumpLoop();

  // Simulates a google::protobuf::io::CodedInputStream read.
  base::StringPiece DoInputStreamRead(int bytes);

  // Simulates a google::protobuf::io::CodedOutputStream write.
  int DoOutputStreamWrite(const base::StringPiece& write_src);

  // Simulates a google::protobuf::io::CodedOutputStream write, but do not call
  // flush.
  int DoOutputStreamWriteWithoutFlush(const base::StringPiece& write_src);

  // Synchronous Refresh wrapper.
  void WaitForData(int msg_size);

  SocketInputStream* input_stream() { return socket_input_stream_.get(); }
  SocketOutputStream* output_stream() { return socket_output_stream_.get(); }

  mojo::Remote<network::mojom::ProxyResolvingSocket> mojo_socket_remote_;

  void set_socket_output_stream(std::unique_ptr<SocketOutputStream> stream) {
    socket_output_stream_ = std::move(stream);
  }

 private:
  void OpenConnection();
  void ResetInputStream();
  void ResetOutputStream();

  base::test::TaskEnvironment task_environment_;

  // SocketStreams and their data providers.
  ReadList mock_reads_;
  WriteList mock_writes_;
  std::unique_ptr<net::StaticSocketDataProvider> data_provider_;
  std::unique_ptr<net::SSLSocketDataProvider> ssl_data_provider_;
  std::unique_ptr<SocketInputStream> socket_input_stream_;
  std::unique_ptr<SocketOutputStream> socket_output_stream_;

  // net:: components.
  net::AddressList address_list_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<network::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  net::MockClientSocketFactory socket_factory_;
  net::TestURLRequestContext url_request_context_;
  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::ProxyResolvingSocketFactory>
      mojo_socket_factory_remote_;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_handle_;
  mojo::ScopedDataPipeProducerHandle send_pipe_handle_;
};

GCMSocketStreamTest::GCMSocketStreamTest()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
      network_change_notifier_(
          net::NetworkChangeNotifier::CreateMockIfNeeded()),
      network_service_(network::NetworkService::CreateForTesting()),
      url_request_context_(true /* delay_initialization */) {
  address_list_ = net::AddressList::CreateFromIPAddress(
      net::IPAddress::IPv4Localhost(), 5228);
  socket_factory_.set_enable_read_if_ready(true);
  url_request_context_.set_client_socket_factory(&socket_factory_);
  url_request_context_.Init();

  network_context_ = std::make_unique<network::NetworkContext>(
      network_service_.get(),
      network_context_remote_.BindNewPipeAndPassReceiver(),
      &url_request_context_,
      /*cors_exempt_header_list=*/std::vector<std::string>());
}

GCMSocketStreamTest::~GCMSocketStreamTest() {}

void GCMSocketStreamTest::BuildSocket(const ReadList& read_list,
                                      const WriteList& write_list) {
  mock_reads_ = read_list;
  mock_writes_ = write_list;
  data_provider_ = std::make_unique<net::StaticSocketDataProvider>(
      mock_reads_, mock_writes_);
  ssl_data_provider_ =
      std::make_unique<net::SSLSocketDataProvider>(net::SYNCHRONOUS, net::OK);
  socket_factory_.AddSocketDataProvider(data_provider_.get());
  socket_factory_.AddSSLSocketDataProvider(ssl_data_provider_.get());
  OpenConnection();
  ResetInputStream();
  ResetOutputStream();
}

void GCMSocketStreamTest::PumpLoop() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

base::StringPiece GCMSocketStreamTest::DoInputStreamRead(int bytes) {
  int total_bytes_read = 0;
  const void* initial_buffer = nullptr;
  const void* buffer = nullptr;
  int size = 0;

  do {
    DCHECK(socket_input_stream_->GetState() == SocketInputStream::EMPTY ||
           socket_input_stream_->GetState() == SocketInputStream::READY);
    if (!socket_input_stream_->Next(&buffer, &size))
      break;
    total_bytes_read += size;
    if (initial_buffer) {  // Verify the buffer doesn't skip data.
      EXPECT_EQ(static_cast<const uint8_t*>(initial_buffer) + total_bytes_read,
                static_cast<const uint8_t*>(buffer) + size);
    } else {
      initial_buffer = buffer;
    }
  } while (total_bytes_read < bytes);

  if (total_bytes_read > bytes) {
    socket_input_stream_->BackUp(total_bytes_read - bytes);
    total_bytes_read = bytes;
  }

  return base::StringPiece(static_cast<const char*>(initial_buffer),
                           total_bytes_read);
}

int GCMSocketStreamTest::DoOutputStreamWrite(
    const base::StringPiece& write_src) {
  int total_bytes_written = DoOutputStreamWriteWithoutFlush(write_src);
  base::RunLoop run_loop;
  if (socket_output_stream_->Flush(run_loop.QuitClosure()) ==
      net::ERR_IO_PENDING) {
    run_loop.Run();
  }

  return total_bytes_written;
}

int GCMSocketStreamTest::DoOutputStreamWriteWithoutFlush(
    const base::StringPiece& write_src) {
  DCHECK_EQ(socket_output_stream_->GetState(), SocketOutputStream::EMPTY);
  int total_bytes_written = 0;
  void* buffer = nullptr;
  int size = 0;
  const int bytes = write_src.size();

  do {
    if (!socket_output_stream_->Next(&buffer, &size))
      break;
    int bytes_to_write = (size < bytes ? size : bytes);
    memcpy(buffer,
           write_src.data() + total_bytes_written,
           bytes_to_write);
    if (bytes_to_write < size)
      socket_output_stream_->BackUp(size - bytes_to_write);
    total_bytes_written += bytes_to_write;
  } while (total_bytes_written < bytes);

  return total_bytes_written;
}

void GCMSocketStreamTest::WaitForData(int msg_size) {
  while (input_stream()->UnreadByteCount() < msg_size) {
    base::RunLoop run_loop;
    if (input_stream()->Refresh(run_loop.QuitClosure(),
                                msg_size - input_stream()->UnreadByteCount()) ==
            net::ERR_IO_PENDING) {
      run_loop.Run();
    }
    if (input_stream()->GetState() == SocketInputStream::CLOSED)
      return;
  }
}

void GCMSocketStreamTest::OpenConnection() {
  network_context_->CreateProxyResolvingSocketFactory(
      mojo_socket_factory_remote_.BindNewPipeAndPassReceiver());
  base::RunLoop run_loop;
  int net_error = net::ERR_FAILED;
  const GURL kDestination("https://example.com");
  network::mojom::ProxyResolvingSocketOptionsPtr options =
      network::mojom::ProxyResolvingSocketOptions::New();
  options->use_tls = true;
  mojo_socket_factory_remote_->CreateProxyResolvingSocket(
      kDestination, std::move(options),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      mojo_socket_remote_.BindNewPipeAndPassReceiver(),
      mojo::NullRemote() /* observer */,
      base::BindLambdaForTesting(
          [&](int result, const base::Optional<net::IPEndPoint>& local_addr,
              const base::Optional<net::IPEndPoint>& peer_addr,
              mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
              mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
            net_error = result;
            receive_pipe_handle_ = std::move(receive_pipe_handle);
            send_pipe_handle_ = std::move(send_pipe_handle);
            run_loop.Quit();
          }));
  run_loop.Run();

  PumpLoop();
}

void GCMSocketStreamTest::ResetInputStream() {
  DCHECK(mojo_socket_remote_);
  socket_input_stream_ =
      std::make_unique<SocketInputStream>(std::move(receive_pipe_handle_));
}

void GCMSocketStreamTest::ResetOutputStream() {
  DCHECK(mojo_socket_remote_);
  socket_output_stream_ =
      std::make_unique<SocketOutputStream>(std::move(send_pipe_handle_));
}

// A read where all data is already available.
TEST_F(GCMSocketStreamTest, ReadDataSync) {
  ReadList read_list;
  read_list.push_back(
      net::MockRead(net::SYNCHRONOUS, kReadData, kReadDataSize));
  read_list.push_back(net::MockRead(net::ASYNC, net::OK) /* EOF */);
  BuildSocket(read_list, WriteList());

  WaitForData(kReadDataSize);
  ASSERT_EQ(std::string(kReadData, kReadDataSize),
            DoInputStreamRead(kReadDataSize));
}

// A read that comes in two parts.
TEST_F(GCMSocketStreamTest, ReadPartialDataSync) {
  int first_read_len = kReadDataSize / 2;
  int second_read_len = kReadDataSize - first_read_len;
  ReadList read_list;
  read_list.push_back(
      net::MockRead(net::SYNCHRONOUS,
                    kReadData,
                    first_read_len));
  read_list.push_back(
      net::MockRead(net::SYNCHRONOUS,
                    &kReadData[first_read_len],
                    second_read_len));
  // Add an EOF.
  read_list.push_back(net::MockRead(net::SYNCHRONOUS, net::OK));

  BuildSocket(read_list, WriteList());

  WaitForData(kReadDataSize);
  ASSERT_EQ(std::string(kReadData, kReadDataSize),
            DoInputStreamRead(kReadDataSize));
}

// A read where no data is available at first (IO_PENDING will be returned).
TEST_F(GCMSocketStreamTest, ReadAsync) {
  int first_read_len = kReadDataSize / 2;
  int second_read_len = kReadDataSize - first_read_len;
  ReadList read_list;
  read_list.push_back(
      net::MockRead(net::ASYNC, kReadData, first_read_len));
  read_list.push_back(
      net::MockRead(net::ASYNC, &kReadData[first_read_len], second_read_len));
  read_list.push_back(net::MockRead(net::ASYNC, net::OK) /* EOF */);
  BuildSocket(read_list, WriteList());
  WaitForData(kReadDataSize);
  ASSERT_EQ(std::string(kReadData, kReadDataSize),
            DoInputStreamRead(kReadDataSize));
}

// Simulate two packets arriving at once. Read them in two separate calls.
TEST_F(GCMSocketStreamTest, TwoReadsAtOnce) {
  std::string long_data = std::string(kReadData, kReadDataSize) +
                          std::string(kReadData2, kReadData2Size);
  ReadList read_list;
  read_list.push_back(
      net::MockRead(net::SYNCHRONOUS, long_data.c_str(), long_data.size()));
  // Add an EOF.
  read_list.push_back(net::MockRead(net::SYNCHRONOUS, net::OK));

  BuildSocket(read_list, WriteList());

  WaitForData(kReadDataSize);
  ASSERT_EQ(std::string(kReadData, kReadDataSize),
            DoInputStreamRead(kReadDataSize));

  WaitForData(kReadData2Size);
  ASSERT_EQ(std::string(kReadData2, kReadData2Size),
            DoInputStreamRead(kReadData2Size));
}

// Simulate two packets arriving at once. Read them in two calls separated
// by a Rebuild.
TEST_F(GCMSocketStreamTest, TwoReadsAtOnceWithRebuild) {
  std::string long_data = std::string(kReadData, kReadDataSize) +
                          std::string(kReadData2, kReadData2Size);
  ReadList read_list;

  read_list.push_back(
      net::MockRead(net::SYNCHRONOUS, long_data.c_str(), long_data.size()));
  // Add an EOF.
  read_list.push_back(net::MockRead(net::SYNCHRONOUS, net::OK));

  BuildSocket(read_list, WriteList());

  WaitForData(kReadDataSize);
  ASSERT_EQ(std::string(kReadData, kReadDataSize),
              DoInputStreamRead(kReadDataSize));

  input_stream()->RebuildBuffer();
  WaitForData(kReadData2Size);
  ASSERT_EQ(std::string(kReadData2, kReadData2Size),
            DoInputStreamRead(kReadData2Size));
}

// Simulate a read that is aborted.
TEST_F(GCMSocketStreamTest, ReadError) {
  int result = net::ERR_ABORTED;
  BuildSocket(ReadList(1, net::MockRead(net::SYNCHRONOUS, result)),
              WriteList());

  WaitForData(kReadDataSize);
  ASSERT_EQ(SocketInputStream::CLOSED, input_stream()->GetState());
  ASSERT_EQ(net::ERR_FAILED, input_stream()->last_error());
}

// Simulate a read after the connection is closed.
TEST_F(GCMSocketStreamTest, ReadDisconnected) {
  BuildSocket(ReadList(1, net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)),
              WriteList());
  mojo_socket_remote_.reset();
  WaitForData(kReadDataSize);
  ASSERT_EQ(SocketInputStream::CLOSED, input_stream()->GetState());
  ASSERT_EQ(net::ERR_FAILED, input_stream()->last_error());
}

// Write a full message in one go.
TEST_F(GCMSocketStreamTest, WriteFull) {
  BuildSocket(ReadList(1, net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)),
              WriteList(1, net::MockWrite(net::SYNCHRONOUS, kWriteData,
                                          kWriteDataSize)));
  ASSERT_EQ(kWriteDataSize,
            DoOutputStreamWrite(base::StringPiece(kWriteData,
                                                  kWriteDataSize)));
}

// Write a message in two go's.
TEST_F(GCMSocketStreamTest, WritePartial) {
  WriteList write_list;
  write_list.push_back(net::MockWrite(net::SYNCHRONOUS,
                                      kWriteData,
                                      kWriteDataSize / 2));
  write_list.push_back(net::MockWrite(net::SYNCHRONOUS,
                                      kWriteData + kWriteDataSize / 2,
                                      kWriteDataSize / 2));
  BuildSocket(ReadList(1, net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)),
              write_list);
  ASSERT_EQ(kWriteDataSize,
            DoOutputStreamWrite(base::StringPiece(kWriteData,
                                                  kWriteDataSize)));
}

// Regression test for crbug.com/866635.
TEST_F(GCMSocketStreamTest, WritePartialWithLengthChecking) {
  // Add a prefix data in front of kWriteData.
  std::string prefix_data("xxxxx");
  const size_t kPrefixDataSize = 5;
  // |pipe| has a capacity that is one byte smaller than |prefix_data.size()| +
  // |kWriteDataSize|. This is so that the first write is a partial write
  // of |prefix_data|, and the second write is a complete write of kWriteData.
  // The 1 byte shortage is to simulate the partial write.
  mojo::DataPipe pipe(kWriteDataSize + prefix_data.size() - 1 /* size */);
  mojo::ScopedDataPipeConsumerHandle consumer_handle =
      std::move(pipe.consumer_handle);
  mojo::ScopedDataPipeProducerHandle producer_handle =
      std::move(pipe.producer_handle);

  // Prepopulate |producer_handle| of |prefix_data|, now the pipe's capacity is
  // less than |kWriteDataSize|.
  uint32_t num_bytes = prefix_data.size();
  MojoResult r = producer_handle->WriteData(prefix_data.data(), &num_bytes,
                                            MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, r);
  ASSERT_EQ(prefix_data.size(), num_bytes);

  // Create a SocketOutputStream from the producer pipe.
  auto socket_output_stream =
      std::make_unique<SocketOutputStream>(std::move(producer_handle));
  set_socket_output_stream(std::move(socket_output_stream));

  // Write but do not flush.
  EXPECT_EQ(kWriteDataSize, DoOutputStreamWriteWithoutFlush(kWriteData));

  base::RunLoop run_loop;
  output_stream()->Flush(run_loop.QuitClosure());
  // Flush should be able to flush exactly 5 bytes, because of the data pipe
  // capacity.
  base::RunLoop().RunUntilIdle();

  std::string contents;
  // Read prefix.
  char buffer[kPrefixDataSize];
  uint32_t read_size = sizeof(buffer);
  ASSERT_EQ(MOJO_RESULT_OK, consumer_handle->ReadData(
                                buffer, &read_size, MOJO_READ_DATA_FLAG_NONE));
  ASSERT_EQ(kPrefixDataSize, read_size);
  contents += std::string(buffer, read_size);

  base::RunLoop().RunUntilIdle();
  // Flush now should complete.
  run_loop.Run();

  // Closes |producer_handle|.
  set_socket_output_stream(nullptr);

  // Read everything in |consumer_handle| now that |producer_handle| is closed
  // to make sure data is as what we expected, and there is no trailing garbage
  // data.
  while (true) {
    char buffer[5];
    uint32_t read_size = sizeof(buffer);
    MojoResult r =
        consumer_handle->ReadData(buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (r == MOJO_RESULT_SHOULD_WAIT)
      continue;
    if (r != MOJO_RESULT_OK)
      break;
    ASSERT_EQ(MOJO_RESULT_OK, r);
    contents += std::string(buffer, read_size);
  }
  std::string expected(prefix_data);
  expected.append(kWriteData);
  EXPECT_EQ(expected, contents);
}

// Write a message completely asynchronously (returns IO_PENDING before
// finishing the write in two go's).
TEST_F(GCMSocketStreamTest, WriteNone) {
  WriteList write_list;
  write_list.push_back(net::MockWrite(net::SYNCHRONOUS,
                                      kWriteData,
                                      kWriteDataSize / 2));
  write_list.push_back(net::MockWrite(net::SYNCHRONOUS,
                                      kWriteData + kWriteDataSize / 2,
                                      kWriteDataSize / 2));
  BuildSocket(ReadList(1, net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)),
              write_list);
  ASSERT_EQ(kWriteDataSize,
            DoOutputStreamWrite(base::StringPiece(kWriteData,
                                                  kWriteDataSize)));
}

// Write a message then read a message.
TEST_F(GCMSocketStreamTest, WriteThenRead) {
  ReadList read_list;
  read_list.push_back(
      net::MockRead(net::SYNCHRONOUS, kReadData, kReadDataSize));
  // Add an EOF.
  read_list.push_back(net::MockRead(net::SYNCHRONOUS, net::OK));

  BuildSocket(read_list,
              WriteList(1, net::MockWrite(net::SYNCHRONOUS, kWriteData,
                                          kWriteDataSize)));

  ASSERT_EQ(kWriteDataSize,
            DoOutputStreamWrite(base::StringPiece(kWriteData,
                                                  kWriteDataSize)));

  WaitForData(kReadDataSize);
  ASSERT_EQ(std::string(kReadData, kReadDataSize),
              DoInputStreamRead(kReadDataSize));
}

// Read a message then write a message.
TEST_F(GCMSocketStreamTest, ReadThenWrite) {
  ReadList read_list;
  read_list.push_back(
      net::MockRead(net::SYNCHRONOUS, kReadData, kReadDataSize));
  // Add an EOF.
  read_list.push_back(net::MockRead(net::SYNCHRONOUS, net::OK));

  BuildSocket(read_list,
              WriteList(1, net::MockWrite(net::SYNCHRONOUS, kWriteData,
                                          kWriteDataSize)));

  WaitForData(kReadDataSize);
  ASSERT_EQ(std::string(kReadData, kReadDataSize),
              DoInputStreamRead(kReadDataSize));

  ASSERT_EQ(kWriteDataSize,
            DoOutputStreamWrite(base::StringPiece(kWriteData,
                                                  kWriteDataSize)));
}

// Simulate a write that gets aborted.
TEST_F(GCMSocketStreamTest, WriteError) {
  int result = net::ERR_ABORTED;
  BuildSocket(ReadList(1, net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)),
              WriteList(1, net::MockWrite(net::SYNCHRONOUS, result)));
  // Mojo data pipe buffers data, so there is a delay before write error is
  // observed.Continue writing if error is not observed.
  while (output_stream()->GetState() != SocketOutputStream::CLOSED) {
    DoOutputStreamWrite(base::StringPiece(kWriteData, kWriteDataSize));
  }
  ASSERT_EQ(SocketOutputStream::CLOSED, output_stream()->GetState());
  ASSERT_EQ(net::ERR_FAILED, output_stream()->last_error());
}

// Simulate a write after the connection is closed.
TEST_F(GCMSocketStreamTest, WriteDisconnected) {
  BuildSocket(ReadList(1, net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)),
              WriteList());
  mojo_socket_remote_.reset();
  DoOutputStreamWrite(base::StringPiece(kWriteData, kWriteDataSize));
  ASSERT_EQ(SocketOutputStream::CLOSED, output_stream()->GetState());
  ASSERT_EQ(net::ERR_FAILED, output_stream()->last_error());
}

}  // namespace
}  // namespace gcm

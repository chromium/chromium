// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/base/buffered_socket_writer.h"

#include <stddef.h>
#include <stdlib.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const int kTestBufferSize = 10000;
const size_t kWriteChunkSize = 1024U;

int WriteNetSocket(net::Socket* socket,
                   const scoped_refptr<net::IOBuffer>& buf,
                   int buf_len,
                   net::CompletionOnceCallback callback,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  return socket->Write(buf.get(), buf_len, std::move(callback),
                       traffic_annotation);
}

class SocketDataProvider : public net::SocketDataProvider {
 public:
  SocketDataProvider()
      : write_limit_(-1), async_write_(false), next_write_error_(net::OK) {}

  net::MockRead OnRead() override {
    return net::MockRead(net::ASYNC, net::ERR_IO_PENDING);
  }

  net::MockWriteResult OnWrite(const std::string& data) override {
    if (next_write_error_ != net::OK) {
      int r = next_write_error_;
      next_write_error_ = net::OK;
      return net::MockWriteResult(async_write_ ? net::ASYNC : net::SYNCHRONOUS,
                                  r);
    }
    int size = data.size();
    if (write_limit_ > 0) {
      size = std::min(write_limit_, size);
    }
    written_data_.append(data, 0, size);
    return net::MockWriteResult(async_write_ ? net::ASYNC : net::SYNCHRONOUS,
                                size);
  }

  bool AllReadDataConsumed() const override { return true; }

  bool AllWriteDataConsumed() const override { return true; }

  void Reset() override {}

  std::string written_data() { return written_data_; }

  void set_write_limit(int limit) { write_limit_ = limit; }
  void set_async_write(bool async_write) { async_write_ = async_write; }
  void set_next_write_error(int error) { next_write_error_ = error; }

 private:
  std::string written_data_;
  int write_limit_;
  bool async_write_;
  int next_write_error_;
};

}  // namespace

class BufferedSocketWriterTest : public testing::Test {
 public:
  BufferedSocketWriterTest() : write_error_(0) {}

  void DestroyWriter() {
    writer_.reset();
    socket_.reset();
  }

  void Unexpected() { EXPECT_TRUE(false); }

 protected:
  void SetUp() override {
    socket_ = std::make_unique<net::MockTCPClientSocket>(
        net::AddressList(), net::NetLog::Get(), &socket_data_provider_);
    socket_data_provider_.set_connect_data(
        net::MockConnect(net::SYNCHRONOUS, net::OK));
    EXPECT_EQ(net::OK, socket_->Connect(net::CompletionOnceCallback()));

    writer_ = std::make_unique<BufferedSocketWriter>();
    test_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kTestBufferSize);
    test_buffer_2_ =
        base::MakeRefCounted<net::IOBufferWithSize>(kTestBufferSize);
    for (int i = 0; i < kTestBufferSize; ++i) {
      test_buffer_->data()[i] = rand() % 256;
      test_buffer_2_->data()[i] = rand() % 256;
    }
  }

  void StartWriter() {
    writer_->Start(base::BindRepeating(&WriteNetSocket, socket_.get()),
                   base::BindOnce(&BufferedSocketWriterTest::OnWriteFailed,
                                  base::Unretained(this)));
  }

  void OnWriteFailed(int error) { write_error_ = error; }

  void VerifyWrittenData() {
    ASSERT_EQ(
        static_cast<size_t>(test_buffer_->size() + test_buffer_2_->size()),
        socket_data_provider_.written_data().size());
    EXPECT_EQ(0, memcmp(test_buffer_->data(),
                        socket_data_provider_.written_data().data(),
                        test_buffer_->size()));
    EXPECT_EQ(0, memcmp(test_buffer_2_->data(),
                        socket_data_provider_.written_data().data() +
                            test_buffer_->size(),
                        test_buffer_2_->size()));
  }

  void TestWrite() {
    writer_->Write(test_buffer_, base::OnceClosure(),
                   TRAFFIC_ANNOTATION_FOR_TESTS);
    writer_->Write(test_buffer_2_, base::OnceClosure(),
                   TRAFFIC_ANNOTATION_FOR_TESTS);
    base::RunLoop().RunUntilIdle();
    VerifyWrittenData();
  }

  void TestAppendInCallback() {
    writer_->Write(
        test_buffer_,
        base::BindOnce(base::IgnoreResult(&BufferedSocketWriter::Write),
                       base::Unretained(writer_.get()), test_buffer_2_,
                       base::OnceClosure(), TRAFFIC_ANNOTATION_FOR_TESTS),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    base::RunLoop().RunUntilIdle();
    VerifyWrittenData();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  SocketDataProvider socket_data_provider_;
  std::unique_ptr<net::StreamSocket> socket_;
  std::unique_ptr<BufferedSocketWriter> writer_;
  scoped_refptr<net::IOBufferWithSize> test_buffer_;
  scoped_refptr<net::IOBufferWithSize> test_buffer_2_;
  int write_error_;
};

// Test synchronous write.
TEST_F(BufferedSocketWriterTest, WriteFull) {
  StartWriter();
  TestWrite();
}

// Test synchronous write in 1k chunks.
TEST_F(BufferedSocketWriterTest, WriteChunks) {
  StartWriter();
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  TestWrite();
}

// Test asynchronous write.
TEST_F(BufferedSocketWriterTest, WriteAsync) {
  StartWriter();
  socket_data_provider_.set_async_write(true);
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  TestWrite();
}

// Make sure we can call Write() from the done callback.
TEST_F(BufferedSocketWriterTest, AppendInCallbackSync) {
  StartWriter();
  TestAppendInCallback();
}

// Make sure we can call Write() from the done callback.
TEST_F(BufferedSocketWriterTest, AppendInCallbackAsync) {
  StartWriter();
  socket_data_provider_.set_async_write(true);
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  TestAppendInCallback();
}

// Test that the writer can be destroyed from callback.
TEST_F(BufferedSocketWriterTest, DestroyFromCallback) {
  StartWriter();
  socket_data_provider_.set_async_write(true);
  writer_->Write(test_buffer_,
                 base::BindOnce(&BufferedSocketWriterTest::DestroyWriter,
                                base::Unretained(this)),
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  writer_->Write(test_buffer_2_,
                 base::BindOnce(&BufferedSocketWriterTest::Unexpected,
                                base::Unretained(this)),
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  socket_data_provider_.set_async_write(false);
  base::RunLoop().RunUntilIdle();
  ASSERT_GE(socket_data_provider_.written_data().size(),
            static_cast<size_t>(test_buffer_->size()));
  EXPECT_EQ(0, memcmp(test_buffer_->data(),
                      socket_data_provider_.written_data().data(),
                      test_buffer_->size()));
}

// Verify that it stops writing after the first error.
TEST_F(BufferedSocketWriterTest, TestWriteErrorSync) {
  StartWriter();
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  writer_->Write(test_buffer_, base::OnceClosure(),
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  socket_data_provider_.set_async_write(true);
  socket_data_provider_.set_next_write_error(net::ERR_FAILED);
  writer_->Write(test_buffer_2_,
                 base::BindOnce(&BufferedSocketWriterTest::Unexpected,
                                base::Unretained(this)),
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  socket_data_provider_.set_async_write(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FAILED, write_error_);
  EXPECT_EQ(static_cast<size_t>(test_buffer_->size()),
            socket_data_provider_.written_data().size());
}

// Verify that it stops writing after the first error.
TEST_F(BufferedSocketWriterTest, TestWriteErrorAsync) {
  StartWriter();
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  writer_->Write(test_buffer_, base::OnceClosure(),
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  socket_data_provider_.set_async_write(true);
  socket_data_provider_.set_next_write_error(net::ERR_FAILED);
  writer_->Write(test_buffer_2_,
                 base::BindOnce(&BufferedSocketWriterTest::Unexpected,
                                base::Unretained(this)),
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FAILED, write_error_);
  EXPECT_EQ(static_cast<size_t>(test_buffer_->size()),
            socket_data_provider_.written_data().size());
}

TEST_F(BufferedSocketWriterTest, WriteBeforeStart) {
  writer_->Write(test_buffer_, base::OnceClosure(),
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  writer_->Write(test_buffer_2_, base::OnceClosure(),
                 TRAFFIC_ANNOTATION_FOR_TESTS);

  StartWriter();
  base::RunLoop().RunUntilIdle();

  VerifyWrittenData();
}

}  // namespace remoting

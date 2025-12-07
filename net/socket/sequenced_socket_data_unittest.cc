// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsError;
using net::test::IsOk;

//-----------------------------------------------------------------------------

namespace net {

namespace {

const char kMsg1[] = "\0hello!\xff";
const int kLen1 = std::size(kMsg1);
const auto kMsg1Span = base::byte_span_with_nul_from_cstring(kMsg1);
const char kMsg2[] = "\0a2345678\0";
const int kLen2 = std::size(kMsg2);
const auto kMsg2Span = base::byte_span_with_nul_from_cstring(kMsg2);
const char kMsg3[] = "bye!";
const int kLen3 = std::size(kMsg3);
const auto kMsg3Span = base::byte_span_with_nul_from_cstring(kMsg3);
const char kMsg4[] = "supercalifragilisticexpialidocious";
const int kLen4 = std::size(kMsg4);
const auto kMsg4Span = base::byte_span_with_nul_from_cstring(kMsg4);

// Helper class for starting the next operation reentrantly after the
// previous operation completed asynchronously. When OnIOComplete is called,
// it will first verify that the previous operation behaved as expected. This is
// specified by either SetExpectedRead or SetExpectedWrite. It will then invoke
// a read or write operation specified by SetInvokeRead or SetInvokeWrite.
class ReentrantHelper {
 public:
  explicit ReentrantHelper(StreamSocket* socket) : socket_(socket) {}

  ReentrantHelper(const ReentrantHelper&) = delete;
  ReentrantHelper& operator=(const ReentrantHelper&) = delete;

  // Expect that the previous operation will return `first_read.size()` and will
  // fill `first_read_buf_` with `first_read`.
  void SetExpectedRead(base::span<const uint8_t> first_read) {
    verify_read_ = true;
    first_read_buf_ = base::MakeRefCounted<IOBufferWithSize>(first_read.size());
    first_read_ = first_read;
    first_len_ = first_read.size();
  }

  // Expect that the previous operation will return |first_len|.
  void SetExpectedWrite(int first_len) {
    verify_read_ = false;
    first_len_ = first_len;
  }

  // After verifying expectations, invoke a read of |read_len| bytes into
  // |read_buf|, notifying |callback| when complete.
  void SetInvokeRead(scoped_refptr<IOBuffer> read_buf,
                     int read_len,
                     int second_rv,
                     CompletionOnceCallback callback) {
    second_read_ = true;
    second_read_buf_ = read_buf;
    second_rv_ = second_rv;
    second_callback_ = std::move(callback);
    second_len_ = read_len;
  }

  // After verifying expectations, invoke a write of `write.size()` bytes from
  // `write`, notifying `callback` when complete.
  void SetInvokeWrite(base::span<const uint8_t> write,
                      int second_rv,
                      CompletionOnceCallback callback) {
    second_read_ = false;
    second_rv_ = second_rv;
    second_write_ = write;
    second_callback_ = std::move(callback);
    second_len_ = write.size();
  }

  // Returns the OnIOComplete callback for this helper.
  CompletionOnceCallback callback() {
    return base::BindOnce(&ReentrantHelper::OnIOComplete,
                          base::Unretained(this));
  }

  // Retuns the buffer where data is expected to have been written,
  // when checked by SetExpectRead()
  scoped_refptr<IOBuffer> read_buf() { return first_read_buf_; }

 private:
  void OnIOComplete(int rv) {
    CHECK_NE(-1, first_len_) << "Expectation not set.";
    CHECK_NE(-1, second_len_) << "Invocation not set.";
    ASSERT_EQ(first_len_, rv);
    if (verify_read_) {
      ASSERT_EQ(first_read_, first_read_buf_->first(rv));
    }

    if (second_read_) {
      ASSERT_EQ(second_rv_, socket_->Read(second_read_buf_.get(), second_len_,
                                          std::move(second_callback_)));
    } else {
      auto write_buf = base::MakeRefCounted<IOBufferWithSize>(second_len_);
      write_buf->span().copy_from_nonoverlapping(second_write_);
      ASSERT_EQ(second_rv_, socket_->Write(write_buf.get(), second_len_,
                                           std::move(second_callback_),
                                           TRAFFIC_ANNOTATION_FOR_TESTS));
    }
  }

  raw_ptr<StreamSocket> socket_;

  bool verify_read_ = false;
  scoped_refptr<IOBuffer> first_read_buf_;
  int first_len_ = -1;
  base::raw_span<const uint8_t> first_read_;

  CompletionOnceCallback second_callback_;
  bool second_read_ = false;
  int second_rv_;
  scoped_refptr<IOBuffer> second_read_buf_;
  int second_len_ = -1;
  base::raw_span<const uint8_t> second_write_;
};

class SequencedSocketDataTest : public TestWithTaskEnvironment {
 public:
  SequencedSocketDataTest();
  ~SequencedSocketDataTest() override;

  // This method is used as the completion callback for an async read
  // operation and when invoked, it verifies that the correct data was read,
  // then reads from the socket and verifies that it returns the correct
  // value.
  void ReentrantReadCallback(base::span<const uint8_t> data,
                             int len2,
                             int expected_rv2,
                             int rv);

  // This method is used at the completion callback for an async operation.
  // When executed, verifies that |rv| equals |expected_rv| and then
  // attempts an aync read from the socket into |read_buf_| (initialized
  // to |read_buf_len|) using |callback|.
  void ReentrantAsyncReadCallback(int len1, int len2, int rv);

  // This method is used as the completion callback for an async write
  // operation and when invoked, it verifies that the write returned correctly,
  // then
  // attempts to write to the socket and verifies that it returns the
  // correct value.
  void ReentrantWriteCallback(int expected_rv1,
                              base::span<const uint8_t> data,
                              int expected_rv2,
                              int rv);

  // This method is used at the completion callback for an async operation.
  // When executed, verifies that |rv| equals |expected_rv| and then
  // attempts an aync write of |data| with |callback|
  void ReentrantAsyncWriteCallback(base::span<const uint8_t> data,
                                   CompletionOnceCallback callback,
                                   int expected_rv,
                                   int rv);

  // Callback which adds a failure if it's ever called.
  void FailingCompletionCallback(int rv);

 protected:
  void Initialize(base::span<const MockRead> reads,
                  base::span<const MockWrite> writes);

  void AssertSyncReadEquals(base::span<const uint8_t> data);
  void AssertAsyncReadEquals(base::span<const uint8_t> data);
  void AssertReadReturns(int len, int rv);
  void AssertReadBufferEquals(base::span<const uint8_t> data);

  void AssertSyncWriteEquals(base::span<const uint8_t> data);
  void AssertAsyncWriteEquals(base::span<const uint8_t> data);
  void AssertWriteReturns(base::span<const uint8_t> data, int rv);

  bool IsPaused() const;
  void Resume();
  void RunUntilPaused();

  // When a given test completes, data_.at_eof() is expected to
  // match the value specified here. Most test should consume all
  // reads and writes, but some tests verify error handling behavior
  // do not consume all data.
  void set_expect_eof(bool expect_eof) { expect_eof_ = expect_eof; }

  CompletionOnceCallback failing_callback() {
    return base::BindOnce(&SequencedSocketDataTest::FailingCompletionCallback,
                          base::Unretained(this));
  }

  TestCompletionCallback read_callback_;
  scoped_refptr<IOBuffer> read_buf_;
  TestCompletionCallback write_callback_;

  std::unique_ptr<SequencedSocketData> data_;

  MockClientSocketFactory socket_factory_;
  bool expect_eof_ = true;

  std::unique_ptr<StreamSocket> sock_;
};

SequencedSocketDataTest::SequencedSocketDataTest() = default;

SequencedSocketDataTest::~SequencedSocketDataTest() {
  // Make sure no unexpected pending tasks will cause a failure.
  base::RunLoop().RunUntilIdle();
  if (expect_eof_) {
    EXPECT_EQ(expect_eof_, data_->AllReadDataConsumed());
    EXPECT_EQ(expect_eof_, data_->AllWriteDataConsumed());
  }
}

void SequencedSocketDataTest::Initialize(base::span<const MockRead> reads,
                                         base::span<const MockWrite> writes) {
  data_ = std::make_unique<SequencedSocketData>(MockConnect(SYNCHRONOUS, OK),
                                                reads, writes);
  socket_factory_.AddSocketDataProvider(data_.get());
  sock_ = socket_factory_.CreateTransportClientSocket(
      AddressList(IPEndPoint(IPAddress::IPv4Localhost(), 443)),
      nullptr /* socket_performance_watcher */,
      nullptr /* network_quality_estimator */, nullptr /* net_log */,
      NetLogSource());
  TestCompletionCallback callback;
  EXPECT_EQ(OK, sock_->Connect(callback.callback()));
}

void SequencedSocketDataTest::AssertSyncReadEquals(
    base::span<const uint8_t> data) {
  // Issue the read, which will complete immediately.
  AssertReadReturns(data.size(), data.size());
  AssertReadBufferEquals(data);
}

void SequencedSocketDataTest::AssertAsyncReadEquals(
    base::span<const uint8_t> data) {
  // Issue the read, which will be completed asynchronously.
  AssertReadReturns(data.size(), ERR_IO_PENDING);

  EXPECT_TRUE(sock_->IsConnected());

  // Now the read should complete.
  ASSERT_EQ(data.size(), read_callback_.WaitForResult());
  AssertReadBufferEquals(data);
}

void SequencedSocketDataTest::AssertReadReturns(int len, int rv) {
  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(len);
  if (rv == ERR_IO_PENDING) {
    ASSERT_EQ(rv, sock_->Read(read_buf_.get(), len, read_callback_.callback()));
    ASSERT_FALSE(read_callback_.have_result());
  } else {
    ASSERT_EQ(rv, sock_->Read(read_buf_.get(), len, failing_callback()));
  }
}

void SequencedSocketDataTest::AssertReadBufferEquals(
    base::span<const uint8_t> data) {
  ASSERT_EQ(data, read_buf_->first(data.size()));
}

void SequencedSocketDataTest::AssertSyncWriteEquals(
    base::span<const uint8_t> data) {
  // Issue the write, which should be complete immediately.
  AssertWriteReturns(data, data.size());
  ASSERT_FALSE(write_callback_.have_result());
}

void SequencedSocketDataTest::AssertAsyncWriteEquals(
    base::span<const uint8_t> data) {
  // Issue the read, which should be completed asynchronously.
  AssertWriteReturns(data, ERR_IO_PENDING);

  EXPECT_FALSE(read_callback_.have_result());
  EXPECT_TRUE(sock_->IsConnected());

  ASSERT_EQ(data.size(), write_callback_.WaitForResult());
}

bool SequencedSocketDataTest::IsPaused() const {
  return data_->IsPaused();
}

void SequencedSocketDataTest::Resume() {
  data_->Resume();
}

void SequencedSocketDataTest::RunUntilPaused() {
  data_->RunUntilPaused();
}

void SequencedSocketDataTest::AssertWriteReturns(base::span<const uint8_t> data,
                                                 int rv) {
  auto buf = base::MakeRefCounted<IOBufferWithSize>(data.size());
  buf->span().copy_from_nonoverlapping(data);

  if (rv == ERR_IO_PENDING) {
    ASSERT_EQ(rv,
              sock_->Write(buf.get(), data.size(), write_callback_.callback(),
                           TRAFFIC_ANNOTATION_FOR_TESTS));
    ASSERT_FALSE(write_callback_.have_result());
  } else {
    ASSERT_EQ(rv, sock_->Write(buf.get(), data.size(), failing_callback(),
                               TRAFFIC_ANNOTATION_FOR_TESTS));
  }
}

void SequencedSocketDataTest::ReentrantReadCallback(
    base::span<const uint8_t> data,
    int len2,
    int expected_rv2,
    int rv) {
  ASSERT_EQ(data.size(), rv);
  AssertReadBufferEquals(data);

  AssertReadReturns(len2, expected_rv2);
}

void SequencedSocketDataTest::ReentrantAsyncReadCallback(int expected_rv,
                                                         int len,
                                                         int rv) {
  ASSERT_EQ(expected_rv, rv);

  AssertReadReturns(len, ERR_IO_PENDING);
}

void SequencedSocketDataTest::ReentrantWriteCallback(
    int expected_rv1,
    base::span<const uint8_t> data,
    int expected_rv2,
    int rv) {
  ASSERT_EQ(expected_rv1, rv);

  AssertWriteReturns(data, expected_rv2);
}

void SequencedSocketDataTest::ReentrantAsyncWriteCallback(
    base::span<const uint8_t> data,
    CompletionOnceCallback callback,
    int expected_rv,
    int rv) {
  EXPECT_EQ(expected_rv, rv);
  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(data.size());
  write_buf->span().copy_from_nonoverlapping(data);
  EXPECT_THAT(sock_->Write(write_buf.get(), data.size(), std::move(callback),
                           TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));
}

void SequencedSocketDataTest::FailingCompletionCallback(int rv) {
  ADD_FAILURE() << "Callback should not have been invoked";
}

// ----------- Read

TEST_F(SequencedSocketDataTest, SingleSyncRead) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());
  AssertSyncReadEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, MultipleSyncReads) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kMsg1Span), MockRead(SYNCHRONOUS, 1, kMsg2Span),
      MockRead(SYNCHRONOUS, 2, kMsg3Span), MockRead(SYNCHRONOUS, 3, kMsg3Span),
      MockRead(SYNCHRONOUS, 4, kMsg2Span), MockRead(SYNCHRONOUS, 5, kMsg3Span),
      MockRead(SYNCHRONOUS, 6, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertSyncReadEquals(kMsg1Span);
  AssertSyncReadEquals(kMsg2Span);
  AssertSyncReadEquals(kMsg3Span);
  AssertSyncReadEquals(kMsg3Span);
  AssertSyncReadEquals(kMsg2Span);
  AssertSyncReadEquals(kMsg3Span);
  AssertSyncReadEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, SingleAsyncRead) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertAsyncReadEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, MultipleAsyncReads) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span), MockRead(ASYNC, 1, kMsg2Span),
      MockRead(ASYNC, 2, kMsg3Span), MockRead(ASYNC, 3, kMsg3Span),
      MockRead(ASYNC, 4, kMsg2Span), MockRead(ASYNC, 5, kMsg3Span),
      MockRead(ASYNC, 6, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertAsyncReadEquals(kMsg1Span);
  AssertAsyncReadEquals(kMsg2Span);
  AssertAsyncReadEquals(kMsg3Span);
  AssertAsyncReadEquals(kMsg3Span);
  AssertAsyncReadEquals(kMsg2Span);
  AssertAsyncReadEquals(kMsg3Span);
  AssertAsyncReadEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, MixedReads) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kMsg1Span), MockRead(ASYNC, 1, kMsg2Span),
      MockRead(SYNCHRONOUS, 2, kMsg3Span), MockRead(ASYNC, 3, kMsg3Span),
      MockRead(SYNCHRONOUS, 4, kMsg2Span), MockRead(ASYNC, 5, kMsg3Span),
      MockRead(SYNCHRONOUS, 6, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertSyncReadEquals(kMsg1Span);
  AssertAsyncReadEquals(kMsg2Span);
  AssertSyncReadEquals(kMsg3Span);
  AssertAsyncReadEquals(kMsg3Span);
  AssertSyncReadEquals(kMsg2Span);
  AssertAsyncReadEquals(kMsg3Span);
  AssertSyncReadEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, SyncReadFromCompletionCallback) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
      MockRead(SYNCHRONOUS, 1, kMsg2Span),
  };

  Initialize(reads, base::span<MockWrite>());

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf_.get(), kLen1,
                        base::BindOnce(
                            &SequencedSocketDataTest::ReentrantReadCallback,
                            base::Unretained(this), kMsg1Span, kLen2, kLen2)));

  base::RunLoop().RunUntilIdle();
  AssertReadBufferEquals(kMsg2Span);
}

TEST_F(SequencedSocketDataTest, ManyReentrantReads) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
      MockRead(ASYNC, 1, kMsg2Span),
      MockRead(ASYNC, 2, kMsg3Span),
      MockRead(ASYNC, 3, kMsg4Span),
  };

  Initialize(reads, base::span<MockWrite>());

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen4);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedRead(kMsg3Span);
  helper3.SetInvokeRead(read_buf_, kLen4, ERR_IO_PENDING,
                        read_callback_.callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedRead(kMsg2Span);
  helper2.SetInvokeRead(helper3.read_buf(), kLen3, ERR_IO_PENDING,
                        helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedRead(kMsg1Span);
  helper.SetInvokeRead(helper2.read_buf(), kLen2, ERR_IO_PENDING,
                       helper2.callback());

  sock_->Read(helper.read_buf().get(), kLen1, helper.callback());

  ASSERT_EQ(kLen4, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg4Span);
}

TEST_F(SequencedSocketDataTest, AsyncReadFromCompletionCallback) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
      MockRead(ASYNC, 1, kMsg2Span),
  };

  Initialize(reads, base::span<MockWrite>());

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(
                read_buf_.get(), kLen1,
                base::BindOnce(&SequencedSocketDataTest::ReentrantReadCallback,
                               base::Unretained(this), kMsg1Span, kLen2,
                               ERR_IO_PENDING)));

  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2Span);
}

TEST_F(SequencedSocketDataTest, SingleSyncReadTooEarly) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 1, kMsg1Span),
  };

  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, 0)};

  Initialize(reads, writes);

  EXPECT_NONFATAL_FAILURE(AssertReadReturns(kLen1, ERR_UNEXPECTED),
                          "Unable to perform synchronous IO while stopped");
  set_expect_eof(false);
}

TEST_F(SequencedSocketDataTest, SingleSyncReadSmallBuffer) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());

  auto [chunk_1, chunk_2] = kMsg1Span.split_at(kMsg1Span.size() - 1);
  EXPECT_EQ(chunk_2.size(), 1u);

  // Read the first chunk.
  AssertReadReturns(chunk_1.size(), chunk_1.size());
  AssertReadBufferEquals(chunk_1);
  // Then read the second chunk.
  AssertReadReturns(1, 1);
  AssertReadBufferEquals(chunk_2);
}

TEST_F(SequencedSocketDataTest, SingleSyncReadLargeBuffer) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(2 * kLen1);
  ASSERT_EQ(kLen1, sock_->Read(read_buf.get(), 2 * kLen1, failing_callback()));
  ASSERT_EQ(std::string(kMsg1, kLen1), std::string(read_buf->data(), kLen1));
}

TEST_F(SequencedSocketDataTest, SingleAsyncReadLargeBuffer) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());

  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(2 * kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf.get(), 2 * kLen1, read_callback_.callback()));
  ASSERT_EQ(kLen1, read_callback_.WaitForResult());
  ASSERT_EQ(std::string(kMsg1, kLen1), std::string(read_buf->data(), kLen1));
}

TEST_F(SequencedSocketDataTest, HangingRead) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };

  Initialize(reads, base::span<MockWrite>());

  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf.get(), 1, read_callback_.callback()));
  ASSERT_FALSE(read_callback_.have_result());

  // Even though the read is scheduled to complete at sequence number 0,
  // verify that the read callback in never called.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(read_callback_.have_result());
}

// ----------- Write

TEST_F(SequencedSocketDataTest, SingleSyncWriteTooEarly) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 1, kMsg1Span),
  };

  MockRead reads[] = {MockRead(SYNCHRONOUS, 0, 0)};

  Initialize(reads, writes);

  EXPECT_NONFATAL_FAILURE(AssertWriteReturns(kMsg1Span, ERR_UNEXPECTED),
                          "Unable to perform synchronous IO while stopped");

  set_expect_eof(false);
}

TEST_F(SequencedSocketDataTest, SingleSyncWriteTooSmall) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, kMsg1Span),
  };

  Initialize(base::span<MockRead>(), writes);

  // Expecting too small of a write triggers multiple expectation failures.
  //
  // The gtest infrastructure does not have a macro similar to
  // EXPECT_NONFATAL_FAILURE which works when there is more than one
  // failure.
  //
  // However, tests can gather the TestPartResultArray and directly
  // validate the test failures. That's what the rest of this test does.

  ::testing::TestPartResultArray gtest_failures;

  {
    ::testing::ScopedFakeTestPartResultReporter gtest_reporter(
        ::testing::ScopedFakeTestPartResultReporter::
            INTERCEPT_ONLY_CURRENT_THREAD,
        &gtest_failures);
    AssertSyncWriteEquals(kMsg1Span.first(kMsg1Span.size() - 1));
  }

  static auto kExpectedFailures =
      std::to_array<const char*>({"Value of: actual_data == expected_data\n  "
                                  "Actual: false\nExpected: true",
                                  "Expected equality of these values:\n  rv"});
  ASSERT_EQ(std::size(kExpectedFailures),
            static_cast<size_t>(gtest_failures.size()));

  for (int i = 0; i < gtest_failures.size(); ++i) {
    const ::testing::TestPartResult& result =
        gtest_failures.GetTestPartResult(i);
    EXPECT_NE(std::string(result.message()).find(kExpectedFailures[i]),
              std::string::npos);
  }

  set_expect_eof(false);
}

TEST_F(SequencedSocketDataTest, SingleSyncPartialWrite) {
  size_t split = base::checked_cast<size_t>(kLen1 - 1);
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, kMsg1Span.first(split)),
      MockWrite(SYNCHRONOUS, 1, kMsg1Span.subspan(split)),
  };

  Initialize(base::span<MockRead>(), writes);

  // Attempt to write all of the message, but only some will be written.
  AssertSyncWriteEquals(kMsg1Span.first(split));
  // Write the rest of the message.
  AssertSyncWriteEquals(kMsg1Span.subspan(split));
}

TEST_F(SequencedSocketDataTest, SingleSyncWrite) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, kMsg1Span),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertSyncWriteEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, MultipleSyncWrites) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, kMsg1Span),
      MockWrite(SYNCHRONOUS, 1, kMsg2Span),
      MockWrite(SYNCHRONOUS, 2, kMsg3Span),
      MockWrite(SYNCHRONOUS, 3, kMsg3Span),
      MockWrite(SYNCHRONOUS, 4, kMsg2Span),
      MockWrite(SYNCHRONOUS, 5, kMsg3Span),
      MockWrite(SYNCHRONOUS, 6, kMsg1Span),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertSyncWriteEquals(kMsg1Span);
  AssertSyncWriteEquals(kMsg2Span);
  AssertSyncWriteEquals(kMsg3Span);
  AssertSyncWriteEquals(kMsg3Span);
  AssertSyncWriteEquals(kMsg2Span);
  AssertSyncWriteEquals(kMsg3Span);
  AssertSyncWriteEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, SingleAsyncWrite) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, kMsg1Span),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertAsyncWriteEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, MultipleAsyncWrites) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, kMsg1Span), MockWrite(ASYNC, 1, kMsg2Span),
      MockWrite(ASYNC, 2, kMsg3Span), MockWrite(ASYNC, 3, kMsg3Span),
      MockWrite(ASYNC, 4, kMsg2Span), MockWrite(ASYNC, 5, kMsg3Span),
      MockWrite(ASYNC, 6, kMsg1Span),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertAsyncWriteEquals(kMsg1Span);
  AssertAsyncWriteEquals(kMsg2Span);
  AssertAsyncWriteEquals(kMsg3Span);
  AssertAsyncWriteEquals(kMsg3Span);
  AssertAsyncWriteEquals(kMsg2Span);
  AssertAsyncWriteEquals(kMsg3Span);
  AssertAsyncWriteEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, MixedWrites) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, kMsg1Span), MockWrite(ASYNC, 1, kMsg2Span),
      MockWrite(SYNCHRONOUS, 2, kMsg3Span), MockWrite(ASYNC, 3, kMsg3Span),
      MockWrite(SYNCHRONOUS, 4, kMsg2Span), MockWrite(ASYNC, 5, kMsg3Span),
      MockWrite(SYNCHRONOUS, 6, kMsg1Span),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertSyncWriteEquals(kMsg1Span);
  AssertAsyncWriteEquals(kMsg2Span);
  AssertSyncWriteEquals(kMsg3Span);
  AssertAsyncWriteEquals(kMsg3Span);
  AssertSyncWriteEquals(kMsg2Span);
  AssertAsyncWriteEquals(kMsg3Span);
  AssertSyncWriteEquals(kMsg1Span);
}

TEST_F(SequencedSocketDataTest, SyncWriteFromCompletionCallback) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, kMsg1Span),
      MockWrite(SYNCHRONOUS, 1, kMsg2Span),
  };

  Initialize(base::span<MockRead>(), writes);

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  write_buf->span().copy_from_nonoverlapping(kMsg1Span);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Write(
                write_buf.get(), kLen1,
                base::BindOnce(&SequencedSocketDataTest::ReentrantWriteCallback,
                               base::Unretained(this), kLen1, kMsg2Span, kLen2),
                TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();
}

TEST_F(SequencedSocketDataTest, AsyncWriteFromCompletionCallback) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, kMsg1Span),
      MockWrite(ASYNC, 1, kMsg2Span),
  };

  Initialize(base::span<MockRead>(), writes);

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  write_buf->span().copy_from_nonoverlapping(kMsg1Span);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Write(
                write_buf.get(), kLen1,
                base::BindOnce(&SequencedSocketDataTest::ReentrantWriteCallback,
                               base::Unretained(this), kLen1, kMsg2Span,
                               ERR_IO_PENDING),
                TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());
}

TEST_F(SequencedSocketDataTest, ManyReentrantWrites) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, kMsg1Span),
      MockWrite(ASYNC, 1, kMsg2Span),
      MockWrite(ASYNC, 2, kMsg3Span),
      MockWrite(ASYNC, 3, kMsg4Span),
  };

  Initialize(base::span<MockRead>(), writes);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedWrite(kLen3);
  helper3.SetInvokeWrite(kMsg4Span, ERR_IO_PENDING, write_callback_.callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedWrite(kLen2);
  helper2.SetInvokeWrite(kMsg3Span, ERR_IO_PENDING, helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedWrite(kLen1);
  helper.SetInvokeWrite(kMsg2Span, ERR_IO_PENDING, helper2.callback());

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  write_buf->span().copy_from_nonoverlapping(kMsg1Span);
  sock_->Write(write_buf.get(), kLen1, helper.callback(),
               TRAFFIC_ANNOTATION_FOR_TESTS);

  ASSERT_EQ(kLen4, write_callback_.WaitForResult());
}

// ----------- Mixed Reads and Writes

TEST_F(SequencedSocketDataTest, MixedSyncOperations) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kMsg1Span),
      MockRead(SYNCHRONOUS, 3, kMsg2Span),
  };

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 1, kMsg2Span),
      MockWrite(SYNCHRONOUS, 2, kMsg3Span),
  };

  Initialize(reads, writes);

  AssertSyncReadEquals(kMsg1Span);
  AssertSyncWriteEquals(kMsg2Span);
  AssertSyncWriteEquals(kMsg3Span);
  AssertSyncReadEquals(kMsg2Span);
}

TEST_F(SequencedSocketDataTest, MixedAsyncOperations) {
  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
      MockRead(ASYNC, 3, kMsg2Span),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, 1, kMsg2Span),
      MockWrite(ASYNC, 2, kMsg3Span),
  };

  Initialize(reads, writes);

  AssertAsyncReadEquals(kMsg1Span);
  AssertAsyncWriteEquals(kMsg2Span);
  AssertAsyncWriteEquals(kMsg3Span);
  AssertAsyncReadEquals(kMsg2Span);
}

TEST_F(SequencedSocketDataTest, InterleavedAsyncOperations) {
  // Order of completion is read, write, write, read.
  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
      MockRead(ASYNC, 3, kMsg2Span),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, 1, kMsg2Span),
      MockWrite(ASYNC, 2, kMsg3Span),
  };

  Initialize(reads, writes);

  // Issue the write, which will block until the read completes.
  AssertWriteReturns(kMsg2Span, ERR_IO_PENDING);

  // Issue the read which will return first.
  AssertReadReturns(kLen1, ERR_IO_PENDING);

  ASSERT_EQ(kLen1, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg1Span);

  // Run posted OnWriteComplete().
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());

  // Issue the read, which will block until the write completes.
  AssertReadReturns(kLen2, ERR_IO_PENDING);

  // Issue the writes which will return first.
  AssertWriteReturns(kMsg3Span, ERR_IO_PENDING);
  ASSERT_EQ(kLen3, write_callback_.WaitForResult());

  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2Span);
}

TEST_F(SequencedSocketDataTest, InterleavedMixedOperations) {
  // Order of completion is read, write, write, read.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kMsg1Span),
      MockRead(ASYNC, 3, kMsg2Span),
      MockRead(ASYNC, 5, kMsg3Span),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, 1, kMsg2Span),
      MockWrite(SYNCHRONOUS, 2, kMsg3Span),
      MockWrite(SYNCHRONOUS, 4, kMsg1Span),
  };

  Initialize(reads, writes);

  // Issue the write, which will block until the read completes.
  AssertWriteReturns(kMsg2Span, ERR_IO_PENDING);

  // Issue the writes which will complete immediately.
  AssertSyncReadEquals(kMsg1Span);

  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());

  // Issue the read, which will block until the write completes.
  AssertReadReturns(kLen2, ERR_IO_PENDING);

  // Issue the writes which will complete immediately.
  AssertSyncWriteEquals(kMsg3Span);

  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2Span);

  // Issue the read, which will block until the write completes.
  AssertReadReturns(kLen2, ERR_IO_PENDING);

  // Issue the writes which will complete immediately.
  AssertSyncWriteEquals(kMsg1Span);

  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_EQ(kLen3, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg3Span);
}

TEST_F(SequencedSocketDataTest, AsyncReadFromWriteCompletionCallback) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, kMsg1Span),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 1, kMsg2Span),
  };

  Initialize(reads, writes);

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  write_buf->span().copy_from_nonoverlapping(kMsg1Span);
  ASSERT_EQ(
      ERR_IO_PENDING,
      sock_->Write(
          write_buf.get(), kLen1,
          base::BindOnce(&SequencedSocketDataTest::ReentrantAsyncReadCallback,
                         base::Unretained(this), kLen1, kLen2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2Span);
}

TEST_F(SequencedSocketDataTest, AsyncWriteFromReadCompletionCallback) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 1, kMsg2Span),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
  };

  Initialize(reads, writes);

  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(
      ERR_IO_PENDING,
      sock_->Read(
          read_buf.get(), kLen1,
          base::BindOnce(&SequencedSocketDataTest::ReentrantAsyncWriteCallback,
                         base::Unretained(this), kMsg2Span,
                         write_callback_.callback(), kLen1)));

  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());
}

TEST_F(SequencedSocketDataTest, MixedReentrantOperations) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, kMsg1Span),
      MockWrite(ASYNC, 2, kMsg3Span),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 1, kMsg2Span),
      MockRead(ASYNC, 3, kMsg4Span),
  };

  Initialize(reads, writes);

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen4);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedWrite(kLen3);
  helper3.SetInvokeRead(read_buf_, kLen4, ERR_IO_PENDING,
                        read_callback_.callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedRead(kMsg2Span);
  helper2.SetInvokeWrite(kMsg3Span, ERR_IO_PENDING, helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedWrite(kLen1);
  helper.SetInvokeRead(helper2.read_buf(), kLen2, ERR_IO_PENDING,
                       helper2.callback());

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  write_buf->span().copy_from_nonoverlapping(kMsg1Span);
  sock_->Write(write_buf.get(), kLen1, helper.callback(),
               TRAFFIC_ANNOTATION_FOR_TESTS);

  ASSERT_EQ(kLen4, read_callback_.WaitForResult());
}

TEST_F(SequencedSocketDataTest, MixedReentrantOperationsThenSynchronousRead) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, kMsg1Span),
      MockWrite(ASYNC, 2, kMsg3Span),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 1, kMsg2Span),
      MockRead(SYNCHRONOUS, 3, kMsg4Span),
  };

  Initialize(reads, writes);

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen4);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedWrite(kLen3);
  helper3.SetInvokeRead(read_buf_, kLen4, kLen4, failing_callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedRead(kMsg2Span);
  helper2.SetInvokeWrite(kMsg3Span, ERR_IO_PENDING, helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedWrite(kLen1);
  helper.SetInvokeRead(helper2.read_buf(), kLen2, ERR_IO_PENDING,
                       helper2.callback());

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  write_buf->span().copy_from_nonoverlapping(kMsg1Span);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Write(write_buf.get(), kLen1, helper.callback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();
  AssertReadBufferEquals(kMsg4Span);
}

TEST_F(SequencedSocketDataTest, MixedReentrantOperationsThenSynchronousWrite) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 1, kMsg2Span),
      MockWrite(SYNCHRONOUS, 3, kMsg4Span),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 0, kMsg1Span),
      MockRead(ASYNC, 2, kMsg3Span),
  };

  Initialize(reads, writes);

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen4);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedRead(kMsg3Span);
  helper3.SetInvokeWrite(kMsg4Span, kLen4, failing_callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedWrite(kLen2);
  helper2.SetInvokeRead(helper3.read_buf(), kLen3, ERR_IO_PENDING,
                        helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedRead(kMsg1Span);
  helper.SetInvokeWrite(kMsg2Span, ERR_IO_PENDING, helper2.callback());

  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(helper.read_buf().get(), kLen1, helper.callback()));

  base::RunLoop().RunUntilIdle();
}

// Test the basic case where a read is paused.
TEST_F(SequencedSocketDataTest, PauseAndResume_PauseRead) {
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 0),
      MockRead(ASYNC, 1, kMsg1Span),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertReadReturns(kLen1, ERR_IO_PENDING);
  ASSERT_FALSE(read_callback_.have_result());

  RunUntilPaused();
  ASSERT_TRUE(IsPaused());

  // Spinning the message loop should do nothing.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_TRUE(IsPaused());

  Resume();
  ASSERT_FALSE(IsPaused());
  ASSERT_TRUE(read_callback_.have_result());
  ASSERT_EQ(kLen1, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg1Span);
}

// Test the case where a read that will be paused is started before write that
// completes before the pause.
TEST_F(SequencedSocketDataTest, PauseAndResume_WritePauseRead) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, kMsg1Span),
  };

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 2, kMsg2Span),
  };

  Initialize(reads, writes);

  AssertReadReturns(kLen2, ERR_IO_PENDING);
  ASSERT_FALSE(read_callback_.have_result());

  // Nothing should happen until the write starts.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_FALSE(IsPaused());

  AssertSyncWriteEquals(kMsg1Span);

  RunUntilPaused();
  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_TRUE(IsPaused());

  // Spinning the message loop should do nothing.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_TRUE(IsPaused());

  Resume();
  ASSERT_FALSE(IsPaused());
  ASSERT_TRUE(read_callback_.have_result());
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2Span);
}

// Test the basic case where a write is paused.
TEST_F(SequencedSocketDataTest, PauseAndResume_PauseWrite) {
  MockWrite writes[] = {
      MockWrite(ASYNC, ERR_IO_PENDING, 0),
      MockWrite(ASYNC, 1, kMsg1Span),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertWriteReturns(kMsg1Span, ERR_IO_PENDING);
  ASSERT_FALSE(write_callback_.have_result());

  RunUntilPaused();
  ASSERT_TRUE(IsPaused());

  // Spinning the message loop should do nothing.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_TRUE(IsPaused());

  Resume();
  ASSERT_FALSE(IsPaused());
  ASSERT_TRUE(write_callback_.have_result());
  ASSERT_EQ(kLen1, write_callback_.WaitForResult());
}

// Test the case where a write that will be paused is started before read that
// completes before the pause.
TEST_F(SequencedSocketDataTest, PauseAndResume_ReadPauseWrite) {
  MockWrite writes[] = {
      MockWrite(ASYNC, ERR_IO_PENDING, 1),
      MockWrite(ASYNC, 2, kMsg2Span),
  };

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, kMsg1Span),
  };

  Initialize(reads, writes);

  AssertWriteReturns(kMsg2Span, ERR_IO_PENDING);
  ASSERT_FALSE(write_callback_.have_result());

  // Nothing should happen until the write starts.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_FALSE(IsPaused());

  AssertSyncReadEquals(kMsg1Span);

  RunUntilPaused();
  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_TRUE(IsPaused());

  // Spinning the message loop should do nothing.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_TRUE(IsPaused());

  Resume();
  ASSERT_FALSE(IsPaused());
  ASSERT_TRUE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());
}

}  // namespace

}  // namespace net

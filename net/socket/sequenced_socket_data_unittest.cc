// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string>

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
const char kMsg2[] = "\0a2345678\0";
const int kLen2 = std::size(kMsg2);
const char kMsg3[] = "bye!";
const int kLen3 = std::size(kMsg3);
const char kMsg4[] = "supercalifragilisticexpialidocious";
const int kLen4 = std::size(kMsg4);

// Helper class for starting the next operation operation reentrantly after the
// previous operation completed asynchronously. When OnIOComplete is called,
// it will first verify that the previous operation behaved as expected. This is
// specified by either SetExpectedRead or SetExpectedWrite. It will then invoke
// a read or write operation specified by SetInvokeRead or SetInvokeWrite.
class ReentrantHelper {
 public:
  explicit ReentrantHelper(StreamSocket* socket) : socket_(socket) {}

  ReentrantHelper(const ReentrantHelper&) = delete;
  ReentrantHelper& operator=(const ReentrantHelper&) = delete;

  // Expect that the previous operation will return |first_len| and will fill
  // |first_read_data_| with |first_read_data|.
  void SetExpectedRead(const char* first_read_data, int first_len) {
    verify_read_ = true;
    first_read_buf_ = base::MakeRefCounted<IOBufferWithSize>(first_len);
    first_read_data_ = first_read_data;
    first_len_ = first_len;
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

  // After verifying expectations, invoke a write of |write_len| bytes from
  // |write_data|, notifying |callback| when complete.
  void SetInvokeWrite(const char* write_data,
                      int write_len,
                      int second_rv,
                      CompletionOnceCallback callback) {
    second_read_ = false;
    second_rv_ = second_rv;
    second_write_data_ = write_data;
    second_callback_ = std::move(callback);
    second_len_ = write_len;
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
      ASSERT_EQ(std::string(first_read_data_, first_len_),
                std::string(first_read_buf_->data(), rv));
    }

    if (second_read_) {
      ASSERT_EQ(second_rv_, socket_->Read(second_read_buf_.get(), second_len_,
                                          std::move(second_callback_)));
    } else {
      auto write_buf = base::MakeRefCounted<IOBufferWithSize>(second_len_);
      memcpy(write_buf->data(), second_write_data_, second_len_);
      ASSERT_EQ(second_rv_, socket_->Write(write_buf.get(), second_len_,
                                           std::move(second_callback_),
                                           TRAFFIC_ANNOTATION_FOR_TESTS));
    }
  }

  raw_ptr<StreamSocket> socket_;

  bool verify_read_ = false;
  scoped_refptr<IOBuffer> first_read_buf_;
  const char* first_read_data_ = nullptr;
  int first_len_ = -1;

  CompletionOnceCallback second_callback_;
  bool second_read_ = false;
  int second_rv_;
  scoped_refptr<IOBuffer> second_read_buf_;
  const char* second_write_data_ = nullptr;
  int second_len_ = -1;
};

class SequencedSocketDataTest : public TestWithTaskEnvironment {
 public:
  SequencedSocketDataTest();
  ~SequencedSocketDataTest() override;

  // This method is used as the completion callback for an async read
  // operation and when invoked, it verifies that the correct data was read,
  // then reads from the socket and verifies that that it returns the correct
  // value.
  void ReentrantReadCallback(const char* data,
                             int len1,
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
  // attempts to write to the socket and verifies that that it returns the
  // correct value.
  void ReentrantWriteCallback(int expected_rv1,
                              const char* data,
                              int len,
                              int expected_rv2,
                              int rv);

  // This method is used at the completion callback for an async operation.
  // When executed, verifies that |rv| equals |expected_rv| and then
  // attempts an aync write of |data| with |callback|
  void ReentrantAsyncWriteCallback(const char* data,
                                   int len,
                                   CompletionOnceCallback callback,
                                   int expected_rv,
                                   int rv);

  // Callback which adds a failure if it's ever called.
  void FailingCompletionCallback(int rv);

 protected:
  void Initialize(base::span<const MockRead> reads,
                  base::span<const MockWrite> writes);

  void AssertSyncReadEquals(const char* data, int len);
  void AssertAsyncReadEquals(const char* data, int len);
  void AssertReadReturns(int len, int rv);
  void AssertReadBufferEquals(const char* data, int len);

  void AssertSyncWriteEquals(const char* data, int len);
  void AssertAsyncWriteEquals(const char* data, int len);
  void AssertWriteReturns(const char* data, int len, int rv);

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

void SequencedSocketDataTest::AssertSyncReadEquals(const char* data, int len) {
  // Issue the read, which will complete immediately.
  AssertReadReturns(len, len);
  AssertReadBufferEquals(data, len);
}

void SequencedSocketDataTest::AssertAsyncReadEquals(const char* data, int len) {
  // Issue the read, which will be completed asynchronously.
  AssertReadReturns(len, ERR_IO_PENDING);

  EXPECT_TRUE(sock_->IsConnected());

  // Now the read should complete.
  ASSERT_EQ(len, read_callback_.WaitForResult());
  AssertReadBufferEquals(data, len);
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

void SequencedSocketDataTest::AssertReadBufferEquals(const char* data,
                                                     int len) {
  ASSERT_EQ(std::string(data, len), std::string(read_buf_->data(), len));
}

void SequencedSocketDataTest::AssertSyncWriteEquals(const char* data, int len) {
  // Issue the write, which should be complete immediately.
  AssertWriteReturns(data, len, len);
  ASSERT_FALSE(write_callback_.have_result());
}

void SequencedSocketDataTest::AssertAsyncWriteEquals(const char* data,
                                                     int len) {
  // Issue the read, which should be completed asynchronously.
  AssertWriteReturns(data, len, ERR_IO_PENDING);

  EXPECT_FALSE(read_callback_.have_result());
  EXPECT_TRUE(sock_->IsConnected());

  ASSERT_EQ(len, write_callback_.WaitForResult());
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

void SequencedSocketDataTest::AssertWriteReturns(const char* data,
                                                 int len,
                                                 int rv) {
  auto buf = base::MakeRefCounted<IOBufferWithSize>(len);
  memcpy(buf->data(), data, len);

  if (rv == ERR_IO_PENDING) {
    ASSERT_EQ(rv, sock_->Write(buf.get(), len, write_callback_.callback(),
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    ASSERT_FALSE(write_callback_.have_result());
  } else {
    ASSERT_EQ(rv, sock_->Write(buf.get(), len, failing_callback(),
                               TRAFFIC_ANNOTATION_FOR_TESTS));
  }
}

void SequencedSocketDataTest::ReentrantReadCallback(const char* data,
                                                    int len1,
                                                    int len2,
                                                    int expected_rv2,
                                                    int rv) {
  ASSERT_EQ(len1, rv);
  AssertReadBufferEquals(data, len1);

  AssertReadReturns(len2, expected_rv2);
}

void SequencedSocketDataTest::ReentrantAsyncReadCallback(int expected_rv,
                                                         int len,
                                                         int rv) {
  ASSERT_EQ(expected_rv, rv);

  AssertReadReturns(len, ERR_IO_PENDING);
}

void SequencedSocketDataTest::ReentrantWriteCallback(int expected_rv1,
                                                     const char* data,
                                                     int len,
                                                     int expected_rv2,
                                                     int rv) {
  ASSERT_EQ(expected_rv1, rv);

  AssertWriteReturns(data, len, expected_rv2);
}

void SequencedSocketDataTest::ReentrantAsyncWriteCallback(
    const char* data,
    int len,
    CompletionOnceCallback callback,
    int expected_rv,
    int rv) {
  EXPECT_EQ(expected_rv, rv);
  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(len);
  memcpy(write_buf->data(), data, len);
  EXPECT_THAT(sock_->Write(write_buf.get(), len, std::move(callback),
                           TRAFFIC_ANNOTATION_FOR_TESTS),
              IsError(ERR_IO_PENDING));
}

void SequencedSocketDataTest::FailingCompletionCallback(int rv) {
  ADD_FAILURE() << "Callback should not have been invoked";
}

// ----------- Read

TEST_F(SequencedSocketDataTest, SingleSyncRead) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 0),
  };

  Initialize(reads, base::span<MockWrite>());
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, MultipleSyncReads) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 0),
      MockRead(SYNCHRONOUS, kMsg2, kLen2, 1),
      MockRead(SYNCHRONOUS, kMsg3, kLen3, 2),
      MockRead(SYNCHRONOUS, kMsg3, kLen3, 3),
      MockRead(SYNCHRONOUS, kMsg2, kLen2, 4),
      MockRead(SYNCHRONOUS, kMsg3, kLen3, 5),
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 6),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg2, kLen2);
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, SingleAsyncRead) {
  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertAsyncReadEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, MultipleAsyncReads) {
  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0),
      MockRead(ASYNC, kMsg2, kLen2, 1),
      MockRead(ASYNC, kMsg3, kLen3, 2),
      MockRead(ASYNC, kMsg3, kLen3, 3),
      MockRead(ASYNC, kMsg2, kLen2, 4),
      MockRead(ASYNC, kMsg3, kLen3, 5),
      MockRead(ASYNC, kMsg1, kLen1, 6),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertAsyncReadEquals(kMsg1, kLen1);
  AssertAsyncReadEquals(kMsg2, kLen2);
  AssertAsyncReadEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg2, kLen2);
  AssertAsyncReadEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, MixedReads) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 0),
      MockRead(ASYNC, kMsg2, kLen2, 1),
      MockRead(SYNCHRONOUS, kMsg3, kLen3, 2),
      MockRead(ASYNC, kMsg3, kLen3, 3),
      MockRead(SYNCHRONOUS, kMsg2, kLen2, 4),
      MockRead(ASYNC, kMsg3, kLen3, 5),
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 6),
  };

  Initialize(reads, base::span<MockWrite>());

  AssertSyncReadEquals(kMsg1, kLen1);
  AssertAsyncReadEquals(kMsg2, kLen2);
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg2, kLen2);
  AssertAsyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, SyncReadFromCompletionCallback) {
  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0), MockRead(SYNCHRONOUS, kMsg2, kLen2, 1),
  };

  Initialize(reads, base::span<MockWrite>());

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(
      ERR_IO_PENDING,
      sock_->Read(
          read_buf_.get(), kLen1,
          base::BindOnce(&SequencedSocketDataTest::ReentrantReadCallback,
                         base::Unretained(this), kMsg1, kLen1, kLen2, kLen2)));

  base::RunLoop().RunUntilIdle();
  AssertReadBufferEquals(kMsg2, kLen2);
}

TEST_F(SequencedSocketDataTest, ManyReentrantReads) {
  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0),
      MockRead(ASYNC, kMsg2, kLen2, 1),
      MockRead(ASYNC, kMsg3, kLen3, 2),
      MockRead(ASYNC, kMsg4, kLen4, 3),
  };

  Initialize(reads, base::span<MockWrite>());

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen4);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedRead(kMsg3, kLen3);
  helper3.SetInvokeRead(read_buf_, kLen4, ERR_IO_PENDING,
                        read_callback_.callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedRead(kMsg2, kLen2);
  helper2.SetInvokeRead(helper3.read_buf(), kLen3, ERR_IO_PENDING,
                        helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedRead(kMsg1, kLen1);
  helper.SetInvokeRead(helper2.read_buf(), kLen2, ERR_IO_PENDING,
                       helper2.callback());

  sock_->Read(helper.read_buf().get(), kLen1, helper.callback());

  ASSERT_EQ(kLen4, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg4, kLen4);
}

TEST_F(SequencedSocketDataTest, AsyncReadFromCompletionCallback) {
  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0), MockRead(ASYNC, kMsg2, kLen2, 1),
  };

  Initialize(reads, base::span<MockWrite>());

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(
                read_buf_.get(), kLen1,
                base::BindOnce(&SequencedSocketDataTest::ReentrantReadCallback,
                               base::Unretained(this), kMsg1, kLen1, kLen2,
                               ERR_IO_PENDING)));

  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2, kLen2);
}

TEST_F(SequencedSocketDataTest, SingleSyncReadTooEarly) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 1),
  };

  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, 0)};

  Initialize(reads, writes);

  EXPECT_NONFATAL_FAILURE(AssertReadReturns(kLen1, ERR_UNEXPECTED),
                          "Unable to perform synchronous IO while stopped");
  set_expect_eof(false);
}

TEST_F(SequencedSocketDataTest, SingleSyncReadSmallBuffer) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 0),
  };

  Initialize(reads, base::span<MockWrite>());

  // Read the first chunk.
  AssertReadReturns(kLen1 - 1, kLen1 - 1);
  AssertReadBufferEquals(kMsg1, kLen1 - 1);
  // Then read the second chunk.
  AssertReadReturns(1, 1);
  AssertReadBufferEquals(kMsg1 + kLen1 - 1, 1);
}

TEST_F(SequencedSocketDataTest, SingleSyncReadLargeBuffer) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 0),
  };

  Initialize(reads, base::span<MockWrite>());
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(2 * kLen1);
  ASSERT_EQ(kLen1, sock_->Read(read_buf.get(), 2 * kLen1, failing_callback()));
  ASSERT_EQ(std::string(kMsg1, kLen1), std::string(read_buf->data(), kLen1));
}

TEST_F(SequencedSocketDataTest, SingleAsyncReadLargeBuffer) {
  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0),
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
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 1),
  };

  MockRead reads[] = {MockRead(SYNCHRONOUS, 0, 0)};

  Initialize(reads, writes);

  EXPECT_NONFATAL_FAILURE(AssertWriteReturns(kMsg1, kLen1, ERR_UNEXPECTED),
                          "Unable to perform synchronous IO while stopped");

  set_expect_eof(false);
}

TEST_F(SequencedSocketDataTest, SingleSyncWriteTooSmall) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 0),
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
    AssertSyncWriteEquals(kMsg1, kLen1 - 1);
  }

  static const char* kExpectedFailures[] = {
      "Value of: actual_data == expected_data\n  Actual: false\nExpected: true",
      "Expected equality of these values:\n  rv"};
  ASSERT_EQ(std::size(kExpectedFailures),
            static_cast<size_t>(gtest_failures.size()));

  for (int i = 0; i < gtest_failures.size(); ++i) {
    const ::testing::TestPartResult& result =
        gtest_failures.GetTestPartResult(i);
    EXPECT_TRUE(strstr(result.message(), kExpectedFailures[i]) != nullptr);
  }

  set_expect_eof(false);
}

TEST_F(SequencedSocketDataTest, SingleSyncPartialWrite) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kMsg1, kLen1 - 1, 0),
      MockWrite(SYNCHRONOUS, kMsg1 + kLen1 - 1, 1, 1),
  };

  Initialize(base::span<MockRead>(), writes);

  // Attempt to write all of the message, but only some will be written.
  AssertSyncWriteEquals(kMsg1, kLen1 - 1);
  // Write the rest of the message.
  AssertSyncWriteEquals(kMsg1 + kLen1 - 1, 1);
}

TEST_F(SequencedSocketDataTest, SingleSyncWrite) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 0),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertSyncWriteEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, MultipleSyncWrites) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 0),
      MockWrite(SYNCHRONOUS, kMsg2, kLen2, 1),
      MockWrite(SYNCHRONOUS, kMsg3, kLen3, 2),
      MockWrite(SYNCHRONOUS, kMsg3, kLen3, 3),
      MockWrite(SYNCHRONOUS, kMsg2, kLen2, 4),
      MockWrite(SYNCHRONOUS, kMsg3, kLen3, 5),
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 6),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertSyncWriteEquals(kMsg1, kLen1);
  AssertSyncWriteEquals(kMsg2, kLen2);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertSyncWriteEquals(kMsg2, kLen2);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertSyncWriteEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, SingleAsyncWrite) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg1, kLen1, 0),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertAsyncWriteEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, MultipleAsyncWrites) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg1, kLen1, 0),
      MockWrite(ASYNC, kMsg2, kLen2, 1),
      MockWrite(ASYNC, kMsg3, kLen3, 2),
      MockWrite(ASYNC, kMsg3, kLen3, 3),
      MockWrite(ASYNC, kMsg2, kLen2, 4),
      MockWrite(ASYNC, kMsg3, kLen3, 5),
      MockWrite(ASYNC, kMsg1, kLen1, 6),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertAsyncWriteEquals(kMsg1, kLen1);
  AssertAsyncWriteEquals(kMsg2, kLen2);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertAsyncWriteEquals(kMsg2, kLen2);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertAsyncWriteEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, MixedWrites) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 0),
      MockWrite(ASYNC, kMsg2, kLen2, 1),
      MockWrite(SYNCHRONOUS, kMsg3, kLen3, 2),
      MockWrite(ASYNC, kMsg3, kLen3, 3),
      MockWrite(SYNCHRONOUS, kMsg2, kLen2, 4),
      MockWrite(ASYNC, kMsg3, kLen3, 5),
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 6),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertSyncWriteEquals(kMsg1, kLen1);
  AssertAsyncWriteEquals(kMsg2, kLen2);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertSyncWriteEquals(kMsg2, kLen2);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertSyncWriteEquals(kMsg1, kLen1);
}

TEST_F(SequencedSocketDataTest, SyncWriteFromCompletionCallback) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg1, kLen1, 0),
      MockWrite(SYNCHRONOUS, kMsg2, kLen2, 1),
  };

  Initialize(base::span<MockRead>(), writes);

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  memcpy(write_buf->data(), kMsg1, kLen1);
  ASSERT_EQ(
      ERR_IO_PENDING,
      sock_->Write(
          write_buf.get(), kLen1,
          base::BindOnce(&SequencedSocketDataTest::ReentrantWriteCallback,
                         base::Unretained(this), kLen1, kMsg2, kLen2, kLen2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();
}

TEST_F(SequencedSocketDataTest, AsyncWriteFromCompletionCallback) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg1, kLen1, 0), MockWrite(ASYNC, kMsg2, kLen2, 1),
  };

  Initialize(base::span<MockRead>(), writes);

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  memcpy(write_buf->data(), kMsg1, kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Write(
                write_buf.get(), kLen1,
                base::BindOnce(&SequencedSocketDataTest::ReentrantWriteCallback,
                               base::Unretained(this), kLen1, kMsg2, kLen2,
                               ERR_IO_PENDING),
                TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());
}

TEST_F(SequencedSocketDataTest, ManyReentrantWrites) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg1, kLen1, 0),
      MockWrite(ASYNC, kMsg2, kLen2, 1),
      MockWrite(ASYNC, kMsg3, kLen3, 2),
      MockWrite(ASYNC, kMsg4, kLen4, 3),
  };

  Initialize(base::span<MockRead>(), writes);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedWrite(kLen3);
  helper3.SetInvokeWrite(kMsg4, kLen4, ERR_IO_PENDING,
                         write_callback_.callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedWrite(kLen2);
  helper2.SetInvokeWrite(kMsg3, kLen3, ERR_IO_PENDING, helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedWrite(kLen1);
  helper.SetInvokeWrite(kMsg2, kLen2, ERR_IO_PENDING, helper2.callback());

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  memcpy(write_buf->data(), kMsg1, kLen1);
  sock_->Write(write_buf.get(), kLen1, helper.callback(),
               TRAFFIC_ANNOTATION_FOR_TESTS);

  ASSERT_EQ(kLen4, write_callback_.WaitForResult());
}

// ----------- Mixed Reads and Writes

TEST_F(SequencedSocketDataTest, MixedSyncOperations) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 0),
      MockRead(SYNCHRONOUS, kMsg2, kLen2, 3),
  };

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kMsg2, kLen2, 1),
      MockWrite(SYNCHRONOUS, kMsg3, kLen3, 2),
  };

  Initialize(reads, writes);

  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncWriteEquals(kMsg2, kLen2);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(SequencedSocketDataTest, MixedAsyncOperations) {
  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0), MockRead(ASYNC, kMsg2, kLen2, 3),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg2, kLen2, 1), MockWrite(ASYNC, kMsg3, kLen3, 2),
  };

  Initialize(reads, writes);

  AssertAsyncReadEquals(kMsg1, kLen1);
  AssertAsyncWriteEquals(kMsg2, kLen2);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg2, kLen2);
}

TEST_F(SequencedSocketDataTest, InterleavedAsyncOperations) {
  // Order of completion is read, write, write, read.
  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0), MockRead(ASYNC, kMsg2, kLen2, 3),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg2, kLen2, 1), MockWrite(ASYNC, kMsg3, kLen3, 2),
  };

  Initialize(reads, writes);

  // Issue the write, which will block until the read completes.
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  // Issue the read which will return first.
  AssertReadReturns(kLen1, ERR_IO_PENDING);

  ASSERT_EQ(kLen1, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg1, kLen1);

  // Run posted OnWriteComplete().
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());

  // Issue the read, which will block until the write completes.
  AssertReadReturns(kLen2, ERR_IO_PENDING);

  // Issue the writes which will return first.
  AssertWriteReturns(kMsg3, kLen3, ERR_IO_PENDING);
  ASSERT_EQ(kLen3, write_callback_.WaitForResult());

  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2, kLen2);
}

TEST_F(SequencedSocketDataTest, InterleavedMixedOperations) {
  // Order of completion is read, write, write, read.
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 0),
      MockRead(ASYNC, kMsg2, kLen2, 3),
      MockRead(ASYNC, kMsg3, kLen3, 5),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg2, kLen2, 1),
      MockWrite(SYNCHRONOUS, kMsg3, kLen3, 2),
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 4),
  };

  Initialize(reads, writes);

  // Issue the write, which will block until the read completes.
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  // Issue the writes which will complete immediately.
  AssertSyncReadEquals(kMsg1, kLen1);

  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());

  // Issue the read, which will block until the write completes.
  AssertReadReturns(kLen2, ERR_IO_PENDING);

  // Issue the writes which will complete immediately.
  AssertSyncWriteEquals(kMsg3, kLen3);

  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2, kLen2);

  // Issue the read, which will block until the write completes.
  AssertReadReturns(kLen2, ERR_IO_PENDING);

  // Issue the writes which will complete immediately.
  AssertSyncWriteEquals(kMsg1, kLen1);

  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_EQ(kLen3, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg3, kLen3);
}

TEST_F(SequencedSocketDataTest, AsyncReadFromWriteCompletionCallback) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg1, kLen1, 0),
  };

  MockRead reads[] = {
      MockRead(ASYNC, kMsg2, kLen2, 1),
  };

  Initialize(reads, writes);

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  memcpy(write_buf->data(), kMsg1, kLen1);
  ASSERT_EQ(
      ERR_IO_PENDING,
      sock_->Write(
          write_buf.get(), kLen1,
          base::BindOnce(&SequencedSocketDataTest::ReentrantAsyncReadCallback,
                         base::Unretained(this), kLen1, kLen2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2, kLen2);
}

TEST_F(SequencedSocketDataTest, AsyncWriteFromReadCompletionCallback) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg2, kLen2, 1),
  };

  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0),
  };

  Initialize(reads, writes);

  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(
      ERR_IO_PENDING,
      sock_->Read(
          read_buf.get(), kLen1,
          base::BindOnce(&SequencedSocketDataTest::ReentrantAsyncWriteCallback,
                         base::Unretained(this), kMsg2, kLen2,
                         write_callback_.callback(), kLen1)));

  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());
}

TEST_F(SequencedSocketDataTest, MixedReentrantOperations) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg1, kLen1, 0), MockWrite(ASYNC, kMsg3, kLen3, 2),
  };

  MockRead reads[] = {
      MockRead(ASYNC, kMsg2, kLen2, 1), MockRead(ASYNC, kMsg4, kLen4, 3),
  };

  Initialize(reads, writes);

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen4);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedWrite(kLen3);
  helper3.SetInvokeRead(read_buf_, kLen4, ERR_IO_PENDING,
                        read_callback_.callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedRead(kMsg2, kLen2);
  helper2.SetInvokeWrite(kMsg3, kLen3, ERR_IO_PENDING, helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedWrite(kLen1);
  helper.SetInvokeRead(helper2.read_buf(), kLen2, ERR_IO_PENDING,
                       helper2.callback());

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  memcpy(write_buf->data(), kMsg1, kLen1);
  sock_->Write(write_buf.get(), kLen1, helper.callback(),
               TRAFFIC_ANNOTATION_FOR_TESTS);

  ASSERT_EQ(kLen4, read_callback_.WaitForResult());
}

TEST_F(SequencedSocketDataTest, MixedReentrantOperationsThenSynchronousRead) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg1, kLen1, 0), MockWrite(ASYNC, kMsg3, kLen3, 2),
  };

  MockRead reads[] = {
      MockRead(ASYNC, kMsg2, kLen2, 1), MockRead(SYNCHRONOUS, kMsg4, kLen4, 3),
  };

  Initialize(reads, writes);

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen4);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedWrite(kLen3);
  helper3.SetInvokeRead(read_buf_, kLen4, kLen4, failing_callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedRead(kMsg2, kLen2);
  helper2.SetInvokeWrite(kMsg3, kLen3, ERR_IO_PENDING, helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedWrite(kLen1);
  helper.SetInvokeRead(helper2.read_buf(), kLen2, ERR_IO_PENDING,
                       helper2.callback());

  auto write_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  memcpy(write_buf->data(), kMsg1, kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Write(write_buf.get(), kLen1, helper.callback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();
  AssertReadBufferEquals(kMsg4, kLen4);
}

TEST_F(SequencedSocketDataTest, MixedReentrantOperationsThenSynchronousWrite) {
  MockWrite writes[] = {
      MockWrite(ASYNC, kMsg2, kLen2, 1),
      MockWrite(SYNCHRONOUS, kMsg4, kLen4, 3),
  };

  MockRead reads[] = {
      MockRead(ASYNC, kMsg1, kLen1, 0), MockRead(ASYNC, kMsg3, kLen3, 2),
  };

  Initialize(reads, writes);

  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(kLen4);

  ReentrantHelper helper3(sock_.get());
  helper3.SetExpectedRead(kMsg3, kLen3);
  helper3.SetInvokeWrite(kMsg4, kLen4, kLen4, failing_callback());

  ReentrantHelper helper2(sock_.get());
  helper2.SetExpectedWrite(kLen2);
  helper2.SetInvokeRead(helper3.read_buf(), kLen3, ERR_IO_PENDING,
                        helper3.callback());

  ReentrantHelper helper(sock_.get());
  helper.SetExpectedRead(kMsg1, kLen1);
  helper.SetInvokeWrite(kMsg2, kLen2, ERR_IO_PENDING, helper2.callback());

  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(helper.read_buf().get(), kLen1, helper.callback()));

  base::RunLoop().RunUntilIdle();
}

// Test the basic case where a read is paused.
TEST_F(SequencedSocketDataTest, PauseAndResume_PauseRead) {
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 0), MockRead(ASYNC, kMsg1, kLen1, 1),
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
  AssertReadBufferEquals(kMsg1, kLen1);
}

// Test the case where a read that will be paused is started before write that
// completes before the pause.
TEST_F(SequencedSocketDataTest, PauseAndResume_WritePauseRead) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kMsg1, kLen1, 0),
  };

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1), MockRead(ASYNC, kMsg2, kLen2, 2),
  };

  Initialize(reads, writes);

  AssertReadReturns(kLen2, ERR_IO_PENDING);
  ASSERT_FALSE(read_callback_.have_result());

  // Nothing should happen until the write starts.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(read_callback_.have_result());
  ASSERT_FALSE(IsPaused());

  AssertSyncWriteEquals(kMsg1, kLen1);

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
  AssertReadBufferEquals(kMsg2, kLen2);
}

// Test the basic case where a write is paused.
TEST_F(SequencedSocketDataTest, PauseAndResume_PauseWrite) {
  MockWrite writes[] = {
      MockWrite(ASYNC, ERR_IO_PENDING, 0), MockWrite(ASYNC, kMsg1, kLen1, 1),
  };

  Initialize(base::span<MockRead>(), writes);

  AssertWriteReturns(kMsg1, kLen1, ERR_IO_PENDING);
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
      MockWrite(ASYNC, ERR_IO_PENDING, 1), MockWrite(ASYNC, kMsg2, kLen2, 2),
  };

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMsg1, kLen1, 0),
  };

  Initialize(reads, writes);

  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);
  ASSERT_FALSE(write_callback_.have_result());

  // Nothing should happen until the write starts.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(write_callback_.have_result());
  ASSERT_FALSE(IsPaused());

  AssertSyncReadEquals(kMsg1, kLen1);

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

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_bio_adapter.h"

#include <string.h>

#include <memory>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "crypto/openssl_util.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/openssl_ssl_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

enum ReadIfReadySupport {
  // ReadyIfReady() is implemented.
  READ_IF_READY_SUPPORTED,
  // ReadyIfReady() is unimplemented.
  READ_IF_READY_NOT_SUPPORTED,
};

class SocketBIOAdapterTest : public testing::TestWithParam<ReadIfReadySupport>,
                             public SocketBIOAdapter::Delegate,
                             public WithTaskEnvironment {
 protected:
  void SetUp() override {
    if (GetParam() == READ_IF_READY_SUPPORTED) {
      factory_.set_enable_read_if_ready(true);
    }
  }

  std::unique_ptr<StreamSocket> MakeTestSocket(SocketDataProvider* data) {
    data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    factory_.AddSocketDataProvider(data);
    std::unique_ptr<StreamSocket> socket = factory_.CreateTransportClientSocket(
        AddressList(), nullptr, nullptr, nullptr, NetLogSource());
    CHECK_EQ(OK, socket->Connect(CompletionOnceCallback()));
    return socket;
  }

  void set_reset_on_write_ready(
      std::unique_ptr<SocketBIOAdapter>* reset_on_write_ready) {
    reset_on_write_ready_ = reset_on_write_ready;
  }

  void ExpectReadError(BIO* bio,
                       int error,
                       const crypto::OpenSSLErrStackTracer& tracer) {
    // BIO_read should fail.
    char buf;
    EXPECT_EQ(-1, BIO_read(bio, &buf, 1));
    EXPECT_EQ(error, MapOpenSSLError(SSL_ERROR_SSL, tracer));
    EXPECT_FALSE(BIO_should_read(bio));

    // Repeating the operation should replay the error.
    EXPECT_EQ(-1, BIO_read(bio, &buf, 1));
    EXPECT_EQ(error, MapOpenSSLError(SSL_ERROR_SSL, tracer));
    EXPECT_FALSE(BIO_should_read(bio));
  }

  void ExpectBlockingRead(BIO* bio, void* buf, int len) {
    // BIO_read should return a retryable error.
    EXPECT_EQ(-1, BIO_read(bio, buf, len));
    EXPECT_TRUE(BIO_should_read(bio));
    EXPECT_EQ(0u, ERR_peek_error());

    // Repeating the operation has the same result.
    EXPECT_EQ(-1, BIO_read(bio, buf, len));
    EXPECT_TRUE(BIO_should_read(bio));
    EXPECT_EQ(0u, ERR_peek_error());
  }

  void ExpectWriteError(BIO* bio,
                        int error,
                        const crypto::OpenSSLErrStackTracer& tracer) {
    // BIO_write should fail.
    char buf = '?';
    EXPECT_EQ(-1, BIO_write(bio, &buf, 1));
    EXPECT_EQ(error, MapOpenSSLError(SSL_ERROR_SSL, tracer));
    EXPECT_FALSE(BIO_should_write(bio));

    // Repeating the operation should replay the error.
    EXPECT_EQ(-1, BIO_write(bio, &buf, 1));
    EXPECT_EQ(error, MapOpenSSLError(SSL_ERROR_SSL, tracer));
    EXPECT_FALSE(BIO_should_write(bio));
  }

  void ExpectBlockingWrite(BIO* bio, const void* buf, int len) {
    // BIO_write should return a retryable error.
    EXPECT_EQ(-1, BIO_write(bio, buf, len));
    EXPECT_TRUE(BIO_should_write(bio));
    EXPECT_EQ(0u, ERR_peek_error());

    // Repeating the operation has the same result.
    EXPECT_EQ(-1, BIO_write(bio, buf, len));
    EXPECT_TRUE(BIO_should_write(bio));
    EXPECT_EQ(0u, ERR_peek_error());
  }

  void WaitForReadReady() {
    expect_read_ready_ = true;
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(expect_read_ready_);
  }

  void WaitForWriteReady(SequencedSocketData* to_resume) {
    expect_write_ready_ = true;
    if (to_resume) {
      to_resume->Resume();
    }
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(expect_write_ready_);
  }

  void WaitForBothReady() {
    expect_read_ready_ = true;
    expect_write_ready_ = true;
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(expect_read_ready_);
    EXPECT_FALSE(expect_write_ready_);
  }

  // SocketBIOAdapter::Delegate implementation:
  void OnReadReady() override {
    EXPECT_TRUE(expect_read_ready_);
    expect_read_ready_ = false;
  }

  void OnWriteReady() override {
    EXPECT_TRUE(expect_write_ready_);
    expect_write_ready_ = false;
    if (reset_on_write_ready_)
      reset_on_write_ready_->reset();
  }

 private:
  bool expect_read_ready_ = false;
  bool expect_write_ready_ = false;
  MockClientSocketFactory factory_;
  raw_ptr<std::unique_ptr<SocketBIOAdapter>> reset_on_write_ready_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SocketBIOAdapterTest,
                         testing::Values(READ_IF_READY_SUPPORTED,
                                         READ_IF_READY_NOT_SUPPORTED));

// Test that data can be read synchronously.
TEST_P(SocketBIOAdapterTest, ReadSync) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, "hello"), MockRead(SYNCHRONOUS, 1, "world"),
      MockRead(SYNCHRONOUS, ERR_CONNECTION_RESET, 2),
  };

  SequencedSocketData data(reads, base::span<MockWrite>());
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 100, 100, this);
  BIO* bio = adapter->bio();
  EXPECT_FALSE(adapter->HasPendingReadData());

  // Read the data synchronously. Although the buffer has room for both,
  // BIO_read only reports one socket-level Read.
  char buf[10];
  EXPECT_EQ(5, BIO_read(bio, buf, sizeof(buf)));
  EXPECT_EQ(0, memcmp("hello", buf, 5));
  EXPECT_FALSE(adapter->HasPendingReadData());

  // Consume the next portion one byte at a time.
  EXPECT_EQ(1, BIO_read(bio, buf, 1));
  EXPECT_EQ('w', buf[0]);
  EXPECT_TRUE(adapter->HasPendingReadData());

  EXPECT_EQ(1, BIO_read(bio, buf, 1));
  EXPECT_EQ('o', buf[0]);
  EXPECT_TRUE(adapter->HasPendingReadData());

  // The remainder may be consumed in a single BIO_read.
  EXPECT_EQ(3, BIO_read(bio, buf, sizeof(buf)));
  EXPECT_EQ(0, memcmp("rld", buf, 3));
  EXPECT_FALSE(adapter->HasPendingReadData());

  // The error is available synchoronously.
  ExpectReadError(bio, ERR_CONNECTION_RESET, tracer);
}

// Test that data can be read asynchronously.
TEST_P(SocketBIOAdapterTest, ReadAsync) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockRead reads[] = {
      MockRead(ASYNC, 0, "hello"), MockRead(ASYNC, 1, "world"),
      MockRead(ASYNC, ERR_CONNECTION_RESET, 2),
  };

  SequencedSocketData data(reads, base::span<MockWrite>());
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 100, 100, this);
  BIO* bio = adapter->bio();
  EXPECT_FALSE(adapter->HasPendingReadData());

  // Attempt to read data. It will fail but schedule a Read.
  char buf[10];
  ExpectBlockingRead(bio, buf, sizeof(buf));
  EXPECT_FALSE(adapter->HasPendingReadData());

  // After waiting, the data is available if Read() is used.
  WaitForReadReady();
  if (GetParam() == READ_IF_READY_SUPPORTED) {
    EXPECT_FALSE(adapter->HasPendingReadData());
  } else {
    EXPECT_TRUE(adapter->HasPendingReadData());
  }

  // The first read is now available synchronously.
  EXPECT_EQ(5, BIO_read(bio, buf, sizeof(buf)));
  EXPECT_EQ(0, memcmp("hello", buf, 5));
  EXPECT_FALSE(adapter->HasPendingReadData());

  // The adapter does not schedule another Read until BIO_read is next called.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(adapter->HasPendingReadData());

  // This time, under-request the data. The adapter should still read the full
  // amount.
  ExpectBlockingRead(bio, buf, 1);
  EXPECT_FALSE(adapter->HasPendingReadData());

  // After waiting, the data is available if Read() is used.
  WaitForReadReady();
  if (GetParam() == READ_IF_READY_SUPPORTED) {
    EXPECT_FALSE(adapter->HasPendingReadData());
  } else {
    EXPECT_TRUE(adapter->HasPendingReadData());
  }

  // The next read is now available synchronously.
  EXPECT_EQ(5, BIO_read(bio, buf, sizeof(buf)));
  EXPECT_EQ(0, memcmp("world", buf, 5));
  EXPECT_FALSE(adapter->HasPendingReadData());

  // The error is not yet available.
  ExpectBlockingRead(bio, buf, sizeof(buf));
  WaitForReadReady();

  // The error is now available synchoronously.
  ExpectReadError(bio, ERR_CONNECTION_RESET, tracer);
}

// Test that synchronous EOF is mapped to ERR_CONNECTION_CLOSED.
TEST_P(SocketBIOAdapterTest, ReadEOFSync) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 0, 0),
  };

  SequencedSocketData data(reads, base::span<MockWrite>());
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 100, 100, this);

  ExpectReadError(adapter->bio(), ERR_CONNECTION_CLOSED, tracer);
}

#if BUILDFLAG(IS_ANDROID)
// Test that asynchronous EOF is mapped to ERR_CONNECTION_CLOSED.
// TODO(crbug.com/40281159): Test is flaky on Android.
#define MAYBE_ReadEOFAsync DISABLED_ReadEOFAsync
#else
#define MAYBE_ReadEOFAsync ReadEOFAsync
#endif
TEST_P(SocketBIOAdapterTest, MAYBE_ReadEOFAsync) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockRead reads[] = {
      MockRead(ASYNC, 0, 0),
  };

  SequencedSocketData data(reads, base::span<MockWrite>());
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 100, 100, this);

  char buf;
  ExpectBlockingRead(adapter->bio(), &buf, 1);
  WaitForReadReady();
  ExpectReadError(adapter->bio(), ERR_CONNECTION_CLOSED, tracer);
}

// Test that data can be written synchronously.
TEST_P(SocketBIOAdapterTest, WriteSync) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, "hello"),
      MockWrite(SYNCHRONOUS, 1, "wor"),
      MockWrite(SYNCHRONOUS, 2, "ld"),
      MockWrite(SYNCHRONOUS, 3, "helloworld"),
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET, 4),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 10, 10, this);
  BIO* bio = adapter->bio();

  // Test data entering and leaving the buffer synchronously. The second write
  // takes multiple iterations (events 0 to 2).
  EXPECT_EQ(5, BIO_write(bio, "hello", 5));
  EXPECT_EQ(5, BIO_write(bio, "world", 5));

  // If writing larger than the buffer size, only part of the data is written
  // (event 3).
  EXPECT_EQ(10, BIO_write(bio, "helloworldhelloworld", 20));

  // Writing "aaaaa" fails (event 4), but there is a write buffer, so errors
  // are delayed.
  EXPECT_EQ(5, BIO_write(bio, "aaaaa", 5));

  // However once the error is registered, subsequent writes fail.
  ExpectWriteError(bio, ERR_CONNECTION_RESET, tracer);
}

// Test that data can be written asynchronously.
TEST_P(SocketBIOAdapterTest, WriteAsync) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockWrite writes[] = {
      MockWrite(ASYNC, 0, "aaa"),
      MockWrite(ASYNC, ERR_IO_PENDING, 1),  // pause
      MockWrite(ASYNC, 2, "aabbbbb"),
      MockWrite(ASYNC, 3, "ccc"),
      MockWrite(ASYNC, 4, "ddd"),
      MockWrite(ASYNC, ERR_IO_PENDING, 5),  // pause
      MockWrite(ASYNC, 6, "dd"),
      MockWrite(SYNCHRONOUS, 7, "e"),
      MockWrite(SYNCHRONOUS, 8, "e"),
      MockWrite(ASYNC, 9, "e"),
      MockWrite(ASYNC, 10, "ee"),
      MockWrite(ASYNC, ERR_IO_PENDING, 11),  // pause
      MockWrite(ASYNC, 12, "eff"),
      MockWrite(ASYNC, 13, "ggggggg"),
      MockWrite(ASYNC, ERR_CONNECTION_RESET, 14),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 10, 10, this);
  BIO* bio = adapter->bio();

  // Data which fits in the buffer is returned synchronously, even if not
  // flushed synchronously.
  EXPECT_EQ(5, BIO_write(bio, "aaaaa", 5));
  EXPECT_EQ(5, BIO_write(bio, "bbbbb", 5));

  // The buffer contains:
  //
  //   [aaaaabbbbb]
  //    ^

  // The buffer is full now, so the next write will block.
  ExpectBlockingWrite(bio, "zzzzz", 5);

  // Let the first socket write complete (event 0) and pause (event 1).
  WaitForWriteReady(nullptr);
  EXPECT_TRUE(data.IsPaused());

  // The buffer contains:
  //
  //   [...aabbbbb]
  //       ^

  // The ring buffer now has 3 bytes of space with "aabbbbb" still to be
  // written. Attempting to write 3 bytes means 3 succeed.
  EXPECT_EQ(3, BIO_write(bio, "cccccccccc", 10));

  // The buffer contains:
  //
  //   [cccaabbbbb]
  //       ^

  // Drain the buffer (events 2 and 3).
  WaitForWriteReady(&data);

  // The buffer is now empty.

  // Now test something similar but arrange for a BIO_write (the 'e's below) to
  // wrap around the buffer.  Write five bytes into the buffer, flush the first
  // three (event 4), and pause (event 5). OnWriteReady is not signaled because
  // the buffer was not full.
  EXPECT_EQ(5, BIO_write(bio, "ddddd", 5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data.IsPaused());

  // The buffer contains:
  //
  //   [...dd.....]
  //       ^

  // The adapter maintains a ring buffer, so 6 bytes fit.
  EXPECT_EQ(6, BIO_write(bio, "eeeeee", 6));

  // The buffer contains:
  //
  //   [e..ddeeeee]
  //       ^

  // The remaining space may be filled in.
  EXPECT_EQ(2, BIO_write(bio, "ffffffffff", 10));

  // The buffer contains:
  //
  //   [effddeeeee]
  //       ^

  // Drain to the end of the ring buffer, so it wraps around (events 6 to 10)
  // and pause (event 11). Test that synchronous and asynchronous writes both
  // drain. The start of the buffer has now wrapped around.
  WaitForWriteReady(&data);
  EXPECT_TRUE(data.IsPaused());

  // The buffer contains:
  //
  //   [eff.......]
  //    ^

  // Test wrapping around works correctly and the buffer may be appended to.
  EXPECT_EQ(7, BIO_write(bio, "gggggggggg", 10));

  // The buffer contains:
  //
  //   [effggggggg]
  //    ^

  // The buffer is full now, so the next write will block.
  ExpectBlockingWrite(bio, "zzzzz", 5);

  // Drain the buffer to confirm the ring buffer's contents are as expected
  // (events 12 and 13).
  WaitForWriteReady(&data);

  // Write again so the write error may be discovered.
  EXPECT_EQ(5, BIO_write(bio, "hhhhh", 5));

  // Release the write error (event 14). At this point future BIO_write calls
  // fail. The buffer was not full, so OnWriteReady is not signalled.
  base::RunLoop().RunUntilIdle();
  ExpectWriteError(bio, ERR_CONNECTION_RESET, tracer);
}

// Test that a failed socket write is reported through BIO_read and prevents it
// from scheduling a socket read. See https://crbug.com/249848.
TEST_P(SocketBIOAdapterTest, WriteStopsRead) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET, 0),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 100, 100, this);
  BIO* bio = adapter->bio();

  // The write fails, but there is a write buffer, so errors are delayed.
  EXPECT_EQ(5, BIO_write(bio, "aaaaa", 5));

  // The write error is surfaced out of BIO_read. There are no MockReads, so
  // this also tests that no socket reads are attempted.
  ExpectReadError(bio, ERR_CONNECTION_RESET, tracer);
}

// Test that a synchronous failed socket write interrupts a blocked
// BIO_read. See https://crbug.com/249848.
TEST_P(SocketBIOAdapterTest, SyncWriteInterruptsRead) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET, 1),
  };

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 100, 100, this);
  BIO* bio = adapter->bio();

  // Attempt to read from the transport. It will block indefinitely.
  char buf;
  ExpectBlockingRead(adapter->bio(), &buf, 1);

  // Schedule a socket write.
  EXPECT_EQ(5, BIO_write(bio, "aaaaa", 5));

  // The write error triggers OnReadReady.
  WaitForReadReady();

  // The write error is surfaced out of BIO_read.
  ExpectReadError(bio, ERR_CONNECTION_RESET, tracer);
}

// Test that an asynchronous failed socket write interrupts a blocked
// BIO_read. See https://crbug.com/249848.
TEST_P(SocketBIOAdapterTest, AsyncWriteInterruptsRead) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, ERR_CONNECTION_RESET, 1),
  };

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 100, 100, this);
  BIO* bio = adapter->bio();

  // Attempt to read from the transport. It will block indefinitely.
  char buf;
  ExpectBlockingRead(adapter->bio(), &buf, 1);

  // Schedule a socket write.
  EXPECT_EQ(5, BIO_write(bio, "aaaaa", 5));

  // The write error is signaled asynchronously and interrupts BIO_read, so
  // OnReadReady is signaled. The write buffer was not full, so OnWriteReady is
  // not signaled.
  WaitForReadReady();

  // The write error is surfaced out of BIO_read.
  ExpectReadError(bio, ERR_CONNECTION_RESET, tracer);
}

// Test that an asynchronous failed socket write interrupts a blocked BIO_read,
// signaling both if the buffer was full. See https://crbug.com/249848.
TEST_P(SocketBIOAdapterTest, AsyncWriteInterruptsBoth) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, ERR_CONNECTION_RESET, 1),
  };

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 5, 5, this);
  BIO* bio = adapter->bio();

  // Attempt to read from the transport. It will block indefinitely.
  char buf;
  ExpectBlockingRead(adapter->bio(), &buf, 1);

  // Schedule a socket write.
  EXPECT_EQ(5, BIO_write(bio, "aaaaa", 5));

  // The write error is signaled asynchronously and interrupts BIO_read, so
  // OnReadReady is signaled. The write buffer was full, so both OnWriteReady is
  // also signaled.
  WaitForBothReady();

  // The write error is surfaced out of BIO_read.
  ExpectReadError(bio, ERR_CONNECTION_RESET, tracer);
}

// Test that SocketBIOAdapter handles OnWriteReady deleting itself when both
// need to be signaled.
TEST_P(SocketBIOAdapterTest, DeleteOnWriteReady) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };

  MockWrite writes[] = {
      MockWrite(ASYNC, ERR_CONNECTION_RESET, 1),
  };

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 5, 5, this);
  BIO* bio = adapter->bio();

  // Arrange for OnReadReady and OnWriteReady to both be signaled due to write
  // error propagation (see the AsyncWriteInterruptsBoth test).
  char buf;
  ExpectBlockingRead(adapter->bio(), &buf, 1);
  EXPECT_EQ(5, BIO_write(bio, "aaaaa", 5));

  // Both OnWriteReady and OnReadReady would be signaled, but OnWriteReady
  // deletes the adapter first.
  set_reset_on_write_ready(&adapter);
  WaitForWriteReady(nullptr);

  EXPECT_FALSE(adapter);
}

// Test that using a BIO after the underlying adapter is destroyed fails
// gracefully.
TEST_P(SocketBIOAdapterTest, Detached) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  SequencedSocketData data;
  std::unique_ptr<StreamSocket> socket = MakeTestSocket(&data);
  std::unique_ptr<SocketBIOAdapter> adapter =
      std::make_unique<SocketBIOAdapter>(socket.get(), 100, 100, this);

  // Retain an additional reference to the BIO.
  bssl::UniquePtr<BIO> bio = bssl::UpRef(adapter->bio());

  // Release the adapter.
  adapter.reset();

  ExpectReadError(bio.get(), ERR_UNEXPECTED, tracer);
  ExpectWriteError(bio.get(), ERR_UNEXPECTED, tracer);
}

}  // namespace net

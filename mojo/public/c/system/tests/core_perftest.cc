// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tests the performance of the C API.

#include "mojo/public/c/system/core.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "mojo/public/cpp/test_support/test_support.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(WIN32)
#include <time.h>
#endif  // !defined(WIN32)

namespace {

#if !defined(WIN32)
class MessagePipeWriterThread : public base::SimpleThread {
 public:
  MessagePipeWriterThread(MojoHandle handle, uint32_t num_bytes)
      : SimpleThread("MessagePipeWriterThread"),
        handle_(handle),
        num_bytes_(num_bytes),
        num_writes_(0) {}

  MessagePipeWriterThread(const MessagePipeWriterThread&) = delete;
  MessagePipeWriterThread& operator=(const MessagePipeWriterThread&) = delete;

  ~MessagePipeWriterThread() override {}

  void Run() override {
    char buffer[10000];
    assert(num_bytes_ <= sizeof(buffer));

    for (;;) {
      MojoResult result = mojo::WriteMessageRaw(
          mojo::MessagePipeHandle(handle_), buffer, num_bytes_, nullptr, 0,
          MOJO_WRITE_MESSAGE_FLAG_NONE);
      if (result == MOJO_RESULT_OK) {
        num_writes_++;
        continue;
      }

      // We failed to write.
      // Either |handle_| or its peer was closed.
      assert(result == MOJO_RESULT_INVALID_ARGUMENT ||
             result == MOJO_RESULT_FAILED_PRECONDITION);
      break;
    }
  }

  // Use only after joining the thread.
  int64_t num_writes() const { return num_writes_; }

 private:
  const MojoHandle handle_;
  const uint32_t num_bytes_;
  int64_t num_writes_;
};

class MessagePipeReaderThread : public base::SimpleThread {
 public:
  explicit MessagePipeReaderThread(MojoHandle handle)
      : SimpleThread("MessagePipeReaderThread"),
        handle_(handle),
        num_reads_(0) {}

  MessagePipeReaderThread(const MessagePipeReaderThread&) = delete;
  MessagePipeReaderThread& operator=(const MessagePipeReaderThread&) = delete;

  ~MessagePipeReaderThread() override {}

  void Run() override {
    for (;;) {
      std::vector<uint8_t> bytes;
      MojoResult result =
          mojo::ReadMessageRaw(mojo::MessagePipeHandle(handle_), &bytes,
                               nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
      if (result == MOJO_RESULT_OK) {
        num_reads_++;
        continue;
      }

      if (result == MOJO_RESULT_SHOULD_WAIT) {
        result = mojo::Wait(mojo::Handle(handle_), MOJO_HANDLE_SIGNAL_READABLE);
        if (result == MOJO_RESULT_OK) {
          // Go to the top of the loop to read again.
          continue;
        }
      }

      // We failed to read and possibly failed to wait.
      // Either |handle_| or its peer was closed.
      assert(result == MOJO_RESULT_INVALID_ARGUMENT ||
             result == MOJO_RESULT_FAILED_PRECONDITION);
      break;
    }
  }

  // Use only after joining the thread.
  int64_t num_reads() const { return num_reads_; }

 private:
  const MojoHandle handle_;
  int64_t num_reads_;
};
#endif  // !defined(WIN32)

class CorePerftest : public testing::Test {
 public:
  CorePerftest() {}

  CorePerftest(const CorePerftest&) = delete;
  CorePerftest& operator=(const CorePerftest&) = delete;

  ~CorePerftest() override {}

  static void NoOp(void* /*closure*/) {}

  static void MessagePipe_CreateAndClose(void* closure) {
    CorePerftest* self = static_cast<CorePerftest*>(closure);
    [[maybe_unused]] MojoResult result =
        MojoCreateMessagePipe(nullptr, &self->h0_, &self->h1_);
    assert(result == MOJO_RESULT_OK);
    result = MojoClose(self->h0_);
    assert(result == MOJO_RESULT_OK);
    result = MojoClose(self->h1_);
    assert(result == MOJO_RESULT_OK);
  }

  static void MessagePipe_WriteAndRead(void* closure) {
    CorePerftest* self = static_cast<CorePerftest*>(closure);
    [[maybe_unused]] MojoResult result = mojo::WriteMessageRaw(
        mojo::MessagePipeHandle(self->h0_), self->buffer_.data(),
        self->buffer_.size(), nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
    assert(result == MOJO_RESULT_OK);
    result =
        mojo::ReadMessageRaw(mojo::MessagePipeHandle(self->h1_), &self->buffer_,
                             nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
    assert(result == MOJO_RESULT_OK);
  }

  static void MessagePipe_EmptyRead(void* closure) {
    CorePerftest* self = static_cast<CorePerftest*>(closure);
    MojoMessageHandle message;
    [[maybe_unused]] MojoResult result =
        MojoReadMessage(self->h0_, nullptr, &message);
    assert(result == MOJO_RESULT_SHOULD_WAIT);
  }

 protected:
#if !defined(WIN32)
  void DoMessagePipeThreadedTest(unsigned num_writers,
                                 unsigned num_readers,
                                 uint32_t num_bytes) {
    static const int64_t kPerftestTimeMicroseconds = 3 * 1000000;

    assert(num_writers > 0);
    assert(num_readers > 0);

    [[maybe_unused]] MojoResult result =
        MojoCreateMessagePipe(nullptr, &h0_, &h1_);
    assert(result == MOJO_RESULT_OK);

    std::vector<MessagePipeWriterThread*> writers;
    for (unsigned i = 0; i < num_writers; i++)
      writers.push_back(new MessagePipeWriterThread(h0_, num_bytes));

    std::vector<MessagePipeReaderThread*> readers;
    for (unsigned i = 0; i < num_readers; i++)
      readers.push_back(new MessagePipeReaderThread(h1_));

    // Start time here, just before we fire off the threads.
    const MojoTimeTicks start_time = MojoGetTimeTicksNow();

    // Interleave the starts.
    for (unsigned i = 0; i < num_writers || i < num_readers; i++) {
      if (i < num_writers)
        writers[i]->Start();
      if (i < num_readers)
        readers[i]->Start();
    }

    Sleep(kPerftestTimeMicroseconds);

    // Close both handles to make writers and readers stop immediately.
    result = MojoClose(h0_);
    assert(result == MOJO_RESULT_OK);
    result = MojoClose(h1_);
    assert(result == MOJO_RESULT_OK);

    // Join everything.
    for (unsigned i = 0; i < num_writers; i++)
      writers[i]->Join();
    for (unsigned i = 0; i < num_readers; i++)
      readers[i]->Join();

    // Stop time here.
    MojoTimeTicks end_time = MojoGetTimeTicksNow();

    // Add up write and read counts, and destroy the threads.
    int64_t num_writes = 0;
    for (unsigned i = 0; i < num_writers; i++) {
      num_writes += writers[i]->num_writes();
      delete writers[i];
    }
    writers.clear();
    int64_t num_reads = 0;
    for (unsigned i = 0; i < num_readers; i++) {
      num_reads += readers[i]->num_reads();
      delete readers[i];
    }
    readers.clear();

    char sub_test_name[200];
    snprintf(sub_test_name, sizeof(sub_test_name), "%uw_%ur_%ubytes",
             num_writers, num_readers, static_cast<unsigned>(num_bytes));
    mojo::test::LogPerfResult(
        "MessagePipe_Threaded_Writes", sub_test_name,
        1000000.0 * static_cast<double>(num_writes) / (end_time - start_time),
        "writes/second");
    mojo::test::LogPerfResult(
        "MessagePipe_Threaded_Reads", sub_test_name,
        1000000.0 * static_cast<double>(num_reads) / (end_time - start_time),
        "reads/second");
  }
#endif  // !defined(WIN32)

  MojoHandle h0_;
  MojoHandle h1_;

  std::vector<uint8_t> buffer_;

 private:
#if !defined(WIN32)
  void Sleep(int64_t microseconds) {
    struct timespec req = {
        static_cast<time_t>(microseconds / 1000000),       // Seconds.
        static_cast<long>(microseconds % 1000000) * 1000L  // Nanoseconds.
    };
    [[maybe_unused]] int rv = nanosleep(&req, nullptr);
    assert(rv == 0);
  }
#endif  // !defined(WIN32)
};

// A no-op test so we can compare performance.
TEST_F(CorePerftest, NoOp) {
  mojo::test::IterateAndReportPerf("Iterate_NoOp", nullptr, &CorePerftest::NoOp,
                                   this);
}

TEST_F(CorePerftest, MessagePipe_CreateAndClose) {
  mojo::test::IterateAndReportPerf("MessagePipe_CreateAndClose", nullptr,
                                   &CorePerftest::MessagePipe_CreateAndClose,
                                   this);
}

TEST_F(CorePerftest, MessagePipe_WriteAndRead) {
  [[maybe_unused]] MojoResult result =
      MojoCreateMessagePipe(nullptr, &h0_, &h1_);
  assert(result == MOJO_RESULT_OK);
  buffer_.resize(10);
  mojo::test::IterateAndReportPerf("MessagePipe_WriteAndRead", "10bytes",
                                   &CorePerftest::MessagePipe_WriteAndRead,
                                   this);
  buffer_.resize(100);
  mojo::test::IterateAndReportPerf("MessagePipe_WriteAndRead", "100bytes",
                                   &CorePerftest::MessagePipe_WriteAndRead,
                                   this);
  buffer_.resize(1000);
  mojo::test::IterateAndReportPerf("MessagePipe_WriteAndRead", "1000bytes",
                                   &CorePerftest::MessagePipe_WriteAndRead,
                                   this);
  buffer_.resize(10000);
  mojo::test::IterateAndReportPerf("MessagePipe_WriteAndRead", "10000bytes",
                                   &CorePerftest::MessagePipe_WriteAndRead,
                                   this);
  result = MojoClose(h0_);
  assert(result == MOJO_RESULT_OK);
  result = MojoClose(h1_);
  assert(result == MOJO_RESULT_OK);
}

TEST_F(CorePerftest, MessagePipe_EmptyRead) {
  [[maybe_unused]] MojoResult result =
      MojoCreateMessagePipe(nullptr, &h0_, &h1_);
  assert(result == MOJO_RESULT_OK);
  mojo::test::IterateAndReportPerf("MessagePipe_EmptyRead", nullptr,
                                   &CorePerftest::MessagePipe_EmptyRead, this);
  result = MojoClose(h0_);
  assert(result == MOJO_RESULT_OK);
  result = MojoClose(h1_);
  assert(result == MOJO_RESULT_OK);
}

#if !defined(WIN32)
TEST_F(CorePerftest, MessagePipe_Threaded) {
  DoMessagePipeThreadedTest(1u, 1u, 100u);
  DoMessagePipeThreadedTest(2u, 2u, 100u);
  DoMessagePipeThreadedTest(3u, 3u, 100u);
  DoMessagePipeThreadedTest(10u, 10u, 100u);
  DoMessagePipeThreadedTest(10u, 1u, 100u);
  DoMessagePipeThreadedTest(1u, 10u, 100u);

  // For comparison of overhead:
  DoMessagePipeThreadedTest(1u, 1u, 10u);
  // 100 was done above.
  DoMessagePipeThreadedTest(1u, 1u, 1000u);
  DoMessagePipeThreadedTest(1u, 1u, 10000u);

  DoMessagePipeThreadedTest(3u, 3u, 10u);
  // 100 was done above.
  DoMessagePipeThreadedTest(3u, 3u, 1000u);
  DoMessagePipeThreadedTest(3u, 3u, 10000u);
}
#endif  // !defined(WIN32)

}  // namespace

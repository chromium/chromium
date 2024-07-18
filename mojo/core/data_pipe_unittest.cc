// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/c/system/data_pipe.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

const uint32_t kSizeOfOptions =
    static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions));

// In various places, we have to poll (since, e.g., we can't yet wait for a
// certain amount of data to be available). This is the maximum number of
// iterations (separated by a short sleep).
// TODO(vtl): Get rid of this.
const size_t kMaxPoll = 100;

// Used in Multiprocess test.
const size_t kMultiprocessCapacity = 37;
const char kMultiprocessTestData[] = "hello i'm a string that is 36 bytes";
const int kMultiprocessMaxIter = 5;

// Capacity that will cause data pipe creation to fail.
constexpr size_t kOversizedCapacity = std::numeric_limits<uint32_t>::max();

// A timeout smaller than |TestTimeouts::tiny_timeout()|, as a |MojoDeadline|.
// Warning: This may lead to flakiness, but this is unavoidable if, e.g., you're
// trying to ensure that functions with timeouts are reasonably accurate. We
// want this to be as small as possible without causing too much flakiness.
base::TimeDelta EpsilonDeadline() {
  const int64_t tiny_timeout = TestTimeouts::tiny_timeout().InMicroseconds();
// Originally, our epsilon timeout was 10 ms, which was mostly fine but flaky on
// some Windows bots. I don't recall ever seeing flakes on other bots. At 30 ms
// tests seem reliable on Windows bots, but not at 25 ms. We'd like this timeout
// to be as small as possible (see the description in the .h file).
//
// Currently, |tiny_timeout()| is usually 100 ms (possibly scaled under ASAN,
// etc.). Based on this, set it to (usually be) 30 ms on Windows and 20 ms
// elsewhere.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  const int64_t deadline = (tiny_timeout * 3) / 10;
#else
  const int64_t deadline = (tiny_timeout * 2) / 10;
#endif
  return base::Microseconds(deadline);
}

// TODO(rockot): There are many uses of ASSERT where EXPECT would be more
// appropriate. Fix this.

class DataPipeTest : public test::MojoTestBase {
 public:
  DataPipeTest()
      : producer_(MOJO_HANDLE_INVALID), consumer_(MOJO_HANDLE_INVALID) {}

  DataPipeTest(const DataPipeTest&) = delete;
  DataPipeTest& operator=(const DataPipeTest&) = delete;

  ~DataPipeTest() override {
    if (producer_ != MOJO_HANDLE_INVALID)
      CHECK_EQ(MOJO_RESULT_OK, MojoClose(producer_));
    if (consumer_ != MOJO_HANDLE_INVALID)
      CHECK_EQ(MOJO_RESULT_OK, MojoClose(consumer_));
  }

  MojoResult ReadEmptyMessageWithHandles(MojoHandle pipe,
                                         MojoHandle* out_handles,
                                         uint32_t num_handles) {
    std::vector<uint8_t> bytes;
    std::vector<ScopedHandle> handles;
    MojoResult rv = ReadMessageRaw(MessagePipeHandle(pipe), &bytes, &handles,
                                   MOJO_READ_MESSAGE_FLAG_NONE);
    if (rv == MOJO_RESULT_OK) {
      CHECK_EQ(0u, bytes.size());
      CHECK_EQ(num_handles, handles.size());
      for (size_t i = 0; i < num_handles; ++i)
        out_handles[i] = handles[i].release().value();
    }
    return rv;
  }

  MojoResult Create(const MojoCreateDataPipeOptions* options) {
    return MojoCreateDataPipe(options, &producer_, &consumer_);
  }

  MojoResult WriteData(const void* elements,
                       uint32_t* num_bytes,
                       bool all_or_none = false) {
    MojoWriteDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = all_or_none ? MOJO_WRITE_DATA_FLAG_ALL_OR_NONE
                                : MOJO_WRITE_DATA_FLAG_NONE;
    return MojoWriteData(producer_, elements, num_bytes, &options);
  }

  MojoResult ReadData(void* elements,
                      uint32_t* num_bytes,
                      bool all_or_none = false,
                      bool peek = false) {
    MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_NONE;
    if (all_or_none)
      flags |= MOJO_READ_DATA_FLAG_ALL_OR_NONE;
    if (peek)
      flags |= MOJO_READ_DATA_FLAG_PEEK;

    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;
    return MojoReadData(consumer_, &options, elements, num_bytes);
  }

  MojoResult QueryData(uint32_t* num_bytes) {
    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_READ_DATA_FLAG_QUERY;
    return MojoReadData(consumer_, &options, nullptr, num_bytes);
  }

  MojoResult DiscardData(uint32_t* num_bytes, bool all_or_none = false) {
    MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_DISCARD;
    if (all_or_none)
      flags |= MOJO_READ_DATA_FLAG_ALL_OR_NONE;
    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = flags;
    return MojoReadData(consumer_, &options, nullptr, num_bytes);
  }

  MojoResult BeginReadData(const void** elements, uint32_t* num_bytes) {
    return MojoBeginReadData(consumer_, nullptr, elements, num_bytes);
  }

  MojoResult EndReadData(uint32_t num_bytes_read) {
    return MojoEndReadData(consumer_, num_bytes_read, nullptr);
  }

  MojoResult BeginWriteData(void** elements, uint32_t* num_bytes) {
    return MojoBeginWriteData(producer_, nullptr, elements, num_bytes);
  }

  MojoResult EndWriteData(uint32_t num_bytes_written) {
    return MojoEndWriteData(producer_, num_bytes_written, nullptr);
  }

  MojoResult CloseProducer() {
    MojoResult rv = MojoClose(producer_);
    producer_ = MOJO_HANDLE_INVALID;
    return rv;
  }

  MojoResult CloseConsumer() {
    MojoResult rv = MojoClose(consumer_);
    consumer_ = MOJO_HANDLE_INVALID;
    return rv;
  }

  MojoHandle producer_, consumer_;
};

TEST_F(DataPipeTest, Basic) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      1000 * sizeof(int32_t)                   // |capacity_num_bytes|.
  };

  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));

  // We can write to a data pipe handle immediately.
  int32_t elements[10] = {};
  uint32_t num_bytes = 0;

  num_bytes = static_cast<uint32_t>(std::size(elements) * sizeof(elements[0]));

  elements[0] = 123;
  elements[1] = 456;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(&elements[0], &num_bytes));

  // Now wait for the other side to become readable.
  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            state.satisfied_signals);

  elements[0] = -1;
  elements[1] = -1;
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(&elements[0], &num_bytes));
  ASSERT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);
  ASSERT_EQ(elements[0], 123);
  ASSERT_EQ(elements[1], 456);
}

// Tests creation of data pipes with various (valid) options.
TEST_F(DataPipeTest, CreateAndMaybeTransfer) {
  MojoCreateDataPipeOptions test_options[] = {
      // Default options.
      {},
      // Trivial element size, non-default capacity.
      {kSizeOfOptions,                   // |struct_size|.
       MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
       1,                                // |element_num_bytes|.
       1000},                            // |capacity_num_bytes|.
      // Nontrivial element size, non-default capacity.
      {kSizeOfOptions,                   // |struct_size|.
       MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
       4,                                // |element_num_bytes|.
       4000},                            // |capacity_num_bytes|.
      // Nontrivial element size, default capacity.
      {kSizeOfOptions,                   // |struct_size|.
       MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
       100,                              // |element_num_bytes|.
       0}                                // |capacity_num_bytes|.
  };
  for (size_t i = 0; i < std::size(test_options); i++) {
    MojoHandle producer_handle, consumer_handle;
    MojoCreateDataPipeOptions* options = i ? &test_options[i] : nullptr;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoCreateDataPipe(options, &producer_handle, &consumer_handle));
    ASSERT_EQ(MOJO_RESULT_OK, MojoClose(producer_handle));
    ASSERT_EQ(MOJO_RESULT_OK, MojoClose(consumer_handle));
  }
}

TEST_F(DataPipeTest, SimpleReadWrite) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      1000 * sizeof(int32_t)                   // |capacity_num_bytes|.
  };

  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  int32_t elements[10] = {};
  uint32_t num_bytes = 0;

  // Try reading; nothing there yet.
  num_bytes = static_cast<uint32_t>(std::size(elements) * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadData(elements, &num_bytes));

  // Query; nothing there yet.
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(0u, num_bytes);

  // Discard; nothing there yet.
  num_bytes = static_cast<uint32_t>(5u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, DiscardData(&num_bytes));

  // Read with invalid |num_bytes|.
  num_bytes = sizeof(elements[0]) + 1;
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, ReadData(elements, &num_bytes));

  // Write two elements.
  elements[0] = 123;
  elements[1] = 456;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(elements, &num_bytes));
  // It should have written everything (even without "all or none").
  ASSERT_EQ(2u * sizeof(elements[0]), num_bytes);

  // Wait.
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Query.
  // TODO(vtl): It's theoretically possible (though not with the current
  // implementation/configured limits) that not all the data has arrived yet.
  // (The theoretically-correct assertion here is that |num_bytes| is |1 * ...|
  // or |2 * ...|.)
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(2 * sizeof(elements[0]), num_bytes);

  // Read one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(elements, &num_bytes));
  ASSERT_EQ(1u * sizeof(elements[0]), num_bytes);
  ASSERT_EQ(123, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Query.
  // TODO(vtl): See previous TODO. (If we got 2 elements there, however, we
  // should get 1 here.)
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(1 * sizeof(elements[0]), num_bytes);

  // Peek one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(elements, &num_bytes, false, true));
  ASSERT_EQ(1u * sizeof(elements[0]), num_bytes);
  ASSERT_EQ(456, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Query. Still has 1 element remaining.
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(1 * sizeof(elements[0]), num_bytes);

  // Try to read two elements, with "all or none".
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            ReadData(elements, &num_bytes, true, false));
  ASSERT_EQ(-1, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Try to read two elements, without "all or none".
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(elements, &num_bytes, false, false));
  ASSERT_EQ(1u * sizeof(elements[0]), num_bytes);
  ASSERT_EQ(456, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Query.
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(0u, num_bytes);
}

// Note: The "basic" waiting tests test that the "wait states" are correct in
// various situations; they don't test that waiters are properly awoken on state
// changes. (For that, we need to use multiple threads.)
TEST_F(DataPipeTest, BasicProducerWaiting) {
  // Note: We take advantage of the fact that current for current
  // implementations capacities are strict maximums. This is not guaranteed by
  // the API.

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      2 * sizeof(int32_t)                      // |capacity_num_bytes|.
  };
  Create(&options);
  MojoHandleSignalsState hss;

  // Never readable. Already writable.
  hss = GetSignalsState(producer_);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Write two elements.
  int32_t elements[2] = {123, 456};
  uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(elements, &num_bytes, true));
  ASSERT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);

  // Wait for data to become available to the consumer.
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Peek one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(elements, &num_bytes, true, true));
  ASSERT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  ASSERT_EQ(123, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Read one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(elements, &num_bytes, true, false));
  ASSERT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  ASSERT_EQ(123, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Try writing, using a two-phase write.
  void* buffer = nullptr;
  num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&buffer, &num_bytes));
  EXPECT_TRUE(buffer);
  ASSERT_GE(num_bytes, static_cast<uint32_t>(1u * sizeof(elements[0])));

  static_cast<int32_t*>(buffer)[0] = 789;
  ASSERT_EQ(MOJO_RESULT_OK,
            EndWriteData(static_cast<uint32_t>(1u * sizeof(elements[0]))));

  // Read one element, using a two-phase read.
  const void* read_buffer = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer, &num_bytes));
  EXPECT_TRUE(read_buffer);
  // The two-phase read should be able to read at least one element.
  ASSERT_GE(num_bytes, static_cast<uint32_t>(1u * sizeof(elements[0])));
  ASSERT_EQ(456, static_cast<const int32_t*>(read_buffer)[0]);
  ASSERT_EQ(MOJO_RESULT_OK,
            EndReadData(static_cast<uint32_t>(1u * sizeof(elements[0]))));

  // Write one element.
  elements[0] = 123;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(elements, &num_bytes));
  ASSERT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

  // Close the consumer.
  CloseConsumer();

  // It should now be never-writable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(producer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
}

TEST_F(DataPipeTest, PeerClosedProducerWaiting) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      2 * sizeof(int32_t)                      // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Close the consumer.
  CloseConsumer();

  // It should be signaled.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(producer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
}

TEST_F(DataPipeTest, PeerClosedConsumerWaiting) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      2 * sizeof(int32_t)                      // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Close the producer.
  CloseProducer();

  // It should be signaled.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
}

TEST_F(DataPipeTest, BasicConsumerWaiting) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      1000 * sizeof(int32_t)                   // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Never writable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_WRITABLE, &hss));
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Write two elements.
  int32_t elements[2] = {123, 456};
  uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(elements, &num_bytes, true));

  // Wait for readability.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Discard one element.
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, DiscardData(&num_bytes, true));
  ASSERT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);

  // Should still be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Peek one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(elements, &num_bytes, true, true));
  ASSERT_EQ(456, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Should still be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Read one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(elements, &num_bytes, true));
  ASSERT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  ASSERT_EQ(456, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Write one element.
  elements[0] = 789;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(elements, &num_bytes, true));

  // Waiting should now succeed.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Close the producer.
  CloseProducer();

  // Should still be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_TRUE(hss.satisfied_signals & (MOJO_HANDLE_SIGNAL_READABLE |
                                       MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfiable_signals);

  // Wait for the peer closed signal.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfiable_signals);

  // Read one element.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = static_cast<uint32_t>(1u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(elements, &num_bytes, true));
  ASSERT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  ASSERT_EQ(789, elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Should be never-readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
}

TEST_F(DataPipeTest, ConsumerNewDataReadable) {
  const MojoCreateDataPipeOptions create_options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      1000 * sizeof(int32_t)                   // |capacity_num_bytes|.
  };
  EXPECT_EQ(MOJO_RESULT_OK, Create(&create_options));

  int32_t elements[2] = {123, 456};
  uint32_t num_bytes = static_cast<uint32_t>(2u * sizeof(elements[0]));
  EXPECT_EQ(MOJO_RESULT_OK, WriteData(elements, &num_bytes, true));

  // The consumer handle should appear to be readable and have new data.
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_TRUE(GetSignalsState(consumer_).satisfied_signals &
              MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE);

  // Now try to read a minimum of 6 elements.
  int32_t read_elements[6];
  uint32_t num_read_bytes = sizeof(read_elements);
  MojoReadDataOptions read_options;
  read_options.struct_size = sizeof(read_options);
  read_options.flags = MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  EXPECT_EQ(
      MOJO_RESULT_OUT_OF_RANGE,
      MojoReadData(consumer_, &read_options, read_elements, &num_read_bytes));

  // The consumer should still appear to be readable but not with new data.
  EXPECT_TRUE(GetSignalsState(consumer_).satisfied_signals &
              MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(consumer_).satisfied_signals &
               MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE);

  // Write four more elements.
  EXPECT_EQ(MOJO_RESULT_OK, WriteData(elements, &num_bytes, true));
  EXPECT_EQ(MOJO_RESULT_OK, WriteData(elements, &num_bytes, true));

  // The consumer handle should once again appear to be readable.
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE));

  // Try again to read a minimum of 6 elements. Should succeed this time.
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadData(consumer_, &read_options,
                                         read_elements, &num_read_bytes));

  // And now the consumer is unreadable.
  EXPECT_FALSE(GetSignalsState(consumer_).satisfied_signals &
               MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_FALSE(GetSignalsState(consumer_).satisfied_signals &
               MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE);
}

// Test with two-phase APIs and also closing the producer with an active
// consumer waiter.
TEST_F(DataPipeTest, ConsumerWaitingTwoPhase) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      1000 * sizeof(int32_t)                   // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Write two elements.
  int32_t* elements = nullptr;
  void* buffer = nullptr;
  // Request room for three (but we'll only write two).
  uint32_t num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&buffer, &num_bytes));
  EXPECT_TRUE(buffer);
  EXPECT_GE(num_bytes, static_cast<uint32_t>(3u * sizeof(elements[0])));
  elements = static_cast<int32_t*>(buffer);
  elements[0] = 123;
  elements[1] = 456;
  ASSERT_EQ(MOJO_RESULT_OK, EndWriteData(2u * sizeof(elements[0])));

  // Wait for readability.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Read one element.
  // Two should be available, but only read one.
  const void* read_buffer = nullptr;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer, &num_bytes));
  EXPECT_TRUE(read_buffer);
  ASSERT_EQ(static_cast<uint32_t>(2u * sizeof(elements[0])), num_bytes);
  const int32_t* read_elements = static_cast<const int32_t*>(read_buffer);
  ASSERT_EQ(123, read_elements[0]);
  ASSERT_EQ(MOJO_RESULT_OK, EndReadData(1u * sizeof(elements[0])));

  // Should still be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Read one element.
  // Request three, but not in all-or-none mode.
  read_buffer = nullptr;
  num_bytes = static_cast<uint32_t>(3u * sizeof(elements[0]));
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer, &num_bytes));
  EXPECT_TRUE(read_buffer);
  ASSERT_EQ(static_cast<uint32_t>(1u * sizeof(elements[0])), num_bytes);
  read_elements = static_cast<const int32_t*>(read_buffer);
  ASSERT_EQ(456, read_elements[0]);
  ASSERT_EQ(MOJO_RESULT_OK, EndReadData(1u * sizeof(elements[0])));

  // Close the producer.
  CloseProducer();

  // Should be never-readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);
}

// Tests that data pipes aren't writable/readable during two-phase writes/reads.
TEST_F(DataPipeTest, BasicTwoPhaseWaiting) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      1000 * sizeof(int32_t)                   // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // It should be writable.
  hss = GetSignalsState(producer_);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  uint32_t num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  void* write_ptr = nullptr;
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&write_ptr, &num_bytes));
  EXPECT_TRUE(write_ptr);
  EXPECT_GE(num_bytes, static_cast<uint32_t>(1u * sizeof(int32_t)));

  // It shouldn't be readable yet (we'll wait later).
  hss = GetSignalsState(consumer_);
  ASSERT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  static_cast<int32_t*>(write_ptr)[0] = 123;
  ASSERT_EQ(MOJO_RESULT_OK, EndWriteData(1u * sizeof(int32_t)));

  // It should immediately be writable again.
  hss = GetSignalsState(producer_);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // It should become readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Start another two-phase write and check that it's readable even in the
  // middle of it.
  num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  write_ptr = nullptr;
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&write_ptr, &num_bytes));
  EXPECT_TRUE(write_ptr);
  EXPECT_GE(num_bytes, static_cast<uint32_t>(1u * sizeof(int32_t)));

  // It should be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // End the two-phase write without writing anything.
  ASSERT_EQ(MOJO_RESULT_OK, EndWriteData(0u));

  // Start a two-phase read.
  num_bytes = static_cast<uint32_t>(1u * sizeof(int32_t));
  const void* read_ptr = nullptr;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_ptr, &num_bytes));
  EXPECT_TRUE(read_ptr);
  ASSERT_EQ(static_cast<uint32_t>(1u * sizeof(int32_t)), num_bytes);

  // At this point, it should still be writable.
  hss = GetSignalsState(producer_);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // End the two-phase read without reading anything.
  ASSERT_EQ(MOJO_RESULT_OK, EndReadData(0u));

  // It should still be readable.
  hss = GetSignalsState(consumer_);
  ASSERT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);
}

void Seq(int32_t start, size_t count, int32_t* out) {
  for (size_t i = 0; i < count; i++)
    out[i] = start + static_cast<int32_t>(i);
}

TEST_F(DataPipeTest, AllOrNone) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      10 * sizeof(int32_t)                     // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Try writing more than the total capacity of the pipe.
  uint32_t num_bytes = 20u * sizeof(int32_t);
  int32_t buffer[100];
  Seq(0, std::size(buffer), buffer);
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE, WriteData(buffer, &num_bytes, true));

  // Should still be empty.
  num_bytes = ~0u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(0u, num_bytes);

  // Write some data.
  num_bytes = 5u * sizeof(int32_t);
  Seq(100, std::size(buffer), buffer);
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(buffer, &num_bytes, true));
  ASSERT_EQ(5u * sizeof(int32_t), num_bytes);

  // Wait for data.
  // TODO(vtl): There's no real guarantee that all the data will become
  // available at once (except that in current implementations, with reasonable
  // limits, it will). Eventually, we'll be able to wait for a specified amount
  // of data to become available.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Half full.
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(5u * sizeof(int32_t), num_bytes);

  // Try writing more than the available capacity of the pipe, but less than the
  // total capacity.
  num_bytes = 6u * sizeof(int32_t);
  Seq(200, std::size(buffer), buffer);
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE, WriteData(buffer, &num_bytes, true));

  // Try reading too much.
  num_bytes = 11u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE, ReadData(buffer, &num_bytes, true));
  int32_t expected_buffer[100];
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  ASSERT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much.
  num_bytes = 11u * sizeof(int32_t);
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE, DiscardData(&num_bytes, true));

  // Just a little.
  num_bytes = 2u * sizeof(int32_t);
  Seq(300, std::size(buffer), buffer);
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(buffer, &num_bytes, true));
  ASSERT_EQ(2u * sizeof(int32_t), num_bytes);

  // Just right.
  num_bytes = 3u * sizeof(int32_t);
  Seq(400, std::size(buffer), buffer);
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(buffer, &num_bytes, true));
  ASSERT_EQ(3u * sizeof(int32_t), num_bytes);

  // TODO(vtl): Hack (see also the TODO above): We can't currently wait for a
  // specified amount of data to be available, so poll.
  for (size_t i = 0; i < kMaxPoll; i++) {
    num_bytes = 0u;
    ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
    if (num_bytes >= 10u * sizeof(int32_t))
      break;

    base::PlatformThread::Sleep(EpsilonDeadline());
  }
  ASSERT_EQ(10u * sizeof(int32_t), num_bytes);

  // Read half.
  num_bytes = 5u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(buffer, &num_bytes, true));
  ASSERT_EQ(5u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  Seq(100, 5, expected_buffer);
  ASSERT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try reading too much again.
  num_bytes = 6u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE, ReadData(buffer, &num_bytes, true));
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  ASSERT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much again.
  num_bytes = 6u * sizeof(int32_t);
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE, DiscardData(&num_bytes, true));

  // Discard a little.
  num_bytes = 2u * sizeof(int32_t);
  ASSERT_EQ(MOJO_RESULT_OK, DiscardData(&num_bytes, true));
  ASSERT_EQ(2u * sizeof(int32_t), num_bytes);

  // Three left.
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(3u * sizeof(int32_t), num_bytes);

  // Close the producer, then test producer-closed cases.
  CloseProducer();

  // Wait.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  EXPECT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_TRUE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // Try reading too much; "failed precondition" since the producer is closed.
  num_bytes = 4u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            ReadData(buffer, &num_bytes, true));
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  ASSERT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Try discarding too much; "failed precondition" again.
  num_bytes = 4u * sizeof(int32_t);
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, DiscardData(&num_bytes, true));

  // Read a little.
  num_bytes = 2u * sizeof(int32_t);
  memset(buffer, 0xab, sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(buffer, &num_bytes, true));
  ASSERT_EQ(2u * sizeof(int32_t), num_bytes);
  memset(expected_buffer, 0xab, sizeof(expected_buffer));
  Seq(400, 2, expected_buffer);
  ASSERT_EQ(0, memcmp(buffer, expected_buffer, sizeof(buffer)));

  // Discard the remaining element.
  num_bytes = 1u * sizeof(int32_t);
  ASSERT_EQ(MOJO_RESULT_OK, DiscardData(&num_bytes, true));
  ASSERT_EQ(1u * sizeof(int32_t), num_bytes);

  // Empty again.
  num_bytes = ~0u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(0u, num_bytes);
}

// Tests that |ProducerWriteData()| and |ConsumerReadData()| writes and reads,
// respectively, as much as possible, even if it may have to "wrap around" the
// internal circular buffer. (Note that the two-phase write and read need not do
// this.)
TEST_F(DataPipeTest, WrapAround) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "This test covers implementation details that are only "
                 << "relevant with MojoIpcz disabled; namely that a data pipe "
                 << "is backed by a circular ring buffer.";
  }

  unsigned char test_data[1000];
  for (size_t i = 0; i < std::size(test_data); i++)
    test_data[i] = static_cast<unsigned char>(i);

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1u,                               // |element_num_bytes|.
      100u                              // |capacity_num_bytes|.
  };

  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Write 20 bytes.
  uint32_t num_bytes = 20u;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(&test_data[0], &num_bytes, true));
  ASSERT_EQ(20u, num_bytes);

  // Wait for data.
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Read 10 bytes.
  unsigned char read_buffer[1000] = {0};
  num_bytes = 10u;
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(read_buffer, &num_bytes, true));
  ASSERT_EQ(10u, num_bytes);
  ASSERT_EQ(0, memcmp(read_buffer, &test_data[0], 10u));

  // Check that a two-phase write can now only write (at most) 80 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed.)
  void* write_buffer_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&write_buffer_ptr, &num_bytes));
  EXPECT_TRUE(write_buffer_ptr);
  ASSERT_EQ(80u, num_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, EndWriteData(0));

  size_t total_num_bytes = 0;
  while (total_num_bytes < 90) {
    // Wait to write.
    ASSERT_EQ(MOJO_RESULT_OK,
              WaitForSignals(producer_, MOJO_HANDLE_SIGNAL_WRITABLE, &hss));
    ASSERT_EQ(hss.satisfied_signals, MOJO_HANDLE_SIGNAL_WRITABLE);
    ASSERT_EQ(hss.satisfiable_signals, MOJO_HANDLE_SIGNAL_WRITABLE |
                                           MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                                           MOJO_HANDLE_SIGNAL_PEER_REMOTE);

    // Write as much as we can.
    num_bytes = 100;
    ASSERT_EQ(MOJO_RESULT_OK,
              WriteData(&test_data[20 + total_num_bytes], &num_bytes, false));
    total_num_bytes += num_bytes;
  }

  ASSERT_EQ(90u, total_num_bytes);

  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(100u, num_bytes);

  // Check that a two-phase read can now only read (at most) 90 bytes. (This
  // checks an implementation detail; this behavior is not guaranteed.)
  const void* read_buffer_ptr = nullptr;
  num_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer_ptr, &num_bytes));
  EXPECT_TRUE(read_buffer_ptr);
  ASSERT_EQ(90u, num_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, EndReadData(0));

  // Read as much as possible. We should read 100 bytes.
  num_bytes =
      static_cast<uint32_t>(std::size(read_buffer) * sizeof(read_buffer[0]));
  memset(read_buffer, 0, num_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(read_buffer, &num_bytes));
  ASSERT_EQ(100u, num_bytes);
  ASSERT_EQ(0, memcmp(read_buffer, &test_data[10], 100u));
}

// Tests the behavior of writing (simple and two-phase), closing the producer,
// then reading (simple and two-phase).
TEST_F(DataPipeTest, WriteCloseProducerRead) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1u,                               // |element_num_bytes|.
      1000u                             // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));

  // Write some data, so we'll have something to read.
  uint32_t num_bytes = kTestDataSize;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(kTestData, &num_bytes, false));
  ASSERT_EQ(kTestDataSize, num_bytes);

  // Write it again, so we'll have something left over.
  num_bytes = kTestDataSize;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(kTestData, &num_bytes, false));
  ASSERT_EQ(kTestDataSize, num_bytes);

  // Start two-phase write.
  void* write_buffer_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&write_buffer_ptr, &num_bytes));
  EXPECT_TRUE(write_buffer_ptr);
  EXPECT_GT(num_bytes, 0u);

  // TODO(vtl): (See corresponding TODO in TwoPhaseAllOrNone.)
  for (size_t i = 0; i < kMaxPoll; i++) {
    num_bytes = 0u;
    ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
    if (num_bytes >= 2u * kTestDataSize)
      break;

    base::PlatformThread::Sleep(EpsilonDeadline());
  }
  ASSERT_GE(num_bytes, kTestDataSize);

  // Start two-phase read.
  const void* read_buffer_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer_ptr, &num_bytes));
  EXPECT_TRUE(read_buffer_ptr);
  ASSERT_GE(num_bytes, kTestDataSize);

  // Close the producer.
  CloseProducer();

  // The consumer can finish its two-phase read.
  ASSERT_EQ(0, memcmp(read_buffer_ptr, kTestData, kTestDataSize));
  ASSERT_EQ(MOJO_RESULT_OK, EndReadData(kTestDataSize));

  // And start another.
  read_buffer_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer_ptr, &num_bytes));
  EXPECT_TRUE(read_buffer_ptr);
  ASSERT_EQ(kTestDataSize, num_bytes);
}

// Tests the behavior of interrupting a two-phase read and write by closing the
// consumer.
TEST_F(DataPipeTest, TwoPhaseWriteReadCloseConsumer) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1u,                               // |element_num_bytes|.
      1000u                             // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Write some data, so we'll have something to read.
  uint32_t num_bytes = kTestDataSize;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(kTestData, &num_bytes));
  ASSERT_EQ(kTestDataSize, num_bytes);

  // Start two-phase write.
  void* write_buffer_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&write_buffer_ptr, &num_bytes));
  EXPECT_TRUE(write_buffer_ptr);
  ASSERT_GT(num_bytes, kTestDataSize);

  // Wait for data.
  // TODO(vtl): (See corresponding TODO in AllOrNone.)
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Start two-phase read.
  const void* read_buffer_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer_ptr, &num_bytes));
  EXPECT_TRUE(read_buffer_ptr);
  ASSERT_EQ(kTestDataSize, num_bytes);

  // Close the consumer.
  CloseConsumer();

  // Wait for producer to know that the consumer is closed.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(producer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

  // Actually write some data. (Note: Premature freeing of the buffer would
  // probably only be detected under ASAN or similar.)
  memcpy(write_buffer_ptr, kTestData, kTestDataSize);
  // Note: Even though the consumer has been closed, ending the two-phase
  // write will report success.
  ASSERT_EQ(MOJO_RESULT_OK, EndWriteData(kTestDataSize));

  // But trying to write should result in failure.
  num_bytes = kTestDataSize;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, WriteData(kTestData, &num_bytes));

  // As will trying to start another two-phase write.
  write_buffer_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            BeginWriteData(&write_buffer_ptr, &num_bytes));
}

// Tests the behavior of "interrupting" a two-phase write by closing both the
// producer and the consumer.
TEST_F(DataPipeTest, TwoPhaseWriteCloseBoth) {
  const uint32_t kTestDataSize = 15u;

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1u,                               // |element_num_bytes|.
      1000u                             // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));

  // Start two-phase write.
  void* write_buffer_ptr = nullptr;
  uint32_t num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&write_buffer_ptr, &num_bytes));
  EXPECT_TRUE(write_buffer_ptr);
  ASSERT_GT(num_bytes, kTestDataSize);
}

// Tests the behavior of writing, closing the producer, and then reading (with
// and without data remaining).
TEST_F(DataPipeTest, WriteCloseProducerReadNoData) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1u,                               // |element_num_bytes|.
      1000u                             // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Write some data, so we'll have something to read.
  uint32_t num_bytes = kTestDataSize;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(kTestData, &num_bytes));
  ASSERT_EQ(kTestDataSize, num_bytes);

  // Close the producer.
  CloseProducer();

  // Wait. (Note that once the consumer knows that the producer is closed, it
  // must also know about all the data that was sent.)
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfiable_signals);

  // Peek that data.
  char buffer[1000];
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(buffer, &num_bytes, false, true));
  ASSERT_EQ(kTestDataSize, num_bytes);
  ASSERT_EQ(0, memcmp(buffer, kTestData, kTestDataSize));

  // Read that data.
  memset(buffer, 0, 1000);
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(buffer, &num_bytes));
  ASSERT_EQ(kTestDataSize, num_bytes);
  ASSERT_EQ(0, memcmp(buffer, kTestData, kTestDataSize));

  // A second read should fail.
  num_bytes = static_cast<uint32_t>(sizeof(buffer));
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, ReadData(buffer, &num_bytes));

  // A two-phase read should also fail.
  const void* read_buffer_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            BeginReadData(&read_buffer_ptr, &num_bytes));

  // Ditto for discard.
  num_bytes = 10u;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, DiscardData(&num_bytes));
}

// Test that during a two phase read the memory stays valid even if more data
// comes in.
TEST_F(DataPipeTest, TwoPhaseReadMemoryStable) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1u,                               // |element_num_bytes|.
      1000u                             // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Write some data.
  uint32_t num_bytes = kTestDataSize;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(kTestData, &num_bytes));
  ASSERT_EQ(kTestDataSize, num_bytes);

  // Wait for the data.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Begin a two-phase read.
  const void* read_buffer_ptr = nullptr;
  uint32_t read_buffer_size = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer_ptr, &read_buffer_size));

  // Write more data.
  const char kExtraData[] = "bye world";
  const uint32_t kExtraDataSize = static_cast<uint32_t>(sizeof(kExtraData));
  num_bytes = kExtraDataSize;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(kExtraData, &num_bytes));
  ASSERT_EQ(kExtraDataSize, num_bytes);

  // Close the producer.
  CloseProducer();

  // Wait. (Note that once the consumer knows that the producer is closed, it
  // must also have received the extra data).
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  EXPECT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfiable_signals);

  // Read the two phase memory to check it's still valid.
  ASSERT_EQ(0, memcmp(read_buffer_ptr, kTestData, kTestDataSize));
  EndReadData(read_buffer_size);
}

// Test that two-phase reads/writes behave correctly when given invalid
// arguments.
TEST_F(DataPipeTest, TwoPhaseMoreInvalidArguments) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      10 * sizeof(int32_t)                     // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // No data.
  uint32_t num_bytes = 1000u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(0u, num_bytes);

  // Try "ending" a two-phase write when one isn't active.
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            EndWriteData(1u * sizeof(int32_t)));

  // Wait a bit, to make sure that if a signal were (incorrectly) sent, it'd
  // have time to propagate.
  base::PlatformThread::Sleep(EpsilonDeadline());

  // Still no data.
  num_bytes = 1000u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(0u, num_bytes);

  // Try ending a two-phase write with an invalid amount (too much).
  num_bytes = 0u;
  void* write_ptr = nullptr;
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&write_ptr, &num_bytes));
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            EndWriteData(num_bytes + static_cast<uint32_t>(sizeof(int32_t))));

  // But the two-phase write still ended.
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, EndWriteData(0u));

  // Wait a bit (as above).
  base::PlatformThread::Sleep(EpsilonDeadline());

  // Still no data.
  num_bytes = 1000u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(0u, num_bytes);

  // Try ending a two-phase write with an invalid amount (not a multiple of the
  // element size).
  num_bytes = 0u;
  write_ptr = nullptr;
  ASSERT_EQ(MOJO_RESULT_OK, BeginWriteData(&write_ptr, &num_bytes));
  EXPECT_GE(num_bytes, 1u);
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, EndWriteData(1u));

  // But the two-phase write still ended.
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, EndWriteData(0u));

  // Wait a bit (as above).
  base::PlatformThread::Sleep(EpsilonDeadline());

  // Still no data.
  num_bytes = 1000u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(0u, num_bytes);

  // Now write some data, so we'll be able to try reading.
  int32_t element = 123;
  num_bytes = 1u * sizeof(int32_t);
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(&element, &num_bytes));

  // Wait for data.
  // TODO(vtl): (See corresponding TODO in AllOrNone.)
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // One element available.
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try "ending" a two-phase read when one isn't active.
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION, EndReadData(1u * sizeof(int32_t)));

  // Still one element available.
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try ending a two-phase read with an invalid amount (too much).
  num_bytes = 0u;
  const void* read_ptr = nullptr;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_ptr, &num_bytes));
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            EndReadData(num_bytes + static_cast<uint32_t>(sizeof(int32_t))));

  // Still one element available.
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(1u * sizeof(int32_t), num_bytes);

  // Try ending a two-phase read with an invalid amount (not a multiple of the
  // element size).
  num_bytes = 0u;
  read_ptr = nullptr;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_ptr, &num_bytes));
  ASSERT_EQ(1u * sizeof(int32_t), num_bytes);
  ASSERT_EQ(123, static_cast<const int32_t*>(read_ptr)[0]);
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, EndReadData(1u));

  // Still one element available.
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, QueryData(&num_bytes));
  ASSERT_EQ(1u * sizeof(int32_t), num_bytes);
}

// Test that a producer can be sent over a MP.
TEST_F(DataPipeTest, SendProducer) {
  const char kTestData[] = "hello world";
  const uint32_t kTestDataSize = static_cast<uint32_t>(sizeof(kTestData));

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1u,                               // |element_num_bytes|.
      1000u                             // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));
  MojoHandleSignalsState hss;

  // Write some data.
  uint32_t num_bytes = kTestDataSize;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(kTestData, &num_bytes));
  ASSERT_EQ(kTestDataSize, num_bytes);

  // Wait for the data.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Check the data.
  const void* read_buffer = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer, &num_bytes));
  ASSERT_EQ(0, memcmp(read_buffer, kTestData, kTestDataSize));
  EndReadData(num_bytes);

  // Now send the producer over a MP so that it's serialized.
  MojoHandle pipe0, pipe1;
  ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &pipe0, &pipe1));

  ASSERT_EQ(MOJO_RESULT_OK,
            WriteMessageRaw(MessagePipeHandle(pipe0), nullptr, 0, &producer_, 1,
                            MOJO_WRITE_MESSAGE_FLAG_NONE));
  producer_ = MOJO_HANDLE_INVALID;
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_EQ(MOJO_RESULT_OK, ReadEmptyMessageWithHandles(pipe1, &producer_, 1));

  // Write more data.
  const char kExtraData[] = "bye world";
  const uint32_t kExtraDataSize = static_cast<uint32_t>(sizeof(kExtraData));
  num_bytes = kExtraDataSize;
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(kExtraData, &num_bytes));
  ASSERT_EQ(kExtraDataSize, num_bytes);

  // Wait for it.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Check the second write.
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK, BeginReadData(&read_buffer, &num_bytes));
  ASSERT_EQ(0, memcmp(read_buffer, kExtraData, kExtraDataSize));
  EndReadData(num_bytes);

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

// Ensures that if a data pipe consumer whose producer has closed is passed over
// a message pipe, the deserialized dispatcher is also marked as having a closed
// peer.
TEST_F(DataPipeTest, ConsumerWithClosedProducerSent) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                          // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,         // |flags|.
      static_cast<uint32_t>(sizeof(int32_t)),  // |element_num_bytes|.
      1000 * sizeof(int32_t)                   // |capacity_num_bytes|.
  };

  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));

  // We can write to a data pipe handle immediately.
  int32_t data = 123;
  uint32_t num_bytes = sizeof(data);
  ASSERT_EQ(MOJO_RESULT_OK, WriteData(&data, &num_bytes));
  ASSERT_EQ(MOJO_RESULT_OK, CloseProducer());

  // Now wait for the other side to become readable and to see the peer closed.
  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            state.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            state.satisfiable_signals);

  // Now send the consumer over a MP so that it's serialized.
  MojoHandle pipe0, pipe1;
  ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &pipe0, &pipe1));

  ASSERT_EQ(MOJO_RESULT_OK,
            WriteMessageRaw(MessagePipeHandle(pipe0), nullptr, 0, &consumer_, 1,
                            MOJO_WRITE_MESSAGE_FLAG_NONE));
  consumer_ = MOJO_HANDLE_INVALID;
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1, MOJO_HANDLE_SIGNAL_READABLE, &state));
  ASSERT_EQ(MOJO_RESULT_OK, ReadEmptyMessageWithHandles(pipe1, &consumer_, 1));

  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumer_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            state.satisfiable_signals);

  int32_t read_data;
  ASSERT_EQ(MOJO_RESULT_OK, ReadData(&read_data, &num_bytes));
  ASSERT_EQ(sizeof(read_data), num_bytes);
  ASSERT_EQ(data, read_data);

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

bool WriteAllData(MojoHandle producer,
                  const void* elements,
                  uint32_t num_bytes) {
  for (size_t i = 0; i < kMaxPoll; i++) {
    // Write as much data as we can.
    uint32_t write_bytes = num_bytes;
    MojoResult result =
        MojoWriteData(producer, elements, &write_bytes, nullptr);
    if (result == MOJO_RESULT_OK) {
      num_bytes -= write_bytes;
      elements = static_cast<const uint8_t*>(elements) + write_bytes;
      if (num_bytes == 0)
        return true;
    } else {
      EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT, result);
    }

    MojoHandleSignalsState hss = MojoHandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_OK, test::MojoTestBase::WaitForSignals(
                                  producer, MOJO_HANDLE_SIGNAL_WRITABLE, &hss));
    EXPECT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
    EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                  MOJO_HANDLE_SIGNAL_PEER_REMOTE,
              hss.satisfiable_signals);
  }

  return false;
}

// If |expect_empty| is true, expect |consumer| to be empty after reading.
bool ReadAllData(MojoHandle consumer,
                 void* elements,
                 uint32_t num_bytes,
                 bool expect_empty) {
  for (size_t i = 0; i < kMaxPoll; i++) {
    // Read as much data as we can.
    uint32_t read_bytes = num_bytes;
    MojoResult result = MojoReadData(consumer, nullptr, elements, &read_bytes);
    if (result == MOJO_RESULT_OK) {
      num_bytes -= read_bytes;
      elements = static_cast<uint8_t*>(elements) + read_bytes;
      if (num_bytes == 0) {
        if (expect_empty) {
          // Expect no more data.
          base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
          MojoReadDataOptions options;
          options.struct_size = sizeof(options);
          options.flags = MOJO_READ_DATA_FLAG_QUERY;
          MojoReadData(consumer, &options, nullptr, &num_bytes);
          EXPECT_EQ(0u, num_bytes);
        }
        return true;
      }
    } else {
      EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT, result);
    }

    MojoHandleSignalsState hss = MojoHandleSignalsState();
    EXPECT_EQ(MOJO_RESULT_OK, test::MojoTestBase::WaitForSignals(
                                  consumer, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    // Peer could have become closed while we're still waiting for data.
    EXPECT_TRUE(MOJO_HANDLE_SIGNAL_READABLE & hss.satisfied_signals);
    EXPECT_TRUE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
    EXPECT_TRUE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  }

  return num_bytes == 0;
}

TEST_F(DataPipeTest, CreateOversized) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Data pipes do not allocate dedicated capacity when "
                 << "MojoIpcz is enabled, so capacity limits are not enforced "
                 << "and therefore cannot be tested.";
  }

  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1,                                // |element_num_bytes|.
      kOversizedCapacity,               // |capacity_num_bytes|.
  };

  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED, Create(&options));
}

#if BUILDFLAG(USE_BLINK)

constexpr size_t kNoSpuriousEvents_NumIterations = 1000;

TEST_F(DataPipeTest, NoSpuriousEvents) {
  // Regression test for https://crbug.com/1409259. Verifies that data pipe read
  // events are never spurious.
  RunTestClient("NoSpuriousEventsHost", [&](MojoHandle host) {
    RunTestClient("NoSpuriousEventsClient", [&](MojoHandle client) {
      MojoHandle host_to_client;
      MojoHandle client_to_host;
      MojoCreateMessagePipe(nullptr, &host_to_client, &client_to_host);
      WriteMessageWithHandles(host, "x", &host_to_client, 1);
      WriteMessageWithHandles(client, "x", &client_to_host, 1);
      EXPECT_EQ("done", ReadMessage(client));
      WriteMessage(client, "bye");
    });
    EXPECT_EQ("done", ReadMessage(host));
    WriteMessage(host, "bye");
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(NoSpuriousEventsHost, DataPipeTest, parent) {
  const std::vector<uint8_t> kData(512, 'x');

  MojoHandle client;
  EXPECT_EQ("x", ReadMessageWithHandles(parent, &client, 1));

  for (size_t j = 0; j < kNoSpuriousEvents_NumIterations; ++j) {
    ScopedDataPipeProducerHandle producer;
    ScopedDataPipeConsumerHandle consumer;
    CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(2048, producer, consumer));

    MojoHandle ch = consumer.release().value();
    WriteMessageWithHandles(client, "hi", &ch, 1);

    for (size_t i = 0; i < 9; ++i) {
      WaitForSignals(producer.get().value(), MOJO_HANDLE_SIGNAL_WRITABLE);
      size_t bytes_written = 0;
      producer->WriteData(base::as_byte_span(kData), MOJO_WRITE_DATA_FLAG_NONE,
                          bytes_written);
    }
  }

  WriteMessage(parent, "done");
  EXPECT_EQ("bye", ReadMessage(parent));
  MojoClose(client);
  MojoClose(parent);
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(NoSpuriousEventsClient,
                                  DataPipeTest,
                                  parent) {
  base::test::TaskEnvironment task_environment;

  MojoHandle host;
  EXPECT_EQ("x", ReadMessageWithHandles(parent, &host, 1));

  size_t num_spurious_events = 0;
  for (size_t j = 0; j < kNoSpuriousEvents_NumIterations; ++j) {
    MojoHandle ch;
    ASSERT_EQ("hi", ReadMessageWithHandles(host, &ch, 1));
    ScopedDataPipeConsumerHandle consumer(DataPipeConsumerHandle{ch});

    SimpleWatcher watcher(FROM_HERE, SimpleWatcher::ArmingPolicy::MANUAL);
    base::RunLoop loop;
    watcher.Watch(consumer.get(), MOJO_HANDLE_SIGNAL_READABLE,
                  MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                  base::BindLambdaForTesting(
                      [&](MojoResult result, const HandleSignalsState& state) {
                        if (result == MOJO_RESULT_OK) {
                          if (!state.readable()) {
                            ++num_spurious_events;
                          }

                          // Drain everything.
                          base::span<const uint8_t> buffer;
                          consumer->BeginReadData(0, buffer);
                          consumer->EndReadData(buffer.size());
                          watcher.ArmOrNotify();
                        } else {
                          CHECK(state.never_readable());
                          loop.Quit();
                        }
                      }));
    watcher.ArmOrNotify();
    loop.Run();
  }

  EXPECT_EQ(0u, num_spurious_events);

  WriteMessage(parent, "done");
  EXPECT_EQ("bye", ReadMessage(parent));
  MojoClose(host);
  MojoClose(parent);
}

TEST_F(DataPipeTest, Multiprocess) {
  const uint32_t kTestDataSize =
      static_cast<uint32_t>(sizeof(kMultiprocessTestData));
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1,                                // |element_num_bytes|.
      kMultiprocessCapacity             // |capacity_num_bytes|.
  };
  ASSERT_EQ(MOJO_RESULT_OK, Create(&options));

  RunTestClient("MultiprocessClient", [&](MojoHandle server_mp) {
    // Send some data before serialising and sending the data pipe over.
    // This is the first write so we don't need to use WriteAllData.
    uint32_t num_bytes = kTestDataSize;
    ASSERT_EQ(MOJO_RESULT_OK,
              WriteData(kMultiprocessTestData, &num_bytes, true));
    ASSERT_EQ(kTestDataSize, num_bytes);

    // Send child process the data pipe.
    ASSERT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(MessagePipeHandle(server_mp), nullptr, 0,
                              &consumer_, 1, MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Send a bunch of data of varying sizes.
    uint8_t buffer[100];
    int seq = 0;
    for (int i = 0; i < kMultiprocessMaxIter; ++i) {
      for (uint32_t size = 1; size <= kMultiprocessCapacity; size++) {
        for (unsigned int j = 0; j < size; ++j)
          buffer[j] = seq + j;
        EXPECT_TRUE(WriteAllData(producer_, buffer, size));
        seq += size;
      }
    }

    // Write the test string in again.
    ASSERT_TRUE(WriteAllData(producer_, kMultiprocessTestData, kTestDataSize));

    // Swap ends.
    ASSERT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(MessagePipeHandle(server_mp), nullptr, 0,
                              &producer_, 1, MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Receive the consumer from the other side.
    producer_ = MOJO_HANDLE_INVALID;
    MojoHandleSignalsState hss = MojoHandleSignalsState();
    ASSERT_EQ(MOJO_RESULT_OK,
              WaitForSignals(server_mp, MOJO_HANDLE_SIGNAL_READABLE, &hss));
    ASSERT_EQ(MOJO_RESULT_OK,
              ReadEmptyMessageWithHandles(server_mp, &consumer_, 1));

    // Read the test string twice. Once for when we sent it, and once for the
    // other end sending it.
    for (int i = 0; i < 2; ++i) {
      EXPECT_TRUE(ReadAllData(consumer_, buffer, kTestDataSize, i == 1));
      EXPECT_EQ(0, memcmp(buffer, kMultiprocessTestData, kTestDataSize));
    }

    WriteMessage(server_mp, "quit");

    // Don't have to close the consumer here because it will be done for us.
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(MultiprocessClient, DataPipeTest, client_mp) {
  const uint32_t kTestDataSize =
      static_cast<uint32_t>(sizeof(kMultiprocessTestData));

  // Receive the data pipe from the other side.
  MojoHandle consumer = MOJO_HANDLE_INVALID;
  MojoHandleSignalsState hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(client_mp, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_EQ(MOJO_RESULT_OK,
            ReadEmptyMessageWithHandles(client_mp, &consumer, 1));

  // Read the initial string that was sent.
  int32_t buffer[100];
  EXPECT_TRUE(ReadAllData(consumer, buffer, kTestDataSize, false));
  EXPECT_EQ(0, memcmp(buffer, kMultiprocessTestData, kTestDataSize));

  // Receive the main data and check it is correct.
  int seq = 0;
  uint8_t expected_buffer[100];
  for (int i = 0; i < kMultiprocessMaxIter; ++i) {
    for (uint32_t size = 1; size <= kMultiprocessCapacity; ++size) {
      for (unsigned int j = 0; j < size; ++j)
        expected_buffer[j] = seq + j;
      EXPECT_TRUE(ReadAllData(consumer, buffer, size, false));
      EXPECT_EQ(0, memcmp(buffer, expected_buffer, size));

      seq += size;
    }
  }

  // Swap ends.
  ASSERT_EQ(MOJO_RESULT_OK,
            WriteMessageRaw(MessagePipeHandle(client_mp), nullptr, 0, &consumer,
                            1, MOJO_WRITE_MESSAGE_FLAG_NONE));

  // Receive the producer from the other side.
  MojoHandle producer = MOJO_HANDLE_INVALID;
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(client_mp, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_EQ(MOJO_RESULT_OK,
            ReadEmptyMessageWithHandles(client_mp, &producer, 1));

  // Write the test string one more time.
  EXPECT_TRUE(WriteAllData(producer, kMultiprocessTestData, kTestDataSize));

  // We swapped ends, so close the producer.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(producer));

  // Wait to receive a "quit" message before exiting.
  EXPECT_EQ("quit", ReadMessage(client_mp));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(client_mp));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(WriteAndCloseProducer, DataPipeTest, h) {
  MojoHandle p;
  std::string message = ReadMessageWithHandles(h, &p, 1);

  // Write some data to the producer and close it.
  uint32_t num_bytes = static_cast<uint32_t>(message.size());
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteData(p, message.data(), &num_bytes, nullptr));
  EXPECT_EQ(num_bytes, static_cast<uint32_t>(message.size()));

  // Close the producer before quitting.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(p));

  // Wait for a quit message.
  EXPECT_EQ("quit", ReadMessage(h));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReadAndCloseConsumer, DataPipeTest, h) {
  MojoHandle c;
  std::string expected_message = ReadMessageWithHandles(h, &c, 1);

  // Wait for the consumer to become readable.
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(c, MOJO_HANDLE_SIGNAL_READABLE));

  // Drain the consumer and expect to find the given message.
  uint32_t num_bytes = static_cast<uint32_t>(expected_message.size());
  std::vector<char> bytes(expected_message.size());
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadData(c, nullptr, bytes.data(), &num_bytes));
  EXPECT_EQ(num_bytes, static_cast<uint32_t>(bytes.size()));

  std::string message(bytes.data(), bytes.size());
  EXPECT_EQ(expected_message, message);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));

  // Wait for a quit message.
  EXPECT_EQ("quit", ReadMessage(h));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(DataPipeTest, SendConsumerAndCloseProducer) {
  // Create a new data pipe.
  MojoHandle p, c;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateDataPipe(nullptr, &p, &c));

  RunTestClient("WriteAndCloseProducer", [&](MojoHandle producer_client) {
    RunTestClient("ReadAndCloseConsumer", [&](MojoHandle consumer_client) {
      const std::string kMessage = "Hello, world!";
      WriteMessageWithHandles(producer_client, kMessage, &p, 1);
      WriteMessageWithHandles(consumer_client, kMessage, &c, 1);

      WriteMessage(consumer_client, "quit");
    });

    WriteMessage(producer_client, "quit");
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CreateAndWrite, DataPipeTest, h) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1,                                // |element_num_bytes|.
      kMultiprocessCapacity             // |capacity_num_bytes|.
  };

  MojoHandle p, c;
  ASSERT_EQ(MOJO_RESULT_OK, MojoCreateDataPipe(&options, &p, &c));

  const std::string kMessage = "Hello, world!";
  WriteMessageWithHandles(h, kMessage, &c, 1);

  // Write some data to the producer and close it.
  uint32_t num_bytes = static_cast<uint32_t>(kMessage.size());
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteData(p, kMessage.data(), &num_bytes, nullptr));
  EXPECT_EQ(num_bytes, static_cast<uint32_t>(kMessage.size()));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(p));

  // Wait for a quit message.
  EXPECT_EQ("quit", ReadMessage(h));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(DataPipeTest, CreateInChild) {
  RunTestClient("CreateAndWrite", [&](MojoHandle child) {
    MojoHandle c;
    std::string expected_message = ReadMessageWithHandles(child, &c, 1);

    // Wait for the consumer to become readable.
    EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(c, MOJO_HANDLE_SIGNAL_READABLE));

    // Drain the consumer and expect to find the given message.
    uint32_t num_bytes = static_cast<uint32_t>(expected_message.size());
    std::vector<char> bytes(expected_message.size());
    EXPECT_EQ(MOJO_RESULT_OK,
              MojoReadData(c, nullptr, bytes.data(), &num_bytes));
    EXPECT_EQ(num_bytes, static_cast<uint32_t>(bytes.size()));

    std::string message(bytes.data(), bytes.size());
    EXPECT_EQ(expected_message, message);

    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
    WriteMessage(child, "quit");
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(DataPipeStatusChangeInTransitClient,
                                  DataPipeTest,
                                  parent) {
  // This test verifies that peer closure is detectable through various
  // mechanisms when it races with handle transfer.

  MojoHandle handles[6];
  EXPECT_EQ("o_O", ReadMessageWithHandles(parent, handles, 6));
  MojoHandle* producers = &handles[0];
  MojoHandle* consumers = &handles[3];

  // Wait on producer 0
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(producers[0], MOJO_HANDLE_SIGNAL_PEER_CLOSED));

  // Wait on consumer 0
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(consumers[0], MOJO_HANDLE_SIGNAL_PEER_CLOSED));

  base::test::SingleThreadTaskEnvironment task_environment;

  // Wait on producer 1 and consumer 1 using SimpleWatchers.
  {
    base::RunLoop run_loop;
    int count = 0;
    auto callback = base::BindRepeating(
        [](base::RunLoop* loop, int* count, MojoResult result) {
          EXPECT_EQ(MOJO_RESULT_OK, result);
          if (++*count == 2)
            loop->Quit();
        },
        &run_loop, &count);
    SimpleWatcher producer_watcher(
        FROM_HERE, SimpleWatcher::ArmingPolicy::AUTOMATIC,
        base::SequencedTaskRunner::GetCurrentDefault());
    SimpleWatcher consumer_watcher(
        FROM_HERE, SimpleWatcher::ArmingPolicy::AUTOMATIC,
        base::SequencedTaskRunner::GetCurrentDefault());
    producer_watcher.Watch(Handle(producers[1]), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                           callback);
    consumer_watcher.Watch(Handle(consumers[1]), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                           callback);
    run_loop.Run();
    EXPECT_EQ(2, count);
  }

  // Wait on producer 2 by polling with MojoWriteData.
  MojoResult result;
  do {
    uint32_t num_bytes = 0;
    result = MojoWriteData(producers[2], nullptr, &num_bytes, nullptr);
  } while (result == MOJO_RESULT_OK);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);

  // Wait on consumer 2 by polling with MojoReadData.
  do {
    char byte;
    uint32_t num_bytes = 1;
    result = MojoReadData(consumers[2], nullptr, &byte, &num_bytes);
  } while (result == MOJO_RESULT_SHOULD_WAIT);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);

  for (size_t i = 0; i < 6; ++i)
    CloseHandle(handles[i]);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(parent));
}

TEST_F(DataPipeTest, StatusChangeInTransit) {
  MojoHandle producers[6];
  MojoHandle consumers[6];
  for (size_t i = 0; i < 6; ++i)
    CreateDataPipe(&producers[i], &consumers[i], 1);

  RunTestClient("DataPipeStatusChangeInTransitClient", [&](MojoHandle child) {
    MojoHandle handles[] = {producers[0], producers[1], producers[2],
                            consumers[3], consumers[4], consumers[5]};

    // Send 3 producers and 3 consumers, and let their transfer race with their
    // peers' closure.
    WriteMessageWithHandles(child, "o_O", handles, 6);

    for (size_t i = 0; i < 3; ++i)
      CloseHandle(consumers[i]);
    for (size_t i = 3; i < 6; ++i)
      CloseHandle(producers[i]);
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CreateOversizedChild, DataPipeTest, h) {
  const MojoCreateDataPipeOptions options = {
      kSizeOfOptions,                   // |struct_size|.
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,  // |flags|.
      1,                                // |element_num_bytes|.
      kOversizedCapacity                // |capacity_num_bytes|.
  };

  MojoHandle p, c;
  ASSERT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            MojoCreateDataPipe(&options, &p, &c));
  WriteMessage(h, "success");

  // Wait for a quit message.
  EXPECT_EQ("quit", ReadMessage(h));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(DataPipeTest, CreateOversizedInChild) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Data pipes do not allocate dedicated capacity when "
                 << "MojoIpcz is enabled, so capacity limits are not enforced "
                 << "and therefore cannot be tested.";
  }

  RunTestClient("CreateOversizedChild", [&](MojoHandle child) {
    // Wait for the child to finish the test.
    std::string expected_message = ReadMessage(child);
    EXPECT_EQ("success", expected_message);

    WriteMessage(child, "quit");
  });
}

// Helper to fill a data pipe with data up to a given total size, using chunked
// two-phase writes. Automatically waits when the pipe is full and resumes as
// capacity allows.
class TestDataProducer {
 public:
  // Push `total_size` bytes through `producer`, via chunks that are at most
  // `chunk_size` bytes. Once all data has been pushed, `quit_closure` is run.
  explicit TestDataProducer(ScopedDataPipeProducerHandle producer,
                            base::OnceClosure quit_closure,
                            uint32_t total_size,
                            uint32_t chunk_size)
      : producer_(std::move(producer)),
        quit_closure_(std::move(quit_closure)),
        chunk_size_(chunk_size),
        bytes_remaining_(total_size) {
    watcher_.Watch(producer_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                   base::BindRepeating(&TestDataProducer::ProduceMore,
                                       base::Unretained(this)));
    ProduceMore(MOJO_RESULT_OK);
  }

  ~TestDataProducer() = default;

 private:
  void ProduceMore(MojoResult) {
    void* data;
    uint32_t num_bytes = std::min(chunk_size_, bytes_remaining_);
    if (num_bytes == 0) {
      producer_.reset();
      std::move(quit_closure_).Run();
      return;
    }
    MojoResult rv =
        MojoBeginWriteData(producer_->value(), nullptr, &data, &num_bytes);
    if (rv == MOJO_RESULT_SHOULD_WAIT) {
      watcher_.ArmOrNotify();
      return;
    }
    CHECK_EQ(rv, MOJO_RESULT_OK);

    num_bytes = std::min(num_bytes, bytes_remaining_);
    memset(data, 42, num_bytes);
    CHECK_EQ(MOJO_RESULT_OK,
             MojoEndWriteData(producer_->value(), num_bytes, nullptr));
    bytes_remaining_ -= num_bytes;

    ProduceMore(MOJO_RESULT_OK);
  }

  ScopedDataPipeProducerHandle producer_;
  SimpleWatcher watcher_{FROM_HERE, SimpleWatcher::ArmingPolicy::MANUAL};
  base::OnceClosure quit_closure_;
  const uint32_t chunk_size_;
  uint32_t bytes_remaining_;
};

// Drains all data from a data pipe consumer endpoint. This combines read
// operations and trap usage (via watcher) in a way that is likely to trigger a
// regression path in the data pipe implementation if a certain type of bug is
// present. See comments in the implementation below.
class TestDataDrain {
 public:
  explicit TestDataDrain(ScopedDataPipeConsumerHandle consumer,
                         base::OnceClosure quit_closure)
      : consumer_(std::move(consumer)), quit_closure_(std::move(quit_closure)) {
    watcher_.Watch(
        consumer_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&TestDataDrain::Notify, base::Unretained(this)));
    Update();
  }

  ~TestDataDrain() = default;

  size_t num_bytes_drained() const { return num_bytes_drained_; }

 private:
  void Notify(MojoResult) {
    auto state = consumer_->QuerySignalsState();
    if (state.never_readable()) {
      consumer_.reset();
      std::move(quit_closure_).Run();
      return;
    } else if (!state.readable()) {
      watcher_.ArmOrNotify();
      return;
    }

    Update();
  }

  void Update() {
    for (;;) {
      // Ensure the watcher is armed before we start trying to read, so there's
      // a chance of its disarmament racing on the IO thread with the reads
      // below.
      watcher_.ArmOrNotify();

      // We do multiple redundant read attempts per cycle to increase likelihood
      // of flushing data pipe status on this thread while the IO thread is
      // processing a trap event.
      constexpr size_t kNumReadAttempts = 10;
      const void* data;
      uint32_t num_bytes;
      MojoResult result;
      for (size_t i = 0; i < kNumReadAttempts; ++i) {
        result =
            MojoBeginReadData(consumer_->value(), nullptr, &data, &num_bytes);
        if (result == MOJO_RESULT_OK) {
          const uint32_t num_bytes_read =
              (i == kNumReadAttempts - 1) ? num_bytes : 0;

          // Quick consistency check. We don't want to spend too much time
          // testing every byte.
          const uint8_t* bytes = static_cast<const uint8_t*>(data);
          EXPECT_EQ(42u, bytes[0]);
          EXPECT_EQ(42u, bytes[num_bytes - 1]);

          result = MojoEndReadData(consumer_->value(), num_bytes_read, nullptr);
        }
      }

      switch (result) {
        case MOJO_RESULT_SHOULD_WAIT:
          // If the bug we're testing for is present, this arming attempt can
          // be incorrectly ignored while nothing is actually watching the pipe,
          // resulting in no further Update() calls and an effectively stalled
          // consumer.
          watcher_.ArmOrNotify();
          return;
        case MOJO_RESULT_OK:
          num_bytes_drained_ += num_bytes;
          break;
        case MOJO_RESULT_FAILED_PRECONDITION:
          Notify(MOJO_RESULT_FAILED_PRECONDITION);
          return;
      }
    }
  }

  ScopedDataPipeConsumerHandle consumer_;
  SimpleWatcher watcher_{FROM_HERE, SimpleWatcher::ArmingPolicy::MANUAL};
  base::OnceClosure quit_closure_;
  size_t num_bytes_drained_ = 0;
  base::WeakPtrFactory<TestDataDrain> weak_ptr_factory_{this};
};

constexpr uint32_t kStressTestDataSize = 512 * 1024 * 1024;

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(StressTestRacyTrapsClient, DataPipeTest, h) {
  base::test::TaskEnvironment task_environment;

  constexpr uint32_t kChunkSize = 4096;
  MojoHandle p;
  EXPECT_EQ("sup", ReadMessageWithHandles(h, &p, 1));
  base::RunLoop loop;
  TestDataProducer producer(
      ScopedDataPipeProducerHandle{DataPipeProducerHandle{p}},
      loop.QuitClosure(), kStressTestDataSize, kChunkSize);
  loop.Run();

  WriteMessage(h, "bye");
  EXPECT_EQ("bye", ReadMessage(h));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

// Temporarily disabled during experimentation with suppression of this fix for
// metrics collection only. Re-enable once the experiment is done.
// See https://crbug.com/41494387.
TEST_F(DataPipeTest, DISABLED_StressTestRacyTraps) {
  // Regression test for https://crbug.com/1468933. This bug was caused by a
  // race between trap arming and internal data pipe flushes which could result
  // in a data pipe trap appearing to be armed (and thus never re-arming) while
  // having no internal ipcz portal trap registered. This test is designed to
  // trigger the relevant code paths and it should hang flakily if such a bug is
  // present.

  base::test::TaskEnvironment task_environment;

  const MojoCreateDataPipeOptions options = {
      sizeof(options),
      MOJO_CREATE_DATA_PIPE_FLAG_NONE,
      1,
      128 * 1024,
  };

  MojoHandle p, c;
  ASSERT_EQ(MOJO_RESULT_OK, MojoCreateDataPipe(&options, &p, &c));

  RunTestClient("StressTestRacyTrapsClient", [&](MojoHandle child) {
    WriteMessageWithHandles(child, "sup", &p, 1);

    base::RunLoop loop;
    TestDataDrain drain(ScopedDataPipeConsumerHandle{DataPipeConsumerHandle{c}},
                        loop.QuitClosure());
    loop.Run();

    EXPECT_EQ(kStressTestDataSize, drain.num_bytes_drained());
    EXPECT_EQ("bye", ReadMessage(child));
    WriteMessage(child, "bye");
  });
}

#endif  // BUILDFLAG(USE_BLINK)

}  // namespace
}  // namespace core
}  // namespace mojo

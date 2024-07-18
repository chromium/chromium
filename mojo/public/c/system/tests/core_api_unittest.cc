// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file tests the C API.

#include "mojo/public/c/system/core.h"

#include <stdint.h>
#include <string.h>

#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const MojoHandleSignals kSignalReadadableWritable =
    MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE;

const MojoHandleSignals kSignalAll =
    MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
    MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE |
    MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED;

TEST(CoreAPITest, GetTimeTicksNow) {
  const MojoTimeTicks start = MojoGetTimeTicksNow();
  EXPECT_NE(static_cast<MojoTimeTicks>(0), start)
      << "MojoGetTimeTicksNow should return nonzero value";
}

// The only handle that's guaranteed to be invalid is |MOJO_HANDLE_INVALID|.
// Tests that everything that takes a handle properly recognizes it.
TEST(CoreAPITest, InvalidHandle) {
  MojoHandle h0, h1;
  char buffer[10] = {0};
  uint32_t buffer_size;
  void* write_pointer;
  const void* read_pointer;

  // Close:
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(MOJO_HANDLE_INVALID));

  // Message pipe:
  h0 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoWriteMessage(h0, MOJO_MESSAGE_HANDLE_INVALID, nullptr));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoReadMessage(h0, nullptr, nullptr));

  // Data pipe:
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoWriteData(h0, buffer, &buffer_size, nullptr));
  write_pointer = nullptr;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoBeginWriteData(h0, nullptr, &write_pointer, &buffer_size));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoEndWriteData(h0, 1, nullptr));
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoReadData(h0, nullptr, buffer, &buffer_size));
  read_pointer = nullptr;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoBeginReadData(h0, nullptr, &read_pointer, &buffer_size));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoEndReadData(h0, 1, nullptr));

  // Shared buffer:
  h1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoDuplicateBufferHandle(h0, nullptr, &h1));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoMapBuffer(h0, 0, 1, nullptr, &write_pointer));
}

TEST(CoreAPITest, BasicMessagePipe) {
  MojoHandle h0, h1;
  MojoHandleSignals sig;

  h0 = MOJO_HANDLE_INVALID;
  h1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &h0, &h1));
  EXPECT_NE(h0, MOJO_HANDLE_INVALID);
  EXPECT_NE(h1, MOJO_HANDLE_INVALID);

  // Shouldn't be readable, we haven't written anything. Should be writable.
  MojoHandleSignalsState state;
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(h0, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(kSignalAll, state.satisfiable_signals);

  // Try to read.
  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT, MojoReadMessage(h0, nullptr, &message));

  // Write to |h1|.
  const uintptr_t kTestMessageContext = 1234;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoSetMessageContext(message, kTestMessageContext,
                                                  nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoWriteMessage(h1, message, nullptr));

  // |h0| should be readable.
  size_t result_index = 1;
  MojoHandleSignalsState states[1];
  sig = MOJO_HANDLE_SIGNAL_READABLE;
  Handle handle0(h0);
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo::WaitMany(&handle0, &sig, 1, &result_index, states));

  EXPECT_EQ(0u, result_index);
  EXPECT_EQ(kSignalReadadableWritable, states[0].satisfied_signals);
  EXPECT_EQ(kSignalAll, states[0].satisfiable_signals);

  // Read from |h0|.
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(h0, nullptr, &message));
  uintptr_t context;
  EXPECT_EQ(MOJO_RESULT_OK, MojoGetMessageContext(message, nullptr, &context));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetMessageContext(message, 0, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));
  EXPECT_EQ(kTestMessageContext, context);

  // |h0| should no longer be readable.
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(h0, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(kSignalAll, state.satisfiable_signals);

  // Close |h0|.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h0));

  EXPECT_EQ(MOJO_RESULT_OK, mojo::Wait(mojo::Handle(h1),
                                       MOJO_HANDLE_SIGNAL_PEER_CLOSED, &state));

  // |h1| should no longer be readable or writable.
  EXPECT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      mojo::Wait(mojo::Handle(h1),
                 MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
                 &state));

  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfied_signals);
  EXPECT_FALSE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_FALSE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h1));
}

TEST(CoreAPITest, BasicDataPipe) {
  MojoHandle hp, hc;
  MojoHandleSignals sig;
  char buffer[20] = {0};
  uint32_t buffer_size;
  void* write_pointer;
  const void* read_pointer;

  hp = MOJO_HANDLE_INVALID;
  hc = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateDataPipe(nullptr, &hp, &hc));
  EXPECT_NE(hp, MOJO_HANDLE_INVALID);
  EXPECT_NE(hc, MOJO_HANDLE_INVALID);

  // The consumer |hc| shouldn't be readable.
  MojoHandleSignalsState state;
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(hc, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_NONE, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            state.satisfiable_signals);

  // The producer |hp| should be writable.
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(hp, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            state.satisfiable_signals);

  // Try to read from |hc|.
  buffer_size = static_cast<uint32_t>(sizeof(buffer));
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            MojoReadData(hc, nullptr, buffer, &buffer_size));

  // Try to begin a two-phase read from |hc|.
  read_pointer = nullptr;
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            MojoBeginReadData(hc, nullptr, &read_pointer, &buffer_size));

  // Write to |hp|.
  static const char kHello[] = "hello ";
  // Don't include terminating null.
  buffer_size = static_cast<uint32_t>(strlen(kHello));
  EXPECT_EQ(MOJO_RESULT_OK, MojoWriteData(hp, kHello, &buffer_size, nullptr));

  // |hc| should be(come) readable.
  size_t result_index = 1;
  MojoHandleSignalsState states[1];
  sig = MOJO_HANDLE_SIGNAL_READABLE;
  Handle consumer_handle(hc);
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo::WaitMany(&consumer_handle, &sig, 1, &result_index, states));

  EXPECT_EQ(0u, result_index);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            states[0].satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            states[0].satisfiable_signals);

  // Do a two-phase write to |hp|.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoBeginWriteData(hp, nullptr, &write_pointer, &buffer_size));
  static const char kWorld[] = "world";
  ASSERT_GE(buffer_size, sizeof(kWorld));
  // Include the terminating null.
  memcpy(write_pointer, kWorld, sizeof(kWorld));
  EXPECT_EQ(
      MOJO_RESULT_OK,
      MojoEndWriteData(hp, static_cast<uint32_t>(sizeof(kWorld)), nullptr));

  // Read one character from |hc|.
  memset(buffer, 0, sizeof(buffer));
  buffer_size = 1;
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadData(hc, nullptr, buffer, &buffer_size));

  // Close |hp|.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(hp));

  // |hc| should still be readable.
  EXPECT_EQ(MOJO_RESULT_OK, mojo::Wait(mojo::Handle(hc),
                                       MOJO_HANDLE_SIGNAL_PEER_CLOSED, &state));

  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // Do two-phase reads from |hc| until drained.
  size_t read_offset = 1;
  for (;;) {
    read_pointer = nullptr;
    const MojoResult result =
        MojoBeginReadData(hc, nullptr, &read_pointer, &buffer_size);
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      break;
    }
    ASSERT_EQ(result, MOJO_RESULT_OK);
    ASSERT_LE(buffer_size, sizeof(buffer) - 1);
    memcpy(&buffer[read_offset], read_pointer, buffer_size);
    EXPECT_EQ(MOJO_RESULT_OK, MojoEndReadData(hc, buffer_size, nullptr));
    read_offset += buffer_size;
  }
  EXPECT_STREQ("hello world", buffer);

  // |hc| should no longer be readable.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mojo::Wait(mojo::Handle(hc), MOJO_HANDLE_SIGNAL_READABLE, &state));

  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfiable_signals);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(hc));

  // TODO(vtl): Test the other way around -- closing the consumer should make
  // the producer never-writable?
}

TEST(CoreAPITest, BasicSharedBuffer) {
  MojoSharedBufferInfo buffer_info;
  buffer_info.struct_size = sizeof(buffer_info);
  MojoHandle h0 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoGetBufferInfo(h0, nullptr, &buffer_info));

  // Create a shared buffer (|h0|).
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateSharedBuffer(100, nullptr, &h0));
  EXPECT_NE(h0, MOJO_HANDLE_INVALID);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoGetBufferInfo(h0, nullptr, nullptr));
  ASSERT_EQ(MOJO_RESULT_OK, MojoGetBufferInfo(h0, nullptr, &buffer_info));
  EXPECT_GE(buffer_info.size, 100U);

  // Map everything.
  void* pointer = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK, MojoMapBuffer(h0, 0, 100, nullptr, &pointer));
  ASSERT_TRUE(pointer);
  static_cast<char*>(pointer)[50] = 'x';

  // Duplicate |h0| to |h1|.
  MojoHandle h1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoDuplicateBufferHandle(h0, nullptr, &h1));
  EXPECT_NE(h1, MOJO_HANDLE_INVALID);

  // Close |h0|.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h0));

  // The mapping should still be good.
  static_cast<char*>(pointer)[51] = 'y';

  // Unmap it.
  EXPECT_EQ(MOJO_RESULT_OK, MojoUnmapBuffer(pointer));

  // Map half of |h1|.
  pointer = nullptr;
  EXPECT_EQ(MOJO_RESULT_OK, MojoMapBuffer(h1, 50, 50, nullptr, &pointer));
  ASSERT_TRUE(pointer);

  // It should have what we wrote.
  EXPECT_EQ('x', static_cast<char*>(pointer)[0]);
  EXPECT_EQ('y', static_cast<char*>(pointer)[1]);

  // Unmap it.
  EXPECT_EQ(MOJO_RESULT_OK, MojoUnmapBuffer(pointer));

  buffer_info.size = 0;
  ASSERT_EQ(MOJO_RESULT_OK, MojoGetBufferInfo(h1, nullptr, &buffer_info));
  EXPECT_GE(buffer_info.size, 100U);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h1));
}

// Defined in core_unittest_pure_c.c.
extern "C" const char* MinimalCTest(void);

// This checks that things actually work in C (not C++).
TEST(CoreAPITest, MinimalCTest) {
  const char* failure = MinimalCTest();
  EXPECT_FALSE(failure) << failure;
}

// TODO(vtl): Add multi-threaded tests.

}  // namespace
}  // namespace mojo

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/core.h"

#include <stdint.h>

#include <limits>

#include "base/functional/bind.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "mojo/core/core_test_base.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/system/wait.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace mojo {
namespace core {
namespace {

const MojoHandleSignalsState kEmptyMojoHandleSignalsState = {0u, 0u};
const MojoHandleSignalsState kFullMojoHandleSignalsState = {~0u, ~0u};
const MojoHandleSignals kAllSignals =
    MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
    MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE |
    MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED;

using CoreTest = test::CoreTestBase;

TEST_F(CoreTest, GetTimeTicksNow) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Not relevant when MojoIpcz is enabled.";
  }

  const MojoTimeTicks start = core()->GetTimeTicksNow();
  ASSERT_NE(static_cast<MojoTimeTicks>(0), start)
      << "GetTimeTicksNow should return nonzero value";
  base::PlatformThread::Sleep(base::Milliseconds(15));
  const MojoTimeTicks finish = core()->GetTimeTicksNow();
  // Allow for some fuzz in sleep.
  ASSERT_GE((finish - start), static_cast<MojoTimeTicks>(8000))
      << "Sleeping should result in increasing time ticks";
}

TEST_F(CoreTest, Basic) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Not relevant when MojoIpcz is enabled.";
  }

  MockHandleInfo info;

  ASSERT_EQ(0u, info.GetCtorCallCount());
  MojoHandle h = CreateMockHandle(&info);
  ASSERT_EQ(1u, info.GetCtorCallCount());
  ASSERT_NE(h, MOJO_HANDLE_INVALID);

  ASSERT_EQ(0u, info.GetWriteMessageCallCount());
  MojoMessageHandle message;
  ASSERT_EQ(MOJO_RESULT_OK, core()->CreateMessage(nullptr, &message));
  ASSERT_EQ(MOJO_RESULT_OK, core()->WriteMessage(h, message, nullptr));
  ASSERT_EQ(1u, info.GetWriteMessageCallCount());

  ASSERT_EQ(0u, info.GetReadMessageCallCount());
  ASSERT_EQ(MOJO_RESULT_OK, core()->ReadMessage(h, nullptr, &message));
  ASSERT_EQ(1u, info.GetReadMessageCallCount());
  ASSERT_EQ(MOJO_RESULT_OK, core()->ReadMessage(h, nullptr, &message));
  ASSERT_EQ(2u, info.GetReadMessageCallCount());

  ASSERT_EQ(0u, info.GetWriteDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->WriteData(h, nullptr, nullptr, nullptr));
  ASSERT_EQ(1u, info.GetWriteDataCallCount());

  ASSERT_EQ(0u, info.GetBeginWriteDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->BeginWriteData(h, nullptr, nullptr, nullptr));
  ASSERT_EQ(1u, info.GetBeginWriteDataCallCount());

  ASSERT_EQ(0u, info.GetEndWriteDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED, core()->EndWriteData(h, 0, nullptr));
  ASSERT_EQ(1u, info.GetEndWriteDataCallCount());

  ASSERT_EQ(0u, info.GetReadDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->ReadData(h, nullptr, nullptr, nullptr));
  ASSERT_EQ(1u, info.GetReadDataCallCount());

  ASSERT_EQ(0u, info.GetBeginReadDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED,
            core()->BeginReadData(h, nullptr, nullptr, nullptr));
  ASSERT_EQ(1u, info.GetBeginReadDataCallCount());

  ASSERT_EQ(0u, info.GetEndReadDataCallCount());
  ASSERT_EQ(MOJO_RESULT_UNIMPLEMENTED, core()->EndReadData(h, 0, nullptr));
  ASSERT_EQ(1u, info.GetEndReadDataCallCount());

  ASSERT_EQ(0u, info.GetDtorCallCount());
  ASSERT_EQ(0u, info.GetCloseCallCount());
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h));
  ASSERT_EQ(1u, info.GetCloseCallCount());
  ASSERT_EQ(1u, info.GetDtorCallCount());
}

TEST_F(CoreTest, InvalidArguments) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Not relevant when MojoIpcz is enabled.";
  }

  // |Close()|:
  {
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(MOJO_HANDLE_INVALID));
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(10));
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(1000000000));

    // Test a double-close.
    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);
    ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h));
    ASSERT_EQ(1u, info.GetCloseCallCount());
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, core()->Close(h));
    ASSERT_EQ(1u, info.GetCloseCallCount());
  }

  // |CreateMessagePipe()|: Nothing to check (apart from things that cause
  // death).

  // |WriteMessageNew()|:
  // Only check arguments checked by |Core|, namely |handle|, |handles|, and
  // |num_handles|.
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->WriteMessage(MOJO_HANDLE_INVALID, 0, nullptr));

  // |ReadMessageNew()|:
  // Only check arguments checked by |Core|, namely |handle|, |handles|, and
  // |num_handles|.
  {
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->ReadMessage(MOJO_HANDLE_INVALID, nullptr, nullptr));

    MockHandleInfo info;
    MojoHandle h = CreateMockHandle(&info);
    ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              core()->ReadMessage(h, nullptr, nullptr));
    // Checked by |Core|, shouldn't go through to the dispatcher.
    ASSERT_EQ(0u, info.GetReadMessageCallCount());
    ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h));
  }
}

TEST_F(CoreTest, MessagePipe) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Not relevant when MojoIpcz is enabled.";
  }

  MojoHandle h[2];
  MojoHandleSignalsState hss[2];

  ASSERT_EQ(MOJO_RESULT_OK, core()->CreateMessagePipe(nullptr, &h[0], &h[1]));
  // Should get two distinct, valid handles.
  ASSERT_NE(h[0], MOJO_HANDLE_INVALID);
  ASSERT_NE(h[1], MOJO_HANDLE_INVALID);
  ASSERT_NE(h[0], h[1]);

  // Neither should be readable.
  hss[0] = kEmptyMojoHandleSignalsState;
  hss[1] = kEmptyMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(h[0], &hss[0]));
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(h[1], &hss[1]));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[0].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[0].satisfiable_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[1].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[1].satisfiable_signals);

  // Try to read anyway.
  MojoMessageHandle message;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT,
            core()->ReadMessage(h[0], nullptr, &message));

  // Write to |h[1]|.
  const uintptr_t kTestMessageContext = 123;
  ASSERT_EQ(MOJO_RESULT_OK, core()->CreateMessage(nullptr, &message));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->SetMessageContext(message, kTestMessageContext, nullptr,
                                      nullptr, nullptr));
  ASSERT_EQ(MOJO_RESULT_OK, core()->WriteMessage(h[1], message, nullptr));

  // Wait for |h[0]| to become readable.
  EXPECT_EQ(MOJO_RESULT_OK, mojo::Wait(mojo::Handle(h[0]),
                                       MOJO_HANDLE_SIGNAL_READABLE, &hss[0]));

  // Read from |h[0]|.
  ASSERT_EQ(MOJO_RESULT_OK, core()->ReadMessage(h[0], nullptr, &message));
  uintptr_t context;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->GetMessageContext(message, nullptr, &context));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->SetMessageContext(message, 0, nullptr, nullptr, nullptr));
  ASSERT_EQ(kTestMessageContext, context);
  ASSERT_EQ(MOJO_RESULT_OK, core()->DestroyMessage(message));

  // |h[0]| should no longer be readable.
  hss[0] = kEmptyMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(h[0], &hss[0]));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss[0].satisfied_signals);
  ASSERT_EQ(kAllSignals, hss[0].satisfiable_signals);

  // Write to |h[0]|.
  ASSERT_EQ(MOJO_RESULT_OK, core()->CreateMessage(nullptr, &message));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->SetMessageContext(message, kTestMessageContext, nullptr,
                                      nullptr, nullptr));
  ASSERT_EQ(MOJO_RESULT_OK, core()->WriteMessage(h[0], message, nullptr));

  // Close |h[0]|.
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h[0]));

  // Wait for |h[1]| to learn about the other end's closure.
  EXPECT_EQ(
      MOJO_RESULT_OK,
      mojo::Wait(mojo::Handle(h[1]), MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss[1]));

  // Check that |h[1]| is no longer writable (and will never be).
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss[1].satisfied_signals);
  EXPECT_FALSE(hss[1].satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);

  // Check that |h[1]| is still readable (for the moment).
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            hss[1].satisfied_signals);
  EXPECT_TRUE(hss[1].satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);

  // Discard a message from |h[1]|.
  ASSERT_EQ(MOJO_RESULT_OK, core()->ReadMessage(h[1], nullptr, &message));
  ASSERT_EQ(MOJO_RESULT_OK, core()->DestroyMessage(message));

  // |h[1]| is no longer readable (and will never be).
  hss[1] = kFullMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(h[1], &hss[1]));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss[1].satisfied_signals);
  EXPECT_FALSE(hss[1].satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);

  // Try writing to |h[1]|.
  ASSERT_EQ(MOJO_RESULT_OK, core()->CreateMessage(nullptr, &message));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->SetMessageContext(message, kTestMessageContext, nullptr,
                                      nullptr, nullptr));
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            core()->WriteMessage(h[1], message, nullptr));

  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h[1]));
}

// Tests passing a message pipe handle.
TEST_F(CoreTest, MessagePipeBasicLocalHandlePassing1) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Not relevant when MojoIpcz is enabled.";
  }

  MojoHandleSignalsState hss;
  MojoHandle h_passing[2];
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->CreateMessagePipe(nullptr, &h_passing[0], &h_passing[1]));

  // Make sure that |h_passing[]| work properly.
  const uintptr_t kTestMessageContext = 42;
  MojoMessageHandle message;
  ASSERT_EQ(MOJO_RESULT_OK, core()->CreateMessage(nullptr, &message));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->SetMessageContext(message, kTestMessageContext, nullptr,
                                      nullptr, nullptr));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteMessage(h_passing[0], message, nullptr));
  hss = kEmptyMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, mojo::Wait(mojo::Handle(h_passing[1]),
                                       MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  MojoMessageHandle message_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadMessage(h_passing[1], nullptr, &message_handle));
  uintptr_t context;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->GetMessageContext(message_handle, nullptr, &context));
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->SetMessageContext(message, 0, nullptr, nullptr, nullptr));
  ASSERT_EQ(kTestMessageContext, context);
  ASSERT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));

  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[0]));
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(h_passing[1]));
}

TEST_F(CoreTest, DataPipe) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Not relevant when MojoIpcz is enabled.";
  }

  MojoHandle ph, ch;  // p is for producer and c is for consumer.
  MojoHandleSignalsState hss;

  ASSERT_EQ(MOJO_RESULT_OK, core()->CreateDataPipe(nullptr, &ph, &ch));
  // Should get two distinct, valid handles.
  ASSERT_NE(ph, MOJO_HANDLE_INVALID);
  ASSERT_NE(ch, MOJO_HANDLE_INVALID);
  ASSERT_NE(ph, ch);

  // Producer should be never-readable, but already writable.
  hss = kEmptyMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(ph, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Consumer should be never-writable, and not yet readable.
  hss = kFullMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(ch, &hss));
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Write.
  signed char elements[2] = {'A', 'B'};
  uint32_t num_bytes = 2u;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->WriteData(ph, elements, &num_bytes, nullptr));
  ASSERT_EQ(2u, num_bytes);

  // Wait for the data to arrive to the consumer.
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo::Wait(mojo::Handle(ch), MOJO_HANDLE_SIGNAL_READABLE, &hss));

  // Consumer should now be readable.
  hss = kEmptyMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(ch, &hss));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
            hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // Peek one character.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = 1u;
  MojoReadDataOptions read_options;
  read_options.struct_size = sizeof(read_options);
  read_options.flags = MOJO_READ_DATA_FLAG_NONE | MOJO_READ_DATA_FLAG_PEEK;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch, &read_options, elements, &num_bytes));
  ASSERT_EQ('A', elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Read one character.
  elements[0] = -1;
  elements[1] = -1;
  num_bytes = 1u;
  read_options.flags = MOJO_READ_DATA_FLAG_NONE;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch, &read_options, elements, &num_bytes));
  ASSERT_EQ('A', elements[0]);
  ASSERT_EQ(-1, elements[1]);

  // Two-phase write.
  void* write_ptr = nullptr;
  num_bytes = 0u;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginWriteData(ph, nullptr, &write_ptr, &num_bytes));
  // We count on the default options providing a decent buffer size.
  ASSERT_GE(num_bytes, 3u);

  // Trying to do a normal write during a two-phase write should fail.
  elements[0] = 'X';
  num_bytes = 1u;
  ASSERT_EQ(MOJO_RESULT_BUSY,
            core()->WriteData(ph, elements, &num_bytes, nullptr));

  // Actually write the data, and complete it now.
  static_cast<char*>(write_ptr)[0] = 'C';
  static_cast<char*>(write_ptr)[1] = 'D';
  static_cast<char*>(write_ptr)[2] = 'E';
  ASSERT_EQ(MOJO_RESULT_OK, core()->EndWriteData(ph, 3u, nullptr));

  // Wait for the data to arrive to the consumer.
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::Wait(mojo::Handle(ch), MOJO_HANDLE_SIGNAL_READABLE, &hss));

  // Query how much data we have.
  num_bytes = 0;
  read_options.flags = MOJO_READ_DATA_FLAG_QUERY;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch, &read_options, nullptr, &num_bytes));
  ASSERT_GE(num_bytes, 1u);

  // Try to query with peek. Should fail.
  num_bytes = 0;
  read_options.flags = MOJO_READ_DATA_FLAG_QUERY | MOJO_READ_DATA_FLAG_PEEK;
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->ReadData(ch, &read_options, nullptr, &num_bytes));
  ASSERT_EQ(0u, num_bytes);

  // Try to discard ten characters, in all-or-none mode. Should fail.
  num_bytes = 10;
  read_options.flags =
      MOJO_READ_DATA_FLAG_DISCARD | MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  ASSERT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            core()->ReadData(ch, &read_options, nullptr, &num_bytes));

  // Try to discard two characters, in peek mode. Should fail.
  num_bytes = 2;
  read_options.flags = MOJO_READ_DATA_FLAG_DISCARD | MOJO_READ_DATA_FLAG_PEEK;
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            core()->ReadData(ch, &read_options, nullptr, &num_bytes));

  // Discard a character.
  num_bytes = 1;
  read_options.flags =
      MOJO_READ_DATA_FLAG_DISCARD | MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->ReadData(ch, &read_options, nullptr, &num_bytes));

  // Ensure the 3 bytes were read.
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::Wait(mojo::Handle(ch), MOJO_HANDLE_SIGNAL_READABLE, &hss));

  // Read the remaining three characters, in two-phase mode.
  const void* read_ptr = nullptr;
  num_bytes = 3;
  ASSERT_EQ(MOJO_RESULT_OK,
            core()->BeginReadData(ch, nullptr, &read_ptr, &num_bytes));
  // Note: Count on still being able to do the contiguous read here.
  ASSERT_EQ(3u, num_bytes);

  // Discarding right now should fail.
  num_bytes = 1;
  read_options.flags = MOJO_READ_DATA_FLAG_DISCARD;
  ASSERT_EQ(MOJO_RESULT_BUSY,
            core()->ReadData(ch, &read_options, nullptr, &num_bytes));

  // Actually check our data and end the two-phase read.
  ASSERT_EQ('C', static_cast<const char*>(read_ptr)[0]);
  ASSERT_EQ('D', static_cast<const char*>(read_ptr)[1]);
  ASSERT_EQ('E', static_cast<const char*>(read_ptr)[2]);
  ASSERT_EQ(MOJO_RESULT_OK, core()->EndReadData(ch, 3u, nullptr));

  // Consumer should now be no longer readable.
  hss = kFullMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(ch, &hss));
  EXPECT_EQ(0u, hss.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE,
            hss.satisfiable_signals);

  // TODO(vtl): More.

  // Close the producer.
  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(ph));

  // Wait for this to get to the consumer.
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo::Wait(mojo::Handle(ch), MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));

  // The consumer should now be never-readable.
  hss = kFullMojoHandleSignalsState;
  EXPECT_EQ(MOJO_RESULT_OK, core()->QueryHandleSignalsState(ch, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfiable_signals);

  ASSERT_EQ(MOJO_RESULT_OK, core()->Close(ch));
}

}  // namespace
}  // namespace core
}  // namespace mojo

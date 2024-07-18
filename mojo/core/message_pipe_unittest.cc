// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace core {
namespace {

const MojoHandleSignals kAllSignals =
    MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
    MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE |
    MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED;

static const char kHelloWorld[] = "hello world";

class MessagePipeTest : public test::MojoTestBase {
 public:
  MessagePipeTest() {
    CHECK_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(nullptr, &pipe0_, &pipe1_));
  }

  MessagePipeTest(const MessagePipeTest&) = delete;
  MessagePipeTest& operator=(const MessagePipeTest&) = delete;

  ~MessagePipeTest() override {
    if (pipe0_ != MOJO_HANDLE_INVALID)
      CHECK_EQ(MOJO_RESULT_OK, MojoClose(pipe0_));
    if (pipe1_ != MOJO_HANDLE_INVALID)
      CHECK_EQ(MOJO_RESULT_OK, MojoClose(pipe1_));
  }

  MojoResult WriteMessage(MojoHandle message_pipe_handle,
                          const void* bytes,
                          uint32_t num_bytes) {
    return mojo::WriteMessageRaw(MessagePipeHandle(message_pipe_handle), bytes,
                                 num_bytes, nullptr, 0,
                                 MOJO_WRITE_MESSAGE_FLAG_NONE);
  }

  MojoResult ReadMessage(MojoHandle message_pipe_handle,
                         void* bytes,
                         uint32_t* num_bytes,
                         bool may_discard = false) {
    MojoMessageHandle message_handle;
    MojoResult rv =
        MojoReadMessage(message_pipe_handle, nullptr, &message_handle);
    if (rv != MOJO_RESULT_OK)
      return rv;

    const uint32_t expected_num_bytes = *num_bytes;
    void* buffer;
    rv = MojoGetMessageData(message_handle, nullptr, &buffer, num_bytes,
                            nullptr, nullptr);

    if (rv == MOJO_RESULT_RESOURCE_EXHAUSTED) {
      CHECK(may_discard);
    } else if (*num_bytes) {
      CHECK_EQ(MOJO_RESULT_OK, rv);
      CHECK_GE(expected_num_bytes, *num_bytes);
      CHECK(bytes);
      memcpy(bytes, buffer, *num_bytes);
    }
    CHECK_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
    return rv;
  }

  MojoHandle pipe0_, pipe1_;
};

using FuseMessagePipeTest = test::MojoTestBase;

TEST_F(MessagePipeTest, WriteData) {
  ASSERT_EQ(MOJO_RESULT_OK,
            WriteMessage(pipe0_, kHelloWorld, sizeof(kHelloWorld)));
}

// Tests:
//  - only default flags
//  - reading messages from a port
//    - when there are no/one/two messages available for that port
//    - with buffer size 0 (and null buffer) -- should get size
//    - with too-small buffer -- should get size
//    - also verify that buffers aren't modified when/where they shouldn't be
//  - writing messages to a port
//    - in the obvious scenarios (as above)
//    - to a port that's been closed
//  - writing a message to a port, closing the other (would be the source) port,
//    and reading it
TEST_F(MessagePipeTest, Basic) {
  int32_t buffer[2];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t buffer_size;

  // Nothing to read yet on port 0.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadMessage(pipe0_, buffer, &buffer_size));
  ASSERT_EQ(kBufferSize, buffer_size);
  ASSERT_EQ(123, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Ditto for port 1.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadMessage(pipe1_, buffer, &buffer_size));

  // Write from port 1 (to port 0).
  buffer[0] = 789012345;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe1_, buffer, sizeof(buffer[0])));

  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe0_, MOJO_HANDLE_SIGNAL_READABLE, &state));

  // Read from port 0.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe0_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(789012345, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Read again from port 0 -- it should be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadMessage(pipe0_, buffer, &buffer_size));

  // Write two messages from port 0 (to port 1).
  buffer[0] = 123456789;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe0_, buffer, sizeof(buffer[0])));
  buffer[0] = 234567890;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe0_, buffer, sizeof(buffer[0])));

  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_READABLE, &state));

  // Read from port 1.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(123456789, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_READABLE, &state));

  // Read again from port 1.
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(234567890, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Read again from port 1 -- it should be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, ReadMessage(pipe1_, buffer, &buffer_size));

  // Write from port 0 (to port 1).
  buffer[0] = 345678901;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe0_, buffer, sizeof(buffer[0])));

  // Close port 0.
  MojoClose(pipe0_);
  pipe0_ = MOJO_HANDLE_INVALID;

  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &state));

  // Try to write from port 1 (to port 0).
  buffer[0] = 456789012;
  buffer[1] = 0;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WriteMessage(pipe1_, buffer, sizeof(buffer[0])));

  // Read from port 1; should still get message (even though port 0 was closed).
  buffer[0] = 123;
  buffer[1] = 456;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(static_cast<uint32_t>(sizeof(buffer[0])), buffer_size);
  ASSERT_EQ(345678901, buffer[0]);
  ASSERT_EQ(456, buffer[1]);

  // Read again from port 1 -- it should be empty (and port 0 is closed).
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            ReadMessage(pipe1_, buffer, &buffer_size));
}

TEST_F(MessagePipeTest, CloseWithQueuedIncomingMessages) {
  int32_t buffer[1];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t buffer_size;

  // Write some messages from port 1 (to port 0).
  for (int32_t i = 0; i < 5; i++) {
    buffer[0] = i;
    ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe1_, buffer, kBufferSize));
  }

  MojoHandleSignalsState state;
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe0_, MOJO_HANDLE_SIGNAL_READABLE, &state));

  // Port 0 shouldn't be empty.
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe0_, buffer, &buffer_size));
  ASSERT_EQ(kBufferSize, buffer_size);

  // Close port 0 first, which should have outstanding (incoming) messages.
  MojoClose(pipe0_);
  MojoClose(pipe1_);
  pipe0_ = pipe1_ = MOJO_HANDLE_INVALID;
}

TEST_F(MessagePipeTest, BasicWaiting) {
  MojoHandleSignalsState hss;

  int32_t buffer[1];
  const uint32_t kBufferSize = static_cast<uint32_t>(sizeof(buffer));
  uint32_t buffer_size;

  // Always writable (until the other port is closed). Not yet readable. Peer
  // not closed.
  hss = GetSignalsState(pipe0_);
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  hss = MojoHandleSignalsState();

  // Write from port 0 (to port 1), to make port 1 readable.
  buffer[0] = 123456789;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessage(pipe0_, buffer, kBufferSize));

  // Port 1 should already be readable now.
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);
  // ... and still writable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_WRITABLE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            hss.satisfied_signals);
  ASSERT_EQ(kAllSignals, hss.satisfiable_signals);

  // Close port 0.
  MojoClose(pipe0_);
  pipe0_ = MOJO_HANDLE_INVALID;

  // Port 1 should be signaled with peer closed.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_PEER_CLOSED, &hss));
  ASSERT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  ASSERT_TRUE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  // Port 1 should not be writable now or ever again.
  hss = MojoHandleSignalsState();

  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_WRITABLE, &hss));
  ASSERT_FALSE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  ASSERT_FALSE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);

  // But it should still be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_TRUE(hss.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_TRUE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);

  // Read from port 1.
  buffer[0] = 0;
  buffer_size = kBufferSize;
  ASSERT_EQ(MOJO_RESULT_OK, ReadMessage(pipe1_, buffer, &buffer_size));
  ASSERT_EQ(123456789, buffer[0]);

  // Now port 1 should no longer be readable.
  hss = MojoHandleSignalsState();
  ASSERT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            WaitForSignals(pipe1_, MOJO_HANDLE_SIGNAL_READABLE, &hss));
  ASSERT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, hss.satisfied_signals);
  ASSERT_FALSE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_FALSE(hss.satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
}

#if BUILDFLAG(USE_BLINK)

const size_t kPingPongHandlesPerIteration = 30;
const size_t kPingPongIterations = 500;

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(HandlePingPong, MessagePipeTest, h) {
  // Waits for a handle to become readable and writes it back to the sender.
  for (size_t i = 0; i < kPingPongIterations; i++) {
    MojoHandle handles[kPingPongHandlesPerIteration];
    ReadMessageWithHandles(h, handles, kPingPongHandlesPerIteration);
    WriteMessageWithHandles(h, "", handles, kPingPongHandlesPerIteration);
  }

  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE));
  char msg[4];
  uint32_t num_bytes = 4;
  EXPECT_EQ(MOJO_RESULT_OK, ReadMessage(h, msg, &num_bytes));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MessagePipeTest, DataPipeConsumerHandlePingPong) {
  MojoHandle p, c[kPingPongHandlesPerIteration];
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i) {
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateDataPipe(nullptr, &p, &c[i]));
    MojoClose(p);
  }

  RunTestClient("HandlePingPong", [&](MojoHandle h) {
    for (size_t i = 0; i < kPingPongIterations; i++) {
      WriteMessageWithHandles(h, "", c, kPingPongHandlesPerIteration);
      ReadMessageWithHandles(h, c, kPingPongHandlesPerIteration);
    }
    WriteMessage(h, "quit", 4);
  });
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i)
    MojoClose(c[i]);
}

TEST_F(MessagePipeTest, DataPipeProducerHandlePingPong) {
  MojoHandle p[kPingPongHandlesPerIteration], c;
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i) {
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateDataPipe(nullptr, &p[i], &c));
    MojoClose(c);
  }

  RunTestClient("HandlePingPong", [&](MojoHandle h) {
    for (size_t i = 0; i < kPingPongIterations; i++) {
      WriteMessageWithHandles(h, "", p, kPingPongHandlesPerIteration);
      ReadMessageWithHandles(h, p, kPingPongHandlesPerIteration);
    }
    WriteMessage(h, "quit", 4);
  });
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i)
    MojoClose(p[i]);
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/40257752): Test currently fails on iOS.
#define MAYBE_SharedBufferHandlePingPong DISABLED_SharedBufferHandlePingPong
#else
#define MAYBE_SharedBufferHandlePingPong SharedBufferHandlePingPong
#endif  // BUILDFLAG(IS_IOS)
TEST_F(MessagePipeTest, MAYBE_SharedBufferHandlePingPong) {
  MojoHandle buffers[kPingPongHandlesPerIteration];
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i)
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateSharedBuffer(1, nullptr, &buffers[i]));

  RunTestClient("HandlePingPong", [&](MojoHandle h) {
    for (size_t i = 0; i < kPingPongIterations; i++) {
      WriteMessageWithHandles(h, "", buffers, kPingPongHandlesPerIteration);
      ReadMessageWithHandles(h, buffers, kPingPongHandlesPerIteration);
    }
    WriteMessage(h, "quit", 4);
  });
  for (size_t i = 0; i < kPingPongHandlesPerIteration; ++i)
    MojoClose(buffers[i]);
}

#endif  // BUILDFLAG(USE_BLINK)

TEST_F(FuseMessagePipeTest, Basic) {
  // Test that we can fuse pipes and they still work.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(b, c, nullptr));

  const std::string kTestMessage1 = "Hello, world!";
  const std::string kTestMessage2 = "Goodbye, world!";

  WriteMessage(a, kTestMessage1);
  EXPECT_EQ(kTestMessage1, ReadMessage(d));

  WriteMessage(d, kTestMessage2);
  EXPECT_EQ(kTestMessage2, ReadMessage(a));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(FuseMessagePipeTest, FuseAfterPeerWrite) {
  // Test that messages written before fusion are eventually delivered.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  const std::string kTestMessage1 = "Hello, world!";
  const std::string kTestMessage2 = "Goodbye, world!";
  WriteMessage(a, kTestMessage1);
  WriteMessage(d, kTestMessage2);

  EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(b, c, nullptr));

  EXPECT_EQ(kTestMessage1, ReadMessage(d));
  EXPECT_EQ(kTestMessage2, ReadMessage(a));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(FuseMessagePipeTest, NoFuseAfterWrite) {
  // Test that a pipe endpoint which has been written to cannot be fused.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  WriteMessage(b, "shouldn't have done that!");
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoFuseMessagePipes(b, c, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(FuseMessagePipeTest, NoFuseSelf) {
  // Test that a pipe's own endpoints can't be fused together.

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoFuseMessagePipes(a, b, nullptr));
}

TEST_F(FuseMessagePipeTest, FuseInvalidArguments) {
  if (IsMojoIpczEnabled()) {
    // The MergePortals() API which supports MojoFuseMessagePipes() with
    // MojoIpcz enabled is simpler and has fewer side effects on failure. Making
    // this test pass would require additional complexity with no real value to
    // production code.
    GTEST_SKIP() << "Not relevant to MojoIpcz";
  }

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));

  // Can't fuse an invalid handle.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoFuseMessagePipes(b, c, nullptr));

  // Handle c should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(c));

  // Can't fuse a non-message pipe handle.
  MojoHandle e, f;
  CreateDataPipe(&e, &f, 16);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoFuseMessagePipes(e, d, nullptr));

  // Handles d and e should be closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(d));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(e));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(f));
}

TEST_F(FuseMessagePipeTest, FuseAfterPeerClosure) {
  // Test that peer closure prior to fusion can still be detected after fusion.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(b, c, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(d, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(FuseMessagePipeTest, FuseAfterPeerWriteAndClosure) {
  // Test that peer write and closure prior to fusion still results in the
  // both message arrival and awareness of peer closure.

  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  const std::string kTestMessage = "ayyy lmao";
  WriteMessage(a, kTestMessage);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));

  EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(b, c, nullptr));

  EXPECT_EQ(kTestMessage, ReadMessage(d));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(d, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(MessagePipeTest, ClosePipesStressTest) {
  // Stress test to exercise https://crbug.com/665869.
  const size_t kNumPipes = 100000;
  for (size_t i = 0; i < kNumPipes; ++i) {
    MojoHandle a, b;
    CreateMessagePipe(&a, &b);
    MojoClose(a);
    MojoClose(b);
  }
}

}  // namespace
}  // namespace core
}  // namespace mojo

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace core {
namespace {

using SignalsTest = test::MojoTestBase;

TEST_F(SignalsTest, QueryInvalidArguments) {
  MojoHandleSignalsState state = {0, 0};
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoQueryHandleSignalsState(MOJO_HANDLE_INVALID, &state));

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoQueryHandleSignalsState(a, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

TEST_F(SignalsTest, QueryMessagePipeSignals) {
  MojoHandleSignalsState state = {0, 0};

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE |
                MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
            state.satisfiable_signals);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE |
                MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
            state.satisfiable_signals);

  WriteMessage(a, "ok");
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE,
            state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE |
                MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
            state.satisfiable_signals);

  EXPECT_EQ("ok", ReadMessage(b));

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
                MOJO_HANDLE_SIGNAL_PEER_CLOSED |
                MOJO_HANDLE_SIGNAL_PEER_REMOTE |
                MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
            state.satisfiable_signals);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));

  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_PEER_CLOSED));

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfied_signals);
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
            state.satisfiable_signals);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

TEST_F(SignalsTest, LocalPeers) {
  MojoHandleSignalsState state = {0, 0};
  MojoHandle a, b, c, d;
  CreateMessagePipe(&a, &b);
  CreateMessagePipe(&c, &d);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &state));
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(c, &state));
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(d, &state));
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);

  // Verify that sending a local pipe over a local pipe doesn't change the
  // perceived locality of the peer.
  const char kMessage[] = "ayyy";
  WriteMessageWithHandles(a, kMessage, &c, 1);
  EXPECT_EQ(kMessage, ReadMessageWithHandles(b, &c, 1));

  WriteMessage(c, kMessage);
  EXPECT_EQ(kMessage, ReadMessage(d));

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(c, &state));
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(d, &state));
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);

  // Sanity check: a closed peer can never signal remoteness.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(d, &state));
  EXPECT_FALSE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

#if !BUILDFLAG(IS_IOS)

TEST_F(SignalsTest, RemotePeers) {
  if (IsMojoIpczEnabled()) {
    GTEST_SKIP() << "Peer remoteness tracking is not implemented by MojoIpcz.";
  }

  MojoHandleSignalsState state = {0, 0};
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &state));
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(a, MOJO_HANDLE_SIGNAL_PEER_REMOTE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED));

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &state));
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(b, MOJO_HANDLE_SIGNAL_PEER_REMOTE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED));

  RunTestClient("RemotePeersClient", [&](MojoHandle h) {
    // The bootstrap pipe should eventually signal remoteness.
    EXPECT_EQ(MOJO_RESULT_OK,
              WaitForSignals(h, MOJO_HANDLE_SIGNAL_PEER_REMOTE,
                             MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED));

    // And so should |a| after we send its peer.
    WriteMessageWithHandles(h, ":)", &b, 1);
    EXPECT_EQ(MOJO_RESULT_OK,
              WaitForSignals(a, MOJO_HANDLE_SIGNAL_PEER_REMOTE,
                             MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED));
    EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &state));
    EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);

    // And so should |c| after we fuse |d| to |a|.
    MojoHandle c, d;
    CreateMessagePipe(&c, &d);
    EXPECT_EQ(MOJO_RESULT_OK, MojoFuseMessagePipes(d, a, nullptr));
    EXPECT_EQ(MOJO_RESULT_OK,
              WaitForSignals(c, MOJO_HANDLE_SIGNAL_PEER_REMOTE,
                             MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED));
    EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(c, &state));
    EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);

    // We fused c-d to a-b, so we'll just sort of "rename" |c| back to |a| so
    // the system resembles the state it was in before we did that.
    a = c;

    WriteMessage(h, "OK!");

    // Read |b| back before joining the client.
    EXPECT_EQ("O_O", ReadMessageWithHandles(h, &b, 1));

    // Wait for |a| to see its peer as local again.
    EXPECT_EQ(MOJO_RESULT_OK,
              WaitForSignals(a, MOJO_HANDLE_SIGNAL_PEER_REMOTE,
                             MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED));
    EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &state));
    EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_REMOTE);
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(RemotePeersClient, SignalsTest, h) {
  // The bootstrap pipe should eventually signal remoteness.
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(h, MOJO_HANDLE_SIGNAL_PEER_REMOTE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED));

  MojoHandle b;
  EXPECT_EQ(":)", ReadMessageWithHandles(h, &b, 1));

  // And so should |b|.
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(b, MOJO_HANDLE_SIGNAL_PEER_REMOTE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED));

  // Wait for the test to signal that it's ready to read |b| back.
  EXPECT_EQ("OK!", ReadMessage(h));

  // Now send |b| back home.
  WriteMessageWithHandles(h, "O_O", &b, 1);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

#endif  // !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace core
}  // namespace mojo

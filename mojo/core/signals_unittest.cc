// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

}  // namespace
}  // namespace core
}  // namespace mojo

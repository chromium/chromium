// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/core_ipcz.h"

#include <cstring>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/public/c/system/thunks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::core {
namespace {

// Basic smoke tests for the Mojo Core API as implemented over ipcz.
class CoreIpczTest : public testing::Test {
 public:
  const MojoSystemThunks2& mojo() const { return *mojo_; }
  const IpczAPI& ipcz() const { return GetIpczAPI(); }
  IpczHandle node() const { return GetIpczNode(); }

  CoreIpczTest() { CHECK(InitializeIpczNodeForProcess({.is_broker = true})); }

  ~CoreIpczTest() override { DestroyIpczNodeForProcess(); }

  MojoMessageHandle CreateMessage(base::StringPiece contents,
                                  base::span<MojoHandle> handles = {}) {
    MojoMessageHandle message;
    EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessage(nullptr, &message));

    void* buffer;
    uint32_t buffer_size;
    MojoAppendMessageDataOptions options = {.struct_size = sizeof(options)};
    options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo().AppendMessageData(message, contents.size(), handles.data(),
                                       handles.size(), &options, &buffer,
                                       &buffer_size));
    EXPECT_GE(buffer_size, contents.size());
    memcpy(buffer, contents.data(), contents.size());
    return message;
  }

 private:
  const MojoSystemThunks2* const mojo_{GetMojoIpczImpl()};
};

TEST_F(CoreIpczTest, Close) {
  // With ipcz-based Mojo Core, Mojo handles are ipcz handles. So Mojo Close()
  // forwards to ipcz Close().

  IpczHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node(), IPCZ_NO_FLAGS, nullptr, &a, &b));

  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_FALSE(status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_TRUE(status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(b));
}

TEST_F(CoreIpczTest, BasicMessageUsage) {
  MojoHandle a, b;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &a, &b));

  constexpr base::StringPiece kMessage = "hellllooooo";
  MojoMessageHandle message = CreateMessage(kMessage, {&b, 1});

  void* buffer;
  uint32_t num_bytes;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            mojo().GetMessageData(message, nullptr, &buffer, &num_bytes,
                                  nullptr, nullptr));

  const MojoGetMessageDataOptions options = {
      .struct_size = sizeof(options),
      .flags = MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES,
  };
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().GetMessageData(message, &options, &buffer, &num_bytes,
                                  nullptr, nullptr));
  EXPECT_EQ(kMessage,
            base::StringPiece(static_cast<const char*>(buffer), num_bytes));

  b = MOJO_HANDLE_INVALID;
  uint32_t num_handles = 1;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().GetMessageData(message, nullptr, &buffer, &num_bytes, &b,
                                  &num_handles));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().DestroyMessage(message));

  MojoHandleSignalsState signals_state;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(a, &signals_state));
  EXPECT_EQ(0u,
            signals_state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(b));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(a, &signals_state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            signals_state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));
}

TEST_F(CoreIpczTest, MessageDestruction) {
  MojoHandle a, b;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &a, &b));

  constexpr base::StringPiece kMessage = "hellllooooo";
  MojoMessageHandle message = CreateMessage(kMessage, {&b, 1});

  // Destroying the message must also close the attached pipe.
  MojoHandleSignalsState signals_state;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(a, &signals_state));
  EXPECT_EQ(0u,
            signals_state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().DestroyMessage(message));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(a, &signals_state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            signals_state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));
}

TEST_F(CoreIpczTest, MessagePipes) {
  MojoHandle a, b;
  MojoHandle c, d;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &a, &b));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &c, &d));

  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT, mojo().ReadMessage(a, nullptr, &message));

  constexpr base::StringPiece kMessage = "bazongo";
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().WriteMessage(a, CreateMessage(kMessage), nullptr));

  MojoHandleSignalsState state;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(b, &state));
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().FuseMessagePipes(b, c, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(d, &state));
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().ReadMessage(d, nullptr, &message));
  EXPECT_NE(MOJO_MESSAGE_HANDLE_INVALID, message);

  void* buffer;
  uint32_t buffer_size;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().GetMessageData(message, nullptr, &buffer, &buffer_size,
                                  nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));

  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mojo().WriteMessage(d, message, nullptr));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mojo().ReadMessage(d, nullptr, &message));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(d));
}

}  // namespace
}  // namespace mojo::core

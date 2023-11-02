// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/handle_signals_state.h"

#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

using HandleSignalsStateTest = testing::Test;

TEST_F(HandleSignalsStateTest, SanityCheck) {
  // There's not much to test here. Just a quick sanity check to make sure the
  // code compiles and the helper methods do what they're supposed to do.

  HandleSignalsState empty_signals(MOJO_HANDLE_SIGNAL_NONE,
                                   MOJO_HANDLE_SIGNAL_NONE);
  EXPECT_FALSE(empty_signals.readable());
  EXPECT_FALSE(empty_signals.writable());
  EXPECT_FALSE(empty_signals.peer_closed());
  EXPECT_FALSE(empty_signals.peer_remote());
  EXPECT_TRUE(empty_signals.never_readable());
  EXPECT_TRUE(empty_signals.never_writable());
  EXPECT_TRUE(empty_signals.never_peer_closed());
  EXPECT_TRUE(empty_signals.never_peer_remote());

  HandleSignalsState all_signals(
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
          MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE,
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
          MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE);
  EXPECT_TRUE(all_signals.readable());
  EXPECT_TRUE(all_signals.writable());
  EXPECT_TRUE(all_signals.peer_closed());
  EXPECT_TRUE(all_signals.peer_remote());
  EXPECT_FALSE(all_signals.never_readable());
  EXPECT_FALSE(all_signals.never_writable());
  EXPECT_FALSE(all_signals.never_peer_closed());
  EXPECT_FALSE(all_signals.never_peer_remote());
}

}  // namespace
}  // namespace mojo

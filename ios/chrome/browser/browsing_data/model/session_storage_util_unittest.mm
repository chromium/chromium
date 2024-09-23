// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/sessions_storage_util.h"

#import "testing/platform_test.h"

// Test fixture for the session storage utility functions.
using SessionStorageUtilTest = PlatformTest;

// Tests `MarkSessionsForRemoval` adds to the existing list of sessions that are
// marked for removal.
TEST_F(SessionStorageUtilTest, MarkSessionRemovalTest) {
  EXPECT_EQ(0u, sessions_storage_util::GetDiscardedSessions().size());
  sessions_storage_util::MarkSessionsForRemoval(
      {"session_1", "session_2", "session_3"});
  EXPECT_EQ(3u, sessions_storage_util::GetDiscardedSessions().size());

  // Make sure that adding new sessions to the list doesn't overwrite existing
  // sessions (and also remove duplicates).
  sessions_storage_util::MarkSessionsForRemoval(
      {"session_3", "session_4", "session_5"});
  EXPECT_EQ(5u, sessions_storage_util::GetDiscardedSessions().size());

  sessions_storage_util::ResetDiscardedSessions();

  // Make sure that adding no sessions does work.
  sessions_storage_util::MarkSessionsForRemoval({});
  EXPECT_EQ(0u, sessions_storage_util::GetDiscardedSessions().size());
}

// Makes sure that `ResetDiscardedSessions` removes the list of sessions that
// are marked for removal.
TEST_F(SessionStorageUtilTest, ResetDiscardedSessionsTest) {
  EXPECT_EQ(0u, sessions_storage_util::GetDiscardedSessions().size());
  sessions_storage_util::MarkSessionsForRemoval(
      {"session_1", "session_2", "session_3"});
  EXPECT_EQ(3u, sessions_storage_util::GetDiscardedSessions().size());

  sessions_storage_util::ResetDiscardedSessions();
  EXPECT_EQ(0u, sessions_storage_util::GetDiscardedSessions().size());

  // Make sure that re-adding sessions work after reseting the sessions list.
  sessions_storage_util::MarkSessionsForRemoval({"session_4", "session_5"});
  EXPECT_EQ(2u, sessions_storage_util::GetDiscardedSessions().size());
  sessions_storage_util::ResetDiscardedSessions();
  EXPECT_EQ(0u, sessions_storage_util::GetDiscardedSessions().size());
}

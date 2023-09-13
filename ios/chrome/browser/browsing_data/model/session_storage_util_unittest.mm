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
  EXPECT_FALSE(sessions_storage_util::GetDiscardedSessions());
  NSArray<NSString*>* session_ids_1 =
      @[ @"session_1", @"session_2", @"session_3" ];
  sessions_storage_util::MarkSessionsForRemoval(session_ids_1);
  EXPECT_EQ(3u, sessions_storage_util::GetDiscardedSessions().count);
  // Make sure that adding new sessions to the list doesn't overwrite existing
  // sessions.
  NSArray<NSString*>* session_ids_2 = @[ @"session_4", @"session_5" ];
  sessions_storage_util::MarkSessionsForRemoval(session_ids_2);
  EXPECT_EQ(5u, sessions_storage_util::GetDiscardedSessions().count);
  sessions_storage_util::ResetDiscardedSessions();
}

// Makes sure that `ResetDiscardedSessions` removes the list of sessions that
// are marked for removal.
TEST_F(SessionStorageUtilTest, ResetDiscardedSessionsTest) {
  EXPECT_FALSE(sessions_storage_util::GetDiscardedSessions());
  NSArray<NSString*>* session_ids_1 =
      @[ @"session_1", @"session_2", @"session_3" ];
  sessions_storage_util::MarkSessionsForRemoval(session_ids_1);
  EXPECT_EQ(3u, sessions_storage_util::GetDiscardedSessions().count);
  sessions_storage_util::ResetDiscardedSessions();
  EXPECT_FALSE(sessions_storage_util::GetDiscardedSessions());
  // Make sure that re-adding sessions work after reseting the sessions list.
  NSArray<NSString*>* session_ids_2 = @[ @"session_4", @"session_5" ];
  sessions_storage_util::MarkSessionsForRemoval(session_ids_2);
  EXPECT_EQ(2u, sessions_storage_util::GetDiscardedSessions().count);
  sessions_storage_util::ResetDiscardedSessions();
  EXPECT_FALSE(sessions_storage_util::GetDiscardedSessions());
}

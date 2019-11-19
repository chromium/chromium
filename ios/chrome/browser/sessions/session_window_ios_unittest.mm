// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_window_ios.h"

#import <Foundation/Foundation.h>

#import "ios/web/public/session/crw_session_storage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

CRWSessionStorage* CreateSessionForTest(BOOL has_opener) {
  CRWSessionStorage* session = [[CRWSessionStorage alloc] init];
  session.hasOpener = has_opener;
  return session;
}

SessionWindowIOS* CreateSessionWindowForTest(NSUInteger selectedIndex) {
  return [[SessionWindowIOS alloc]
      initWithSessions:@[ CreateSessionForTest(YES), CreateSessionForTest(NO) ]
         selectedIndex:selectedIndex];
}

}  // namespace

// Required to clear the autorelease pool automatically between each tests.
using SessionWindowIOSTest = PlatformTest;

TEST_F(SessionWindowIOSTest, InitEmpty) {
  SessionWindowIOS* session_window = [[SessionWindowIOS alloc] init];
  EXPECT_EQ(0u, [session_window.sessions count]);
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), session_window.selectedIndex);
}

TEST_F(SessionWindowIOSTest, InitWithSessions) {
  SessionWindowIOS* session_window = CreateSessionWindowForTest(0u);

  EXPECT_EQ(2u, [session_window.sessions count]);
  EXPECT_EQ(0u, session_window.selectedIndex);
}

TEST_F(SessionWindowIOSTest, CodingEncoding) {
  SessionWindowIOS* original_session_window = CreateSessionWindowForTest(1u);

  NSError* error = nil;
  NSData* data =
      [NSKeyedArchiver archivedDataWithRootObject:original_session_window
                            requiringSecureCoding:NO
                                            error:&error];
  ASSERT_TRUE(data != nil);
  ASSERT_TRUE(error == nil);

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  SessionWindowIOS* unarchived_session_window =
      [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
  ASSERT_TRUE(unarchived_session_window != nil);

  EXPECT_EQ(1u, unarchived_session_window.selectedIndex);
  ASSERT_EQ(2u, [unarchived_session_window.sessions count]);

  EXPECT_TRUE(unarchived_session_window.sessions[0].hasOpener);
  EXPECT_FALSE(unarchived_session_window.sessions[1].hasOpener);
}

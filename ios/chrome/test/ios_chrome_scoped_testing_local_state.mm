// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"

#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeScopedTestingLocalState::IOSChromeScopedTestingLocalState() {
  RegisterLocalStatePrefs(local_state_.registry());
  EXPECT_FALSE(TestingApplicationContext::GetGlobal()->GetLocalState());
  TestingApplicationContext::GetGlobal()->SetLocalState(&local_state_);
}

IOSChromeScopedTestingLocalState::~IOSChromeScopedTestingLocalState() {
  EXPECT_EQ(&local_state_,
            TestingApplicationContext::GetGlobal()->GetLocalState());
  TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
}

TestingPrefServiceSimple* IOSChromeScopedTestingLocalState::Get() {
  return &local_state_;
}

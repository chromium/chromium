// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"

#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/test/testing_application_context.h"
#include "testing/gtest/include/gtest/gtest.h"

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

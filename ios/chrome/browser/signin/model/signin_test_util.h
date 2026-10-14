// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"

@protocol SystemIdentity;
class TestProfileManagerIOS;

// Registers `managed_identity` in `FakeSystemIdentityManager` (if not already
// registered) and adds a managed `TestProfileIOS` for `managed_identity` to
// `profile_manager` using `builder`. The personal profile must already have
// been added to `profile_manager`, and `managed_identity` must represent a
// managed account. Returns a pointer to the created `TestProfileIOS`.
TestProfileIOS* AddManagedProfileWithBuilder(
    TestProfileManagerIOS& profile_manager,
    id<SystemIdentity> managed_identity,
    TestProfileIOS::Builder builder = TestProfileIOS::Builder());

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_TEST_UTIL_H_

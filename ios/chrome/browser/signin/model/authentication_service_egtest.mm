// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface AuthenticationServiceTestCase : ChromeTestCase
@end

@implementation AuthenticationServiceTestCase

// Test the following scenario:
// * Add an identity and sign in
// * Quit Chrome
// * Simulate the identity to be removed by another Google app
// * Start Chrome
// * Add the identity again and sign in.
- (void)testRestart {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  // Adds and signs in with `fakeIdentity`.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Restarts Chrome.
  AppLaunchConfiguration configToCleanPolicy;
  configToCleanPolicy.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:configToCleanPolicy];
  // Verifies that the user is not signed in anymore (fake identities are
  // not preserved with a restart).
  [SigninEarlGrey verifySignedOut];
  // Adds and signs in with `fakeIdentity`.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

@end

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

@implementation SigninEarlGreyImpl

- (FakeChromeIdentity*)fakeIdentity1 {
  return [SigninEarlGreyAppInterface fakeIdentity1];
}

- (FakeChromeIdentity*)fakeIdentity2 {
  return [SigninEarlGreyAppInterface fakeIdentity2];
}

- (FakeChromeIdentity*)fakeManagedIdentity {
  return [SigninEarlGreyAppInterface fakeManagedIdentity];
}

- (void)addFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  [SigninEarlGreyAppInterface addFakeIdentity:fakeIdentity];
}

- (void)setCapabilities:(NSDictionary*)capabilities
            forIdentity:(FakeChromeIdentity*)fakeIdentity {
  [SigninEarlGreyAppInterface setCapabilities:capabilities
                                  forIdentity:fakeIdentity];
}

- (void)forgetFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  [SigninEarlGreyAppInterface forgetFakeIdentity:fakeIdentity];
}

- (void)signOut {
  [SigninEarlGreyAppInterface signOut];
  [self verifySignedOut];
}

- (void)verifySignedInWithFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  BOOL fakeIdentityIsNonNil = fakeIdentity != nil;
  EG_TEST_HELPER_ASSERT_TRUE(fakeIdentityIsNonNil, @"Need to give an identity");

  // Required to avoid any problem since the following test is not dependant
  // to UI, and the previous action has to be totally finished before going
  // through the assert.
  GREYAssert(WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool {
                   NSString* primaryAccountGaiaID =
                       [SigninEarlGreyAppInterface primaryAccountGaiaID];
                   return primaryAccountGaiaID.length > 0;
                 }),
             @"Sign in did not complete.");
  GREYWaitForAppToIdle(@"App failed to idle");

  NSString* primaryAccountGaiaID =
      [SigninEarlGreyAppInterface primaryAccountGaiaID];

  NSString* errorStr = [NSString
      stringWithFormat:@"Unexpected Gaia ID of the signed in user [expected = "
                       @"\"%@\", actual = \"%@\"]",
                       fakeIdentity.gaiaID, primaryAccountGaiaID];
  EG_TEST_HELPER_ASSERT_TRUE(
      [fakeIdentity.gaiaID isEqualToString:primaryAccountGaiaID], errorStr);
}

- (void)verifySignedOut {
  // Required to avoid any problem since the following test is not dependant to
  // UI, and the previous action has to be totally finished before going through
  // the assert.
  GREYWaitForAppToIdle(@"App failed to idle");

  ConditionBlock condition = ^bool {
    return [SigninEarlGreyAppInterface isSignedOut];
  };
  EG_TEST_HELPER_ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForActionTimeout,
                                  condition),
      @"Unexpected signed in user");
}

- (void)verifyAuthenticated {
  EG_TEST_HELPER_ASSERT_TRUE([SigninEarlGreyAppInterface hasPrimaryIdentity],
                             @"User is not signed in");
}

@end

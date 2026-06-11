// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/test/signin_matchers.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using l10n_util::GetNSString;

// Tests for Deeplink Sign-in.
@interface DeeplinkSigninTestCase : ChromeTestCase
@end

@implementation DeeplinkSigninTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled_and_params.push_back(
      {switches::kCrossDeviceSignin,
       {{switches::kCrossDeviceSigninUrl.name,
         "https://www.google.com/chrome/go-mobile"}}});
  return config;
}

// Tests that opening a cross-device sign-in deep link for an account that is
// already signed in shows the "already signed in" snackbar and does not show
// the sign-in flow.
- (void)testCrossDeviceSigninAlreadySignedIn {
  // Sign in with a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Construct the deep link URL.
  NSString* urlString = [NSString
      stringWithFormat:
          @"https://www.google.com/chrome/go-mobile?email=%@&entry_point_id=1",
          fakeIdentity.userEmail];

  // Simulate opening the URL from an external app.
  NSURL* NSURLToOpen = [NSURL URLWithString:urlString];
  [ChromeEarlGrey simulateExternalAppURLOpeningWithURL:NSURLToOpen];

  // Verify that the "already signed in" snackbar is shown.
  NSString* expectedMessage = l10n_util::GetNSStringF(
      IDS_IOS_DEEPLINK_SIGNIN_ALREADY_SIGNED_IN_DESCRIPTION,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  [SigninEarlGreyUI dismissSigninConfirmationSnackbarWithTitle:expectedMessage
                                                 assertVisible:YES];
}

// Tests that opening a cross-device sign-in deep link for an account that is
// not signed in shows the sign-in flow.
- (void)testCrossDeviceSigninNotSignedIn {
  // Add a fake identity to the device, but keep the user signed out.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedOut];

  // Construct the deep link URL.
  NSString* urlString = [NSString
      stringWithFormat:
          @"https://www.google.com/chrome/go-mobile?email=%@&entry_point_id=1",
          fakeIdentity.userEmail];
  NSURL* url = [NSURL URLWithString:urlString];

  // Simulate opening the URL from an external app.
  [ChromeEarlGrey simulateExternalAppURLOpeningWithURL:url];

  // Verify that the sign-in screen is presented.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::SigninScreenPromoMatcher()];
}

@end

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/google_one/test/constants.h"
#import "ios/chrome/browser/google_one/test/google_one_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

// Tests the Google One deep link feature.
@interface GoogleOneTestCase : ChromeTestCase
@end

@implementation GoogleOneTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kSupportGoogleOneDeepLink);
  return config;
}

- (void)setUp {
  [super setUp];
  [GoogleOneAppInterface overrideGoogleOneController];
}

- (void)tearDownHelper {
  [GoogleOneAppInterface restoreGoogleOneController];
  [super tearDownHelper];
}

#pragma mark - Tests

// Tests that opening a Google One deep link URL when signed-out opens the URL
// in a new tab without crashing or opening to a blank state.
- (void)testOpenGoogleOneURLSignedOut {
  GURL googleOneURL = GURL("https://one.google.com/deeplink");
  [ChromeEarlGrey sceneOpenURL:googleOneURL];

  [ChromeEarlGrey waitForWebStateVisibleURL:googleOneURL];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that opening a Google One deep link URL when signed-in launches the
// Google One flow.
- (void)testOpenGoogleOneURLSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  GURL googleOneURL = GURL("https://one.google.com/deeplink");
  [ChromeEarlGrey sceneOpenURL:googleOneURL];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kTestGoogleOneControllerAccessibilityID)];
}

@end

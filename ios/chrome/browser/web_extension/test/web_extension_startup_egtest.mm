// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/web_extension/test/web_extension_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Test suite verifying that Chrome startup halts until ExtensionService is
// ready.
@interface WebExtensionStartupTestCase : ChromeTestCase
@end

@implementation WebExtensionStartupTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      std::string("--") +
      std::string(test_switches::kEnableFakeExtensionService));
  return config;
}

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
}

// Tests that startup halts at kPrepareUI stage until ExtensionService is ready.
- (void)testStartupWaitsForExtensionService {
  // Verify that the extension service was waited upon and is not ready.
  GREYAssertTrue([WebExtensionAppInterface wasExtensionServiceWaitedUpon],
                 @"ExtensionService should have been waited upon.");
  GREYAssertTrue([WebExtensionAppInterface isExtensionServiceWaiting],
                 @"ExtensionService should be waiting for callback.");
  GREYAssertFalse([WebExtensionAppInterface isExtensionServiceReady],
                  @"ExtensionService should not be ready yet.");
  GREYAssertTrue([WebExtensionAppInterface isProfileAtPrepareUIStage],
                 @"Profile should be halted at kPrepareUI stage.");
  GREYAssertFalse([WebExtensionAppInterface isProfileUIReady],
                  @"Profile should not have reached UIReady stage yet.");
  GREYAssertFalse([WebExtensionAppInterface isProfileAtFinalStage],
                  @"Profile should not have reached Final stage yet.");

  // Make the extension service ready.
  [WebExtensionAppInterface setExtensionServiceReady:YES];

  // Verify that the profile transitions to UIReady and all the way to Final.
  ConditionBlock condition = ^{
    return [WebExtensionAppInterface isProfileAtFinalStage];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Profile failed to transition to Final stage.");

  GREYAssertTrue([WebExtensionAppInterface isExtensionServiceReady],
                 @"ExtensionService should now be ready.");
  GREYAssertFalse([WebExtensionAppInterface isExtensionServiceWaiting],
                  @"ExtensionService should no longer have waiting callbacks.");

  // Verify that the UI is opened and can be interacted with.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::FakeOmnibox()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::NTPLogo()];

  // Focus the Omnibox and verify it appears.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::Omnibox()];
}

@end

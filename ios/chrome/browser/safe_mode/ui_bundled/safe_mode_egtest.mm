// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_app_interface.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabel;

namespace {

// Returns a localized string by looking the given key up in
// Localizable.strings.
NSString* LocalizedString(NSString* key) {
  return [base::apple::FrameworkBundle() localizedStringForKey:key
                                                         value:@""
                                                         table:nil];
}

// Verifies that `message` is displayed.
void AssertMessageOnPage(NSString* message) {
  id<GREYMatcher> messageMatcher =
      grey_allOf(grey_text(message), grey_kindOfClass([UILabel class]), nil);
  [[EarlGrey selectElementWithMatcher:messageMatcher]
      assertWithMatcher:grey_notNil()];
}

// Verifies that `message` is not displayed.
void AssertMessageNotOnPage(NSString* message) {
  id<GREYMatcher> messageMatcher =
      grey_allOf(grey_text(message), grey_kindOfClass([UILabel class]),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:messageMatcher]
      assertWithMatcher:grey_nil()];
}

// Verifies that the button to reload chrome is displayed.
void AssertTryAgainButtonOnPage() {
  id<GREYMatcher> tryAgainMatcher = ButtonWithAccessibilityLabel(
      LocalizedString(@"IDS_IOS_SAFE_MODE_RELOAD_CHROME"));
  [[EarlGrey selectElementWithMatcher:tryAgainMatcher]
      assertWithMatcher:grey_notNil()];
}

}  // namespace


// Tests the display of Safe Mode Controller under different error states of
// jailbroken-ness and whether a crash dump was saved.
@interface SafeModeTestCase : ChromeTestCase
@end

@implementation SafeModeTestCase

// Tests that Safe Mode crash upload screen is displayed when there are crash
// reports to upload.
- (void)testSafeModeSendingCrashReport {
  // Mocks the +hasReportToUpload method by swizzling to return positively that
  // there are crash reports to upload.
  EarlGreyScopedBlockSwizzler hasReport(@"SafeModeViewController",
                                        @"hasReportToUpload", ^{
                                          return YES;
                                        });
  [SafeModeAppInterface presentSafeMode];

  // Verifies screen content that shows that crash report is being uploaded.
  AssertMessageOnPage(LocalizedString(@"IDS_IOS_SAFE_MODE_AW_SNAP"));
  AssertMessageOnPage(LocalizedString(@"IDS_IOS_SAFE_MODE_UNKNOWN_CAUSE"));
  AssertTryAgainButtonOnPage();
  AssertMessageOnPage(
      LocalizedString(@"IDS_IOS_SAFE_MODE_SENDING_CRASH_REPORT"));
}

// Tests that Safe Mode screen is displayed with a message that there are
// jailbroken mods that caused a crash. Crash reports are not sent.
- (void)testSafeModeDetectedThirdPartyMods {
  // Mocks the +detectedThirdPartyMods method by swizzling to return positively
  // that device appears to be jailbroken and contains third party mods.
  EarlGreyScopedBlockSwizzler thirdParty(@"SafeModeViewController",
                                         @"detectedThirdPartyMods", ^{
                                           return YES;
                                         });
  // Returns an empty list to simulate no known mods detected.
  EarlGreyScopedBlockSwizzler badModules(@"SafeModeViewController",
                                         @"startupCrashModules", ^{
                                           return @[];
                                         });
  [SafeModeAppInterface presentSafeMode];
  // Verifies screen content that does not show crash report being uploaded.
  // When devices are jailbroken, the crash reports are not very useful.
  AssertMessageOnPage(LocalizedString(@"IDS_IOS_SAFE_MODE_AW_SNAP"));
  AssertMessageOnPage(LocalizedString(@"IDS_IOS_SAFE_MODE_TWEAKS_FOUND"));
  AssertTryAgainButtonOnPage();
  AssertMessageNotOnPage(
      LocalizedString(@"IDS_IOS_SAFE_MODE_SENDING_CRASH_REPORT"));
}

// Tests that Safe Mode screen is displayed with a message that there are
// jailbroken mods listing the names of the known to be bad mods that caused a
// crash. Crash reports are not sent.
- (void)testSafeModeBothThirdPartyModsAndHasReport {
  // Mocks the +detectedThirdPartyMods method by swizzling to return positively
  // that device appears to be jailbroken and contains third party mods.
  EarlGreyScopedBlockSwizzler thirdParty(@"SafeModeViewController",
                                         @"detectedThirdPartyMods", ^{
                                           return YES;
                                         });
  // Mocked list of bad jailbroken mods. These will be checked later.
  NSArray* badModulesList = @[ @"iAmBad", @"MJackson" ];
  EarlGreyScopedBlockSwizzler badModules(@"SafeModeViewController",
                                         @"startupCrashModules", ^{
                                           return badModulesList;
                                         });
  EarlGreyScopedBlockSwizzler hasReport(@"SafeModeViewController",
                                        @"hasReportToUpload", ^{
                                          return YES;
                                        });
  [SafeModeAppInterface presentSafeMode];
  // Verifies screen content that does not show crash report being uploaded.
  // When devices are jailbroken, the crash reports are not very useful.
  AssertMessageOnPage(LocalizedString(@"IDS_IOS_SAFE_MODE_AW_SNAP"));
  // Constructs the list of bad mods based on `badModulesList` above.
  NSString* message = [LocalizedString(@"IDS_IOS_SAFE_MODE_NAMED_TWEAKS_FOUND")
      stringByAppendingString:@"\n\n    iAmBad\n    MJackson"];
  AssertMessageOnPage(message);
  AssertTryAgainButtonOnPage();
  AssertMessageNotOnPage(
      LocalizedString(@"IDS_IOS_SAFE_MODE_SENDING_CRASH_REPORT"));
}

// Tests that an NTP is shown after 2 crashes.
- (void)testPostCrashNTP {
  [SafeModeAppInterface setFailedStartupAttemptCount:0];
  [ChromeEarlGrey closeAllTabsInCurrentMode];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  // The best way to ensure the session is synced to disk is by triggering a
  // background (which forces an immediate session save) and a terminate (which
  // waits for the session save disk write). Alternatively this test could just
  // wait 2-4 seconds.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      relaunchPolicy:ForceRelaunchByCleanShutdown];
  [SafeModeAppInterface setFailedStartupAttemptCount:2];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      relaunchPolicy:ForceRelaunchByKilling];
  [ChromeEarlGrey waitForMainTabCount:3];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SafeModeAppInterface setFailedStartupAttemptCount:0];
}

@end

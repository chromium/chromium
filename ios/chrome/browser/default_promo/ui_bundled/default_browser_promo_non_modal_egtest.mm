// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Wait for 2 seconds longer than the default promo show time, in case it's
// slightly delayed.
constexpr base::TimeDelta kShowPromoWebpageLoadWaitTime = base::Seconds(5);

id<GREYMatcher> NonModalShareTitleMatcher() {
  NSString* a11yLabelText =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_NON_MODAL_SHARE_TITLE);
  return grey_accessibilityLabel(a11yLabelText);
}

id<GREYMatcher> NonModalPasteTitleMatcher() {
  NSString* a11yLabelText = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE);
  return grey_accessibilityLabel(a11yLabelText);
}

}  // namespace

// Tests Non Modal Default Promo.
@interface NonModalEGTest : ChromeTestCase
@end

@implementation NonModalEGTest

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey clearDefaultBrowserPromoData];
}

- (void)tearDownHelper {
  [super tearDownHelper];
  [ChromeEarlGrey clearDefaultBrowserPromoData];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTailoredNonModalDBPromo);
  return config;
}

- (void)setupIPHConfig:(std::string)IPHconfigName {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.iph_feature_enabled = IPHconfigName;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Test that a non modal default modal promo appears when it is triggered by
// using a pasted URL.
- (void)testNonModalAppearsFromPaste {
  [self setupIPHConfig:"IPH_iOSPromoNonModalUrlPasteDefaultBrowser"];

  // Copy URL to the clipboard
  [ChromeEarlGrey copyTextToPasteboard:@"google.com"];

  // Access test URL
  const GURL destinationUrl = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  // Paste the copied URL
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];

  [[[[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_VISIT_COPIED_LINK))] atIndex:0]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()];

  // Wait until the promo appears.
  NSString* description = @"Wait for the promo to appear.";
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:NonModalPasteTitleMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      WaitUntilConditionOrTimeout(kShowPromoWebpageLoadWaitTime, condition),
      description);
}

// Test that a non modal default modal promo appears when it is triggered by
// using the share menu.
- (void)testNonModalAppearsFromShare {
  [self setupIPHConfig:"IPH_iOSPromoNonModalShareDefaultBrowser"];

  const GURL destinationUrl = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [ChromeEarlGreyUI openShareMenu];

  // Verify that the share menu is up and contains a Copy action.
  [ChromeEarlGrey verifyActivitySheetVisible];
  // Start the Copy action and verify that the share menu gets dismissed.
  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];
  [ChromeEarlGrey verifyActivitySheetNotVisible];

  // Wait until the promo appears.
  NSString* description = @"Wait for the promo to appear.";
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:NonModalShareTitleMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      WaitUntilConditionOrTimeout(kShowPromoWebpageLoadWaitTime, condition),
      description);
}

@end

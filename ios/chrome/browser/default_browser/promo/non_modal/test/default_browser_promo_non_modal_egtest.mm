// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/earl_grey2/src/UILib/GREYScreenshotter.h"
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

id<GREYMatcher> NonModalPasteTitleMatcherForArm(int arm) {
  int title_id = 0;
  switch (arm) {
    case 1:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP1;
      break;
    case 2:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP2;
      break;
    case 3:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP3;
      break;
    case 4:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP4;
      break;
    case 5:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP5;
      break;
    case 6:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP6;
      break;
    case 7:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP7;
      break;
    case 8:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP8;
      break;
    case 9:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP9;
      break;
    case 10:
      title_id =
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP10;
      break;
  }
  NSString* a11yLabelText = l10n_util::GetNSString(title_id);
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
  return config;
}

- (void)setupIPHConfig:(std::string)IPHconfigName {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.iph_feature_enabled = IPHconfigName;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

- (void)captureScreenshotForArm:(int)arm {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.iph_feature_enabled = "IPH_iOSPromoNonModalUrlPasteDefaultBrowser";
  config.additional_args.push_back(base::StringPrintf(
      "--enable-features=OmniboxPastePromoExperiment:arm/%d", arm));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  GURL testURL = self.testServer->GetURL("/my_page.html");
  [ChromeEarlGrey copyTextToPasteboard:base::SysUTF8ToNSString(testURL.spec())];

  const GURL destinationUrl = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];

  [[[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                           IDS_IOS_VISIT_COPIED_LINK))]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:NonModalPasteTitleMatcherForArm(arm)]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(
      WaitUntilConditionOrTimeout(kShowPromoWebpageLoadWaitTime, condition),
      @"Wait for promo to appear.");

  UIImage* screenshot = [GREYScreenshotter takeScreenshot];
  NSString* filename = [NSString stringWithFormat:@"promo_arm_%d.png", arm];
  [GREYScreenshotter saveImageAsPNG:screenshot
                             toFile:filename
                        inDirectory:@"/tmp"];
}

- (void)testCapturePromoScreenshotArm1 {
  [self captureScreenshotForArm:1];
}
- (void)testCapturePromoScreenshotArm2 {
  [self captureScreenshotForArm:2];
}
- (void)testCapturePromoScreenshotArm3 {
  [self captureScreenshotForArm:3];
}
- (void)testCapturePromoScreenshotArm4 {
  [self captureScreenshotForArm:4];
}
- (void)testCapturePromoScreenshotArm5 {
  [self captureScreenshotForArm:5];
}
- (void)testCapturePromoScreenshotArm6 {
  [self captureScreenshotForArm:6];
}
- (void)testCapturePromoScreenshotArm7 {
  [self captureScreenshotForArm:7];
}
- (void)testCapturePromoScreenshotArm8 {
  [self captureScreenshotForArm:8];
}
- (void)testCapturePromoScreenshotArm9 {
  [self captureScreenshotForArm:9];
}
- (void)testCapturePromoScreenshotArm10 {
  [self captureScreenshotForArm:10];
}

// Test that a non modal default modal promo appears when it is triggered by
// using a pasted URL.
- (void)testNonModalAppearsFromPaste {
  [self setupIPHConfig:"IPH_iOSPromoNonModalUrlPasteDefaultBrowser"];

  // Copy URL to the clipboard
  GURL testURL = self.testServer->GetURL("/my_page.html");
  [ChromeEarlGrey copyTextToPasteboard:base::SysUTF8ToNSString(testURL.spec())];

  // Access test URL
  const GURL destinationUrl = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  // Paste the copied URL
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];

  [[[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                           IDS_IOS_VISIT_COPIED_LINK))]
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

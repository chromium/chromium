// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/enterprise/data_protection/test/data_protection_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Test URLs.
const char kEchoPath[] = "/echo";
const char kEchoRawPath[] = "/echo_raw";

// Watermark test text constants.
NSString* const kWatermarkConfidential = @"Confidential";
NSString* const kWatermarkTopSecret = @"Top Secret";
NSString* const kEmptyWatermark = @"";

// Returns a matcher for a `WatermarkView` displaying `text`.
id<GREYMatcher> WatermarkWithText(NSString* text) {
  return grey_allOf(grey_kindOfClassName(@"WatermarkView"),
                    grey_descendant(grey_text(text)), nil);
}

}  // namespace

// Tests the enterprise watermark feature in DataProtectionTabHelper.
@interface DataProtectionTestCase : ChromeTestCase
@end

@implementation DataProtectionTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kEnableEnterpriseWatermarkingIOS);
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

#pragma mark - Helpers

// Asserts that the watermark is presented with the expected text across UI, tab
// helper, and profile preferences.
- (void)assertWatermarkPresentedWithText:(NSString*)expectedText {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:WatermarkWithText(expectedText)];
}

// Asserts that no watermark is presented and that tab helper and profile
// preferences are cleared.
- (void)assertWatermarkNotPresented {
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:grey_kindOfClassName(
                                                             @"WatermarkView")];
}

#pragma mark - Tests

// Tests that setting watermark text shows the overlay, updates the tab helper
// and preferences, and clearing it dismisses the overlay and cleans up.
- (void)testWatermarkOverlayLifecycle {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)];

  [self assertWatermarkNotPresented];
  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Update watermark text.
  [DataProtectionAppInterface setWatermarkText:kWatermarkTopSecret];
  [self assertWatermarkPresentedWithText:kWatermarkTopSecret];

  // Clear watermark text.
  [DataProtectionAppInterface setWatermarkText:kEmptyWatermark];
  [self assertWatermarkNotPresented];
}

// Tests that watermarking is tab-scoped: opening a new tab does not show the
// watermark, and switching back restores it.
- (void)testWatermarkAcrossTabs {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)];
  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Open Tab 2.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)];
  [self assertWatermarkNotPresented];

  // Switch back to tab 1 and it should still have the watermark.
  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
}

// Tests that navigating to a page clears any active watermark when the new
// page has no enterprise watermark policy.
- (void)testWatermarkClearedOnNavigation {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)];

  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Navigate to another page and no watermark should be shown.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoRawPath)];
  [self assertWatermarkNotPresented];
}

@end

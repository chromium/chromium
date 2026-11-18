// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/enterprise/data_protection/test/data_protection_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using base::test::ios::kWaitForPageLoadTimeout;

namespace {

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Test URLs.
const char kEchoPath[] = "/echo";
const char kEchoRawPath[] = "/echo-raw";

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

// Tests enterprise watermarking in `DataProtectionTabHelper` and
// `DataProtectionSceneAgent`.
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
  [DataProtectionAppInterface
      setRealTimeUrlCheckMode:enterprise_connectors::
                                  REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED];
}

- (void)tearDownHelper {
  [DataProtectionAppInterface clearRealTimeUrlCheckMode];
  [super tearDownHelper];
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

// Enters the Tab Grid and synchronizes until the regular grid is in the
// hierarchy so the browser's tab-level `WatermarkView` has been detached.
- (void)enterTabGrid {
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::RegularTabGrid()]
      assertWithMatcher:grey_notNil()];
}

// Exits the Tab Grid and waits for the transition animation to finish and the
// active tab's web view to become visible again.
- (void)exitTabGrid {
  GREYWaitForAppToIdle(@"App failed to idle before exiting Tab Grid");
  [DataProtectionAppInterface exitTabGrid];
  GREYWaitForAppToIdle(@"App failed to idle after exiting Tab Grid");
  [ChromeEarlGrey waitForWebStateVisible];
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

// Tests that entering the Tab Grid displays the watermark overlay when an open
// tab is watermarked, and exiting the Tab Grid removes the Tab Grid watermark
// overlay while preserving the tab-level watermark on the active tab.
- (void)testTabGridWatermarkShownOnEntryAndTabWatermarkPreservedOnExit {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)
              withTimeout:kWaitForPageLoadTimeout];
  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Enter the Tab Grid: the watermark overlay must appear on the Tab Grid.
  [self enterTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Exit the Tab Grid: the active tab must still display its watermark.
  [self exitTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
}

// Tests that the Tab Grid shows the watermark even when the active tab is not
// watermarked, as long as another tab in the regular browser requires
// watermarking. Selecting the unwatermarked tab from the grid shows no
// watermark.
- (void)testTabGridWatermarkWithMultipleTabs {
  // Tab 0 has a watermark.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)
              withTimeout:kWaitForPageLoadTimeout];
  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Tab 1 does not have a watermark.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)];
  [self assertWatermarkNotPresented];

  // Enter the Tab Grid: watermark is shown because Tab 0 is watermarked.
  [self enterTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Select Tab 1 (unwatermarked) and exit the Tab Grid.
  [ChromeEarlGrey selectTabAtIndex:1];
  [self exitTabGrid];
  [self assertWatermarkNotPresented];

  // Re-enter Tab Grid and select Tab 0 (watermarked) and exit the Tab Grid.
  [self enterTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
  [ChromeEarlGrey selectTabAtIndex:0];
  [self exitTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
}

// Tests that the watermark is not displayed on the Incognito Tab Grid even when
// an open regular tab is watermarked, and is restored when switching back to
// the regular Tab Grid page.
- (void)testTabGridWatermarkNotAppliedInIncognito {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)
              withTimeout:kWaitForPageLoadTimeout];
  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Enter the Tab Grid: regular page shows watermark.
  [self enterTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Switch to Incognito panel: watermark must be removed.
  [DataProtectionAppInterface showTabGridPage:TabGridPageIncognitoTabs];
  [self assertWatermarkNotPresented];

  // Switch back to Regular panel: watermark must be restored.
  [DataProtectionAppInterface showTabGridPage:TabGridPageRegularTabs];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];

  // Clean up by exiting the Tab Grid.
  [self exitTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
}

// Tests that closing watermarked tabs in the Tab Grid dynamically updates the
// watermark text to the next watermarked tab, and removes the watermark view
// once the last watermarked tab is closed.
- (void)testTabGridWatermarkUpdatedAndRemovedWhenTabsClosedInGrid {
  // Tab 0 has the Confidential watermark.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)
              withTimeout:kWaitForPageLoadTimeout];
  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
  // Tab 1 has the Top Secret watermark.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)];
  [DataProtectionAppInterface setWatermarkText:kWatermarkTopSecret];
  [self assertWatermarkPresentedWithText:kWatermarkTopSecret];
  // Tab 2 does not have a watermark.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)];
  [self assertWatermarkNotPresented];
  // Enter the Tab Grid: Tab 0's watermark text is displayed first.
  [self enterTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
  // Close Tab 0: the Tab Grid watermark updates in-place to Tab 1's text.
  [ChromeEarlGrey closeTabAtIndex:0];
  [self assertWatermarkPresentedWithText:kWatermarkTopSecret];
  // Close the remaining watermarked tab (now at index 0): the watermark view is
  // removed from the Tab Grid.
  [ChromeEarlGrey closeTabAtIndex:0];
  [self assertWatermarkNotPresented];
  // Exit the Tab Grid to the remaining unwatermarked tab.
  [self exitTabGrid];
  [self assertWatermarkNotPresented];
}

// Tests that navigating a watermarked tab to an unwatermarked page clears its
// preference entry so entering the Tab Grid displays no stale watermark.
- (void)testTabGridWatermarkClearedAfterNavigation {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)
              withTimeout:kWaitForPageLoadTimeout];
  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoRawPath)];
  [self assertWatermarkNotPresented];
  [self enterTabGrid];
  [self assertWatermarkNotPresented];
  [self exitTabGrid];
  [self assertWatermarkNotPresented];
}

// Tests that when enterprise lookups are disabled, the Tab Grid watermark
// is not displayed even if an open tab requires watermarking.
- (void)testTabGridWatermarkDisabledWhenEnterpriseLookupsDisabled {
  [DataProtectionAppInterface
      setRealTimeUrlCheckMode:enterprise_connectors::REAL_TIME_CHECK_DISABLED];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEchoPath)
              withTimeout:kWaitForPageLoadTimeout];
  [DataProtectionAppInterface setWatermarkText:kWatermarkConfidential];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
  // Enter the Tab Grid: no watermark overlay should be displayed.
  [self enterTabGrid];
  [self assertWatermarkNotPresented];
  // Clean up by exiting the Tab Grid.
  [self exitTabGrid];
  [self assertWatermarkPresentedWithText:kWatermarkConfidential];
}

@end

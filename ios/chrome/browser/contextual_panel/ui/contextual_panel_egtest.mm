// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/stringprintf.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

namespace {
// The page height of test pages. This must be big enough to triger fullscreen.
const int kPageHeightEM = 200;

// Provides test page long enough to allow fullscreen.
std::unique_ptr<net::test_server::HttpResponse> GetLongResponseForFullscreen(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content(base::StringPrintf(
      "<p style='height:%dem'>test1</p><p>test2</p>", kPageHeightEM));
  return result;
}
}  // namespace

@interface ContextualPanelTestCase : ChromeTestCase
@end

@implementation ContextualPanelTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey resetDataForLocalStatePref:prefs::kBottomOmnibox];

  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/long-fullscreen",
      base::BindRepeating(&GetLongResponseForFullscreen)));

  bool started = self.testServer->Start();
  GREYAssertTrue(started, @"Test server failed to start.");
}

- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey resetDataForLocalStatePref:prefs::kBottomOmnibox];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  if ([self isRunningTest:@selector(testOpenContextualPanelFromNormalIPH)]) {
    config.features_enabled_and_params.push_back(
        {kContextualPanel, {{{"entrypoint-rich-iph", "false"}}}});
  } else {
    config.features_enabled_and_params.push_back({kContextualPanel, {}});
  }

  config.features_enabled_and_params.push_back(
      {kContextualPanelForceShowEntrypoint, {}});

  if ([self isRunningTest:@selector(testOpenContextualPanelFromNormalIPH)] ||
      [self isRunningTest:@selector(testOpenContextualPanelFromRichIPH)] ||
      [self isRunningTest:@selector(testOrientationChangeDismissesIPH)]) {
    config.iph_feature_enabled =
        feature_engagement::kIPHiOSContextualPanelSampleModelFeature.name;
  }

  return config;
}

// Tests that the contextual panel opens correctly.
- (void)testOpenContextualPanel {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close panel
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Tests that the contextual panel opens correctly when tapping the rich IPH.
- (void)testOpenContextualPanelFromRichIPH {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  // Check that the IPH has appeared.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"BubbleViewLabelIdentifier")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close panel
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Tests that the contextual panel opens correctly when tapping the normal IPH.
- (void)testOpenContextualPanelFromNormalIPH {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  // Check that the IPH has appeared.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"BubbleViewLabelIdentifier")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close panel
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Test that the Contextual Panel can still be closed after rotating to
// landscape.
- (void)testContextualPanelLandscape {
  // This test is not relevant on iPads as iPads aren't compact height in
  // landscape.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Switch to landscape.
  GREYAssert(
      [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                    error:nil],
      @"Could not rotate device to Landscape Left");

  // Make sure that panel can still be closed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Tests that closing the last tab with the panel open doesn't crash.
- (void)testCloseLastTabWithPanelOpen {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the tab.
  [ChromeEarlGrey closeTabAtIndex:0];
}

// Tests that closing the last tab before the large entrypoint callback is run
// doesn't crash.
- (void)testCloseLastTabBeforeLargeEntrypointAppears {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  // Close the tab.
  [ChromeEarlGrey closeTabAtIndex:0];
}

// Tests that the contextual panel transitions neatly between iOS sheet
// controller (full iPad layout) and the panel's custom sheet component (other
// window open/iPhone-style layout).
- (void)testContexutalPaneliPadMultiwindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Check that the panel is still visible in the first window
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the second window.
  [ChromeEarlGrey closeWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:1];

  // Check that the panel is still visible in the first window
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Tests that fullscreen is disabled when the omnibox switches to bottom
// position.
- (void)testBottomOmniboxDisablesFullscreen {
  if (![ChromeEarlGrey isBottomOmniboxAvailable]) {
    EARL_GREY_TEST_SKIPPED(@"Test requires bottom omnibox");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/long-fullscreen")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll up in the webpage to enter fullscreen
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Enable bottom omnibox.
  [ChromeEarlGrey setBoolValue:YES forLocalStatePref:prefs::kBottomOmnibox];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Make sure that panel can still be closed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelCloseButtonAXID")]
      performAction:grey_tap()];
}

// Test that the Contextual Panel entrypoint's large chip can be dismissed via
// swipe.
- (void)testContextualPanelEntrypointLargeChipDismissable {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  // Wait for large chip entrypoint to appear.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(@"ContextualPanelEntrypointLabelAXID")];

  // Side swipe on the entrypoint.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointLabelAXID")]
      performAction:grey_swipeSlowInDirectionWithStartPoint(kGREYDirectionLeft,
                                                            0.9, 0.5)];

  // Check that the entrypoint is now back to default size.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointLabelAXID")]
      assertWithMatcher:grey_notVisible()];
}

// Test that the Contextual Panel entrypoint IPH is dismissed when the device
// orientation changes.
- (void)testOrientationChangeDismissesIPH {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];

  // Check that the IPH has appeared.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];

  // Switch to landscape.
  GREYAssert(
      [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                    error:nil],
      @"Could not rotate device to Landscape Left");

  // Check that the IPH has disappeared.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:grey_accessibilityID(
                                                 @"BubbleViewLabelIdentifier")];
}

// Tests that opening the keyboard on iPhone closes the panel. On iPad, the
// panel is presented modally, so the panel wouldn't close.
- (void)testKeyboardOpenClosesPanelOniPhone {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test conditions don't happen on iPad.");
  }

  // Open a page wth a text field.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  // Check that the contextual panel opened up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the keyboard by tapping a text field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("un")];

  // Check that the contextual panel is closed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"PanelContentViewAXID")]
      assertWithMatcher:grey_nil()];
}

@end

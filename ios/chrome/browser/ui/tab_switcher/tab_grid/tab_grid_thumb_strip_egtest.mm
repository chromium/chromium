// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::PrimaryToolbar;
using chrome_test_util::RegularTabGrid;
using chrome_test_util::TabGridBackground;
using chrome_test_util::TabGridDoneButton;
using chrome_test_util::TabGridOtherDevicesPanelButton;
using chrome_test_util::TabGridOpenTabsPanelButton;
using chrome_test_util::WebStateScrollViewMatcher;

namespace {

// net::EmbeddedTestServer handler that responds with the request's query as the
// title and body.
std::unique_ptr<net::test_server::HttpResponse> HandleQueryTitle(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content("<html><head><title>" + request.GetURL().query() +
                             "</title></head><body>" +
                             request.GetURL().query() + "</body></html>");
  return std::move(http_response);
}

// Returns a matcher making sure element is not hidden.
id<GREYMatcher> isNotHidden() {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    return !view.hidden;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:@"is not hidden"];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> cellWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label), isNotHidden(),
                    grey_kindOfClassName(@"GridCell"), nil);
}

}  // namespace

// Tab Grid Thumb Strip tests for Chrome.
@interface TabGridThumbStripTestCase : ChromeTestCase
@end

@implementation TabGridThumbStripTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kExpandedTabStrip);
  config.features_disabled.push_back(
      fullscreen::features::kSmoothScrollingDefault);
  return config;
}

// Sets up the EmbeddedTestServer as needed for tests.
- (void)setUpTestServer {
  self.testServer->RegisterDefaultHandler(base::BindRepeating(
      net::test_server::HandlePrefixedRequest, "/querytitle",
      base::BindRepeating(&HandleQueryTitle)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

// Tests that the basic plus button behavior works. There should be either a
// plus button as the last item in the tab grid or a floating plus button on
// the trailing edge of the thumb strip if the last item is offscreen.
- (void)testBasicPlusButtonBehavior {
  // The feature only works on iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Thumb strip is not enabled on iPhone");
  }

  [self setUpTestServer];

  const GURL URL = self.testServer->GetURL("/querytitle?Tab1");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Tab1"];

  // Swipe down to reveal the thumb strip.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];

  // Make sure that the entire tab thumbnail is fully visible and not covered.
  // This acts as a good proxy to the entire thumbstrip being visible.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Check plus button collection view item is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"PlusSignCell"),
                                   grey_accessibilityLabel(@"Create New Tab"),
                                   nil)]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Check plus button overlay button is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                              @"ThumbStripPlusSignButton"),
                                          nil)]
      assertWithMatcher:grey_notVisible()];

  // Add enough new tabs to fill the entirety of the screen.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];

  // At this point, the plus button in the tab grid should still be visible,
  // because the thumb strip scrolls after adding a new tab.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"PlusSignCell"),
                                   grey_accessibilityLabel(@"Create New Tab"),
                                   nil)]
      assertWithMatcher:grey_minimumVisiblePercent(1)];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                              @"ThumbStripPlusSignButton"),
                                          nil)]
      assertWithMatcher:grey_notVisible()];

  // Scroll to the leading edge of the thumb strip. Then, the button in the grid
  // should be hidden, and the floating button should be visible.
  [[[EarlGrey selectElementWithMatcher:cellWithLabel(@"New Tab")] atIndex:0]
      performAction:grey_swipeFastInDirection(kGREYDirectionRight)];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"PlusSignCell"),
                                   grey_accessibilityLabel(@"Create New Tab"),
                                   nil)] assertWithMatcher:grey_notVisible()];
  // Even when visible, this button has a visibility percent of around 0.15
  // because it is mostly a gradient.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                              @"ThumbStripPlusSignButton"),
                                          nil)]
      assertWithMatcher:grey_minimumVisiblePercent(.1)];
}

- (void)testTappingBackgroundClosesThumbStrip {
  // The feature only works on iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Thumb strip is not enabled on iPhone");
  }

  [self setUpTestServer];

  const GURL URL = self.testServer->GetURL("/querytitle?Tab1");

  // A relative X-position in a view far to the trailing side.
  CGFloat trailingPercentage = [ChromeEarlGrey isRTL] ? 0.02 : 0.98;
  // A relative X-position in a view far to the leading side.
  CGFloat leadingPercentage = 1 - trailingPercentage;

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Tab1"];

  // Swipe down to reveal the thumb strip.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];

  // Make sure that the entire tab thumbnail is fully visible and not covered.
  // This acts as a good proxy to the entire thumbstrip being visible.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Tap the background, vertically close to the top and at the far leading
  // edge. (This should do nothing).
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TabGridBackground(),
                                          grey_ancestor(RegularTabGrid()), nil)]
      performAction:chrome_test_util::TapAtPointPercentage(leadingPercentage,
                                                           0.05)];

  // Check that the grid is still visible
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Now tap the background again at the far trailing edge. This should dismiss
  // the thumb strip.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TabGridBackground(),
                                          grey_ancestor(RegularTabGrid()), nil)]
      performAction:chrome_test_util::TapAtPointPercentage(trailingPercentage,
                                                           0.05)];

  // Check that the thumb strip is indeed dismissed.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_notVisible()];
}

- (void)testSwappingUpBackgroundClosesThumbStrip {
  // The feature only works on iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Thumb strip is not enabled on iPhone");
  }

  [self setUpTestServer];

  const GURL URL = self.testServer->GetURL("/querytitle?Tab1");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Tab1"];

  // Swipe down to reveal the thumb strip.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];

  // Make sure that the entire tab thumbnail is fully visible and not covered.
  // This acts as a good proxy to the entire thumbstrip being visible.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Now swipe up the background. This should dismiss the thumb strip.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(TabGridBackground(),
                                          grey_ancestor(RegularTabGrid()), nil)]
      performAction:grey_swipeSlowInDirection(kGREYDirectionUp)];

  // Check that the thumb strip is indeed dismissed.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_notVisible()];
}

// After scrolling the thumb strip so the currently selected tab is offscreen,
// when opening the thumb strip again, the selected tab should be back onscreen.
- (void)testThumbnailVisibleWhenThumbStripOpens {
  // The feature only works on iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Thumb strip is not enabled on iPhone");
  }

  [self setUpTestServer];

  const GURL URL = self.testServer->GetURL("/querytitle?Tab1");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Tab1"];

  // Swipe down to reveal the thumb strip.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];

  // Make sure that the entire tab thumbnail is fully visible and not covered.
  // This acts as a good proxy to the entire thumbstrip being visible.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Check plus button collection view item is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"PlusSignCell"),
                                   grey_accessibilityLabel(@"Create New Tab"),
                                   nil)]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Check plus button overlay button is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                              @"ThumbStripPlusSignButton"),
                                          nil)]
      assertWithMatcher:grey_notVisible()];

  // Add enough new tabs to fill the entirety of the screen.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];

  // Reselect Tab 1.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Make sure that the first tab's thumbnail is no longer visible.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_notVisible()];

  // Close tab strip and make sure the first tab is still selected.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionUp)];
  [ChromeEarlGrey waitForWebStateContainingText:"Tab1"];

  // Open Tab strip and make sure the first tab's thumbnail is visible again.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_minimumVisiblePercent(1)];
}

// Tests that the plus button in the collection view actually opens a new tab
// when pressed.
- (void)testGridPlusButtonOpensNewTab {
  // The feature only works on iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Thumb strip is not enabled on iPhone");
  }

  [self setUpTestServer];

  const GURL URL = self.testServer->GetURL("/querytitle?Tab1");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Tab1"];

  // Swipe down to reveal the thumb strip.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];

  // Make sure that the entire tab thumbnail is fully visible and not covered.
  // This acts as a good proxy to the entire thumbstrip being visible.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Tap on new tab button and make sure that a new tab is actually opened.
  GREYAssertEqual(1, [ChromeEarlGrey mainTabCount],
                  @"There should be 1 tab before opening the new tab.");
  GREYAssertEqual(0, [ChromeEarlGrey indexOfActiveNormalTab],
                  @"Tab 0 should be active before opening the new tab.");
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"PlusSignCell"),
                                   grey_accessibilityLabel(@"Create New Tab"),
                                   nil)]
      assertWithMatcher:grey_minimumVisiblePercent(1)]
      performAction:grey_tap()];
  GREYAssertEqual(2, [ChromeEarlGrey mainTabCount],
                  @"There should be 2 tab2 before opening the new tab.");
  GREYAssertEqual(1, [ChromeEarlGrey indexOfActiveNormalTab],
                  @"Tab 1 should be active before opening the new tab.");
}

// Tests that the floating plus button actually opens a new tab when pressed.
- (void)testFloatingPlusButtonOpensNewTab {
  // The feature only works on iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Thumb strip is not enabled on iPhone");
  }

  [self setUpTestServer];

  const GURL URL = self.testServer->GetURL("/querytitle?Tab1");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Tab1"];

  // Swipe down to reveal the thumb strip.
  [[EarlGrey selectElementWithMatcher:PrimaryToolbar()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];

  // Make sure that the entire tab thumbnail is fully visible and not covered.
  // This acts as a good proxy to the entire thumbstrip being visible.
  [[EarlGrey selectElementWithMatcher:cellWithLabel(@"Tab1")]
      assertWithMatcher:grey_minimumVisiblePercent(1)];

  // Add enough new tabs to fill the entirety of the screen.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];

  // Scroll to the leading edge of the thumb strip to cause the floating button
  // to appear.
  [[[EarlGrey selectElementWithMatcher:cellWithLabel(@"New Tab")] atIndex:0]
      performAction:grey_swipeFastInDirection(kGREYDirectionRight)];

  // Tap on new tab button and make sure that a new tab is actually opened.
  GREYAssertEqual(8, [ChromeEarlGrey mainTabCount],
                  @"There should be 8 tabs before opening the new tab.");
  GREYAssertEqual(7, [ChromeEarlGrey indexOfActiveNormalTab],
                  @"Tab 7 should be active before opening the new tab.");
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                              @"ThumbStripPlusSignButton"),
                                          nil)] performAction:grey_tap()];
  GREYAssertEqual(9, [ChromeEarlGrey mainTabCount],
                  @"There should be 9 tabs after opening the new tab.");
  GREYAssertEqual(8, [ChromeEarlGrey indexOfActiveNormalTab],
                  @"Tab 8 should be active before opening the new tab.");
}

@end

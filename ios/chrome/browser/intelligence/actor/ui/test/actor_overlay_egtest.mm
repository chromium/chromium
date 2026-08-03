// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/features/features.h"
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

namespace {

// Accessibility identifier for the overlay scrim view.
NSString* const kActorOverlayScrimAccessibilityIdentifier =
    @"ActorOverlayScrimAccessibilityIdentifier";

// Handles custom requests and serves test HTML pages.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");

  if (request.relative_url == "/input.html") {
    http_response->set_content(
        "<html><body><input type='text' id='input'></body></html>");
    return http_response;
  }
  if (request.relative_url == "/button.html") {
    http_response->set_content(
        "<html><body style='margin:0;'>"
        "<button id='btn' style='width:100vw;height:100vh;border:none;' "
        "onclick='document.body.innerText=\"Clicked\"'>"
        "Button</button></body></html>");
    return http_response;
  }
  return nullptr;
}

}  // namespace

// Tests for the actor overlay.
@interface ActorOverlayTestCase : ChromeTestCase
@end

@implementation ActorOverlayTestCase

#pragma mark - ChromeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kActorTools);
  config.features_enabled.push_back(kChromeNextIa);
  return config;
}

- (void)setUp {
  [super setUp];
  net::test_server::EmbeddedTestServer* testServer = self.testServer;
  testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(testServer->Start(), @"Test server failed to start.");
}

- (void)tearDownHelper {
  [ActorAppInterface setActuating:NO forWebStateAtIndex:0];
  [ActorAppInterface setActuating:NO forWebStateAtIndex:1];
  [super tearDownHelper];
}

#pragma mark - Private

// Asserts whether the overlay is visible or not.
- (void)assertOverlayVisible:(BOOL)visible {
  id<GREYMatcher> scrimMatcher =
      grey_accessibilityID(kActorOverlayScrimAccessibilityIdentifier);
  if (visible) {
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:scrimMatcher];
  } else {
    [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:scrimMatcher];
  }
}

#pragma mark - Tests

// Test that the active keyboard is automatically dismissed when the actor
// overlay is presented.
- (void)testKeyboardDismissalOnOverlay {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/input.html")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("input")];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Present the overlay by setting actuating state.
  [ActorAppInterface setActuating:YES forWebStateAtIndex:0];

  [ChromeEarlGrey waitForKeyboardToDisappear];

  // Clean up.
  [ActorAppInterface setActuating:NO forWebStateAtIndex:0];
}

// Test that the overlay successfully intercepts and blocks touch interactions
// in the scrimmed content area.
- (void)testOverlayBlocksInteraction {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/button.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  // Present the overlay.
  [ActorAppInterface setActuating:YES forWebStateAtIndex:0];

  // Try to tap the content scrim.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kActorOverlayScrimAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify that the button click did not trigger (the text is still "Button"
  // and not "Clicked").
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  // Clean up.
  [ActorAppInterface setActuating:NO forWebStateAtIndex:0];
}

// Test that the overlay successfully intercepts and blocks touch interactions
// in the scrimmed primary toolbar area.
- (void)testOverlayBlocksToolbarInteraction {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Toolbar is not covered by overlay on iPad.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/button.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  // Present the overlay.
  [ActorAppInterface setActuating:YES forWebStateAtIndex:0];

  // Try to tap the defocused location view.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];

  // Verify that the web page content is still visible and unchanged.
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  // Verify that the omnibox is not focused (keyboard is not visible).
  NSError* error = nil;
  GREYAssertFalse([EarlGrey isKeyboardShownWithError:&error],
                  @"Keyboard should not be visible.");

  // Clean up.
  [ActorAppInterface setActuating:NO forWebStateAtIndex:0];
}

// Test that setting the actuation state on/off correctly toggles the overlay
// visibility.
- (void)testOverlayLifecycleOnActuationChange {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/input.html")];
  [self assertOverlayVisible:NO];

  [ActorAppInterface setActuating:YES forWebStateAtIndex:0];
  [self assertOverlayVisible:YES];

  [ActorAppInterface setActuating:NO forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
}

// Test that switching active tabs correctly toggles overlay visibility.
- (void)testOverlayTabSwitchingBehavior {
  net::test_server::EmbeddedTestServer* testServer = self.testServer;
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];

  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertOverlayVisible:NO];

  [ActorAppInterface setActuating:YES forWebStateAtIndex:0];
  [self assertOverlayVisible:YES];

  // Switch to Tab 1 (which is not actuating).
  [ChromeEarlGrey selectTabAtIndex:1];
  [self assertOverlayVisible:NO];

  // Switch back to Tab 0 (which is actuating).
  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertOverlayVisible:YES];

  // Clean up.
  [ActorAppInterface setActuating:NO forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
}

// Test that actuating a background tab does not show the overlay.
- (void)testBackgroundTabActuationDoesNotShowOverlay {
  net::test_server::EmbeddedTestServer* testServer = self.testServer;
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];

  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertOverlayVisible:NO];

  // Actuate Tab 1 (which is in the background).
  [ActorAppInterface setActuating:YES forWebStateAtIndex:1];
  [self assertOverlayVisible:NO];

  // Switch to Tab 1.
  [ChromeEarlGrey selectTabAtIndex:1];
  [self assertOverlayVisible:YES];

  // Switch back to Tab 0.
  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertOverlayVisible:NO];

  // Clean up.
  [ActorAppInterface setActuating:NO forWebStateAtIndex:1];
}

// Test that closing the active actuating tab dismisses the overlay.
- (void)testOverlayDismissedOnActiveTabClosed {
  net::test_server::EmbeddedTestServer* testServer = self.testServer;
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];

  [ChromeEarlGrey selectTabAtIndex:1];
  [self assertOverlayVisible:NO];

  [ActorAppInterface setActuating:YES forWebStateAtIndex:1];
  [self assertOverlayVisible:YES];

  // Close Tab 1. Tab 0 will automatically become active.
  [ChromeEarlGrey closeTabAtIndex:1];

  // Verify overlay is dismissed since Tab 0 is active and not actuating.
  [self assertOverlayVisible:NO];
}

// Test that on iPhone, toolbar buttons in the bottom toolbar (which are
// outside the scrimmed content and primary toolbar areas) remain clickable.
- (void)testOverlayAllowsSecondaryToolbarInteraction {
  if ([ChromeEarlGrey isIPadIdiom]) {
    return;
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/button.html")];

  // Present the overlay.
  [ActorAppInterface setActuating:YES forWebStateAtIndex:0];
  [self assertOverlayVisible:YES];

  // Tap the Show Tabs button in the bottom toolbar using `ChromeEarlGreyUI`.
  [ChromeEarlGreyUI openTabGrid];

  // Verify that the tab grid opened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the tab grid to return to normal state.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Wait for the Tab Grid to close and return to the browser view.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::DefocusedLocationView()];

  // Clean up.
  [ActorAppInterface setActuating:NO forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
}

@end

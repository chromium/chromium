// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Accessibility identifier for the overlay scrim view.
NSString* const kActorOverlayScrimAccessibilityIdentifier =
    @"ActorOverlayScrimAccessibilityIdentifier";

// Accessibility identifier for the overlay glow view.
NSString* const kActorOverlayGlowAccessibilityIdentifier =
    @"ActorOverlayGlowAccessibilityIdentifier";

constexpr char kPageAPath[] = "/page_a.html";
constexpr char kPageBPath[] = "/page_b.html";
constexpr char kPageCPath[] = "/page_c.html";
constexpr char kPageAText[] = "Page A";
constexpr char kPageBText[] = "Page B";
constexpr char kPageCText[] = "Page C";

// Handles custom requests and serves test HTML pages.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");

  if (request.relative_url == "/input.html") {
    http_response->set_content(
        "<html><body><h1>Input</h1><input type='text' id='input'></body>"
        "</html>");
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
  if (request.relative_url == kPageAPath) {
    http_response->set_content("<html><body>Page A</body></html>");
    return http_response;
  }
  if (request.relative_url == kPageBPath) {
    http_response->set_content("<html><body>Page B</body></html>");
    return http_response;
  }
  if (request.relative_url == kPageCPath) {
    http_response->set_content("<html><body>Page C</body></html>");
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
  if ([self isRunningTest:@selector(
                              testAppBarBackForwardDisabledDuringActuation)]) {
    config.features_enabled.push_back(kNextOldDesign);
    config.features_enabled.push_back(kFiveButtonAppBarLayout);
  }
  return config;
}

- (void)setUp {
  [super setUp];
  net::test_server::EmbeddedTestServer* testServer = self.testServer;
  testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(testServer->Start(), @"Test server failed to start.");
}

- (void)tearDownHelper {
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:1];
  [super tearDownHelper];
}

#pragma mark - Private

// Asserts whether the overlay scrim is visible or not.
- (void)assertOverlayVisible:(BOOL)visible {
  id<GREYMatcher> scrimMatcher = grey_allOf(
      grey_accessibilityID(kActorOverlayScrimAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
  if (visible) {
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:scrimMatcher];
  } else {
    [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:scrimMatcher];
  }
}

// Asserts whether the overlay glow is visible or not.
- (void)assertGlowVisible:(BOOL)visible {
  id<GREYMatcher> glowMatcher =
      grey_allOf(grey_accessibilityID(kActorOverlayGlowAccessibilityIdentifier),
                 grey_sufficientlyVisible(), nil);
  if (visible) {
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:glowMatcher];
  } else {
    [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:glowMatcher];
  }
}

// Asserts whether the actor interaction-limited snackbar is visible or not.
- (void)assertInteractionLimitedSnackbarVisible:(BOOL)visible {
  if (visible) {
    id<GREYMatcher> titleMatcher =
        grey_allOf(grey_accessibilityID(kSnackbarTitleAccessibilityId),
                   grey_ancestor(chrome_test_util::SnackbarViewMatcher()),
                   grey_text(l10n_util::GetNSString(
                       IDS_IOS_ACTOR_OVERLAY_INTERACTION_LIMITED_SNACKBAR)),
                   grey_sufficientlyVisible(), nil);
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:titleMatcher];
  } else {
    [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                        chrome_test_util::SnackbarViewMatcher()];
  }
}

#pragma mark - Tests

// Test that the active keyboard is automatically dismissed when the actor
// overlay is presented.
- (void)testKeyboardDismissalOnOverlay {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("input")];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Present the overlay by setting actor-controlled state.
  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];

  [ChromeEarlGrey waitForKeyboardToDisappear];

  // Clean up.
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
}

// Test that the overlay successfully intercepts and blocks touch interactions
// in the scrimmed content area.
- (void)testOverlayBlocksInteraction {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/button.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  // Present the overlay.
  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];

  // Try to tap the content scrim.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kActorOverlayScrimAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify that the button click did not trigger (the text is still "Button"
  // and not "Clicked").
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  // Clean up.
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
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
  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];

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
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
}

// Test that setting the control state on/off correctly toggles the overlay
// visibility.
- (void)testOverlayLifecycleOnControlStateChange {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];
  [self assertOverlayVisible:NO];

  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:YES];

  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
}

// Test that switching active tabs correctly toggles overlay visibility.
- (void)testOverlayTabSwitchingBehavior {
  net::test_server::EmbeddedTestServer* testServer = self.testServer;
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];

  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertOverlayVisible:NO];

  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:YES];

  // Switch to Tab 1 (which is not actor-controlled).
  [ChromeEarlGrey selectTabAtIndex:1];
  [self assertOverlayVisible:NO];

  // Switch back to Tab 0 (which is actor-controlled).
  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertOverlayVisible:YES];

  // Clean up.
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
}

// Test that setting control state on a background tab does not show the
// overlay.
- (void)testBackgroundTabControlStateDoesNotShowOverlay {
  net::test_server::EmbeddedTestServer* testServer = self.testServer;
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];

  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertOverlayVisible:NO];

  // Set Tab 1 (which is in the background) to actor-controlled.
  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:1];
  [self assertOverlayVisible:NO];

  // Switch to Tab 1.
  [ChromeEarlGrey selectTabAtIndex:1];
  [self assertOverlayVisible:YES];

  // Switch back to Tab 0.
  [ChromeEarlGrey selectTabAtIndex:0];
  [self assertOverlayVisible:NO];

  // Clean up.
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:1];
}

// Test that closing the active actor-controlled tab dismisses the overlay.
- (void)testOverlayDismissedOnActiveTabClosed {
  net::test_server::EmbeddedTestServer* testServer = self.testServer;
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];

  [ChromeEarlGrey selectTabAtIndex:1];
  [self assertOverlayVisible:NO];

  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:1];
  [self assertOverlayVisible:YES];

  // Close Tab 1. Tab 0 will automatically become active.
  [ChromeEarlGrey closeTabAtIndex:1];

  // Verify overlay is dismissed since Tab 0 is active and not actor-controlled.
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
  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];
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
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
}

// Tests that tapping the overlay scrim displays the interaction-limited
// snackbar and ending actuation dismisses it.
- (void)testScrimTapShowsInteractionLimitedSnackbar {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/button.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:YES];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kActorOverlayScrimAccessibilityIdentifier)]
      performAction:grey_tap()];
  [self assertInteractionLimitedSnackbarVisible:YES];

  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
  [self assertInteractionLimitedSnackbarVisible:NO];
}

// Tests that on the five-button app bar layout on iPhone, back and forward
// buttons are disabled and do not navigate while the active tab is
// actor-controlled, then re-enable once inactive.
- (void)testAppBarBackForwardDisabledDuringActuation {
  if ([ChromeEarlGrey isRegularXRegularSizeClass]) {
    EARL_GREY_TEST_SKIPPED(
        @"Five-button app bar is only shown in compact size classes.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageAPath)];
  [ChromeEarlGrey waitForWebStateContainingText:kPageAText];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageBPath)];
  [ChromeEarlGrey waitForWebStateContainingText:kPageBText];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageCPath)];
  [ChromeEarlGrey waitForWebStateContainingText:kPageCText];

  id<GREYMatcher> backButton =
      grey_accessibilityID(kAppBarBackButtonIdentifier);
  id<GREYMatcher> forwardButton =
      grey_accessibilityID(kAppBarForwardButtonIdentifier);

  // Navigate back to Page B so both back and forward history exist.
  [[EarlGrey selectElementWithMatcher:backButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageBText];
  [[EarlGrey selectElementWithMatcher:backButton]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:forwardButton]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  // Enter actor-controlled state; both buttons should become disabled and
  // tapping them must not navigate.
  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:YES];
  [[EarlGrey selectElementWithMatcher:backButton]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
  [[EarlGrey selectElementWithMatcher:forwardButton]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:backButton] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:forwardButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageBText];

  // Return to inactive; buttons re-enable and tapping back lands on Page A
  // (confirming neither tap during actuation navigated).
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
  [[EarlGrey selectElementWithMatcher:backButton]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:forwardButton]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:backButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageAText];
}

// Tests that on regular x regular toolbars (iPad and unfolded Duo), back and
// forward controls are disabled while the active tab is actor-controlled and
// restored once inactive.
- (void)testToolbarBackForwardDisabledDuringActuation {
  if (![ChromeEarlGrey isRegularXRegularSizeClass]) {
    EARL_GREY_TEST_SKIPPED(
        @"Primary toolbar back/forward buttons require regular x regular.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageAPath)];
  [ChromeEarlGrey waitForWebStateContainingText:kPageAText];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageBPath)];
  [ChromeEarlGrey waitForWebStateContainingText:kPageBText];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageCPath)];
  [ChromeEarlGrey waitForWebStateContainingText:kPageCText];

  // Navigate back to Page B so both back and forward history exist.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageBText];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_enabled()];

  // Enter actor-controlled state; back greys out, forward hides on default
  // Next IA, and tapping back does not navigate.
  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:YES];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageBText];

  // Return to inactive; back and forward re-appear enabled and back lands on
  // Page A.
  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPageAText];
}

// Tests that `kUserControlled` displays the glow without the scrim, lets web
// page touch interactions pass through, and removes the glow when returning to
// `kInactive`.
- (void)testUserControlledShowsGlowWithoutScrimAndAllowsInteraction {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/button.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  [ActorAppInterface setControlState:actor::ActorControlState::kUserControlled
                  forWebStateAtIndex:0];
  [self assertGlowVisible:YES];
  [self assertOverlayVisible:NO];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("btn")];
  [ChromeEarlGrey waitForWebStateContainingText:"Clicked"];

  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [self assertGlowVisible:NO];
  [self assertOverlayVisible:NO];
}

// Tests that `kUserControlled` allows keyboard presentation and transitions
// cleanly to and from `kActorControlled` (dismissing the keyboard and showing
// the scrim on `kActorControlled`, then hiding the scrim, dismissing the
// interaction-limited snackbar, and re-enabling keyboard input on return to
// `kUserControlled`).
- (void)testUserControlledAllowsKeyboardAndTransitionsWithActorControlled {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/input.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Input"];

  [ActorAppInterface setControlState:actor::ActorControlState::kUserControlled
                  forWebStateAtIndex:0];
  [self assertGlowVisible:YES];
  [self assertOverlayVisible:NO];

  // Tapping an input field opens the keyboard in `kUserControlled`.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("input")];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Transitioning to `kActorControlled` dismisses the keyboard and shows the
  // scrim while keeping the glow visible.
  [ActorAppInterface setControlState:actor::ActorControlState::kActorControlled
                  forWebStateAtIndex:0];
  [ChromeEarlGrey waitForKeyboardToDisappear];
  [self assertOverlayVisible:YES];
  [self assertGlowVisible:YES];

  // Show the interaction-limited snackbar by tapping the scrim.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kActorOverlayScrimAccessibilityIdentifier)]
      performAction:grey_tap()];
  [self assertInteractionLimitedSnackbarVisible:YES];

  // Transitioning back to `kUserControlled` hides the scrim, keeps the glow,
  // dismisses the snackbar, and does not auto-reopen the keyboard.
  [ActorAppInterface setControlState:actor::ActorControlState::kUserControlled
                  forWebStateAtIndex:0];
  [self assertOverlayVisible:NO];
  [self assertGlowVisible:YES];
  [self assertInteractionLimitedSnackbarVisible:NO];
  NSError* error = nil;
  GREYAssertFalse([EarlGrey isKeyboardShownWithError:&error],
                  @"Keyboard should stay dismissed until the user taps.");
  GREYAssertNil(error, @"Unexpected error checking keyboard state: %@", error);

  // Tapping the input field opens the keyboard again in `kUserControlled`.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("input")];
  [ChromeEarlGrey waitForKeyboardToAppear];

  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
  [self assertGlowVisible:NO];
  [self assertOverlayVisible:NO];
}

// Tests that `kUserControlled` leaves the primary toolbar / omnibox interactive
// on iPhone (unlike `kActorControlled`, where the scrim covers it).
- (void)testUserControlledAllowsToolbarInteraction {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Toolbar is not covered by overlay on iPad.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/button.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Button"];

  [ActorAppInterface setControlState:actor::ActorControlState::kUserControlled
                  forWebStateAtIndex:0];
  [self assertGlowVisible:YES];
  [self assertOverlayVisible:NO];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForKeyboardToAppear];

  [ActorAppInterface setControlState:actor::ActorControlState::kInactive
                  forWebStateAtIndex:0];
}

@end

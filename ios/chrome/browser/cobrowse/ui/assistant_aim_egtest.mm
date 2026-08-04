// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/omnibox/browser/aim_eligibility_service_features.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_constants.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/composebox/eg_tests/composebox_app_interface.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/scene/ui/scene_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/start_surface/ui_bundled/home_surface_egtest_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using chrome_test_util::OmniboxText;

namespace {

// Handles simulated search requests to provide an AIM-eligible page with a
// link.
std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url.find("/search") != 0) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content("<html><body>"
                        "<h1>Fake AIM Page</h1>"
                        "<p>This is a simulated AIM search results page.</p>"
                        "<a href=\"/pony.html\" id=\"my_link\" "
                        "target=\"_blank\">link</a></body></html>");
  return response;
}

// Waits for the assistant container to reach a specific detent.
void WaitForDetent(AssistantContainerDetent detent) {
  NSString* detentIdentifier;
  switch (detent) {
    case AssistantContainerDetent::kMinimized:
      detentIdentifier = kAssistantContainerDetentMinimizedIdentifier;
      break;
    case AssistantContainerDetent::kMedium:
      detentIdentifier = kAssistantContainerDetentMediumIdentifier;
      break;
    case AssistantContainerDetent::kLarge:
      detentIdentifier = kAssistantContainerDetentLargeIdentifier;
      break;
  }

  id<GREYMatcher> matcher = grey_accessibilityID(detentIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:matcher];
}

// Opens the composebox, attaches the current tab, and waits for the send button
// to be enabled.
void OpenCoBrowse(const GURL& url) {
  [ComposeboxAppInterface setFuseboxEligible:YES];
  [ComposeboxAppInterface setTabUploadAutoSucceed:YES];

  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Focus the omnibox. Tapping fake omnibox might not be enough on all pages.
  [ChromeEarlGreyUI focusOmnibox];

  // Wait for the composebox to be visible.
  id<GREYMatcher> composeboxMatcher =
      grey_accessibilityID(kComposeboxAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:composeboxMatcher];

  // Tap on the plus button to open the menu.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Tap "Attach current tab" to trigger Co-browse.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxAttachCurrentTabActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Wait for the send button to be enabled.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kComposeboxSendButtonAccessibilityIdentifier),
                     grey_enabled(), nil)];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxSendButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
}

// Returns the matcher for the Main WebState scroll view, ignoring Co-browse's.
id<GREYMatcher> MainWebStateScrollView() {
  return grey_allOf(chrome_test_util::WebStateScrollViewMatcher(),
                    grey_not(grey_ancestor(grey_accessibilityID(
                        kAssistantContainerAccessibilityIdentifier))),
                    nil);
}

// Returns the matcher for the Assistant AIM close button.
id<GREYMatcher> CloseButton() {
  return grey_accessibilityID(kAssistantAIMCloseButtonAccessibilityIdentifier);
}

}  // namespace

@interface AssistantAIMTestCase : ChromeTestCase
@end

@implementation AssistantAIMTestCase {
  GURL _defaultURL;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Enable features needed for composebox.
  config.features_enabled.push_back(kComposeboxIpad);
  config.features_enabled.push_back(kAimCobrowse);
  config.features_enabled.push_back(kAssistantContainer);
  config.features_enabled.push_back(kComposeboxPhysicalKeyboardReturnKeys);
  config.features_disabled.push_back(kComposeboxAIMDisabled);
  config.features_disabled.push_back(omnibox::kAimServerEligibilityEnabled);
  config.features_disabled.push_back(kAssistantAimMinimizedState);
  config.features_disabled.push_back(kComposeboxServerSideState);
  // TODO(crbug.com/536079613): Re-enable kAppBarHideInFullscreen once these
  // tests are updated to support it.
  config.features_disabled.push_back(kAppBarHideInFullscreen);

  // Enable omnibox debugging flags.
  config.additional_args.push_back("-EnableOmniboxDebugging");
  config.additional_args.push_back("YES");

  // Map localhost to 127.0.0.1 and set it as the google base URL
  // to satisfy IsAimURL check while avoiding HSTS upgrades/SSL hangs on
  // google.com.
  config.additional_args.push_back(
      "--host-resolver-rules=MAP localhost 127.0.0.1");
  config.additional_args.push_back("--google-base-url=http://localhost");
  config.additional_args.push_back("--ignore-google-port-numbers");

  return config;
}

- (void)tearDownHelper {
  [ChromeEarlGrey removeUserDefaultsObjectForKey:@"EnableOmniboxDebugging"];
  [super tearDownHelper];
}

- (void)setUp {
  [self addTeardownBlock:^{
    // Explicitly close the Co-browse assistant sheet if it remains open.
    // This acts as a safety net to ensure that an open sheet does not bleed
    // into subsequent tests, even if a test case errors or fails prematurely.
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:CloseButton()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    if (!error) {
      [[EarlGrey selectElementWithMatcher:CloseButton()]
          performAction:grey_tap()];
    }
    [ComposeboxAppInterface setAllToolsEnabled:NO];
    [ComposeboxAppInterface setFuseboxEligible:NO];
    [ComposeboxAppInterface setTabUploadAutoSucceed:NO];
  }];
  [super setUp];
  ResetMakeHomeSurfaceOpenImmediately();
  [ComposeboxAppInterface enableAllTools];
  self.testServer->ServeFilesFromSourceDirectory(
      base::FilePath("ios/testing/data/http_server_files"));
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&HandleSearchRequest));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  _defaultURL = self.testServer->GetURL("/echo");
}

- (void)testCloseButtonDismissesAssistant {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Tap the close button.
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];

  // Verify the assistant is dismissed.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];
}

- (void)testShowsUndoSnackbarAfterClosing {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Tap the close button.
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];

  // Verify the assistant is dismissed.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  NSString* snackbarTitle =
      l10n_util::GetNSString(IDS_IOS_AIM_CLOSE_SNACKBAR_TITLE);
  id<GREYMatcher> snackbarMatcher =
      grey_allOf(chrome_test_util::SnackbarViewMatcher(),
                 grey_descendant(grey_accessibilityLabel(snackbarTitle)), nil);
  // Verify the undo snackbar is shown.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:snackbarMatcher];

  // Press undo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kSnackbarButtonAccessibilityId),
                                   grey_accessibilityLabel(
                                       l10n_util::GetNSString(
                                           IDS_IOS_AIM_SNACKBAR_UNDO_BUTTON)),
                                   nil)] performAction:grey_tap()];

  // Verify it's back.
  id<GREYMatcher> composeboxMatcher =
      grey_accessibilityID(kComposeboxAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:composeboxMatcher];
}

// Tests that opening an external URL from the launcher while the app is in the
// background dismisses an active Co-browse session that is in minimized state.
- (void)testOpenExternalURLWithActiveCoBrowse {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // Open Co-browse session on an eligible search page.
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant container to appear in Medium state.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Background the app.
  [[AppLaunchManager sharedManager] backgroundApplication];

  // Trigger opening an external URL via user activity (e.g. Universal Link /
  // Handoff) while backgrounded.
  GURL destinationURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey
      sceneContinueUserActivityWithType:NSUserActivityTypeBrowsingWeb
                                    url:base::SysUTF8ToNSString(
                                            destinationURL.spec())];

  // Re-activate the application to bring it back to the foreground so the scene
  // processes the user activity.
  [[[XCUIApplication alloc] init] activate];

  // Verify the assistant is dismissed after handling the external URL.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that opening a WidgetKit URL scheme while the app is in the background
// dismisses an active Co-browse session that is in minimized state.
- (void)testWidgetKitInvocationWithActiveCoBrowse {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // Open Co-browse session on an eligible search page.
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant container to appear in Medium state.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  WaitForDetent(AssistantContainerDetent::kMedium);

  // Swipe down to transition to Minimized state.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAssistantContainerDetentMediumIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  WaitForDetent(AssistantContainerDetent::kMinimized);

  // Background the app.
  [[AppLaunchManager sharedManager] backgroundApplication];

  // Trigger opening a WidgetKit URL scheme (e.g. Search Widget).
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/search")];

  // Re-activate the application to bring it back to the foreground.
  [[[XCUIApplication alloc] init] activate];

  // Verify the assistant is dismissed after handling the WidgetKit invocation.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that the assistant can be dismissed and reopened multiple times.
- (void)testOpenCloseAndReopenAssistant {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // First presentation & dismissal.
  OpenCoBrowse(_defaultURL);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  // Second presentation & dismissal.
  OpenCoBrowse(_defaultURL);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];
}

- (void)testAssistantPersistsThroughTabGrid {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  // Enter Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Wait for Tab Grid to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kTabGridScrollViewIdentifier)];

  // Verify the assistant is NOT visible in Tab Grid.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  // Exit Tab Grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Verify the assistant is visible again.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
}

// Tests that tapping the new tab button in the toolbar opens a new tab, hides
// the assistant, and navigating on the new tab shows the assistant again.
- (void)testNewTabButtonHidesAssistant {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Secondary toolbar is not present on iPad.");
  }

  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Tap the new tab button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NewTabButton()]
      performAction:grey_tap()];

  // Verify the assistant is dismissed (hidden).
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  // Verify that a new tab was opened (we should be on NTP).
  [ChromeEarlGrey waitForMainTabCount:2];

  // Navigate to a URL on the new tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify the assistant is visible again.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
}

// Tests that when a new tab is opened from an eligible AIM page while a session
// is already active, the assistant updates to the new tab's context.
- (void)testNewTabTriggersCobrowseUpdatesContext {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // 1. Start Co-browse on a normal URL.
  OpenCoBrowse(_defaultURL);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // 2. Navigate the main browser to a simulated AIM URL.
  // The assistant will hide because AIM URLs themselves cannot show the
  // assistant.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "localhost", "/search?udm=50&q=HelloWorld")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:CloseButton()];

  // 3. Tap a link on the simulated AIM page that opens in a new tab.
  // This correctly simulates the user tapping a link in the Assistant AIM
  // sheet, because in both cases the new tab's opener is an AIM URL.
  [ChromeEarlGrey tapWebStateElementWithID:@"my_link"];

  // Wait for the new tab to open.
  [ChromeEarlGrey waitForMainTabCount:2];

  // 4. Because the new tab's opener is an AIM URL and the session is active,
  // CobrowseTabHelper automatically configures the context and triggers the UI.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Verify the context was successfully updated to the opener's query!
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityLabel(
                                                          @"HelloWorld")];
}

// Tests that the assistant can transition between medium, large, and minimized
// detents.
- (void)testDetentTransitions {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Check it starts in Medium state.
  WaitForDetent(AssistantContainerDetent::kMedium);

  // Expand the assistant to Large state by swiping up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAssistantContainerDetentMediumIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  WaitForDetent(AssistantContainerDetent::kLarge);

  // Collapse the assistant to Minimized state by swiping down.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAssistantContainerDetentLargeIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  WaitForDetent(AssistantContainerDetent::kMinimized);
}

// Tests that the Loaded AIM URL debugger view controller is presented
// correctly when tapping the "AIM Loaded URL" action in the context menu
// under omnibox debugging, and that it can be closed by tapping the Close
// button.
- (void)testLoadedURLDebuggerView {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Tap context menu button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kAssistantAIMContextMenuButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Tap "AIM Loaded URL" from the menu.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     @"AIM Loaded URL")] performAction:grey_tap()];

  // Verify that the AIMSRPDebuggerURLViewController view is presented.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerAccessibilityIdentifier)];

  // Verify that the text view is visible and displays some URL.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerTextViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the Close button to dismiss it.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerCloseButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify that the debugger view is dismissed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kAIMSRPDebuggerURLViewControllerAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that the loaded AIM URL can be edited in the debugger, and that saving
// the changes updates the URL and triggers a navigation.
- (void)testLoadedURLDebuggerEditing {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(_defaultURL);

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Open the debugger.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kAssistantAIMContextMenuButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     @"AIM Loaded URL")] performAction:grey_tap()];

  // Verify debugger presented.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerAccessibilityIdentifier)];

  // Tap "Edit".
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerEditButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Edit full URL using the test server with localhost hostname to satisfy
  // the IsAimURL check and allow it to load locally.
  GURL editedURL =
      self.testServer->GetURL("localhost", "/search?udm=50&q=editedquery");
  NSString* editedURLString = base::SysUTF8ToNSString(editedURL.spec());
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerTextViewAccessibilityIdentifier)]
      performAction:grey_replaceText(editedURLString)];

  // Tap "Done".
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerDoneButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify debugger dismissed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kAIMSRPDebuggerURLViewControllerAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  // Open the debugger again to verify the loaded URL was updated.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kAssistantAIMContextMenuButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                     @"AIM Loaded URL")] performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerAccessibilityIdentifier)];

  // Verify the full URL displays the updated URL.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kAIMSRPDebuggerURLViewControllerTextViewAccessibilityIdentifier),
              grey_text(editedURLString), nil)]
      assertWithMatcher:grey_notNil()];

  // Close debugger.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kAIMSRPDebuggerURLViewControllerCloseButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
}

// Tests that when navigating the main browser to a new AIM URL while a session
// is active, the context is updated, so that when returning to a non-AIM tab,
// the updated context is displayed.
- (void)testNavigateInMainBrowserUpdatesCobrowseContext {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // 1. Navigate to fake aim page A.
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("localhost", "/search?udm=50&q=pageA")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // 2. Open a new tab and navigate to a fake aim page B.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("localhost", "/search?udm=50&q=pageB")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // 3. Open cobrowse by tapping on a link in fake aim page B.
  // Context becomes aim page B.
  [ChromeEarlGrey tapWebStateElementWithID:@"my_link"];
  [ChromeEarlGrey waitForMainTabCount:3];

  // Wait for cobrowse to appear on the new non-AIM tab.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityLabel(@"pageB")];

  // 4. Navigate back to fake aim page A.
  // Tab 0 is page A.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:CloseButton()];

  // 5. Query a new thing in this fake AIM webpage A.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "localhost", "/search?udm=50&q=pageA_updated")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // 6. Go back to a tab that is not an AIM page.
  // Tab 2 is the non-AIM page we opened from page B.
  [ChromeEarlGrey selectTabAtIndex:2];

  // 7. Observe that cobrowse is now displaying the context of AIM page A.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityLabel(
                                                          @"pageA_updated")];
}

// Tests that the assistant starts in minimized state when the flag is enabled.
// All 3 detents are available in this mode. This test verifies that we start
// in minimized and are not stuck in it.
- (void)testMinimizedStateWhenFlagEnabled {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  // Remove from disabled list to allow enabling it.
  std::erase(config.features_disabled, kAssistantAimMinimizedState);

  config.features_enabled.push_back(kAssistantAimMinimizedState);
  config.relaunch_policy = ForceRelaunchByKilling;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear and be visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_allOf(CloseButton(),
                                                     grey_sufficientlyVisible(),
                                                     nil)];

  // Verify the assistant is in minimized state.
  WaitForDetent(AssistantContainerDetent::kMinimized);

  // Expand the assistant by swiping up.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kAssistantContainerDetentMinimizedIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Verify the assistant expanded.
  WaitForDetent(AssistantContainerDetent::kLarge);
}

- (void)testAssistantPersistsOnBackground {
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Background and foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Verify the assistant is still visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
}

- (void)testAssistantDoesNotReappearAfterExplicitClose {
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Tap the close button.
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];

  // Verify the assistant is dismissed.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  // Reload the page.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify the assistant does NOT reappear.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  // Background and foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Verify the assistant does NOT reappear.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that the Co-browse session (including its specific context and thread
// ID) is persisted across cold starts.
- (void)testAssistantPersistsOnColdStart {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // 1. Setup a specific context by navigating to a simulated AIM URL.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(
                              "localhost", "/search?udm=50&q=persisted_query")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // 2. Open cobrowse by tapping on a link in the fake aim page.
  // This opens a new non-AIM tab (pony.html), which will display the sheet.
  [ChromeEarlGrey tapWebStateElementWithID:@"my_link"];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Wait for cobrowse to appear and verify the specific query context.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityLabel(
                                                          @"persisted_query")];

  // 3. Ensure session is saved before clean shutdown so it can be restored.
  [ChromeEarlGrey saveSessionImmediately];

  // 4. Relaunch the app.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByKilling;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // If the tab grid is showing, tap the active tab to display it.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"GridCellIdentifierPrefix0")]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            @"GridCellIdentifierPrefix0")]
        performAction:grey_tap()];
  }

  // 5. Verify the active non-AIM tab (pony.html) is properly restored.
  [ChromeEarlGrey waitForWebStateContainingText:"pony jokes"];

  // 6. Verify the assistant is still visible AND the context was successfully
  // restored on the active non-AIM tab.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityLabel(
                                                          @"persisted_query")];
}

// Tests that the Co-browse assistant is hidden on the New Tab Page (NTP)
// when an NTP is opened after the app restarts with an active session.
- (void)testAssistantHiddenOnNTPAfterColdStart {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // 1. Start a cobrowse session on a normal URL.
  OpenCoBrowse(_defaultURL);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  MakeHomeSurfaceOpenImmediately();

  // 2. Cold start the app. This simulates returning to the app after it was
  // force-closed, which natively triggers the Start Surface NTP to open,
  // preserving the cobrowse session on the background tab.
  [ChromeEarlGrey saveSessionImmediately];
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // The app will automatically open a Start Surface NTP upon cold start.
  // Wait for the fake omnibox to appear, indicating the NTP has loaded.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::FakeOmnibox()];

  // Verify the assistant is NOT visible on the Start Surface NTP.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  // Navigate to a normal URL to ensure the assistant reappears.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  ResetMakeHomeSurfaceOpenImmediately();
}

// Tests that the CoBrowse assistant is only shown in the window where it was
// triggered and not in other windows on iPad.
- (void)testAssistantOnlyInTriggeringWindowOniPad {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear in the first window.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Open a second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Scope interactions to the second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];

  // Load a non-NTP URL in the second window. Use a different URL to make it
  // easier to distinguish windows in screenshots if the test fails.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")
       inWindowWithNumber:1];

  // Verify the assistant is NOT visible in the new window. Disable
  // synchronization to prevent EarlGrey from timing out if continuous web or
  // layout animations are running.
  [[GREYConfiguration sharedConfiguration]
          setValue:@NO
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  [[GREYConfiguration sharedConfiguration]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  // Reset scope to the first window.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];

  // Close the second window.
  [ChromeEarlGrey closeWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:1];

  // Verify the assistant is still visible in the first window.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
}

// Tests that when the assistant detent is changed (e.g., to minimized),
// this detent persists when switching to the tab grid and selecting another
// tab.
- (void)testAssistantDetentPersistsAcrossTabs {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  // 1. Load a page in the first tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // 2. Open a second tab and start Co-browse.
  [ChromeEarlGrey openNewTab];
  OpenCoBrowse(_defaultURL);

  // Wait for Assistant to appear and start in Medium state.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  WaitForDetent(AssistantContainerDetent::kMedium);

  // 3. Switch to minimized.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAssistantContainerDetentMediumIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  WaitForDetent(AssistantContainerDetent::kMinimized);

  // 4. Go to tab grid, check no cobrowse.
  [ChromeEarlGreyUI openTabGrid];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              kTabGridScrollViewIdentifier)];
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  // 5. Go to another tab (the first tab at index 0).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"GridCellIdentifierPrefix0")]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];

  // 6. Check cobrowse is here and minimized.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  WaitForDetent(AssistantContainerDetent::kMinimized);
}

// Tests that when the assistant is closed (killed) and reopened,
// it starts in the default detent rather than the last used detent.
- (void)testReopenAssistantStartsInDefaultDetent {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // 1. Open Co-browse.
  OpenCoBrowse(_defaultURL);

  // Wait for Assistant to appear and start in Medium state.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  WaitForDetent(AssistantContainerDetent::kMedium);

  // 2. Expand to Large state.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAssistantContainerDetentMediumIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  WaitForDetent(AssistantContainerDetent::kLarge);

  // 3. Close the assistant explicitly (killing it).
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_nil()];

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbarMatcher = chrome_test_util::SnackbarViewMatcher();
  [ChromeEarlGrey testUIElementAppearanceWithMatcher:snackbarMatcher];
  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:snackbarMatcher]
      performAction:grey_tap()];

  // 4. Reopen Co-browse.
  OpenCoBrowse(_defaultURL);

  // 5. Verify it starts in the default Medium detent (NOT Large).
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  WaitForDetent(AssistantContainerDetent::kMedium);
}

// Tests that Co-browse Assistant is hidden when toolbars are hidden.
- (void)testCobrowseHidesWhenToolbarsHide {
  // TODO(crbug.com/526935460): Fix this test for ChromeNext.
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped when chromeNext is enabled.");
  }
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(self.testServer->GetURL("/tall_page.html"));

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Minimize the Assistant so the Main page is exposed and can be scrolled.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAssistantContainerDetentMediumIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Wait for the minimized detent.
  WaitForDetent(AssistantContainerDetent::kMinimized);

  // Scroll down on the Main page to hide the toolbar. We start the swipe from
  // the middle of the screen to avoid accidentally swiping up on the bottom
  // toolbar, which would open the Tab Grid.
  [[EarlGrey selectElementWithMatcher:MainWebStateScrollView()]
      performAction:grey_swipeSlowInDirectionWithStartPoint(kGREYDirectionUp,
                                                            0.5, 0.2)];

  // Verify that the toolbar is hidden.
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Verify that Co-browse is no longer visible.
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Scroll up on the Main page to show the toolbar again.
  [[EarlGrey selectElementWithMatcher:MainWebStateScrollView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Verify that the toolbar is visible.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Verify that Co-browse is visible again.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_allOf(CloseButton(),
                                                     grey_sufficientlyVisible(),
                                                     nil)];
}

// Tests that pressing Return in the composebox text view does not send the
// query and Shift+Return adds a newline.
- (void)testComposeboxReturnKeys {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Locate the composebox text input and focus it.
  id<GREYMatcher> composeboxInput = chrome_test_util::Omnibox();
  [[EarlGrey selectElementWithMatcher:composeboxInput]
      performAction:grey_tap()];

  // Type some text using simulated physical keyboard events.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"line 1" flags:0];

  // Spin the run loop to allow the async keyboard events to process.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  // Press Shift+Return.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\r" flags:UIKeyModifierShift];

  // Type more text.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"line 2" flags:0];

  // Spin the run loop to allow the async keyboard events to process.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  // Verify that the text now contains a newline.
  [[EarlGrey selectElementWithMatcher:composeboxInput]
      assertWithMatcher:OmniboxText("line 1\nline 2")];

  // Press Cmd+Return.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\r"
                                          flags:UIKeyModifierCommand];

  // Verify that the text is still the same (Cmd+Return did nothing).
  [[EarlGrey selectElementWithMatcher:composeboxInput]
      assertWithMatcher:OmniboxText("line 1\nline 2")];

  // Now press Return (without shift) to send the query.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\r" flags:0];

  // Verify that the query is not sending (e.g. the text not is cleared).
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_allOf(
                                              composeboxInput,
                                              OmniboxText("line 1\nline 2\n"),
                                              nil)];
}

// Tests that the cobrowse input plate is hidden when the plus menu bottom sheet
// is opened, and reshown after it is dismissed.
- (void)testInputPlateHiddenOnPlusMenu {
  if (!IsComposeboxPlusButtonBottomSheet()) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxPlusButtonBottomSheet is disabled.");
  }
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];

  // Verify that the cobrowse input plate is visible.
  id<GREYMatcher> inputPlate =
      grey_accessibilityID(kComposeboxAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:inputPlate]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Focus the cobrowse input plate by tapping it.
  [[EarlGrey selectElementWithMatcher:inputPlate] performAction:grey_tap()];

  // Wait for the plus button to appear.
  id<GREYMatcher> plusButton =
      grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:plusButton];

  // Tap on the plus button to open the menu.
  [[EarlGrey selectElementWithMatcher:plusButton] performAction:grey_tap()];

  // Wait for the plus menu to appear.
  id<GREYMatcher> menuOption =
      grey_accessibilityID(kComposeboxSelectTabsActionAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:menuOption];

  // Verify that the cobrowse input plate is hidden.
  [[EarlGrey selectElementWithMatcher:inputPlate]
      assertWithMatcher:grey_notVisible()];

  // Dismiss the bottom sheet menu by swiping it down.
  [[EarlGrey selectElementWithMatcher:menuOption]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Wait for the menu to disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:menuOption];

  // Verify that the cobrowse input plate is visible again.
  [[EarlGrey selectElementWithMatcher:inputPlate]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that focusing the Cobrowse input plate automatically attaches the
// active tab.
- (void)testCobrowseAutoAttachesActiveTabWhenTyping {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }

  // 1. Open Co-browse. This loads /echo and opens the Assistant.
  OpenCoBrowse(_defaultURL);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CloseButton()];
  WaitForDetent(AssistantContainerDetent::kMedium);

  // 2. Focus the input plate inside the Cobrowse assistant.
  // We match the omnibox inside the Assistant container.
  id<GREYMatcher> cobrowseOmnibox = grey_allOf(
      chrome_test_util::Omnibox(),
      grey_ancestor(
          grey_accessibilityID(kAssistantContainerDetentMediumIdentifier)),
      nil);

  [[EarlGrey selectElementWithMatcher:cobrowseOmnibox]
      performAction:grey_tap()];
  WaitForDetent(AssistantContainerDetent::kLarge);

  // 3. Verify that the auto-attached tab appears in the Cobrowse tabs
  // accordion.
  id<GREYMatcher> cobrowseTabsAccordion = grey_allOf(
      grey_accessibilityID(kComposeboxTabsAccordionAccessibilityIdentifier),
      grey_ancestor(
          grey_accessibilityID(kAssistantContainerDetentLargeIdentifier)),
      nil);

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:cobrowseTabsAccordion];
}

@end

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/strings/grit/components_strings.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_disable_timer_tracking.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const char kUserAgentTestPath[] = "/user_agent_test_page.html";

const char kMobileSiteLabel[] = "Mobile";

const char kDesktopSiteLabel[] = "Desktop";
const char kDesktopPlatformLabel[] = "MacIntel";

// URL to be used when the page needs to be reloaded on back/forward
// navigations.
const char kPurgeURL[] = "url-purge.com";
// JavaScript used to reload the page on back/forward navigations.
const char kJavaScriptReload[] =
    "<script>window.onpageshow = function(event) {"
    "    if (event.persisted) {"
    "       window.location.href = window.location.href + \"?reloaded\""
    "    }"
    "};</script>";

// Custom timeout used when waiting for a web state after requesting desktop
// or mobile mode.
constexpr base::TimeDelta kWaitForUserAgentChangeTimeout = base::Seconds(15);

// Returns the correct matcher for the collection view containing the Request
// Desktop/Mobile button given the current overflow menu.
id<GREYMatcher> CollectionViewMatcher() {
  return grey_accessibilityID(kPopupMenuToolsMenuActionListId);
}

// Select the button to request desktop site by scrolling the collection.
// 200 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* RequestMobileButton() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuRequestMobileId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:CollectionViewMatcher()];
}

// Select the button to request mobile site by scrolling the collection.
// 200 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* RequestDesktopButton() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuRequestDesktopId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:CollectionViewMatcher()];
}

// A ResponseProvider that provides user agent for httpServer request.
// Handles responses for the test server.
std::unique_ptr<net::test_server::HttpResponse> HandleUserAgentRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");

  // Do not return anything if static plist file has been requested,
  // as plain text is not a valid property list content.
  if ([[base::SysUTF8ToNSString(request.relative_url) pathExtension]
          isEqualToString:@"plist"]) {
    response->set_code(net::HTTP_NO_CONTENT);
    return response;
  }

  // If the user agent test page is requested, return nullptr to allow the
  // default file handler to serve it.
  if (request.GetURL().path().find(kUserAgentTestPath) != std::string::npos) {
    return nullptr;
  }

  std::string purge_additions = "";
  if (request.GetURL().path().find(kPurgeURL) != std::string::npos) {
    purge_additions = kJavaScriptReload;
  }

  std::optional<std::string> userAgent;
  auto it = request.headers.find("User-Agent");
  if (it != request.headers.end()) {
    userAgent = it->second;
  }

  std::string desktop_product =
      "CriOS/" + version_info::GetMajorVersionNumber();
  std::string desktop_user_agent = web::BuildDesktopUserAgent(desktop_product);

  if (userAgent == desktop_user_agent) {
    response->set_content(std::string(kDesktopSiteLabel) + "\n" +
                          purge_additions);
  } else {
    response->set_content(std::string(kMobileSiteLabel) + "\n" +
                          purge_additions);
  }

  return response;
}
}  // namespace

// Tests for the tools popup menu.
@interface RequestDesktopMobileSiteTestCase : ChromeTestCase
@end

@implementation RequestDesktopMobileSiteTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&HandleUserAgentRequest));

  self.testServer->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS)
          .AppendASCII("ios/testing/data/http_server_files/"));

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

#pragma mark - Helper

// Sets the default mode to the passed `defaultMode`.
- (void)selectDefaultMode:(NSString*)defaultMode {
  [ChromeEarlGreyUI openSettingsMenu];
  {
    // Disable the timer in this scope to avoid infinite spinner loop. EarlGrey
    // tests wait for scroll bars to disappear after a scroll operation.
    ScopedDisableTimerTracking disabler;
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kSettingsContentSettingsCellId),
                                            grey_sufficientlyVisible(), nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 300)
        onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsDefaultSiteModeCellId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     defaultMode)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Tests

// Tests that requesting desktop site of a page works and the user agent
// propagates to the next navigations in the same tab.
- (void)testRequestDesktopSitePropagatesToNextNavigations {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that desktop user agent propagates.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];
}

// Tests that requesting desktop site of a page works and the requested user
// agent is kept when restoring the session.
- (void)testRequestDesktopSiteKeptSessionRestoration {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Restart the app to trigger a reload.
  [self triggerRestoreByRestartingApplication];

  // Verify that desktop user agent propagates.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];
}

// Tests that requesting desktop site of a page works and desktop user agent
// does not propagate to next the new tab.
- (void)testRequestDesktopSiteDoesNotPropagateToNewTab {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that desktop user agent does not propagate to new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that when requesting desktop on another page and coming back to a page
// that has been purged from memory, we still display the mobile page.
- (void)testRequestDesktopSiteGoBackToMobilePurged {
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/" + std::string(kPurgeURL))];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/2.com")];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that going back returns to the mobile site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForPageLoadTimeout,
                 ^bool {
                   return [ChromeEarlGrey webStateVisibleURL].GetQuery() ==
                          "reloaded";
                 }),
             @"Page did not reload");
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that navigating forward to a page not using the default mode from a
// restored session is using the mode used in the past session.
- (void)testNavigateForwardToDesktopMode {
  // TODO(crbug.com/329210328): Re-enable the test on iPad device.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad device.");
  }
#endif

  // Load the page in the non-default mode.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/1.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];

  [ChromeEarlGrey goBack];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Restart the app to trigger a reload.
  [self triggerRestoreByRestartingApplication];

  // Make sure that the NTP is displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The session is restored, navigate forward and check the mode.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() assertWithMatcher:grey_notNil()];
}

// Tests that requesting mobile site of a page works and the user agent
// propagates to the next navigations in the same tab.
- (void)testRequestMobileSitePropagatesToNextNavigations {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that mobile user agent propagates.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that requesting desktop site button is not enabled on new tab pages.
- (void)testRequestDesktopSiteNotEnabledOnNewTabPage {
  // Verify tapping on request desktop button is no-op.
  [ChromeEarlGreyUI openToolsMenu];
  [[RequestDesktopButton() assertWithMatcher:grey_notNil()]
      performAction:grey_tap()];
}

// Tests that requesting desktop site button is not enabled on WebUI pages.
- (void)testRequestDesktopSiteNotEnabledOnWebUIPage {
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Verify tapping on request desktop button is no-op.
  [ChromeEarlGreyUI openToolsMenu];
  [[RequestDesktopButton() assertWithMatcher:grey_notNil()]
      performAction:grey_tap()];
}

// Tests that navigator.appVersion JavaScript API returns correct string for
// mobile User Agent and the platform.
- (void)testAppVersionJSAPIWithMobileUserAgent {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kUserAgentTestPath)];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  std::string defaultPlatform;
  std::string nonDefaultPlatform;
  if ([ChromeEarlGrey isMobileModeByDefault]) {
    defaultPlatform = base::SysNSStringToUTF8([[UIDevice currentDevice] model]);
    nonDefaultPlatform = kDesktopPlatformLabel;
  } else {
    defaultPlatform = kDesktopPlatformLabel;
    nonDefaultPlatform =
        base::SysNSStringToUTF8([[UIDevice currentDevice] model]);
  }
  [ChromeEarlGrey waitForWebStateContainingText:defaultPlatform];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:nonDefaultPlatform];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:defaultPlatform];
}

// Tests that changing the default mode of the page load works as expected.
- (void)testRequestDesktopByDefault {
  GREYAssertTrue([ChromeEarlGrey isMobileModeByDefault],
                 @"The default mode should be mobile.");

  [self selectDefaultMode:@"Desktop"];

  GREYAssertFalse([ChromeEarlGrey isMobileModeByDefault],
                  @"The default mode should be desktop.");

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/1.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];

  // Change back to Mobile.
  [self selectDefaultMode:@"Mobile"];

  // Make sure that the page isn't reloaded.
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];

  // Verify that mobile user agent propagates.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that reloading a page after changing its default mode updates to the
// new mode.
- (void)testReloading {
  GREYAssertTrue([ChromeEarlGrey isMobileModeByDefault],
                 @"The default mode should be mobile.");

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  [self selectDefaultMode:@"Desktop"];

  GREYAssertFalse([ChromeEarlGrey isMobileModeByDefault],
                  @"The default mode should be desktop.");

  // Reloading should change to desktop.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];
}

// Tests that navigating back to a page after changing default mode doesn't
// change the page mode.
- (void)testGoBackInDifferentDefaultMode {
  GREYAssertTrue([ChromeEarlGrey isMobileModeByDefault],
                 @"The default mode should be mobile.");

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  [self selectDefaultMode:@"Desktop"];

  GREYAssertFalse([ChromeEarlGrey isMobileModeByDefault],
                  @"The default mode should be desktop.");

  // Move to another page, loaded in desktop mode.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];

  // Go back to the Mobile page.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

@end

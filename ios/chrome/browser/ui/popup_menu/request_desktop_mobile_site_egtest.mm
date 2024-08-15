// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/strings/grit/components_strings.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const char kUserAgentTestURL[] =
    "http://ios/testing/data/http_server_files/user_agent_test_page.html";

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
  return [ChromeEarlGrey isNewOverflowMenuEnabled]
             ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
             : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
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
class UserAgentResponseProvider : public web::DataResponseProvider {
 public:
  bool CanHandleRequest(const Request& request) override { return true; }

  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    // Do not return anything if static plist file has been requested,
    // as plain text is not a valid property list content.
    if ([[base::SysUTF8ToNSString(request.url.spec()) pathExtension]
            isEqualToString:@"plist"]) {
      *headers =
          web::ResponseProvider::GetResponseHeaders("", net::HTTP_NO_CONTENT);
      return;
    }

    std::string purge_additions = "";
    if (base::Contains(request.url.path(), kPurgeURL)) {
      purge_additions = kJavaScriptReload;
    }

    *headers = web::ResponseProvider::GetDefaultResponseHeaders();
    std::optional<std::string> userAgent =
        request.headers.GetHeader("User-Agent");
    std::string desktop_product =
        "CriOS/" + version_info::GetMajorVersionNumber();
    std::string desktop_user_agent =
        web::BuildDesktopUserAgent(desktop_product);
    if (userAgent == desktop_user_agent) {
      response_body->assign(std::string(kDesktopSiteLabel) + "\n" +
                            purge_additions);
    } else {
      response_body->assign(std::string(kMobileSiteLabel) + "\n" +
                            purge_additions);
    }
  }
};
}  // namespace

// Tests for the tools popup menu.
@interface RequestDesktopMobileSiteTestCase : WebHttpServerChromeTestCase
@end

@implementation RequestDesktopMobileSiteTestCase

#pragma mark - Helper

// Sets the default mode to the passed `defaultMode`.
- (void)selectDefaultMode:(NSString*)defaultMode {
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSettingsContentSettingsCellId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:grey_tap()];
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
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that desktop user agent propagates.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];
}

// Tests that requesting desktop site of a page works and the requested user
// agent is kept when restoring the session.
- (void)testRequestDesktopSiteKeptSessionRestoration {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
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
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that desktop user agent does not propagate to new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that when requesting desktop on another page and coming back to a page
// that has been purged from memory, we still display the mobile page.
- (void)testRequestDesktopSiteGoBackToMobilePurged {

  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(
                              "http://" + std::string(kPurgeURL))];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];

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
                   return [ChromeEarlGrey webStateVisibleURL].query() ==
                          "reloaded";
                 }),
             @"Page did not reload");
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that navigating forward to a page not using the default mode from a
// restored session is using the mode used in the past session.
- (void)testNavigateForwardToDesktopMode {
  // TODO(crbug.com/329210328): Re-enable the test on iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad device.");
  }
#endif

  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  // Load the page in the non-default mode.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
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
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
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
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
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
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUserAgentTestURL)];
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

  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];

  // Change back to Mobile.
  [self selectDefaultMode:@"Mobile"];

  // Make sure that the page isn't reloaded.
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];

  // Verify that mobile user agent propagates.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

// Tests that reloading a page after changing its default mode updates to the
// new mode.
- (void)testReloading {
  GREYAssertTrue([ChromeEarlGrey isMobileModeByDefault],
                 @"The default mode should be mobile.");

  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
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

  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  [self selectDefaultMode:@"Desktop"];

  GREYAssertFalse([ChromeEarlGrey isMobileModeByDefault],
                  @"The default mode should be desktop.");

  // Move to another page, loaded in desktop mode.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:kDesktopSiteLabel];

  // Go back to the Mobile page.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
}

@end

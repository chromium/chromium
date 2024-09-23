// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/context_menu/ui_bundled/constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;
using chrome_test_util::OmniboxText;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::SystemSelectionCalloutCopyButton;
using chrome_test_util::WebViewMatcher;

namespace {
// Directory containing the `kLogoPagePath` and `kLogoPageImageSourcePath`
// resources.
// const char kServerFilesDir[] = "ios/testing/data/http_server_files/";
// Path to a page containing the chromium logo and the text `kLogoPageText`.
const char kLogoPagePath[] = "/chromium_logo_page.html";
// Path to the chromium logo.
const char kLogoPageImageSourcePath[] = "/chromium_logo.png";
// The DOM element ID of the chromium image on the logo page.
const char kLogoPageChromiumImageId[] = "chromium_image";
// The text of the message on the logo page.
const char kLogoPageText[] = "Page with some text and the chromium logo image.";

// URL to a page with a static message.
const char kDestinationPageUrl[] = "/destination";
// HTML content of the destination page.
const char kDestinationHtml[] =
    "<html><head><meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "/></head><body><script>document.title='new doc'</script>"
    "<center><span id=\"message\">You made it!</span></center>"
    "</body></html>";
// The DOM element ID of the message on the destination page.
const char kDestinationPageTextId[] = "message";
// The text of the message on the destination page.
const char kDestinationPageText[] = "You made it!";

// URL to a page with a link to the destination page.
const char kInitialPageUrl[] = "/scenarioContextMenuOpenInNewSurface";
// HTML content of a page with a link to the destination page.
const char kInitialPageHtml[] =
    "<html><head><meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' /></head><body><a "
    "style='margin-left:150px' href='/destination' id='link'>"
    "link</a></body></html>";
// The DOM element ID of the link to the destination page.
const char kInitialPageDestinationLinkId[] = "link";
// The text of the link to the destination page.
const char kInitialPageDestinationLinkText[] = "link";

// The DOM element ID of the long link to the destination page.
const char kInitialPageDestinationLongLinkID[] = "LongLink";
// The text of the long link to the destination page.
const char kInitialPageDestinationLongLinkText[] = "LongLink";

// URL to a page with a link with a javascript: scheme.
const char kJavaScriptPageUrl[] = "/scenarionContextMenuJavaScript";
// HTML content of a page with a javascript link.
const char kJavaScriptPageHtml[] =
    "<html><head><meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' /></head><body><a "
    "style='margin-left:150px' href=\"javascript:alert('test')\" id='link'>"
    "link</a></body></html>";

// URL to a page with a link with a magnet scheme.
const char kMagnetPageUrl[] = "/scenarionContextMenuMagnet";
// HTML content of a page with a magnet link.
const char kMagnetPageHtml[] =
    "<html><head><meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' /></head><body><a "
    "style='margin-left:150px' "
    "href='magnet:?xt=urn:btih:c12fe1c06bba254a9dc9f519b335aa7c1367a88a' "
    "id='link'>link</a></body></html>";

// Template HTML, URLs, and link and title values for title truncation tests.
// (Use NSString for easier format printing and matching).
// Template params:
//    [0] NSString - link href.
//    [1] char[]   - link element ID.
//    [2] NSString - image title
//    [3] char[]   - image element ID.
NSString* const kTruncationTestPageTemplateHtml =
    @"<html><head><meta name='viewport' content='width=device-width, "
     "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
     "/></head><body><p style='margin-bottom:50px'>Short title test.</p>"
     "<p><a style='margin-left:150px' href='%@' id='%s'>LINK</a></p>"
     "<img src='chromium_logo.png' title='%@' id='%s'/>"
     "</body></html>";

const char kShortTruncationPageUrl[] = "/shortTruncation";

const char kLongTruncationPageUrl[] = "/longTruncation";

const char kLongLinkPageURL[] = "/longLink";

NSString* const kShortLinkHref = @"/destination";

NSString* const kShortImgTitle = @"Chromium logo with a short title";

const char kLinkImagePageUrl[] = "/imageLink";

// Template HTML value image test. (Use NSString for easier format printing and
// matching).
// Template params:
//    [0] NSString - link href.
//    [1] char[]   - link element ID.
//    [2] NSString - image title
//    [3] char[]   - image element ID.
NSString* const kLinkImageHtml =
    @"<html><head><meta name='viewport' content='width=device-width, "
     "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
     "/></head><body><p style='margin-bottom:50px'>Image that is also a "
     "link.</p>"
     "<p><a style='margin-left:150px' href='%@' id='%s'><img "
     "src='chromium_logo.png' title='%@' id='%s'/></a></p>"
     "</body></html>";

// Long titles should be > 100 chars to test truncation.
NSString* const kLongLinkHref =
    @"/destination?linkspam=10%E4%BB%A3%E5%B7%A5%E5%AD%A6%E3%81%AF%E6%9C%AA%E6"
     "%9D%A5%E3%81%AE%E8%A3%BD%E5%93%81%E3%81%A8%E3%82%B3%E3%83%9F%E3%83%A5%E3"
     "%83%8B%E3%82%B1%E3%83%BC%E3%82%B7%E3%83%A7%E3%83%B3%E3%82%92%E7%94%9F%E3"
     "%81%BF%E5%87%BA%E3%81%99%E3%82%B9%E3%82%BF%E3%82%B8%E3%82%AA%E3%81%A7%E3"
     "%81%99%E3%80&padding=qwertyuiopasdfghjklzxcvbnmqwertyuiopasdfghjklzxcvbn";

NSString* const kLongImgTitle =
    @"Chromium logo with a long title, well in excess of one hundred "
     "characters, so formulated as to thest the very limits of the context "
     "menu layout system, and to ensure that all users can enjoy the full "
     "majesty of image titles, however sesquipedalian they may be!";

// Template HTML, URLs, and link and title values for title truncation tests.
// (Use NSString for easier format printing and matching).
// Template params:
//    [0] NSString - link href.
//    [1] char[]   - link element ID.
NSString* const kLongLinkTestPageTemplateHtml =
    @"<html><head><meta name='viewport' content='width=device-width, "
     "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
     "/></head><body><p style='margin-bottom:50px'>Short title test.</p>"
     "<p><a style='margin-left:150px' href='%@' id='%s'>LongLink</a></p>"
     "</body></html>";

// Matcher for the open image button in the context menu.
id<GREYMatcher> OpenImageButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);
}

// Matcher for the open image in new tab button in the context menu.
id<GREYMatcher> OpenImageInNewTabButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

// Matcher for the open link in new tab group button in the context menu.
id<GREYMatcher> OpenLinkInNewGroupButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENLINKINNEWTABGROUP);
}

// Matcher for the open link in group button in the context menu.
id<GREYMatcher> OpenLinkInGroupButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENLINKINTABGROUP);
}

// Matcher for the open link in an existing tab group (a group containing one
// tab) button in the context menu.
id<GREYMatcher> OpenLinkInOneTabGroupButton() {
  return ContextMenuItemWithAccessibilityLabel(
      l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, 1));
}

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kInitialPageUrl) {
    // The initial page contains a link to the destination page.
    http_response->set_content(kInitialPageHtml);
  } else if (request.relative_url == kDestinationPageUrl) {
    http_response->set_content(kDestinationHtml);
  } else if (request.relative_url == kShortTruncationPageUrl) {
    NSString* content = [NSString
        stringWithFormat:kTruncationTestPageTemplateHtml, kShortLinkHref,
                         kInitialPageDestinationLinkId, kShortImgTitle,
                         kLogoPageChromiumImageId];
    http_response->set_content(base::SysNSStringToUTF8(content));
  } else if (request.relative_url == kLongTruncationPageUrl) {
    NSString* content =
        [NSString stringWithFormat:kTruncationTestPageTemplateHtml,
                                   kLongLinkHref, kInitialPageDestinationLinkId,
                                   kLongImgTitle, kLogoPageChromiumImageId];
    http_response->set_content(base::SysNSStringToUTF8(content));
  } else if (request.relative_url == kJavaScriptPageUrl) {
    http_response->set_content(kJavaScriptPageHtml);
  } else if (request.relative_url == kMagnetPageUrl) {
    http_response->set_content(kMagnetPageHtml);
  } else if (request.relative_url == kLinkImagePageUrl) {
    NSString* content =
        [NSString stringWithFormat:kLinkImageHtml, kShortLinkHref,
                                   kInitialPageDestinationLinkId,
                                   kShortImgTitle, kLogoPageChromiumImageId];
    http_response->set_content(base::SysNSStringToUTF8(content));
  } else if (request.relative_url == kLongLinkPageURL) {
    NSString* content =
        [NSString stringWithFormat:kLongLinkTestPageTemplateHtml, kLongLinkHref,
                                   kInitialPageDestinationLongLinkText];
    http_response->set_content(base::SysNSStringToUTF8(content));
  } else {
    return nullptr;
  }

  return std::move(http_response);
}

// Long presses on `element_id` to trigger context menu.
void LongPressElement(const char* element_id) {
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:element_id],
                        true /* menu should appear */)];
}

// Taps on the web view to dismiss the context menu without using anything on
// it.
void ClearContextMenu() {
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];
}

// Taps on `context_menu_item_button` context menu item.
void TapOnContextMenuButton(id<GREYMatcher> context_menu_item_button) {
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      performAction:grey_tap()];
}

void RelaunchAppWithInactiveTabs2WeeksEnabled() {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kTabInactivityThreshold.name) + ":" +
      kTabInactivityThresholdParameterName + "/" +
      kTabInactivityThresholdTwoWeeksParam);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

}  // namespace

// Context menu tests for Chrome.
@interface ContextMenuTestCase : ChromeTestCase {
  std::unique_ptr<ScopedBlockPopupsPref> _blockPopupsPref;
  bool _setUpHistogramTesterCalled;
}

@end

@implementation ContextMenuTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabGroupsIPad);
  config.features_enabled.push_back(kModernTabStrip);
  config.features_enabled.push_back(kShareInWebContextMenuIOS);
  config.features_disabled.push_back(web::features::kSmoothScrollingDefault);
  return config;
}

- (void)setUp {
  [super setUp];
  _blockPopupsPref =
      std::make_unique<ScopedBlockPopupsPref>(CONTENT_SETTING_ALLOW);
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)setUpHistogramTester {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
  _setUpHistogramTesterCalled = true;
}

- (void)tearDown {
  if (_setUpHistogramTesterCalled) {
    GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                  @"Failed to release histogram tester.");
  }
  [super tearDown];
}

// Tests that selecting "Open Image" from the context menu properly opens the
// image in the current tab.
- (void)testOpenImageInCurrentTabFromContextMenu {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageButton());
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that selecting "Open Image in New Tab" from the context menu properly
// opens the image in a new background tab.
- (void)testOpenImageInNewTabFromContextMenu {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageInNewTabButton());

  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:1];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests "Open in New Tab" on context menu.
- (void)testContextMenuOpenInNewTab {
  // TODO(crbug.com/40706946): Test fails in some iPads.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iPad.");
  }
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kInitialPageDestinationLinkId);
  TapOnContextMenuButton(OpenLinkInNewTabButton());

  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey selectTabAtIndex:1];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText];

  // Verify url.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that the context menu is displayed for an image url.
- (void)testContextMenuDisplayedOnImage {
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [ChromeEarlGrey loadURL:imageURL];

  // Calculate a point inside the displayed image.
  CGFloat topInset = 0.0;
  if ([ChromeEarlGrey webStateWebViewUsesContentInset]) {
    topInset = [FullscreenAppInterface currentViewportInsets].top;
  }
  CGPoint pointOnImage = CGPointZero;
  // Offset by at least status bar height.
  pointOnImage.y = topInset + 25.0;
  pointOnImage.x = [ChromeEarlGrey webStateWebViewSize].width / 2.0;

  // Duration should match `kContextMenuLongPressDuration` as defined in
  // web_view_actions.mm.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:grey_longPressAtPointWithDuration(pointOnImage, 1.0)];

  TapOnContextMenuButton(OpenImageInNewTabButton());
  [ChromeEarlGrey waitForMainTabCount:2];

  [ChromeEarlGrey selectTabAtIndex:1];

  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests context menu title truncation cases.
- (void)testContextMenuTitleTruncation {
  const GURL shortTtileURL = self.testServer->GetURL(kShortTruncationPageUrl);
  [ChromeEarlGrey loadURL:shortTtileURL];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kLogoPageChromiumImageId);
  [[EarlGrey selectElementWithMatcher:grey_text(kShortImgTitle)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  LongPressElement(kInitialPageDestinationLinkId);
  // Links get prefixed with the hostname, so check for partial text match
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          kShortLinkHref)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  const GURL longTtileURL = self.testServer->GetURL(kLongTruncationPageUrl);
  [ChromeEarlGrey loadURL:longTtileURL];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kLogoPageChromiumImageId);
  [[EarlGrey selectElementWithMatcher:grey_text(kLongImgTitle)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  LongPressElement(kInitialPageDestinationLinkId);

  // But expect that some of the link is visible in the title.
  NSString* startOfTitle = [kLongLinkHref substringToIndex:30];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          startOfTitle)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();
}
// Tests that system touches are cancelled when the context menu is shown.
- (void)testContextMenuCancelSystemTouchesMetric {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageButton());
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that system touches were cancelled by ensuring the system text
  // selection callout is not displayed.
  [[EarlGrey selectElementWithMatcher:SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that the system selected text callout is displayed instead of the
// context menu when user long presses on plain text.
- (void)testContextMenuSelectedTextCallout {
  // Load the destination page directly because it has a plain text message on
  // it.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  [ChromeEarlGrey loadURL:destinationURL];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kDestinationPageTextId);
  // TODO(crbug.com/40191349): Xcode 13 gesture recognizers seem to get stuck
  // when the user longs presses on plain text.  For this test, disable EG
  // synchronization.
  ScopedSynchronizationDisabler disabler;
  // Verify that context menu is not shown.
  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      assertWithMatcher:grey_nil()];

  // Verify that system text selection callout is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SystemSelectionCalloutCopyButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // TODO(crbug.com/40191349): Tap to dismiss the system selection callout
  // buttons so tearDown doesn't hang when `disabler` goes out of scope.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:grey_tap()];
}

// Tests cancelling the context menu.
- (void)testDismissContextMenu {
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  // Display the context menu twice.
  for (NSInteger i = 0; i < 2; i++) {
    LongPressElement(kInitialPageDestinationLinkId);

    // Make sure the context menu appeared.
    [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
        assertWithMatcher:grey_notNil()];

    if ([ChromeEarlGrey isIPadIdiom]) {
      // Tap the tools menu to dismiss the popover.
      [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
          performAction:grey_tap()];
    }
    // Tap the drop shadow to dismiss the popover.
    chrome_test_util::TapAtOffsetOf(nil, 0, CGVectorMake(0.5, 0.95));

    // Make sure the context menu disappeared.
    ConditionBlock condition = ^{
      NSError* error = nil;
      [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
          assertWithMatcher:grey_nil()
                      error:&error];
      return error == nil;
    };

    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                   base::test::ios::kWaitForUIElementTimeout, condition),
               @"Waiting for the context menu to disappear");
  }

  // Display the context menu one last time.
  LongPressElement(kInitialPageDestinationLinkId);

  // Make sure the context menu appeared.
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      assertWithMatcher:grey_notNil()];
}

// Checks that all the options are displayed in the context menu.
- (void)testAppropriateContextMenu {
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kInitialPageDestinationLinkId);

  // Check the different buttons.
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKINNEWTABGROUP)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                   IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_COPY_LINK_ACTION_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_ancestor(grey_kindOfClassName(
                                       @"_UIContextMenuCell")),
                                   ContextMenuItemWithAccessibilityLabelId(
                                       IDS_IOS_SHARE_BUTTON_LABEL),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that "open in new window" shows up on a long press of a url link
// and that it actually opens in a new window.
- (void)testOpenLinkInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // Loads url in first window.
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL inWindowWithNumber:0];

  [ChromeEarlGrey waitForWebStateContainingText:kInitialPageDestinationLinkText
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  // Display the context menu.
  LongPressElement(kInitialPageDestinationLinkId);

  // Open link in new window.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OpenLinkInNewWindowButton()]
      performAction:grey_tap()];

  // Assert there's a second window with expected content.
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText
                             inWindowWithNumber:1];
}

// Checks that "open in new window" shows up on a long press of a url link
// and that it actually opens in a new window, and that when the link is in an
// incognito webstate, the newly opened webstate is also incognito.
- (void)testOpenIncognitoLinkInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  [ChromeEarlGrey openNewIncognitoTab];

  // Loads url in first window.
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL inWindowWithNumber:0];

  [ChromeEarlGrey waitForWebStateContainingText:kInitialPageDestinationLinkText
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  // Display the context menu.
  LongPressElement(kInitialPageDestinationLinkId);

  // Open link in new window.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OpenLinkInNewWindowButton()]
      performAction:grey_tap()];

  // Assert there's a second window with expected content.
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText
                             inWindowWithNumber:1];

  // Assert that the second window is incognito, and there are no non-incognito
  // tabs.
  [ChromeEarlGrey waitForIncognitoTabCount:1 inWindowWithNumber:1];
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:1];
}

// Checks that JavaScript links only have the "copy" option.
- (void)testJavaScriptLinks {
  const GURL initialURL = self.testServer->GetURL(kJavaScriptPageUrl);
  [ChromeEarlGrey loadURL:initialURL];

  LongPressElement(kInitialPageDestinationLinkId);

  // Check the different buttons.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_COPY_LINK_ACTION_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Make sure that the open action is not displayed.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_OPEN)]
      assertWithMatcher:grey_nil()];
}

// Checks that JavaScript links only have the "copy" option.
- (void)testMagnetLinks {
  const GURL initialURL = self.testServer->GetURL(kMagnetPageUrl);
  [ChromeEarlGrey loadURL:initialURL];

  LongPressElement(kInitialPageDestinationLinkId);

  // Check the different buttons.
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_COPY_LINK_ACTION_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that opening a tab in the background doesn't mark it as inactive.
// This is a regression test against crbug.com/1490604, where background tabs
// had an unset last active time, and thus were considered inactive on the next
// launch.
- (void)testOpenedInBackgroundStaysActiveAfterRelaunch {
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kInitialPageDestinationLinkId);
  TapOnContextMenuButton(OpenLinkInNewTabButton());

  [ChromeEarlGrey waitForMainTabCount:2];

  // There should be 2 active tabs and no inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  RelaunchAppWithInactiveTabs2WeeksEnabled();

  // Open the Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // There should still be 2 active tabs and no inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");
}

// Tests "Open in Tab Group" on context menu, when no group exists in the tab
// grid, the context menu should only show `Open in group` option, if a group
// exists in the tab grid, the option `Open in group` will become a submenu,
// tapping it will result in listing all the existing tab groups.
- (void)testContextMenuOpenInGroup {
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kInitialPageDestinationLinkId);
  TapOnContextMenuButton(OpenLinkInNewGroupButton());

  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridGroupCellAtIndex(
                                          1)] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText];

  // Verify url.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Open link in an existing tab group.
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];
  LongPressElement(kInitialPageDestinationLinkId);
  TapOnContextMenuButton(OpenLinkInGroupButton());
  TapOnContextMenuButton(OpenLinkInOneTabGroupButton());

  [ChromeEarlGrey waitForMainTabCount:3];
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kDestinationPageText];

  // Verify url.
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests "Share URL" action in the web context menu when long
// pressing a link in a web page, and tests that triggering the
// action does present the Share menu as expected.
- (void)testShareInWebContextMenu {
  [self setUpHistogramTester];
  const GURL pageURL = self.testServer->GetURL(kInitialPageUrl);
  NSString* pageTitle = base::SysUTF8ToNSString(pageURL.GetContent());
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kInitialPageDestinationLinkId);

  [ChromeEarlGrey verifyShareActionWithURL:pageURL pageTitle:pageTitle];

  // Ensure that UMA was logged correctly.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:13  // Number refering to
                       // SharingScenario::ShareInWebContextMenu
      forHistogram:@"Mobile.Share.EntryPoints"];
  if (error) {
    GREYFail([error description]);
  }
}

// Tests that one (and only one, meaning the menu title is not present) button
// partially containing the url is present and tapping on it shows an alert
// showing the full URL.
- (void)testShowFullURLInWebContextMenu {
  const GURL pageURL = self.testServer->GetURL(kLongLinkPageURL);
  const GURL longURL =
      self.testServer->GetURL(base::SysNSStringToUTF8(kLongLinkHref));

  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLongLinkText];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kInitialPageDestinationLongLinkID);

  std::u16string formattedURL = url_formatter::FormatUrl(longURL);
  NSString* stringURL = base::SysUTF16ToNSString(formattedURL);

  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuButtonContainingText([stringURL
                     substringToIndex:20])] performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_ancestor(grey_accessibilityID(
                                              @"AlertAccessibilityIdentifier")),
                                          grey_text(stringURL), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests "Share Image" action in the web context menu when long
// pressing an image in a web page, and tests that triggering the
// action does present the Share menu as expected.
- (void)testShareImageInWebContextMenu {
  [self setUpHistogramTester];

  const GURL shortTitleURL = self.testServer->GetURL(kShortTruncationPageUrl);

  [ChromeEarlGrey loadURL:shortTitleURL];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  LongPressElement(kLogoPageChromiumImageId);
  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [[EarlGrey selectElementWithMatcher:grey_text(kShortImgTitle)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kContextMenuImagePreviewAccessibilityIdentifier)];

  [ChromeEarlGrey verifyShareActionWithURL:shortTitleURL pageTitle:@"Image"];
  // Ensure that UMA was logged correctly.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:13  // Number refering to
                       // SharingScenario::ShareInWebContextMenu
      forHistogram:@"Mobile.Share.EntryPoints"];
  if (error) {
    GREYFail([error description]);
  }

  error = [MetricsAppInterface
       expectCount:1
         forBucket:1  // success
      forHistogram:@"IOS.ContextMenu.ImagePreviewDisplayed"];
  if (error) {
    GREYFail([error description]);
  }
}

@end

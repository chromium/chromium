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
#import "components/data_sharing/public/features.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/context_menu/ui_bundled/constants.h"
#import "ios/chrome/browser/enterprise/data_controls/test/data_controls_app_interface.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/fullscreen_app_interface.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/test/reader_mode_app_interface.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/components/enterprise/data_controls/clipboard_enums.h"
#import "ios/components/enterprise/data_controls/features.h"
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

// Returns an ElementSelector for the chromium image on the logo page.
ElementSelector* LogoPageChromiumImageIdSelector() {
  return [ElementSelector selectorWithElementID:kLogoPageChromiumImageId];
}

// Returns an ElementSelector for the link to the destination page on the
// initial page.
ElementSelector* InitialPageDestinationLinkIdSelector() {
  return [ElementSelector selectorWithElementID:kInitialPageDestinationLinkId];
}

// Returns an ElementSelector for the long link to the destination page.
ElementSelector* InitialPageDestinationLongLinkIDSelector() {
  return
      [ElementSelector selectorWithElementID:kInitialPageDestinationLongLinkID];
}

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
     "characters, so formulated as to test the very limits of the context "
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

// Returns an ElementSelector for long pressing the first link in the page.
ElementSelector* ElementSelectorToLongPressLink() {
  return [ElementSelector selectorWithCSSSelector:"a"];
}

// Returns an ElementSelector for long pressing the first image in the page.
ElementSelector* ElementSelectorToLongPressImage() {
  return [ElementSelector selectorWithCSSSelector:"img"];
}

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

// Matcher for the open link in group button in the context menu.
id<GREYMatcher> CopyImageButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_COPYIMAGE);
}

// Matcher for the copy link button in the context menu.
id<GREYMatcher> CopyLinkButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_COPY_LINK_ACTION_TITLE);
}

// Matcher for the open link in an existing tab group (a group containing one
// tab) button in the context menu.
id<GREYMatcher> OpenLinkInOneTabGroupButton() {
  return ContextMenuItemWithAccessibilityLabel(
      l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, 1));
}

// Matcher for the share button in the context menu.
id<GREYMatcher> ShareButton() {
  return grey_allOf(
      grey_ancestor(grey_kindOfClassName(@"_UIContextMenuCell")),
      ContextMenuItemWithAccessibilityLabelId(IDS_IOS_SHARE_BUTTON_LABEL), nil);
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

// Taps on the web view to dismiss the context menu without using anything on
// it.
void ClearContextMenu() {
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];
}

// Taps on `context_menu_item_button` context menu item.
void TapOnContextMenuButton(id<GREYMatcher> context_menu_item_button) {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:context_menu_item_button];
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      performAction:grey_tap()];
}

void RelaunchApp() {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
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
  config.features_enabled.push_back(
      data_sharing::features::kDataSharingFeature);
  config.features_enabled.push_back(kEnableReaderMode);
  config.features_enabled.push_back(kEnableReaderModeInUS);
  config.features_disabled.push_back(web::features::kSmoothScrollingDefault);

  if ([self isRunningTest:@selector(testCopyImageBlockedByPolicy)] ||
      [self isRunningTest:@selector(testCopyImageWarnByPolicyProceed)] ||
      [self isRunningTest:@selector(testCopyImageWarnByPolicyCancel)] ||
      [self isRunningTest:@selector(testCopyLinkBlockedByPolicy)] ||
      [self isRunningTest:@selector(testCopyLinkWarnByPolicyProceed)] ||
      [self isRunningTest:@selector(testCopyLinkWarnByPolicyCancel)] ||
      [self isRunningTest:@selector(testShareLinkHiddenByPolicy)] ||
      [self isRunningTest:@selector(testShareImageHiddenByPolicy)]) {
    config.features_enabled.push_back(
        data_controls::kEnableClipboardDataControlsIOS);
  }
  if ([self isRunningTest:@selector(testShowFullURLInWebContextMenu)]) {
    config.features_disabled.push_back(kIOSWebContextMenuNewTitle);
  }

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
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  _setUpHistogramTesterCalled = true;
}

- (void)tearDownHelper {
  if (_setUpHistogramTesterCalled) {
    chrome_test_util::GREYAssertErrorNil(
        [MetricsAppInterface releaseHistogramTester]);
  }
  [super tearDownHelper];
}

// Tests that selecting "Copy Image" from the context menu properly copies the
// image in the pasteboard.
- (void)testCopyImageIntoPasteboard {
  [self setUpHistogramTester];
  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

  TapOnContextMenuButton(CopyImageButton());
  GREYCondition* copyCondition =
      [GREYCondition conditionWithName:@"Image copied condition"
                                 block:^BOOL {
                                   return [ChromeEarlGrey pasteboardHasImages];
                                 }];

  // Wait for the image to be copied.
  GREYAssertTrue([copyCondition waitWithTimeout:5], @"Copying image failed");

  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       data_controls::ClipboardSource::kCustomAction)
      forHistogram:@"IOS.WebState.Clipboard.Copy.Source"];
  if (error) {
    GREYFail([error description]);
  }
  error =
      [MetricsAppInterface expectCount:1
                             forBucket:1  // true
                          forHistogram:@"IOS.WebState.Clipboard.Copy.Outcome"];
  if (error) {
    GREYFail([error description]);
  }

  [ChromeEarlGrey clearPasteboard];
}

// Tests that copying an image is blocked when the DataControlsRule policy is
// set to do so.
- (void)testCopyImageBlockedByPolicy {
  [self setUpHistogramTester];
  [DataControlsAppInterface setBlockCopyRule];

  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

  TapOnContextMenuButton(CopyImageButton());

  // Check that the snackbar is shown.
  id<GREYMatcher> snackbarMessage = grey_text(
      l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:snackbarMessage];

  // Check that the image was not copied.
  GREYAssertFalse([ChromeEarlGrey pasteboardHasImages],
                  @"Image should not have been copied");

  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       data_controls::ClipboardSource::kCustomAction)
      forHistogram:@"IOS.WebState.Clipboard.Copy.Source"];
  if (error) {
    GREYFail([error description]);
  }
  error =
      [MetricsAppInterface expectCount:1
                             forBucket:0  // false
                          forHistogram:@"IOS.WebState.Clipboard.Copy.Outcome"];
  if (error) {
    GREYFail([error description]);
  }

  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying an image is allowed after the user proceeds through the
// warning triggered by DataControlRules policy.
- (void)testCopyImageWarnByPolicyProceed {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

  TapOnContextMenuButton(CopyImageButton());

  // Tap the "Copy anyways" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]
      performAction:grey_tap()];

  // Check that the image was copied.
  GREYCondition* copyCondition =
      [GREYCondition conditionWithName:@"Image copied condition"
                                 block:^BOOL {
                                   return [ChromeEarlGrey pasteboardHasImages];
                                 }];
  GREYAssertTrue([copyCondition waitWithTimeout:5], @"Copying image failed");
  [ChromeEarlGrey clearPasteboard];
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying an image is cancelled when the user cancels on the warning
// triggered by DataControlRules policy.
- (void)testCopyImageWarnByPolicyCancel {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

  TapOnContextMenuButton(CopyImageButton());

  // Tap the "cancel" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]
      performAction:grey_tap()];
  // Check that the image was not copied.
  GREYAssertFalse([ChromeEarlGrey pasteboardHasImages],
                  @"Image should not have been copied");
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that selecting "Copy Link" from the context menu properly copies the
// link in the pasteboard.
- (void)testCopyLink {
  [self setUpHistogramTester];
  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

  TapOnContextMenuButton(CopyLinkButton());

  // Check that the link was copied.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:@"Link copied condition"
                  block:^BOOL {
                    return [ChromeEarlGrey pasteboardURL] == destinationURL;
                  }];
  GREYAssertTrue([copyCondition waitWithTimeout:5], @"Copying link failed");

  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       data_controls::ClipboardSource::kCustomAction)
      forHistogram:@"IOS.WebState.Clipboard.Copy.Source"];
  if (error) {
    GREYFail([error description]);
  }
  error =
      [MetricsAppInterface expectCount:1
                             forBucket:1  // true
                          forHistogram:@"IOS.WebState.Clipboard.Copy.Outcome"];
  if (error) {
    GREYFail([error description]);
  }

  [ChromeEarlGrey clearPasteboard];
}

// Tests that copying a link is blocked when the DataControlsRule policy is
// set to do so.
- (void)testCopyLinkBlockedByPolicy {
  [self setUpHistogramTester];
  [DataControlsAppInterface setBlockCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

  TapOnContextMenuButton(CopyLinkButton());

  // Check that the snackbar is shown.
  id<GREYMatcher> snackbarMessage = grey_text(
      l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:snackbarMessage];

  // Check that the link was not copied.
  GREYAssertTrue([ChromeEarlGrey pasteboardURL].is_empty(),
                 @"Link should not have been copied");

  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       data_controls::ClipboardSource::kCustomAction)
      forHistogram:@"IOS.WebState.Clipboard.Copy.Source"];
  if (error) {
    GREYFail([error description]);
  }
  error =
      [MetricsAppInterface expectCount:1
                             forBucket:0  // false
                          forHistogram:@"IOS.WebState.Clipboard.Copy.Outcome"];
  if (error) {
    GREYFail([error description]);
  }

  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying a link is allowed after the user proceeds through the
// warning triggered by DataControlRules policy.
- (void)testCopyLinkWarnByPolicyProceed {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

  TapOnContextMenuButton(CopyLinkButton());

  // Tap the "Copy anyways" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]
      performAction:grey_tap()];

  // Check that the link was copied.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:@"Link copied condition"
                  block:^BOOL {
                    return [ChromeEarlGrey pasteboardURL] == destinationURL;
                  }];
  GREYAssertTrue([copyCondition waitWithTimeout:5], @"Copying link failed");
  [ChromeEarlGrey clearPasteboard];
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that copying a link is cancelled when the user cancels on the warning
// triggered by DataControlRules policy.
- (void)testCopyLinkWarnByPolicyCancel {
  [DataControlsAppInterface setWarnCopyRule];

  [ChromeEarlGrey clearPasteboard];
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

  TapOnContextMenuButton(CopyLinkButton());

  // Tap the "cancel" button on the warning dialog.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AlertItemWithAccessibilityLabelId(
                     IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]
      performAction:grey_tap()];
  // Check that the link was not copied.
  GREYAssertTrue([ChromeEarlGrey pasteboardURL].is_empty(),
                 @"Link should not have been copied");
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that selecting "Open Image" from the context menu properly opens the
// image in the current tab.
- (void)testOpenImageInCurrentTabFromContextMenu {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

  [[EarlGrey selectElementWithMatcher:grey_text(kShortImgTitle)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

  // Links get prefixed with the hostname, so check for partial text match
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          kShortLinkHref)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  const GURL longTtileURL = self.testServer->GetURL(kLongTruncationPageUrl);
  [ChromeEarlGrey loadURL:longTtileURL];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

  [[EarlGrey selectElementWithMatcher:grey_text(kLongImgTitle)]
      assertWithMatcher:grey_notNil()];
  ClearContextMenu();

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:
          [ElementSelector selectorWithElementID:kDestinationPageTextId]];

  // Verify that context menu is not shown.
  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      assertWithMatcher:grey_nil()];

  // Verify that system text selection callout is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SystemSelectionCalloutCopyButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
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
    [ChromeEarlGreyUI
        longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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
  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

  [[EarlGrey selectElementWithMatcher:ShareButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that "open in new window" shows up on a long press of a url link
// and that it actually opens in a new window.
// TODO(crbug.com/441761691): Re-enable the test.
- (void)DISABLED_testOpenLinkInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Loads url in first window.
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL inWindowWithNumber:0];

  [ChromeEarlGrey waitForWebStateContainingText:kInitialPageDestinationLinkText
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  // Display the context menu.
  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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
// TODO(crbug.com/441761691): Test is flaky on iPad simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testOpenIncognitoLinkInNewWindow \
  FLAKY_testOpenIncognitoLinkInNewWindow
#else
#define MAYBE_testOpenIncognitoLinkInNewWindow \
  testOpenIncognitoLinkInNewWindow
#endif
- (void)MAYBE_testOpenIncognitoLinkInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  [ChromeEarlGrey openNewIncognitoTab];

  // Loads url in first window.
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL inWindowWithNumber:0];

  [ChromeEarlGrey waitForWebStateContainingText:kInitialPageDestinationLinkText
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];

  // Display the context menu.
  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

  TapOnContextMenuButton(OpenLinkInNewTabButton());

  [ChromeEarlGrey waitForMainTabCount:2];

  // There should be 2 active tabs and no inactive tab.
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");
  GREYAssertTrue([ChromeEarlGrey inactiveTabCount] == 0,
                 @"Inactive tab count should be 0");

  RelaunchApp();

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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
  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

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

// Tests that the "Share" button is not shown when the DataControlsRule policy
// is set to do so.
- (void)testShareLinkHiddenByPolicy {
  [DataControlsAppInterface setBlockCopyRule];

  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLinkIdSelector()];

  // Check that the "Share" button is not visible.
  [[EarlGrey selectElementWithMatcher:ShareButton()]
      assertWithMatcher:grey_nil()];
  [DataControlsAppInterface clearDataControlRules];
}

// Tests that the "Share" button is not shown for an image when the
// DataControlsRule policy is set to do so.
- (void)testShareImageHiddenByPolicy {
  [DataControlsAppInterface setBlockCopyRule];

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

  // Check that the "Share" button is not visible.
  [[EarlGrey selectElementWithMatcher:ShareButton()]
      assertWithMatcher:grey_nil()];
  [DataControlsAppInterface clearDataControlRules];
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

  [ChromeEarlGreyUI
      longPressElementOnWebView:InitialPageDestinationLongLinkIDSelector()];

  std::u16string formattedURL = url_formatter::FormatUrl(longURL);
  NSString* stringURL = base::SysUTF16ToNSString(formattedURL);

  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuButtonContainingText([stringURL
                     substringToIndex:20])] performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_ancestor(grey_accessibilityID(
                                              @"AlertAccessibilityIdentifier")),
                                          chrome_test_util::ContainsPartialText(
                                              stringURL),
                                          nil)]
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

  [ChromeEarlGreyUI
      longPressElementOnWebView:LogoPageChromiumImageIdSelector()];

  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [[EarlGrey selectElementWithMatcher:grey_text(kShortImgTitle)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kContextMenuImagePreviewAccessibilityIdentifier)];

  // On iOS 26, the name of the image (chromium_logo.png in this case) is used
  // as a page title instead of "Image".
  NSString* pageTitle =
      base::ios::IsRunningOnIOS26OrLater() ? @"chromium_logo" : @"Image";
  [ChromeEarlGrey verifyShareActionWithURL:shortTitleURL pageTitle:pageTitle];
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

// Tests that opening the context menu for a link in Reading mode
// displays all options.
- (void)testOpenContextMenuFromReadingMode {
  [self setUpHistogramTester];

  const GURL initialURL = self.testServer->GetURL("/article.html");
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Wait for Reader Mode UI to appear on-screen.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForWebStateContainingElement:ElementSelectorToLongPressLink()];

  // Open the context menu and tap on an action.
  [ChromeEarlGreyUI longPressElementOnWebView:ElementSelectorToLongPressLink()];
  TapOnContextMenuButton(OpenLinkInNewTabButton());

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

  // Ensure that UMA was logged correctly.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:36  // Number refering to MenuScenarioHistogram enum.
      forHistogram:@"Mobile.ContextMenu.EntryPoints"];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
       expectCount:1
         forBucket:0  // Number refering to MenuActionType enum.
      forHistogram:@"Mobile.ContextMenu.ReaderModeLink.Actions"];
  if (error) {
    GREYFail([error description]);
  }
}

// Tests that the context menu is displayed for an image url in Reading mode.
- (void)testContextMenuDisplayedOnImageForReadingMode {
  [self setUpHistogramTester];

  const GURL pageURL = self.testServer->GetURL("/article.html");
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Wait for Reader Mode UI to appear on-screen.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForWebStateContainingElement:ElementSelectorToLongPressImage()];

  [ChromeEarlGreyUI
      longPressElementOnWebView:ElementSelectorToLongPressImage()];
  TapOnContextMenuButton(OpenImageButton());
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Ensure that UMA was logged correctly.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:34  // Number refering to MenuScenarioHistogram enum.
      forHistogram:@"Mobile.ContextMenu.EntryPoints"];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
       expectCount:1
         forBucket:20  // Number refering to MenuActionType enum.
      forHistogram:@"Mobile.ContextMenu.ReaderModeImage.Actions"];
  if (error) {
    GREYFail([error description]);
  }
}

@end

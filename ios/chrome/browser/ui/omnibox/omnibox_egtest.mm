// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "build/build_config.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_earl_grey.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_test_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::WebViewMatcher;

namespace {

// Web page 1.
const char kPage1[] = "This is the first page";
const char kPage1Title[] = "Title 1";
const char kPage1URL[] = "/page1.html";

// Web page 2.
const char kPage2[] = "This is the second page";
const char kPage2Title[] = "Title 2";
const char kPage2URL[] = "/page2.html";

// Web page to try X-Client-Data header.
const char kHeaderPageURL[] = "/page3.html";
const char kHeaderPageSuccess[] = "header found!";
const char kHeaderPageFailure[] = "header failure";

// Path to a page containing the chromium logo and the text `kLogoPageText`.
const char kLogoPagePath[] = "/chromium_logo_page.html";
// The text of the message on the logo page.
const char kLogoPageText[] = "Page with some text and the chromium logo image.";
// The DOM element ID of the chromium image on the logo page.
const char kLogoPageChromiumImageId[] = "chromium_image";
// Y offset to tap on the middle of the text.
const CGFloat kOmniboxTextFieldMidY = 18;
// X offset to tap on the beginning of the text, tapping on the left edge
// inserts a carret instead of showing the menu.
const CGFloat kOmniboxTextFieldOffsetX = 30;

// Defocus the omnibox.
void DefocusOmnibox() {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // "escape" is a hardcoded key string in hardware_keyboard_util that maps to
    // a HIDUsageCode.
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"escape" flags:0];
  } else {
    id<GREYMatcher> cancel_button =
        grey_accessibilityID(kToolbarCancelOmniboxEditButtonIdentifier);
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(cancel_button,
                                            grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];
  }
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Taps the pre edit text in the omnibox.
void TapOnPreEditTextInOmnibox() {
  // TODO(crbug.com/40266963): Find a better way to tap on the selected url.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tapAtPoint(CGPointMake(kOmniboxTextFieldOffsetX,
                                                kOmniboxTextFieldMidY))];
}

// Provides responses for the different pages.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kPage1URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage1Title) +
        "</title></head><body>" + std::string(kPage1) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kPage2URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage2Title) +
        "</title></head><body>" + std::string(kPage2) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kHeaderPageURL) {
    std::string result = kHeaderPageFailure;
    if (base::Contains(request.headers, "X-Client-Data")) {
      result = kHeaderPageSuccess;
    }
    http_response->set_content("<html><body>" + result + "</body></html>");
    return std::move(http_response);
  }

  return nil;
}

// Returns Visit Copied Link button matcher from UIMenuController.
id<GREYMatcher> VisitCopiedLinkButton() {
  NSString* a11yLabelCopiedLink =
      l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK);
  return grey_allOf(grey_accessibilityLabel(a11yLabelCopiedLink),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

// Returns Paste button matcher from UIMenuController.
id<GREYMatcher> PasteButton() {
  NSString* a11yLabelPaste = @"Paste";
  return grey_allOf(grey_accessibilityLabel(a11yLabelPaste),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

// Returns Select button from UIMenuController.
id<GREYMatcher> SelectButton() {
  NSString* a11yLabelSelect = @"Select";
  return grey_allOf(grey_accessibilityLabel(a11yLabelSelect),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

// Returns Select All button from UIMenuController.
id<GREYMatcher> SelectAllButton() {
  NSString* a11yLabelSelectAll = @"Select All";
  return grey_allOf(grey_accessibilityLabel(a11yLabelSelectAll),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

// Returns Cut button from UIMenuController.
id<GREYMatcher> CutButton() {
  NSString* a11yLabelCut = @"Cut";
  return grey_allOf(grey_accessibilityLabel(a11yLabelCut),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

// Returns Search Copied Text button from UIMenuController.
id<GREYMatcher> SearchCopiedTextButton() {
  NSString* a11yLabelCopiedText =
      l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_TEXT);
  return grey_allOf(grey_accessibilityLabel(a11yLabelCopiedText),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

// Returns Search Copied Image button from UIMenuController.
id<GREYMatcher> SearchCopiedImageButton() {
  NSString* a11yLabelCopiedImage =
      l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_IMAGE);
  return grey_allOf(grey_accessibilityLabel(a11yLabelCopiedImage),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

// Returns Clear button at the trailing edge of the omnibox's text field.
id<GREYMatcher> ClearButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_ACCNAME_CLEAR_TEXT);
}

#pragma mark LocationBar context menu buttons

// LocationBar context menu buttons can be showed in different orders depending
// on the omnibox position (top or bottom toolbar). The context menu is
// configured with UIContextMenuConfigurationElementOrderPriority that will
// create the buttons two times (one for earch ordering) with one of them
// hidden. The added `grey_hidden(NO)` prevents conflict when matching with
// GREYMatcher.

// Returns Copy button from the location bar context menu.
id<GREYMatcher> CopyContextMenuButton() {
  int copyButtonId = IDS_IOS_SHARE_MENU_COPY;
  if ([ChromeEarlGrey isBottomOmniboxAvailable]) {
    copyButtonId = IDS_IOS_COPY_LINK_ACTION_TITLE;
  }
  return grey_allOf(
      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(copyButtonId),
      grey_accessibilityTrait(UIAccessibilityTraitButton), grey_hidden(NO),
      nil);
}

// Returns Visit Copied Link button from the location bar context menu.
id<GREYMatcher> VisitCopiedLinkContextMenuButton() {
  return grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                        IDS_IOS_VISIT_COPIED_LINK),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_hidden(NO), nil);
}

// Returns Search Copied Text button from the location bar context menu.
id<GREYMatcher> SearchCopiedTextContextMenuButton() {
  return grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                        IDS_IOS_SEARCH_COPIED_TEXT),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_hidden(NO), nil);
}

// Returns Search Copied Image button from the location bar context menu.
id<GREYMatcher> SearchCopiedImageContextMenuButton() {
  return grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                        IDS_IOS_SEARCH_COPIED_IMAGE),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_hidden(NO), nil);
}

// Returns Search Copied Image button from the location bar context menu.
id<GREYMatcher> SearchCopiedImageWithLensContextMenuButton() {
  return grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                        IDS_IOS_SEARCH_COPIED_IMAGE_WITH_LENS),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_hidden(NO), nil);
}

// Taps the fake omnibox and waits for the real omnibox to be visible.
void FocusFakebox() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

}  //  namespace

@interface OmniboxTestCase : ChromeTestCase {
  GURL _URL1;
}

@end

@implementation OmniboxTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  _URL1 = self.testServer->GetURL(kPage1URL);

  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey clearBrowsingHistory];

  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
}

- (void)tearDown {
  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
  [super tearDown];
}

#pragma mark - Helpers

// Navigates to Page 1 in a tab and waits for it to load.
- (void)openPage1 {
  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
}

// Copies image from `kLogoPagePath` into the clipboard using web context menu
// interactions.
- (void)copyImageIntoClipboard {
  [ChromeEarlGrey clearPasteboard];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLogoPagePath)];
  [ChromeEarlGrey waitForWebStateContainingText:kLogoPageText];
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector
                            selectorWithElementID:kLogoPageChromiumImageId],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_COPYIMAGE)]
      performAction:grey_tap()];

  GREYCondition* copyCondition =
      [GREYCondition conditionWithName:@"Image copied condition"
                                 block:^BOOL {
                                   return [ChromeEarlGrey pasteboardHasImages];
                                 }];
  // Wait for copy to happen or timeout after 5 seconds.
  GREYAssertTrue([copyCondition waitWithTimeout:5], @"Copying image failed");
}

// Tests that the XClientData header is sent when navigating to
// https://google.com through the omnibox.
- (void)testXClientData {
  // TODO(crbug.com/40145916) This test is flakily because of a DCHECK in
  // ios/web.  Clearing browser history first works around the problem, but
  // shouldn't be necessary otherwise.  Remove once the bug is fixed.
  [ChromeEarlGrey clearBrowsingHistory];

  // Rewrite the google URL to localhost URL.
  [OmniboxAppInterface rewriteGoogleURLToLocalhost];

  // Force variations to send the requests.
  GREYAssert([OmniboxAppInterface forceVariationID:100],
             @"Variation not enabled.");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];

  // The headers are only sent with https requests.
  GURL::Replacements httpsReplacements;
  httpsReplacements.SetSchemeStr(url::kHttpsScheme);

  NSString* URL = base::SysUTF8ToNSString(
      self.testServer->GetURL("www.google.com", kHeaderPageURL)
          .ReplaceComponents(httpsReplacements)
          .spec());

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(URL)];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  [ChromeEarlGrey waitForWebStateContainingText:kHeaderPageSuccess];
}

#pragma mark - Omnibox Menu Paste to Search

// Tests that Visit Copied Link, Search Copied Text, Search Copied Image and
// Paste menu buttons are not shown with an empty Clipboard.
- (void)testOmniboxMenuEmptyPasteboard {
  FocusFakebox();

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  // Pressing should not allow pasting when pasteboard is empty.
  // Verify that system text selection callout is not displayed.
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchCopiedTextButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchCopiedImageButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasteButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that Search Copied Text menu button is shown with text in the clipboard
// and is starting a search.
- (void)testOmniboxMenuPasteTextToSearch {
  FocusFakebox();
  NSString* textToSearch = @"TextToCopy";
  // Copy text in clipboard.
  [ChromeEarlGrey copyTextToPasteboard:textToSearch];
  // Tap Search Copied Text menu button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  // Wait for UIMenuController to appear or timeout after 2 seconds.
  GREYCondition* SearchTextButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Search Copied Text button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:SearchCopiedTextButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([SearchTextButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Search Copied Text button display failed");
  [[EarlGrey selectElementWithMatcher:SearchCopiedTextButton()]
      performAction:grey_tap()];
  // Check that the omnibox contains the copied text.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            base::SysNSStringToUTF8(textToSearch))];
}

// Tests that Visit Copied Link menu button is shown with a link in the
// clipboard and is visiting the URL.
- (void)testOmniboxMenuPasteURLToSearch {
  FocusFakebox();
  // Copy URL into clipboard.
  [ChromeEarlGrey copyTextToPasteboard:base::SysUTF8ToNSString(_URL1.spec())];
  // Tap Visit Copied Link menu button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  // Wait for UIMenuController to appear or timeout after 2 seconds.
  GREYCondition* VisitLinkButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Visit Copied Link button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:VisitCopiedLinkButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([VisitLinkButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Visit Copied Link button display failed");
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkButton()]
      performAction:grey_tap()];
  // Verify that the page is loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
}

// Tests that Search Copied Image menu button is shown with an image in the
// clipboard and is starting an image search.
// TODO(crbug.com/40928559): Fix flakiness and re-enable.
- (void)DISABLED_testOmniboxMenuPasteImageToSearch {
  [self copyImageIntoClipboard];

  // Wait for the context menu to dismiss, so the omnibox can be tapped.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::DefocusedLocationView()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  // Wait for UIMenuController to appear or timeout after 2 seconds.
  GREYCondition* SearchImageButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Search Copied Image button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:SearchCopiedImageButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([SearchImageButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Search Copied Image button display failed");
  [[EarlGrey selectElementWithMatcher:SearchCopiedImageButton()]
      performAction:grey_tap()];

  // Check that the omnibox started a google search.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText("google")];
}

@end

#pragma mark - Steady state tests

@interface LocationBarSteadyStateTestCase : ChromeTestCase {
  GURL _URL1;
  GURL _URL2;
}

@end

@implementation LocationBarSteadyStateTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  _URL1 = self.testServer->GetURL(kPage1URL);
  _URL2 = self.testServer->GetURL(kPage2URL);

  [ChromeEarlGrey clearBrowsingHistory];

  // Clear the pasteboard in case there is a URL copied.
  UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];

  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
}

- (void)tearDown {
  [ChromeEarlGrey setBoolValue:NO forLocalStatePref:prefs::kBottomOmnibox];
  [super tearDown];
}

// Tapping on steady view starts editing.
- (void)testTapSwitchesToEditing {
  [self openPage1];

  [ChromeEarlGreyUI focusOmnibox];
  [self checkLocationBarEditState];
}

// Tests that in compact, a share button is visible.
// Voice search is not enabled on the bots, so the voice search button is
// not tested here.
- (void)testTrailingButton {
  [self openPage1];

  if ([ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}
// Tests that a security info icon is visible on a web page.
- (void)testSecurityInfoIcon {
  [self openPage1];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::PageSecurityInfoIndicator()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that location bar shows the current URL in a short form.
- (void)testLocationBarUrl {
  [self openPage1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:chrome_test_util::LocationViewContainingText(
                            _URL1.host())];
}

- (void)testCopyPaste {
  [self openPage1];

  // Long pressing should allow copying.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];

  // Verify that the Copy button is displayed.
  [[EarlGrey selectElementWithMatcher:CopyContextMenuButton()]
      assertWithMatcher:grey_notNil()];

  // Pressing should not allow pasting when pasteboard is empty.
  // Verify that system text selection callout is not displayed.
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkContextMenuButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchCopiedTextContextMenuButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchCopiedImageContextMenuButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:SearchCopiedImageWithLensContextMenuButton()]
      assertWithMatcher:grey_nil()];

  [self checkLocationBarSteadyState];

  // Tapping it should copy the URL.
  [[EarlGrey selectElementWithMatcher:CopyContextMenuButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyStringCopied:base::SysUTF8ToNSString(_URL1.spec())];

  // Go to another web page.
  [self openPage2];

  // Visit copied link should now be available.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkContextMenuButton()]
      assertWithMatcher:grey_notNil()];

  [self checkLocationBarSteadyState];

  // Tapping it should navigate to Page 1.
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkContextMenuButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
}

- (void)testDismissesEditMenu {
  [self openPage1];

  // Long pressing should open edit menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:CopyContextMenuButton()]
      assertWithMatcher:grey_notNil()];

  // Dismiss context menu.
  GREYAssertTrue([ChromeEarlGreyUI dismissContextMenuIfPresent],
                 @"Failed to dismiss context menu.");

  GREYCondition* contextMenuDismissed = [GREYCondition
      conditionWithName:@"Wait for context menu to be dismissed"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey selectElementWithMatcher:CopyContextMenuButton()]
                        assertWithMatcher:grey_nil()
                                    error:&error];
                    return error == nil;
                  }];

  // Verify that the context menu disappeared.
  GREYAssertTrue([contextMenuDismissed
                     waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                         .InSecondsF()],
                 @"Context menu is still visible.");
}

// Tests that the omnibox defocuses when switching to another tab.
- (void)testOmniboxDefocusesOnTabSwitch {
  [ChromeEarlGrey closeAllTabs];

  [ChromeEarlGrey openNewTab];
  [self openPage1];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];
  [self openPage2];

  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"Obama")];

  // The popup should open.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      assertWithMatcher:grey_notNil()];

  // Switch to the first tab.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // The omnibox shouldn't be focused and the popup should be closed.
  [self checkLocationBarSteadyState];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the omnibox defocuses when switching to another incognito tab.
- (void)testOmniboxDefocusesOnTabSwitchIncognito {
  [ChromeEarlGrey closeAllTabs];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  [self openPage1];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:2];
  [self openPage2];

  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"Obama")];

  // The popup should open.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      assertWithMatcher:grey_notNil()];

  // Switch to the first tab.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // The omnibox shouldn't be focused and the popup should be closed.
  [self checkLocationBarSteadyState];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      assertWithMatcher:grey_notVisible()];

  // Exit incognito.
  [ChromeEarlGrey closeAllTabs];
}

#pragma mark - Helpers

// Navigates to Page 1 in a tab and waits for it to load.
- (void)openPage1 {
  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
}

// Navigates to Page 2 in a tab and waits for it to load.
- (void)openPage2 {
  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];
}

// Checks that the location bar is currently in steady state.
- (void)checkLocationBarSteadyState {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the location bar is currently in edit state.
- (void)checkLocationBarEditState {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end

#pragma mark - Pre-edit state tests

@interface LocationBarPreEditStateTestCase : ChromeTestCase
@end

@implementation LocationBarPreEditStateTestCase {
  GURL _URL;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey clearBrowsingHistory];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  _URL = self.testServer->GetURL(kPage1URL);

  // Clear the pasteboard in case there is a URL copied.
  [ChromeEarlGrey clearPasteboard];
}

- (void)tearDown {
  // Clear the pasteboard after every test.
  [ChromeEarlGrey clearPasteboard];
  [super tearDown];
}

// Tests that tapping on steady view on webpage and starting typing a query
// would clear the page url and have instead the typed query in the omnibox text
// field.
- (void)testTypingOnPreEditState {
  // Load a web page.
  [ChromeEarlGrey loadURL:_URL];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(_URL.GetContent())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"hello")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("hello")];
}

// Tests that tapping on omnibox in pre-edit state in SRP or webpage would
// display a callout menu with cut,copy and paste (if there is a text on the
// pasteboard).
- (void)testTapOmniboxOnPreEditState {
  // Load a web page.
  [ChromeEarlGrey loadURL:_URL];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Focus Omnibox.
  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Add a text to the pasteBoard (So that we have the paste button displayed).
  [ChromeEarlGrey copyTextToPasteboard:@"hello"];

  // Tap on Omnibox on pre edit state.
  TapOnPreEditTextInOmnibox();

  // Wait for callout copy button to be displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::SystemSelectionCalloutCopyButton()];

  // Verify that system text cut callout is displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCutButton()]
      assertWithMatcher:grey_notNil()];

  // Verify that system text paste callout is displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutPasteButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests that Cut callout button would erase the current url and copy it on the
// pasteboard.
- (void)testCutCalloutButton {
  // Load a web page.
  [ChromeEarlGrey loadURL:_URL];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Focus Omnibox.
  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on Omnibox on pre edit state.
  TapOnPreEditTextInOmnibox();

  // Wait for callout cut button to be displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::SystemSelectionCalloutCutButton()];

  // Tapping on Cut.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCutButton()]
      performAction:grey_tap()];

  // Omnibox now should be empty.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];
}

// Tests that Paste callout button would erase the current url and replace it
// with text on pasteboard.
- (void)testPasteCalloutButton {
  // Load a web page.
  [ChromeEarlGrey loadURL:_URL];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Focus Omnibox.
  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Add a text to the pasteBoard (So that we have the paste button displayed).
  [ChromeEarlGrey copyTextToPasteboard:@"hello"];

  // Tap on Omnibox on pre edit state.
  TapOnPreEditTextInOmnibox();

  // Wait for callout paste button to be displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::SystemSelectionCalloutPasteButton()];

  // Tapping on Paste.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutPasteButton()]
      performAction:grey_tap()];

  // Omnibox now should be empty.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("hello")];
}

@end

#pragma mark - Edit state tests

@interface LocationBarEditStateTestCase : ChromeTestCase
@end

@implementation LocationBarEditStateTestCase {
  GURL _URL1;
  GURL _URL2;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey clearBrowsingHistory];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  _URL1 = self.testServer->GetURL(kPage1URL);
  _URL2 = self.testServer->GetURL(kPage2URL);

  // Clear the pasteboard in case there is a URL copied.
  [ChromeEarlGrey clearPasteboard];
}

// Copy button should be hidden when the omnibox is empty otherwise it should be
// displayed. Paste button should be hidden when pasteboard is empty otherwise
// it should be displayed. Select & SelectAll buttons should be hidden when the
// omnibox is empty.
// TODO(b/325908456): This test fails on iPad device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testEmptyOmnibox testEmptyOmnibox
#else
#define MAYBE_testEmptyOmnibox DISABLED_testEmptyOmnibox
#endif
- (void)MAYBE_testEmptyOmnibox {
  // Focus omnibox.
  [self focusFakebox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  // Pressing should not allow copying when omnibox is empty.
  // Wait for Copy button to appear or timeout after 2 seconds.
  GREYCondition* CopyButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Copy button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:
                                   chrome_test_util::
                                       SystemSelectionCalloutCopyButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  // Verify that system text selection callout is not displayed.
  GREYAssertFalse([CopyButtonIsDisplayed
                      waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                  @"Copy button should not be displayed");

  // Pressing should not allow select or selectAll when omnibox is empty.
  // Verify that system text selection callout is not displayed.
  [[EarlGrey selectElementWithMatcher:SelectButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SelectAllButton()]
      assertWithMatcher:grey_nil()];

  // Pressing should not allow pasting when pasteboard is empty.
  // Verify that system text selection callout is not displayed.
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchCopiedTextButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasteButton()]
      assertWithMatcher:grey_nil()];

  // Writing in the omnibox field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"this is a test")];

  // Long press on the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  // SelectAll the omnibox field.
  GREYCondition* SelectAllButtonIsDisplayed = [GREYCondition
      conditionWithName:@"SelectAll button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:SelectAllButton()]
                        performAction:grey_tap()
                                error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([SelectAllButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"SelectAll button display failed");

  // Cut the text.
  GREYCondition* CutButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Cut button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:CutButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([CutButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Cut button display failed");
  [[EarlGrey selectElementWithMatcher:CutButton()] performAction:grey_tap()];

  // Long pressing should allow pasting.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];
  // Verify that system text selection callout is displayed (Search Copied
  // Text).
  GREYCondition* searchCopiedTextButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Search Copied Text button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:SearchCopiedTextButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([searchCopiedTextButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Search Copied Text button display failed");
  // Verify that system text selection callout is displayed (Paste).
  [[EarlGrey selectElementWithMatcher:PasteButton()]
      assertWithMatcher:grey_notNil()];
}

// Select & SelectAll buttons should be displayed when the omnibox is not empty
// and no text is selected. If the selected text is a sub part of the omnibox
// fied, Select button should be hidden & SelectAll button should be displayed.
// If the selected text is the entire omnibox field, select & SelectAll button
// should be hidden.
// TODO(b/325908456): This test fails on iPad device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testSelection testSelection
#else
#define MAYBE_testSelection DISABLED_testSelection
#endif
- (void)MAYBE_testSelection {
  // Focus omnibox.
  [self focusFakebox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Write in the omnibox field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"this is a test")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  // Pressing should allow select and selectAll.
  // Wait for UIMenuController to appear or timeout after 2 seconds.
  GREYCondition* SelectButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Select button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:SelectButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  // Verify that system text selection callout is displayed.
  GREYAssertTrue([SelectButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Select button display failed");
  [[EarlGrey selectElementWithMatcher:SelectAllButton()]
      assertWithMatcher:grey_notNil()];

  // Wait for UIMenuController to appear or timeout after 2 seconds.
  GREYCondition* CopyButtonIsDisplayed = [GREYCondition
      conditionWithName:@"Copy button display condition"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:
                                   chrome_test_util::
                                       SystemSelectionCalloutCopyButton()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  // Pressing select should allow copy.
  // select should be hidden.
  [[EarlGrey selectElementWithMatcher:SelectButton()] performAction:grey_tap()];
  GREYAssertTrue([CopyButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Copy button display failed");
  [[EarlGrey selectElementWithMatcher:SelectButton()]
      assertWithMatcher:grey_nil()];

  // Pressing selectAll should allow copy.
  // selectAll should be hidden.
  [[EarlGrey selectElementWithMatcher:SelectAllButton()]
      performAction:grey_tap()];
  GREYAssertTrue([CopyButtonIsDisplayed
                     waitWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
                 @"Copy button display failed");
  [[EarlGrey selectElementWithMatcher:SelectAllButton()]
      assertWithMatcher:grey_nil()];
}

// TODO(b/325908456): This test fails on iPad device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testNoDefaultMatch testNoDefaultMatch
#else
#define MAYBE_testNoDefaultMatch DISABLED_testNoDefaultMatch
#endif
- (void)MAYBE_testNoDefaultMatch {
  NSString* copiedText = @"test no default match1";

  // Put some text in pasteboard.
  [ChromeEarlGrey copyTextToPasteboard:copiedText];

  [ChromeEarlGrey verifyStringCopied:copiedText];

  // Focus the omnibox.
  [self focusFakebox];

  // Make sure that:
  // 1. Chrome didn't crash (See crbug.com/1024885 for historic context)
  // 2. There's nothing in the omnibox
  // 3. There's a "text you copied" match

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];

  // Returns the "Text you copied" row.
  NSString* textYouCopiedLabel =
      l10n_util::GetNSString(IDS_TEXT_FROM_CLIPBOARD);
  id<GREYMatcher> textYouCopiedMatch = grey_allOf(
      chrome_test_util::OmniboxPopupRow(),
      grey_descendant(grey_accessibilityLabel(textYouCopiedLabel)), nil);
  [[EarlGrey selectElementWithMatcher:textYouCopiedMatch]
      assertWithMatcher:grey_notNil()];
}

// Tests inline autocomplete is shown.
- (void)testInlineAutocompleteSuggestion {
  // Disable all autocomplete providers except the history url provider.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  omnibox::DisableAutocompleteProviders(config, 524279);

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self populateHistory];

  // Clears the url and replace it with local url prefix.
  // TODO(crbug.com/40916974): This should use grey_typeText when fixed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"127")];

  // We expect to have an autocomplete.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assert:[OmniboxAppInterface displaysInlineAutocompleteText:YES]];
}

// Verifies that tapping an autocomplete suggestion in the omnibox successfully
// completes the user's query.
- (void)testTapBehaviors {
  // Disable all autocomplete providers except the history url provider.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  omnibox::DisableAutocompleteProviders(config, 524279);

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self populateHistory];

  // Clears the url and replace it with local url prefix.
  // TODO(crbug.com/40916974): This should use grey_typeText when fixed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"127")];

  // We expect to have an autocomplete.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assert:[OmniboxAppInterface displaysInlineAutocompleteText:YES]];
  // Tapping the inline autocomplete should accept it.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];

  // Inline autocomplete gets accepted and it is not presented anymore.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assert:[OmniboxAppInterface displaysInlineAutocompleteText:NO]];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingAutocompleteText(
                            @"")];
}

#pragma mark - Helpers

// Taps the fake omnibox and waits for the real omnibox to be visible.
- (void)focusFakebox {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

// Populate history by visiting the 2 different pages.
- (void)populateHistory {
  // Add all the pages to the history.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];
}

@end

#pragma mark - Hardware keyboard simulation interaction with omnibox

@interface OmniboxHardwareKeyboardInteractionTestCase : ChromeTestCase
@end

@implementation OmniboxHardwareKeyboardInteractionTestCase {
  GURL _URL1;
  GURL _URL2;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey clearBrowsingHistory];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  _URL1 = self.testServer->GetURL(kPage1URL);
  _URL2 = self.testServer->GetURL(kPage2URL);

  // Clear the pasteboard in case there is a URL copied.
  [ChromeEarlGrey clearPasteboard];
}

- (void)tearDown {
  [super tearDown];
  // HW keyboard simulation can mess up the SW keyboard simulator state.
  // Relaunching resets the state.
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Tests that in SRP/webpage state, pressing on forward delete HW key would
// erase the selected url.
- (void)testHardwareBackspaceKey {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  [ChromeEarlGreyUI focusOmnibox];
  // Omnibox contains the page url.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(_URL1.GetContent())];

  // Press the backspace HW keyboard key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\b" flags:0];

  // Omnibox now should be empty.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::OmniboxText("")];
}

// Tests that in SRP/webpage state, pressing Left/right arrows HW keyboard keys
// would exit pre-edit state and put the cursor to the end (right arrow) or to
// the begining (left arrow)
- (void)testHardwareLeftRightArrowKey {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  [ChromeEarlGreyUI focusOmnibox];
  // Omnibox contains the page url.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(_URL1.GetContent())];

  // Simulate press the HW right arrow key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"rightArrow" flags:0];

  // Type something
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"hello" flags:0];

  // Omnibox now should contain the page url suffixed with 'hello'
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(_URL1.GetContent() +
                                                      "hello")];

  DefocusOmnibox();

  [ChromeEarlGreyUI focusOmnibox];
  // Omnibox contains the page url.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(_URL1.GetContent())];
  // Simulate press the HW right arrow key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"leftArrow" flags:0];

  // Type something
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"hello" flags:0];

  // Omnibox now should contain the page url prefixed with 'hello'
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("hello" +
                                                      _URL1.GetContent())];
}

// Copies and pastes a URL, then performs an undo of the paste, and attempts to
// perform a second undo.
- (void)testCopyPasteUndo {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  [ChromeEarlGreyUI focusOmnibox];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"C"
                                          flags:UIKeyModifierCommand];

  [ChromeEarlGrey verifyStringCopied:base::SysUTF8ToNSString(_URL1.spec())];

  // Defocus the omnibox.
  DefocusOmnibox();

  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  [ChromeEarlGreyUI focusOmnibox];

  // Attempt to paste.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"V"
                                          flags:UIKeyModifierCommand];

  // Verify that paste happened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPage1URL)];

  // Attempt to undo.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"Z"
                                          flags:UIKeyModifierCommand];

  // Verify that undo happened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPage2URL)];

  // Attempt to undo again. Nothing should happen. In the past this could lead
  // to a crash.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"Z"
                                          flags:UIKeyModifierCommand];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPage2URL)];
}

// Focus the omnibox and hit "cmd+X". This should remove all text from the
// omnibox and put it in the clipboard. This had been broken before because of
// the preedit state complexity. Paste to verify that the URL was indeed copied.
- (void)testCutInPreedit {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  [ChromeEarlGreyUI focusOmnibox];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"X"
                                          flags:UIKeyModifierCommand];
  [ChromeEarlGrey verifyStringCopied:base::SysUTF8ToNSString(_URL1.spec())];

  // Verify that the omnibox is empty.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];

  // Attempt to paste.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"V"
                                          flags:UIKeyModifierCommand];

  // Verify that paste happened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPage1URL)];
}

// Tests that pressing spacebar key when having an autocomplete, removes it.
- (void)testHWspacebarKeyOnAutocomplete {
  // Disable all autocomplete providers except the history url provider.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  omnibox::DisableAutocompleteProviders(config, 524279);

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self populateHistory];

  // Clears the url and replace it with local url prefix.
  // TODO(crbug.com/40916974): This should use grey_typeText when fixed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"127")];

  // Autocomplete is present.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assert:[OmniboxAppInterface displaysInlineAutocompleteText:YES]];

  // Pressing spacebar.
  // TODO(crbug.com/40916974): This should use grey_typeText when fixed.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@" " flags:0];

  // Autocomplete removed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assert:[OmniboxAppInterface displaysInlineAutocompleteText:NO]];
}

#pragma mark - Helpers

// Populate history by visiting the 2 different pages.
- (void)populateHistory {
  // Add all the pages to the history.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];
}

@end

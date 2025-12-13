// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <vector>

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/shared_highlighting/core/common/fragment_directives_utils.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_app_interface.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_matchers.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using shared_highlighting::TextFragment;

namespace {

const char kTestPageTextSample[] = "Lorem ipsum";
const char kNoTextTestPageTextSample[] = ".!,";
const char kInputTestPageTextSample[] = "has an input";
const char kSimpleTextElementId[] = "toBeSelected";
const char kToBeSelectedText[] = "VeryUniqueWord";

const char kTestURL[] = "/testPage";
const char kHTMLOfTestPage[] =
    "<html><body>"
    "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Hello foo! Ut enim ad "
    "minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
    "ex ea "
    "commodo consequat. bar</p>"
    "<p id=\"toBeSelected\">VeryUniqueWord</p>"
    "</body></html>";

const char kTestLongPageURL[] = "/longTestPage";
const char kHTMLOfLongTestPage[] =
    "<html><body>"
    "<div style=\"background:blue; height: 4000px; width: 250px;\"></div>"
    "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Hello foo! Ut enim ad "
    "minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
    "ex ea "
    "commodo consequat. bar</p>"
    "<div style=\"background:blue; height: 4000px; width: 250px;\"></div>"
    "</body></html>";

const char kNoTextTestURL[] = "/noTextPage";
const char kHTMLOfNoTextTestPage[] =
    "<html><body>"
    "<div><p id=\"toBeSelected\">.!, \t</p></div>"
    "</body></html>";

const char kInputTestURL[] = "/inputTextPage";
const char kHTMLOfInputTestPage[] =
    "<html><body>"
    "This page has an input"
    "<input type=\"text\" id=\"toBeSelected\"></p>"
    "</body></html>";

std::unique_ptr<net::test_server::HttpResponse> LoadHtml(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content(html);
  return std::move(http_response);
}

}  // namespace

// Test class for the link-to-text feature.
@interface LinkToTextTestCase : ChromeTestCase
@end

@implementation LinkToTextTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kSharedHighlightingIOS);
  return config;
}

- (void)setUp {
  [super setUp];

  RegisterDefaultHandlers(self.testServer);
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, kTestURL,
                          base::BindRepeating(&LoadHtml, kHTMLOfTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kTestLongPageURL,
      base::BindRepeating(&LoadHtml, kHTMLOfLongTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kNoTextTestURL,
      base::BindRepeating(&LoadHtml, kHTMLOfNoTextTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kInputTestURL,
      base::BindRepeating(&LoadHtml, kHTMLOfInputTestPage)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that a link can be generated for a simple text selection.
// crbug.com/1403831 Disable flaky test
- (void)DISABLED_testGenerateLinkForSimpleText {
  [ChromeEarlGrey clearPasteboard];
  GURL pageURL = self.testServer->GetURL(kTestURL);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector
                            selectorWithElementID:kSimpleTextElementId],
                        true)];

  // Wait for the menu to open. The "Copy" menu item will always be present,
  // but other items may be hidden behind the overflow button.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      chrome_test_util::SystemSelectionCalloutCopyButton()];

  // The link to text button may be in the overflow, so use a search action to
  // find it, if necessary.
  id<GREYMatcher> linkToTextMatcher = FindEditMenuActionWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT));

  [[EarlGrey selectElementWithMatcher:linkToTextMatcher]
      performAction:grey_tap()];

  // Make sure the Edit menu is gone.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SystemSelectionCallout()]
      assertWithMatcher:grey_notVisible()];

  // Wait for the Activity View to show up (look for the Copy action).
  id<GREYMatcher> copyActivityButton = chrome_test_util::CopyActivityButton();
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:copyActivityButton];

  // Tap on the Copy action.
  [[EarlGrey selectElementWithMatcher:copyActivityButton]
      performAction:grey_tap()];

  // Assert the values stored in the pasteboard. Lower-casing the expected
  // GURL as that is what the JS library is doing.
  NSString* stringURL = base::SysUTF8ToNSString(pageURL.spec());
  NSString* fragment = @"#:~:text=bar-,";
  NSString* selectedText =
      base::SysUTF8ToNSString(base::ToLowerASCII(kToBeSelectedText));

  NSString* expectedURL =
      [NSString stringWithFormat:@"%@%@%@", stringURL, fragment, selectedText];
  [ChromeEarlGrey verifyStringCopied:expectedURL];

  [ChromeEarlGrey clearPasteboard];
}

- (void)testBadSelectionDisablesGenerateLink {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kNoTextTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:kNoTextTestPageTextSample];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector
                            selectorWithElementID:kSimpleTextElementId],
                        true)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[EditMenuAppInterface
                                                       editMenuMatcher]];

  id<GREYMatcher> linkToTextMatcher = FindEditMenuActionWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT));

  GREYAssertEqual(linkToTextMatcher, nil, @"Link to text button visible");

}

// TODO(crbug.com/386205292): Test is flaky.
- (void)FLAKY_testInputDisablesGenerateLink {
  // In order to make the menu show up later in the test, the pasteboard can't
  // be empty.
  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
  pasteboard.string = @"anything";

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kInputTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:kInputTestPageTextSample];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Tap to focus the field, then long press to make the Edit Menu pop up.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(
                        kSimpleTextElementId)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector
                            selectorWithElementID:kSimpleTextElementId],
                        true)];

  // Ensure the menu is visible by finding the Paste button.
  // TODO(crbug.com/328271981): either remove call to selectElementWithMatcher
  // or do something with its return value
  // id<GREYMatcher> menu = grey_accessibilityLabel(@"Paste");
  // [EarlGrey selectElementWithMatcher:menu];

  // Make sure the Link to Text button is not visible.
  id<GREYMatcher> linkToTextMatcher = FindEditMenuActionWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT));

  GREYAssertEqual(linkToTextMatcher, nil, @"Link to text button visible");
}

@end

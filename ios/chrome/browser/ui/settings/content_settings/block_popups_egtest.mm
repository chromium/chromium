// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/settings/content_settings/block_popups_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using chrome_test_util::ContentSettingsButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::TabGridEditButton;
using testing::NavigationBarBackButton;

namespace {

// URLs used in the tests.
const char* kBlockPopupsUrl = "http://blockpopups";
const char* kOpenedWindowUrl = "http://openedwindow";

// Page with a button that opens a new window after a short delay.
NSString* kBlockPopupsResponseTemplate =
    @"<input type=\"button\" onclick=\"setTimeout(function() {"
     "window.open('%@')}, 1)\" "
     "id=\"open-window\" "
     "value=\"openWindow\">";
// JavaScript that clicks that button.
NSString* kOpenPopupScript = @"document.getElementById('open-window').click()";
const std::string kOpenedWindowResponse = "Opened window";

// Returns matcher for the block popups settings menu button.
id<GREYMatcher> BlockPopupsSettingsButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(IDS_IOS_BLOCK_POPUPS);
}

// ScopedBlockPopupsException adds an exception to the block popups exception
// list for as long as this object is in scope.
class ScopedBlockPopupsException {
 public:
  ScopedBlockPopupsException(const std::string& pattern)
      : pattern_(base::SysUTF8ToNSString(pattern)) {
    [BlockPopupsAppInterface setPopupPolicy:CONTENT_SETTING_ALLOW
                                 forPattern:pattern_];
  }

  ScopedBlockPopupsException(const ScopedBlockPopupsException&) = delete;
  ScopedBlockPopupsException& operator=(const ScopedBlockPopupsException&) =
      delete;

  ~ScopedBlockPopupsException() {
    [BlockPopupsAppInterface setPopupPolicy:CONTENT_SETTING_DEFAULT
                                 forPattern:pattern_];
  }

 private:
  // The exception pattern that this object is managing.
  NSString* pattern_;
};
}  // namespace

// Block Popups tests for Chrome.
@interface BlockPopupsTestCase : WebHttpServerChromeTestCase
@end

@implementation BlockPopupsTestCase

// Opens the block popups settings page and verifies that accessibility is set
// up properly.
- (void)testAccessibilityOfBlockPopupSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:ContentSettingsButton()];
  [[EarlGrey selectElementWithMatcher:BlockPopupsSettingsButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"block_popups_settings_view_controller")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Close the settings menu.
  [[EarlGrey selectElementWithMatcher:NavigationBarBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that popups are opened in new tabs when the preference is set to ALLOW.
- (void)testPopupsAllowed {
  std::map<GURL, std::string> responses;
  const GURL blockPopupsURL = web::test::HttpServer::MakeUrl(kBlockPopupsUrl);
  const GURL openedWindowURL = web::test::HttpServer::MakeUrl(kOpenedWindowUrl);
  NSString* openedWindowURLString =
      base::SysUTF8ToNSString(openedWindowURL.spec());
  responses[blockPopupsURL] = base::SysNSStringToUTF8([NSString
      stringWithFormat:kBlockPopupsResponseTemplate, openedWindowURLString]);
  responses[openedWindowURL] = kOpenedWindowResponse;
  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);
  [ChromeEarlGrey loadURL:blockPopupsURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Request popup (execute script without using a user gesture) and make sure
  // the popup opened in a new tab.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:kOpenPopupScript];
  [ChromeEarlGrey waitForMainTabCount:2];

  // No infobar should be displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          StaticTextWithAccessibilityLabel(
                                              @"Pop-ups blocked (1)")]
      assertWithMatcher:grey_notVisible()];
}

// Tests that popups are prevented from opening and an infobar is displayed when
// the preference is set to BLOCK.
- (void)testPopupsBlocked {
  std::map<GURL, std::string> responses;
  const GURL blockPopupsURL = web::test::HttpServer::MakeUrl(kBlockPopupsUrl);
  const GURL openedWindowURL = web::test::HttpServer::MakeUrl(kOpenedWindowUrl);
  NSString* openedWindowURLString =
      base::SysUTF8ToNSString(openedWindowURL.spec());
  responses[blockPopupsURL] = base::SysNSStringToUTF8([NSString
      stringWithFormat:kBlockPopupsResponseTemplate, openedWindowURLString]);
  responses[openedWindowURL] = kOpenedWindowResponse;
  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_BLOCK);
  [ChromeEarlGrey loadURL:blockPopupsURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Request popup (execute script without using a user gesture), then make sure
  // it was blocked and an infobar was displayed. The window.open() call is run
  // via async JS, so the infobar may not open immediately.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:kOpenPopupScript];

  BOOL infobarVisible = [[GREYCondition
      conditionWithName:@"Wait for blocked popups infobar to show"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:
                            chrome_test_util::StaticTextWithAccessibilityLabel(
                                @"Pop-ups blocked (1)")]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }] waitWithTimeout:4.0];
  GREYAssertTrue(infobarVisible, @"Infobar did not appear");
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that the "exceptions" section on the settings page is hidden and
// revealed properly when the preference switch is toggled.
- (void)testSettingsPageWithExceptions {
  std::string allowedPattern = "[*.]example.com";
  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_BLOCK);
  ScopedBlockPopupsException exceptionSetter(allowedPattern);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:ContentSettingsButton()];
  [[EarlGrey selectElementWithMatcher:BlockPopupsSettingsButton()]
      performAction:grey_tap()];

  // Make sure that the "example.com" exception is listed.
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          allowedPattern))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toggle the switch off via the UI and make sure the exceptions are not
  // visible.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   @"blockPopupsContentView_switch", YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          allowedPattern))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
                            grey_not(grey_accessibilityTrait(
                                UIAccessibilityTraitNotEnabled)),
                            nil)] assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toggle the switch back on via the UI and make sure the exceptions are now
  // visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          @"blockPopupsContentView_switch", NO)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          allowedPattern))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
                            grey_not(TabGridEditButton()),
                            grey_not(grey_accessibilityTrait(
                                UIAccessibilityTraitNotEnabled)),
                            nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Close the settings menu.
  [[EarlGrey selectElementWithMatcher:NavigationBarBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end

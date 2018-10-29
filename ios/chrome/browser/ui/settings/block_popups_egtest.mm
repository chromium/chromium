// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/chrome/test/scoped_block_popups_pref.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ContentSettingsButton;
using chrome_test_util::GetOriginalBrowserState;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;

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
  ScopedBlockPopupsException(const std::string& pattern) : pattern_(pattern) {
    SetException(pattern_, CONTENT_SETTING_ALLOW);
  }
  ~ScopedBlockPopupsException() {
    SetException(pattern_, CONTENT_SETTING_DEFAULT);
  }

 private:
  // Adds an exception for the given |pattern|.  If |setting| is
  // CONTENT_SETTING_DEFAULT, removes the existing exception instead.
  void SetException(const std::string& pattern, ContentSetting setting) {
    ios::ChromeBrowserState* browserState =
        chrome_test_util::GetOriginalBrowserState();

    ContentSettingsPattern exception_pattern =
        ContentSettingsPattern::FromString(pattern);
    ios::HostContentSettingsMapFactory::GetForBrowserState(browserState)
        ->SetContentSettingCustomScope(
            exception_pattern, ContentSettingsPattern::Wildcard(),
            CONTENT_SETTINGS_TYPE_POPUPS, std::string(), setting);
  }

  // The exception pattern that this object is managing.
  std::string pattern_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBlockPopupsException);
};
}  // namespace

// Block Popups tests for Chrome.
@interface BlockPopupsTestCase : ChromeTestCase
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
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Close the settings menu.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
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

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW,
                                   GetOriginalBrowserState());
  [ChromeEarlGrey loadURL:blockPopupsURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Request popup and make sure the popup opened in a new tab.
  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(kOpenPopupScript, &error);
  GREYAssert(!error, @"Error during script execution: %@", error);
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

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_BLOCK,
                                   GetOriginalBrowserState());
  [ChromeEarlGrey loadURL:blockPopupsURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Request popup, then make sure it was blocked and an infobar was displayed.
  // The window.open() call is run via async JS, so the infobar may not open
  // immediately.
  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(kOpenPopupScript, &error);
  GREYAssert(!error, @"Error during script execution: %@", error);

  [[GREYCondition
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
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that the "exceptions" section on the settings page is hidden and
// revealed properly when the preference switch is toggled.
- (void)testSettingsPageWithExceptions {
  std::string allowedPattern = "[*.]example.com";
  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_BLOCK,
                                   GetOriginalBrowserState());
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
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"blockPopupsContentView_switch", YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          allowedPattern))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toggle the switch back on via the UI and make sure the exceptions are now
  // visible.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"blockPopupsContentView_switch", NO)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          allowedPattern))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the settings menu.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1015113) The EG2 macro is breaking indexing for some reason
// without the trailing semicolon.  For now, disable the extra semi warning
// so Xcode indexing works for the egtest.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(OmniboxAppInterface);
#pragma clang diagnostic pop

using base::test::ios::kWaitForUIElementTimeout;

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
    if (request.headers.find("X-Client-Data") != request.headers.end()) {
      result = kHeaderPageSuccess;
    }
    http_response->set_content("<html><body>" + result + "</body></html>");
    return std::move(http_response);
  }

  return nil;
}

// Returns visit Copied Link button matcher from UIMenuController.
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
  NSString* a11yLabelsearchCopiedText = @"Search for Copied Text";
  return grey_allOf(grey_accessibilityLabel(a11yLabelsearchCopiedText),
                    chrome_test_util::SystemSelectionCallout(), nil);
}

}  //  namespace

@interface OmniboxTestCase : ChromeTestCase
@end

@implementation OmniboxTestCase

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the XClientData header is sent when navigating to
// https://google.com through the omnibox.
- (void)testXClientData {
// TODO(crbug.com/1067815): Test doesn't pass on iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"testXClientData doesn't pass on iPad device.");
  }
#endif

  // TODO(crbug.com/1120723) This test is flakily because of a DCHECK in
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
      performAction:grey_typeText([NSString stringWithFormat:@"%@\n", URL])];

  [ChromeEarlGrey waitForWebStateContainingText:kHeaderPageSuccess];
}

@end

#pragma mark - Steady state tests

@interface LocationBarSteadyStateTestCase : ChromeTestCase
@end

@implementation LocationBarSteadyStateTestCase

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [ChromeEarlGrey clearBrowsingHistory];

  // Clear the pasteboard in case there is a URL copied.
  UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];
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
  // TODO(crbug.com/996541) Starting in Xcode 11 beta 6, the share button does
  // not appear (even with a delay) flakily.
  if (@available(iOS 13, *))
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS13.");
  [self openPage1];

  if ([ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Test is flaky: crbug.com/1056700.
- (void)DISABLED_testCopyPaste {
  [self openPage1];

  // Long pressing should allow copying.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];

  // Verify that system text selection callout is displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_notNil()];

  // Pressing should not allow pasting when pasteboard is empty.
  // Verify that system text selection callout is not displayed.
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SearchCopiedTextButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasteButton()]
      assertWithMatcher:grey_nil()];

  [self checkLocationBarSteadyState];

  // Tapping it should copy the URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      performAction:grey_tap()];

  // Edit menu takes a while to copy, and not waiting here will cause Page 2 to
  // load before the copy happens, so Page 2 URL may be copied.
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:@"page1 URL copied condition"
                  block:^BOOL {
                    return [UIPasteboard.generalPasteboard.string
                        hasSuffix:base::SysUTF8ToNSString(kPage1URL)];
                  }];
  // Wait for copy to happen or timeout after 5 seconds.
  GREYAssertTrue([copyCondition waitWithTimeout:5],
                 @"Copying page 1 URL failed");

  // Go to another web page.
  [self openPage2];

  // Visit copied link should now be available.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkButton()]
      assertWithMatcher:grey_notNil()];

  [self checkLocationBarSteadyState];

  // Tapping it should navigate to Page 1.
  [[EarlGrey selectElementWithMatcher:VisitCopiedLinkButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
}

- (void)testFocusingOmniboxDismissesEditMenu {
// TODO(crbug.com/1129095): Re-enable test for iOS 12 device.
#if !TARGET_IPHONE_SIMULATOR
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS 12 devices.");
  }
#endif

  [self openPage1];

  // Long pressing should open edit menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_notNil()];

  // Focus omnibox.
  [ChromeEarlGreyUI focusOmnibox];
  [self checkLocationBarEditState];

  // Verify that the edit menu disappeared.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_nil()];
}

// Copies and pastes a URL, then performs an undo of the paste, and attempts to
// perform a second undo.
// TODO(crbug.com/1041478): This test is flaky.
- (void)FLAKY_testCopyPasteUndo {
  [self openPage1];

  [ChromeEarlGreyUI focusOmnibox];
  [self checkLocationBarEditState];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"C"
                                          flags:UIKeyModifierCommand];

  // Edit menu takes a while to copy, and not waiting here will cause Page 2 to
  // load before the copy happens, so Page 2 URL may be copied.
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:@"page1 URL copied condition"
                  block:^BOOL {
                    return [UIPasteboard.generalPasteboard.string
                        hasSuffix:base::SysUTF8ToNSString(kPage1URL)];
                  }];
  // Wait for copy to happen or timeout after 5 seconds.
  GREYAssertTrue([copyCondition waitWithTimeout:5],
                 @"Copying page 1 URL failed");

  // Defocus the omnibox.
  if ([ChromeEarlGrey isIPadIdiom]) {
    id<GREYMatcher> typingShield = grey_accessibilityID(@"Typing Shield");
    [[EarlGrey selectElementWithMatcher:typingShield] performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:grey_buttonTitle(@"Cancel")]
        performAction:grey_tap()];
  }

  [self openPage2];

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
// TODO(crbug.com/1049603): Re-enable this test.
- (void)DISABLED_testCutInPreedit {
  [self openPage1];

  [ChromeEarlGreyUI focusOmnibox];
  [self checkLocationBarEditState];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"X"
                                          flags:UIKeyModifierCommand];

  // It takes a while to copy, and not waiting here will cause the test to fail.
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:@"page1 URL copied condition"
                  block:^BOOL {
                    return [UIPasteboard.generalPasteboard.string
                        hasSuffix:base::SysUTF8ToNSString(kPage1URL)];
                  }];
  // Wait for copy to happen or timeout after 5 seconds.
  GREYAssertTrue([copyCondition waitWithTimeout:5],
                 @"Copying page 1 URL failed");

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

- (void)testOmniboxDefocusesOnTabSwitch {
  [self openPage1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];
  [self openPage2];

  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"Obama")];

  // The popup should open.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kOmniboxPopupTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Switch to the first tab.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // The omnibox shouldn't be focused and the popup should be closed.
  [self checkLocationBarSteadyState];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kOmniboxPopupTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

- (void)testOmniboxDefocusesOnTabSwitchIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  [self openPage1];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:2];
  [self openPage2];

  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"Obama")];

  // The popup should open.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kOmniboxPopupTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Switch to the first tab.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // The omnibox shouldn't be focused and the popup should be closed.
  [self checkLocationBarSteadyState];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kOmniboxPopupTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

#pragma mark - Helpers

// Navigates to Page 1 in a tab and waits for it to load.
- (void)openPage1 {
  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage1URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
}

// Navigates to Page 2 in a tab and waits for it to load.
- (void)openPage2 {
  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage2URL)];
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

#pragma mark - Edit state tests

@interface LocationBarEditStateTestCase : ChromeTestCase
@end

@implementation LocationBarEditStateTestCase

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey clearBrowsingHistory];

  // Clear the pasteboard in case there is a URL copied.
  UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];
}

// Copy button should be hidden when the omnibox is empty otherwise it should be
// displayed. Paste button should be hidden when pasteboard is empty otherwise
// it should be displayed. Select & SelectAll buttons should be hidden when the
// omnibox is empty.
- (void)testEmptyOmnibox {
// TODO(crbug.com/1209342): test failing on ipad device
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
  // Focus omnibox.
  [self focusFakebox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];

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
  GREYAssertFalse(
      [CopyButtonIsDisplayed waitWithTimeout:kWaitForUIElementTimeout],
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
      performAction:grey_typeText(@"this is a test")];

  // Click on the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];

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
  GREYAssertTrue(
      [SelectAllButtonIsDisplayed waitWithTimeout:kWaitForUIElementTimeout],
      @"SelectAll button display failed");

  // Cut the text.
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
                     waitWithTimeout:kWaitForUIElementTimeout],
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
- (void)testSelection {
// TODO(crbug.com/1209342): test failing on ipad device
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
  // Focus omnibox.
  [self focusFakebox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Write in the omnibox field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"this is a test")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];

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
  GREYAssertTrue(
      [SelectButtonIsDisplayed waitWithTimeout:kWaitForUIElementTimeout],
      @"Select button display failed");
  [[EarlGrey selectElementWithMatcher:SelectAllButton()]
      assertWithMatcher:grey_notNil()];

  // Pressing select should allow copy.
  // select should be hidden.
  [[EarlGrey selectElementWithMatcher:SelectButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SelectButton()]
      assertWithMatcher:grey_nil()];

  // Pressing selectAll should allow copy.
  // selectAll should be hidden.
  [[EarlGrey selectElementWithMatcher:SelectAllButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SelectAllButton()]
      assertWithMatcher:grey_nil()];
}

// TODO(crbug.com/1067815): Test can't pass on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testNoDefaultMatch testNoDefaultMatch
#else
#define MAYBE_testNoDefaultMatch DISABLED_testNoDefaultMatch
#endif
- (void)MAYBE_testNoDefaultMatch {
  // TODO(crbug.com/1105869) Omnibox pasteboard suggestions are currently
  // disabled on iOS14.
  if (@available(iOS 14, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS14.");
  }

  NSString* copiedText = @"test no default match1";

  // Put some text in pasteboard.
  UIPasteboard.generalPasteboard.string = copiedText;

  // Copying can take a while, wait for it to happen.
  GREYCondition* copyCondition =
      [GREYCondition conditionWithName:@"test text copied condition"
                                 block:^BOOL {
                                   return [UIPasteboard.generalPasteboard.string
                                       isEqualToString:copiedText];
                                 }];
  // Wait for copy to happen or timeout after 5 seconds.
  GREYAssertTrue([copyCondition waitWithTimeout:5],
                 @"Copying test text failed");

  // Focus the omnibox.
  [self focusFakebox];

  // Make sure that:
  // 1. Chrome didn't crash (See crbug.com/1024885 for historic context)
  // 2. There's nothing in the omnibox
  // 3. There's a "text you copied" match

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];

  // Returns the popup row containing the |url| as suggestion.
  id<GREYMatcher> textYouCopiedMatch =
      grey_allOf(grey_kindOfClassName(@"OmniboxPopupRowCell"),
                 grey_descendant(grey_accessibilityLabel(
                     [NSString stringWithFormat:@"\"%@\"", copiedText])),
                 nil);
  [[EarlGrey selectElementWithMatcher:textYouCopiedMatch]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Helpers

// Taps the fake omnibox and waits for the real omnibox to be visible.
- (void)focusFakebox {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

@end

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#include "net/base/network_change_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForUIElementTimeout;

namespace {
const char kContentToRemove[] = "Text that distillation should remove.";
const char kContentToKeep[] = "Text that distillation should keep.";
NSString* const kDistillableTitle = @"Tomato";
const char kDistillableURL[] = "/potato";
const char kNonDistillableURL[] = "/beans";
NSString* const kReadTitle = @"foobar";
NSString* const kReadURL = @"http://readfoobar.com";
NSString* const kUnreadTitle = @"I am an unread entry";
NSString* const kUnreadURL = @"http://unreadfoobar.com";
NSString* const kReadURL2 = @"http://kReadURL2.com";
NSString* const kReadTitle2 = @"read item 2";
NSString* const kUnreadTitle2 = @"I am another unread entry";
NSString* const kUnreadURL2 = @"http://unreadfoobar2.com";
const size_t kNumberReadEntries = 2;
const size_t kNumberUnreadEntries = 2;
const CFTimeInterval kSnackbarAppearanceTimeout = 5;
// kSnackbarDisappearanceTimeout = MDCSnackbarMessageDurationMax + 1
const CFTimeInterval kSnackbarDisappearanceTimeout = 10 + 1;
const CFTimeInterval kDelayForSlowWebServer = 4;
const CFTimeInterval kLoadOfflineTimeout = kDelayForSlowWebServer + 1;
const CFTimeInterval kLongPressDuration = 1.0;
const CFTimeInterval kDistillationTimeout = 5;
const CFTimeInterval kServerOperationDelay = 1;
NSString* const kReadHeader = @"Read";
NSString* const kUnreadHeader = @"Unread";

// Returns the string concatenated |n| times.
std::string operator*(const std::string& s, unsigned int n) {
  std::ostringstream out;
  for (unsigned int i = 0; i < n; i++)
    out << s;
  return out.str();
}

// Scroll to the top of the Reading List.
void ScrollToTop() {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      performAction:[ChromeActionsAppInterface scrollToTop]];
}

// Asserts that the "mark" toolbar button is visible and has the a11y label of
// |a11y_label_id|.
void AssertToolbarMarkButtonText(int a11y_label_id) {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(kReadingListToolbarMarkButtonID),
              grey_ancestor(grey_kindOfClassName(@"UIToolbar")),
              chrome_test_util::ButtonWithAccessibilityLabelId(a11y_label_id),
              nil)] assertWithMatcher:grey_sufficientlyVisible()];
}

// Asserts the |button_id| toolbar button is not visible.
void AssertToolbarButtonNotVisibleWithID(NSString* button_id) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(button_id),
                                          grey_ancestor(grey_kindOfClassName(
                                              @"UIToolbar")),
                                          nil)]
      assertWithMatcher:grey_notVisible()];
}

// Assert the |button_id| toolbar button is visible.
void AssertToolbarButtonVisibleWithID(NSString* button_id) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(button_id),
                                          grey_ancestor(grey_kindOfClassName(
                                              @"UIToolbar")),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Taps the |button_id| toolbar button.
void TapToolbarButtonWithID(NSString* button_id) {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(button_id)]
      performAction:grey_tap()];
}

// Taps the context menu button with the a11y label of |a11y_label_id|.
void TapContextMenuButtonWithA11yLabelID(int a11y_label_id) {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   a11y_label_id)] performAction:grey_tap()];
}

// Performs |action| on the entry with the title |entryTitle|. The view can be
// scrolled down to find the entry.
void PerformActionOnEntry(NSString* entryTitle, id<GREYAction> action) {
  ScrollToTop();
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(entryTitle),
                 grey_ancestor(grey_kindOfClassName(@"TableViewURLCell")),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      performAction:action];
}

// Taps the entry with the title |entryTitle|.
void TapEntry(NSString* entryTitle) {
  PerformActionOnEntry(entryTitle, grey_tap());
}

// Long-presses the entry with the title |entryTitle|.
void LongPressEntry(NSString* entryTitle) {
  PerformActionOnEntry(entryTitle,
                       grey_longPressWithDuration(kLongPressDuration));
}

// Asserts that the entry with the title |entryTitle| is visible.
void AssertEntryVisible(NSString* entryTitle) {
  ScrollToTop();
  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::StaticTextWithAccessibilityLabel(entryTitle),
              grey_ancestor(grey_kindOfClassName(@"TableViewURLCell")),
              grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      assertWithMatcher:grey_notNil()];
}

// Asserts that all the entries are visible.
void AssertAllEntriesVisible() {
  AssertEntryVisible(kReadTitle);
  AssertEntryVisible(kReadTitle2);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);

  // If the number of entries changes, make sure this assert gets updated.
  GREYAssertEqual((size_t)2, kNumberReadEntries,
                  @"The number of entries have changed");
  GREYAssertEqual((size_t)2, kNumberUnreadEntries,
                  @"The number of entries have changed");
}

// Asserts that the entry |title| is not visible.
void AssertEntryNotVisible(NSString* title) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ScrollToTop();
  NSError* error;

  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(title),
                     grey_ancestor(grey_kindOfClassName(@"TableViewURLCell")),
                     grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      assertWithMatcher:grey_notNil()
                  error:&error];
  GREYAssertNotNil(error, @"Entry is visible");
}

// Asserts |header| is visible.
void AssertHeaderNotVisible(NSString* header) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ScrollToTop();
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(header)]
      assertWithMatcher:grey_notVisible()];
}

// Opens the reading list menu.
void OpenReadingList() {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::ReadingListMenuButton()];
}

// Adds 20 read and 20 unread entries to the model, opens the reading list menu
// and enter edit mode.
void AddLotOfEntriesAndEnterEdit() {
  for (NSInteger index = 0; index < 10; index++) {
    NSString* url_to_be_added =
        [kReadURL stringByAppendingPathComponent:[@(index) stringValue]];
    GREYAssertNil([ReadingListAppInterface
                      addEntryWithURL:[NSURL URLWithString:url_to_be_added]
                                title:kReadTitle
                                 read:YES],
                  @"Unable to add Reading List item");
  }
  for (NSInteger index = 0; index < 10; index++) {
    NSString* url_to_be_added =
        [kUnreadURL stringByAppendingPathComponent:[@(index) stringValue]];
    GREYAssertNil([ReadingListAppInterface
                      addEntryWithURL:[NSURL URLWithString:url_to_be_added]
                                title:kReadTitle
                                 read:NO],
                  @"Unable to add Reading List item");
  }
  OpenReadingList();

  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
}

// Adds a read and an unread entry to the model, opens the reading list menu and
// enter edit mode.
void AddEntriesAndEnterEdit() {
  GREYAssertNil(
      [ReadingListAppInterface addEntryWithURL:[NSURL URLWithString:kReadURL]
                                         title:kReadTitle
                                          read:YES],
      @"Unable to add Reading List item");
  GREYAssertNil(
      [ReadingListAppInterface addEntryWithURL:[NSURL URLWithString:kReadURL2]
                                         title:kReadTitle2
                                          read:YES],
      @"Unable to add Reading List item");
  GREYAssertNil(
      [ReadingListAppInterface addEntryWithURL:[NSURL URLWithString:kUnreadURL]
                                         title:kUnreadTitle
                                          read:NO],
      @"Unable to add Reading List item");
  GREYAssertNil(
      [ReadingListAppInterface addEntryWithURL:[NSURL URLWithString:kUnreadURL2]
                                         title:kUnreadTitle2
                                          read:NO],
      @"Unable to add Reading List item");

  OpenReadingList();

  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
}

// Returns a match for the Reading List Empty Collection Background.
id<GREYMatcher> EmptyBackground() {
  return grey_accessibilityID(kTableViewEmptyViewID);
}

// Adds the current page to the Reading List.
void AddCurrentPageToReadingList() {
  // Add the page to the reading list.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::ButtonWithAccessibilityLabelId(
                             IDS_IOS_SHARE_MENU_READING_LIST_ACTION)];

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbar_matcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_READING_LIST_SNACKBAR_MESSAGE);
  ConditionBlock wait_for_appearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarAppearanceTimeout, wait_for_appearance),
             @"Snackbar did not appear.");

  // Wait for the snackbar to disappear.
  ConditionBlock wait_for_disappearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarDisappearanceTimeout, wait_for_disappearance),
             @"Snackbar did not disappear.");
  [ReadingListAppInterface notifyWifiConnection];
}

// Wait until one element is distilled.
void WaitForDistillation() {
  ConditionBlock wait_for_distillation_date = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(
                                     kTableViewURLCellFaviconBadgeViewID)]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kDistillationTimeout, wait_for_distillation_date),
             @"Item was not distilled.");
}

// Serves URLs. Response can be delayed by |delay| second or return an error if
// |responds_with_content| is false.
// If |distillable|, result is can be distilled for offline display.
std::unique_ptr<net::test_server::HttpResponse> HandleQueryOrCloseSocket(
    const bool& responds_with_content,
    const int& delay,
    bool distillable,
    const net::test_server::HttpRequest& request) {
  if (!responds_with_content) {
    return std::make_unique<net::test_server::RawHttpResponse>(
        /*headers=*/"", /*contents=*/"");
  }
  auto response = std::make_unique<net::test_server::DelayedHttpResponse>(
      base::TimeDelta::FromSeconds(delay));
  response->set_content_type("text/html");
  if (distillable) {
    std::string page_title = "Tomato";

    std::string content_to_remove(kContentToRemove);
    std::string content_to_keep(kContentToKeep);

    response->set_content("<html><head><title>" + page_title +
                          "</title></head>" + content_to_remove * 20 +
                          "<article>" + content_to_keep * 20 + "</article>" +
                          content_to_remove * 20 + "</html>");
  } else {
    response->set_content("<html><head><title>greens</title></head></html>");
  }
  return std::move(response);
}

// Opens the page security info bubble.
void OpenPageSecurityInfoBubble() {
  // In UI Refresh, the security info is accessed through the tools menu.
  [ChromeEarlGreyUI openToolsMenu];
  // Tap on the Page Info button.
  [ChromeEarlGreyUI
      tapToolsMenuButton:grey_accessibilityID(kToolsMenuSiteInformation)];
}

// Waits for a static html view containing |text|. If the condition is not met
// within a timeout, a GREYAssert is induced.
void WaitForStaticHtmlViewContainingText(NSString* text) {
  bool has_static_view =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
        return [ReadingListAppInterface staticHTMLViewContainingText:text];
      });

  NSString* error_description = [NSString
      stringWithFormat:@"Failed to find static html view containing %@", text];
  GREYAssert(has_static_view, error_description);
}

// Waits for there to be no static html view, or a static html view that does
// not contain |text|. If the condition is not met within a timeout, a
// GREYAssert is induced.
void WaitForStaticHtmlViewNotContainingText(NSString* text) {
  bool no_static_view =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
        return ![ReadingListAppInterface staticHTMLViewContainingText:text];
      });

  NSString* error_description = [NSString
      stringWithFormat:@"Failed, there was a static html view containing %@",
                       text];
  GREYAssert(no_static_view, error_description);
}

void AssertIsShowingDistillablePageNoNativeContent(
    bool online,
    const GURL& distillable_url) {
  [ChromeEarlGrey waitForWebStateContainingText:kContentToKeep];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          distillable_url.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Test that the offline and online pages are properly displayed.
  if (online) {
    [ChromeEarlGrey waitForWebStateContainingText:kContentToRemove];
    [ChromeEarlGrey waitForWebStateContainingText:kContentToKeep];
  } else {
    [ChromeEarlGrey waitForWebStateNotContainingText:kContentToRemove];
    [ChromeEarlGrey waitForWebStateContainingText:kContentToKeep];
  }

  // Test the presence of the omnibox offline chip.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::PageSecurityInfoIndicator(),
                            chrome_test_util::ImageViewWithImageNamed(
                                @"location_bar_offline"),
                            nil)]
      assertWithMatcher:online ? grey_nil() : grey_notNil()];
}

void AssertIsShowingDistillablePageNativeContent(bool online,
                                                 const GURL& distillable_url) {
  NSString* contentToKeep = base::SysUTF8ToNSString(kContentToKeep);
  // There will be multiple reloads, wait for the page to be displayed.
  if (online) {
    // Due to the reloads, a timeout longer than what is provided in
    // [ChromeEarlGrey waitForWebStateContainingText] is required, so call
    // WebViewContainingText directly.
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                   kLoadOfflineTimeout,
                   ^bool {
                     return [ChromeEarlGreyAppInterface
                         webStateContainsText:@(kContentToKeep)];
                   }),
               @"Waiting for online page.");
  } else {
    WaitForStaticHtmlViewContainingText(contentToKeep);
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          distillable_url.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Test that the offline and online pages are properly displayed.
  if (online) {
    [ChromeEarlGrey
        waitForWebStateContainingText:base::SysNSStringToUTF8(contentToKeep)];
    WaitForStaticHtmlViewNotContainingText(contentToKeep);
  } else {
    [ChromeEarlGrey waitForWebStateNotContainingText:kContentToKeep];
    WaitForStaticHtmlViewContainingText(contentToKeep);
  }

  // Test the presence of the omnibox offline chip.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::PageSecurityInfoIndicator(),
                            chrome_test_util::ImageViewWithImageNamed(
                                @"location_bar_offline"),
                            nil)]
      assertWithMatcher:online ? grey_nil() : grey_notNil()];
}

// Tests that the correct version of kDistillableURL is displayed.
void AssertIsShowingDistillablePage(bool online, const GURL& distillable_url) {
  if ([ReadingListAppInterface isOfflinePageWithoutNativeContentEnabled]) {
    return AssertIsShowingDistillablePageNoNativeContent(online,
                                                         distillable_url);
  }
  return AssertIsShowingDistillablePageNativeContent(online, distillable_url);
}

}  // namespace

// Test class for the Reading List menu.
@interface ReadingListTestCase : ChromeTestCase
// YES if test server is replying with valid HTML content (URL query). NO if
// test server closes the socket.
@property(atomic) bool serverRespondsWithContent;

// The delay after which self.testServer will send a response.
@property(atomic) NSTimeInterval serverResponseDelay;
@end

@implementation ReadingListTestCase
@synthesize serverRespondsWithContent = _serverRespondsWithContent;
@synthesize serverResponseDelay = _serverResponseDelay;

- (void)setUp {
  [super setUp];
  GREYAssertNil([ReadingListAppInterface clearEntries],
                @"Unable to clear Reading List entries");
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kDistillableURL,
      base::BindRepeating(&HandleQueryOrCloseSocket,
                          std::cref(_serverRespondsWithContent),
                          std::cref(_serverResponseDelay), true)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kNonDistillableURL,
      base::BindRepeating(&HandleQueryOrCloseSocket,
                          std::cref(_serverRespondsWithContent),
                          std::cref(_serverResponseDelay), false)));
  self.serverRespondsWithContent = true;
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDown {
  [super tearDown];
  [ReadingListAppInterface resetConnectionType];
}

// Tests that the Reading List view is accessible.
- (void)testAccessibility {
  AddEntriesAndEnterEdit();
  // In edit mode.
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  TapToolbarButtonWithID(kReadingListToolbarCancelButtonID);
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version via context menu.
- (void)testSavingToReadingListAndLoadDistilled {
  [ReadingListAppInterface forceConnectionToWifi];
  GURL distillablePageURL(self.testServer->GetURL(kDistillableURL));
  GURL nonDistillablePageURL(self.testServer->GetURL(kNonDistillableURL));
  // Open http://potato
  [ChromeEarlGrey loadURL:distillablePageURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:nonDistillablePageURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(kDistillableTitle);

  WaitForDistillation();

  // Long press the entry, and open it offline.
  LongPressEntry(kDistillableTitle);

  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_CONTENT_CONTEXT_OFFLINE);
  [ChromeEarlGrey waitForPageToFinishLoading];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(1));
  AssertIsShowingDistillablePage(false, distillablePageURL);

  // Tap the Omnibox' Info Bubble to open the Page Info.
  OpenPageSecurityInfoBubble();
  // Verify that the Page Info is about offline pages.
  id<GREYMatcher> pageInfoTitleMatcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_PAGE_INFO_OFFLINE_TITLE);
  [[EarlGrey selectElementWithMatcher:pageInfoTitleMatcher]
      assertWithMatcher:grey_notNil()];

  // Verify that the webState's title is correct.
  GREYAssertEqualObjects([ChromeEarlGreyAppInterface currentTabTitle],
                         kDistillableTitle, @"Wrong page name");
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads online version by tapping on entry.
- (void)testSavingToReadingListAndLoadNormal {
  [ReadingListAppInterface forceConnectionToWifi];
  GURL distillableURL = self.testServer->GetURL(kDistillableURL);
  // Open http://potato
  [ChromeEarlGrey loadURL:distillableURL];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(kDistillableTitle);
  WaitForDistillation();

  // Press the entry, and open it online.
  TapEntry(kDistillableTitle);

  AssertIsShowingDistillablePage(true, distillableURL);
  // Stop server to reload offline.
  self.serverRespondsWithContent = NO;
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));

  [ChromeEarlGreyAppInterface startReloading];
  AssertIsShowingDistillablePage(false, distillableURL);
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version by tapping on entry without web server.
- (void)testSavingToReadingListAndLoadNoNetwork {
  [ReadingListAppInterface forceConnectionToWifi];
  GURL distillableURL = self.testServer->GetURL(kDistillableURL);
  // Open http://potato
  [ChromeEarlGrey loadURL:distillableURL];

  AddCurrentPageToReadingList();

  // Navigate to http://beans

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(kDistillableTitle);
  WaitForDistillation();

  // Stop server to generate error.
  self.serverRespondsWithContent = NO;
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));
  // Long press the entry, and open it offline.
  TapEntry(kDistillableTitle);
  AssertIsShowingDistillablePage(false, distillableURL);

  // Reload. As server is still down, the offline page should show again.
  [ChromeEarlGreyAppInterface startReloading];
  AssertIsShowingDistillablePage(false, distillableURL);

  // TODO(crbug.com/954248) This DCHECK's (but works) with slimnav disabled.
  if ([ChromeEarlGrey isSlimNavigationManagerEnabled]) {
    [ChromeEarlGrey goBack];
    [ChromeEarlGrey goForward];
    AssertIsShowingDistillablePage(false, distillableURL);
  }

  // Start server to reload online error.
  self.serverRespondsWithContent = YES;
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));

  [ChromeEarlGreyAppInterface startReloading];
  AssertIsShowingDistillablePage(true, distillableURL);
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version by tapping on entry with delayed web server.
- (void)testSavingToReadingListAndLoadBadNetwork {
  [ReadingListAppInterface forceConnectionToWifi];
  GURL distillableURL = self.testServer->GetURL(kDistillableURL);
  // Open http://potato
  [ChromeEarlGrey loadURL:distillableURL];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading
  OpenReadingList();
  AssertEntryVisible(kDistillableTitle);
  WaitForDistillation();

  self.serverResponseDelay = kDelayForSlowWebServer;
  // Open the entry.
  TapEntry(kDistillableTitle);

  AssertIsShowingDistillablePage(false, distillableURL);

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey goForward];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(1));
  AssertIsShowingDistillablePage(false, distillableURL);

  // Reload should load online page.
  [ChromeEarlGreyAppInterface startReloading];
  AssertIsShowingDistillablePage(true, distillableURL);
  // Reload should load offline page.
  [ChromeEarlGreyAppInterface startReloading];
  AssertIsShowingDistillablePage(false, distillableURL);
}

// Tests that only the "Edit" button is showing when not editing.
- (void)testVisibleButtonsNonEditingMode {
  GREYAssertNil(
      [ReadingListAppInterface addEntryWithURL:[NSURL URLWithString:kUnreadURL]
                                         title:kUnreadTitle
                                          read:NO],
      @"Unable to add Reading List entry.");
  OpenReadingList();

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarMarkButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarEditButtonID);
}

// Tests that only the "Cancel", "Delete All Read" and "Mark All…" buttons are
// showing when not editing.
- (void)testVisibleButtonsEditingModeEmptySelection {
  AddEntriesAndEnterEdit();

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarEditButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark Unread" buttons are showing
// when not editing.
- (void)testVisibleButtonsOnlyReadEntrySelected {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarEditButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark Read" buttons are showing
// when not editing.
- (void)testVisibleButtonsOnlyUnreadEntrySelected {
  AddEntriesAndEnterEdit();
  TapEntry(kUnreadTitle);

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark…" buttons are showing when
// not editing.
- (void)testVisibleButtonsMixedEntriesSelected {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteAllReadButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarEditButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_BUTTON);
}

// Tests the deletion of selected entries.
- (void)testDeleteEntries {
  AddEntriesAndEnterEdit();

  TapEntry(kReadTitle2);

  TapToolbarButtonWithID(kReadingListToolbarDeleteButtonID);

  AssertEntryVisible(kReadTitle);
  AssertEntryNotVisible(kReadTitle2);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);
  XCTAssertEqual([ReadingListAppInterface readEntriesCount],
                 static_cast<long>(kNumberReadEntries - 1));
  XCTAssertEqual([ReadingListAppInterface unreadEntriesCount],
                 kNumberUnreadEntries);
}

// Tests the deletion of all read entries.
- (void)testDeleteAllReadEntries {
  AddEntriesAndEnterEdit();

  TapToolbarButtonWithID(kReadingListToolbarDeleteAllReadButtonID);

  AssertEntryNotVisible(kReadTitle);
  AssertEntryNotVisible(kReadTitle2);
  AssertHeaderNotVisible(kReadHeader);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);
  XCTAssertEqual(0l, [ReadingListAppInterface readEntriesCount]);
  XCTAssertEqual(kNumberUnreadEntries,
                 [ReadingListAppInterface unreadEntriesCount]);
}

// Marks all unread entries as read.
- (void)testMarkAllRead {
  AddEntriesAndEnterEdit();

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_MARK_ALL_READ_ACTION);

  AssertHeaderNotVisible(kUnreadHeader);
  AssertAllEntriesVisible();
  XCTAssertEqual(static_cast<long>(kNumberUnreadEntries + kNumberReadEntries),
                 [ReadingListAppInterface readEntriesCount]);
  XCTAssertEqual(0l, [ReadingListAppInterface unreadEntriesCount]);
}

// Marks all read entries as unread.
- (void)testMarkAllUnread {
  AddEntriesAndEnterEdit();

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_MARK_ALL_UNREAD_ACTION);

  AssertHeaderNotVisible(kReadHeader);
  AssertAllEntriesVisible();
  XCTAssertEqual(static_cast<long>(kNumberUnreadEntries + kNumberReadEntries),
                 [ReadingListAppInterface unreadEntriesCount]);
  XCTAssertEqual(0l, [ReadingListAppInterface readEntriesCount]);
}

// Marks all read entries as unread, when there is a lot of entries. This is to
// prevent crbug.com/1013708 from regressing.
- (void)testMarkAllUnreadLotOfEntry {
  AddLotOfEntriesAndEnterEdit();

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_MARK_ALL_UNREAD_ACTION);

  AssertHeaderNotVisible(kReadHeader);
}

// Selects an unread entry and mark it as read.
- (void)testMarkEntriesRead {
  AddEntriesAndEnterEdit();
  TapEntry(kUnreadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  AssertAllEntriesVisible();
  XCTAssertEqual(static_cast<long>(kNumberReadEntries + 1),
                 [ReadingListAppInterface readEntriesCount]);
  XCTAssertEqual(static_cast<long>(kNumberUnreadEntries - 1),
                 [ReadingListAppInterface unreadEntriesCount]);
}

// Selects an read entry and mark it as unread.
- (void)testMarkEntriesUnread {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  AssertAllEntriesVisible();
  XCTAssertEqual(static_cast<long>(kNumberReadEntries - 1),
                 [ReadingListAppInterface readEntriesCount]);
  XCTAssertEqual(static_cast<long>(kNumberUnreadEntries + 1),
                 [ReadingListAppInterface unreadEntriesCount]);
}

// Selects read and unread entries and mark them as unread.
- (void)testMarkMixedEntriesUnread {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);

  AssertAllEntriesVisible();
  XCTAssertEqual(static_cast<long>(kNumberReadEntries - 1),
                 [ReadingListAppInterface readEntriesCount]);
  XCTAssertEqual(static_cast<long>(kNumberUnreadEntries + 1),
                 [ReadingListAppInterface unreadEntriesCount]);
}

// Selects read and unread entries and mark them as read.
- (void)testMarkMixedEntriesRead {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);

  AssertAllEntriesVisible();
  XCTAssertEqual(static_cast<long>(kNumberReadEntries + 1),
                 [ReadingListAppInterface readEntriesCount]);
  XCTAssertEqual(static_cast<long>(kNumberUnreadEntries - 1),
                 [ReadingListAppInterface unreadEntriesCount]);
}

// Tests that you can delete multiple read items in the Reading List without
// creating a crash (crbug.com/701956).
- (void)testDeleteMultipleItems {
  // Add entries.
  for (int i = 0; i < 11; i++) {
    NSURL* url =
        [NSURL URLWithString:[kReadURL stringByAppendingFormat:@"%d", i]];
    NSString* title = [kReadURL stringByAppendingFormat:@"%d", i];
    GREYAssertNil([ReadingListAppInterface addEntryWithURL:url
                                                     title:title
                                                      read:YES],
                  @"Unable to add Reading List entry.");
  }

  // Delete them from the Reading List view.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:EmptyBackground()]
      assertWithMatcher:grey_nil()];
  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
  TapToolbarButtonWithID(kReadingListToolbarDeleteAllReadButtonID);

  // Verify the background string is displayed.
  [[EarlGrey selectElementWithMatcher:EmptyBackground()]
      assertWithMatcher:grey_notNil()];
}

// Tests that the VC can be dismissed by swiping down.
- (void)testSwipeDownDismiss {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  if (![ChromeEarlGreyAppInterface isCollectionsCardPresentationStyleEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on when feature flag is off.");
  }

  GREYAssertNil(
      [ReadingListAppInterface addEntryWithURL:[NSURL URLWithString:kUnreadURL]
                                         title:kUnreadTitle
                                          read:NO],
      @"Unable to add Reading List entry.");
  OpenReadingList();

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      assertWithMatcher:grey_nil()];
}

@end

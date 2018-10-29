// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include <memory>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar_button_identifiers.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_cell_favicon_badge_view.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_empty_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/reload_type.h"
#import "ios/web/public/test/http_server/delayed_response_provider.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/base/network_change_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kContentToRemove[] = "Text that distillation should remove.";
const char kContentToKeep[] = "Text that distillation should keep.";
const char kDistillableTitle[] = "Tomato";
const char kDistillableURL[] = "http://potato";
const char kNonDistillableURL[] = "http://beans";
const char kReadTitle[] = "foobar";
const char kReadURL[] = "http://readfoobar.com";
const char kUnreadTitle[] = "I am an unread entry";
const char kUnreadURL[] = "http://unreadfoobar.com";
const char kReadURL2[] = "http://kReadURL2.com";
const char kReadTitle2[] = "read item 2";
const char kUnreadTitle2[] = "I am another unread entry";
const char kUnreadURL2[] = "http://unreadfoobar2.com";
const size_t kNumberReadEntries = 2;
const size_t kNumberUnreadEntries = 2;
const CFTimeInterval kSnackbarAppearanceTimeout = 5;
const CFTimeInterval kSnackbarDisappearanceTimeout =
    MDCSnackbarMessageDurationMax + 1;
const CFTimeInterval kDelayForSlowWebServer = 4;
const CFTimeInterval kLoadOfflineTimeout = kDelayForSlowWebServer + 1;
const CFTimeInterval kLongPressDuration = 1.0;
const CFTimeInterval kDistillationTimeout = 5;
const CFTimeInterval kServerOperationDelay = 1;
const char kReadHeader[] = "Read";
const char kUnreadHeader[] = "Unread";

// Overrides the NetworkChangeNotifier to enable distillation even if the device
// does not have network.
class WifiNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  WifiNetworkChangeNotifier() : net::NetworkChangeNotifier() {}

  ConnectionType GetCurrentConnectionType() const override {
    return CONNECTION_WIFI;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiNetworkChangeNotifier);
};

// Returns the string concatenated |n| times.
std::string operator*(const std::string& s, unsigned int n) {
  std::ostringstream out;
  for (unsigned int i = 0; i < n; i++)
    out << s;
  return out.str();
}

// Returns the reading list model.
ReadingListModel* GetReadingListModel() {
  ReadingListModel* model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(2,
                                                   ^{
                                                     return model->loaded();
                                                   }),
      @"Reading List model did not load");
  return model;
}

// Scroll to the top of the Reading List.
void ScrollToTop() {
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID([
                                          [ReadingListTableViewController class]
                                          accessibilityIdentifier])]
      performAction:grey_scrollToContentEdgeWithStartPoint(kGREYContentEdgeTop,
                                                           0.5, 0.5)
              error:&error];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Asserts that the "mark" toolbar button is visible and has the a11y label of
// |a11y_label_id|.
void AssertToolbarMarkButtonText(int a11y_label_id) {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(kReadingListToolbarMarkButtonID),
              grey_ancestor(grey_kindOfClass([UIToolbar class])),
              chrome_test_util::ButtonWithAccessibilityLabelId(a11y_label_id),
              nil)] assertWithMatcher:grey_sufficientlyVisible()];
}

// Asserts the |button_id| toolbar button is not visible.
void AssertToolbarButtonNotVisibleWithID(NSString* button_id) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(button_id),
                                          grey_ancestor(grey_kindOfClass(
                                              [UIToolbar class])),
                                          nil)]
      assertWithMatcher:grey_notVisible()];
}

// Assert the |button_id| toolbar button is visible.
void AssertToolbarButtonVisibleWithID(NSString* button_id) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(button_id),
                                          grey_ancestor(grey_kindOfClass(
                                              [UIToolbar class])),
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
void PerformActionOnEntry(const std::string& entryTitle,
                          id<GREYAction> action) {
  ScrollToTop();
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                     base::SysUTF8ToNSString(entryTitle)),
                 grey_ancestor(grey_kindOfClass([TableViewURLCell class])),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(
                               [ReadingListTableViewController class])]
      performAction:action];
}

// Taps the entry with the title |entryTitle|.
void TapEntry(const std::string& entryTitle) {
  PerformActionOnEntry(entryTitle, grey_tap());
}

// Long-presses the entry with the title |entryTitle|.
void LongPressEntry(const std::string& entryTitle) {
  PerformActionOnEntry(entryTitle,
                       grey_longPressWithDuration(kLongPressDuration));
}

// Asserts that the entry with the title |entryTitle| is visible.
void AssertEntryVisible(const std::string& entryTitle) {
  ScrollToTop();
  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(entryTitle)),
                     grey_ancestor(grey_kindOfClass([TableViewURLCell class])),
                     grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(
                               [[ReadingListTableViewController class]
                                   accessibilityIdentifier])]
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
void AssertEntryNotVisible(std::string title) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ScrollToTop();
  NSError* error;

  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(title)),
                     grey_ancestor(grey_kindOfClass([TableViewURLCell class])),
                     grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(
                               [[ReadingListTableViewController class]
                                   accessibilityIdentifier])]
      assertWithMatcher:grey_notNil()
                  error:&error];
  GREYAssertNotNil(error, @"Entry is visible");
}

// Asserts |header| is visible.
void AssertHeaderNotVisible(std::string header) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ScrollToTop();
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          StaticTextWithAccessibilityLabel(
                                              base::SysUTF8ToNSString(header))]
      assertWithMatcher:grey_notVisible()];
}

// Opens the reading list menu using command line.
void OpenReadingList() {
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() showReadingList];
}

// Adds a read and an unread entry to the model, opens the reading list menu and
// enter edit mode.
void AddEntriesAndEnterEdit() {
  ReadingListModel* model = GetReadingListModel();
  model->AddEntry(GURL(kReadURL), std::string(kReadTitle),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->SetReadStatus(GURL(kReadURL), true);
  model->AddEntry(GURL(kReadURL2), std::string(kReadTitle2),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->SetReadStatus(GURL(kReadURL2), true);
  model->AddEntry(GURL(kUnreadURL), std::string(kUnreadTitle),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->AddEntry(GURL(kUnreadURL2), std::string(kUnreadTitle2),
                  reading_list::ADDED_VIA_CURRENT_APP);
  OpenReadingList();

  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
}

// Computes the number of read entries in the model.
size_t ModelReadSize(ReadingListModel* model) {
  size_t size = 0;
  for (const auto& url : model->Keys()) {
    if (model->GetEntryByURL(url)->IsRead()) {
      size++;
    }
  }
  return size;
}

// Returns a match for the Reading List Empty Collection Background.
id<GREYMatcher> EmptyBackground() {
  return grey_accessibilityID(
      [[TableViewEmptyView class] accessibilityIdentifier]);
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
  if (net::NetworkChangeNotifier::IsOffline()) {
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
  }
}

// Wait until one element is distilled.
void WaitForDistillation() {
  NSString* a11y_id =
      [TableViewURLCellFaviconBadgeView accessibilityIdentifier];
  ConditionBlock wait_for_distillation_date = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(a11y_id)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kDistillationTimeout, wait_for_distillation_date),
             @"Item was not distilled.");
}

// Returns the responses for a web server that can serve a distillable content
// at kDistillableURL and a not distillable content at kNotDistillableURL.
std::map<GURL, std::string> ResponsesForDistillationServer() {
  // Setup a server serving a distillable page at http://potato with the title
  // "tomato", and a non distillable page at http://beans
  std::map<GURL, std::string> responses;
  std::string page_title = "Tomato";
  const GURL distillable_page_url =
      web::test::HttpServer::MakeUrl(kDistillableURL);

  std::string content_to_remove(kContentToRemove);
  std::string content_to_keep(kContentToKeep);
  // Distillation only occurs on pages that are not too small.
  responses[distillable_page_url] =
      "<html><head><title>" + page_title + "</title></head>" +
      content_to_remove * 20 + "<article>" + content_to_keep * 20 +
      "</article>" + content_to_remove * 20 + "</html>";
  const GURL non_distillable_page_url =
      web::test::HttpServer::MakeUrl(kNonDistillableURL);
  responses[non_distillable_page_url] =
      "<html><head><title>greens</title></head></html>";
  return responses;
}

// Opens the page security info bubble.
void OpenPageSecurityInfoBubble() {
  // In UI Refresh, the security info is accessed through the tools menu.
  [ChromeEarlGreyUI openToolsMenu];
  // Tap on the Page Info button.
  [ChromeEarlGreyUI
      tapToolsMenuButton:grey_accessibilityID(kToolsMenuSiteInformation)];
}

// Tests that the correct version of kDistillableURL is displayed.
void AssertIsShowingDistillablePage(bool online) {
  NSString* contentToKeep = base::SysUTF8ToNSString(kContentToKeep);
  // There will be multiple reloads, wait for the page to be displayed.
  if (online) {
    // Due to the reloads, a timeout longer than what is provided in
    // [ChromeEarlGrey waitForWebViewContainingText] is required, so call
    // WebViewContainingText directly.
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                   kLoadOfflineTimeout,
                   ^bool {
                     return web::test::IsWebViewContainingText(
                         chrome_test_util::GetCurrentWebState(),
                         kContentToKeep);
                   }),
               @"Waiting for online page.");
  } else {
    [ChromeEarlGrey waitForStaticHTMLViewContainingText:contentToKeep];
  }

  // Test Omnibox URL
  GURL distillableURL = web::test::HttpServer::MakeUrl(kDistillableURL);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          distillableURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Test that the offline and online pages are properly displayed.
  if (online) {
    [ChromeEarlGrey
        waitForWebViewContainingText:base::SysNSStringToUTF8(contentToKeep)];
    [ChromeEarlGrey waitForStaticHTMLViewNotContainingText:contentToKeep];
  } else {
    [ChromeEarlGrey waitForWebViewNotContainingText:kContentToKeep];
    [ChromeEarlGrey waitForStaticHTMLViewContainingText:contentToKeep];
  }

  // Test the presence of the omnibox offline chip.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::PageSecurityInfoIndicator(),
                            chrome_test_util::ImageViewWithImageNamed(
                                @"location_bar_offline"),
                            nil)]
      assertWithMatcher:online ? grey_nil() : grey_notNil()];
}

}  // namespace

// Test class for the Reading List menu.
@interface ReadingListTestCase : ChromeTestCase

@end

@implementation ReadingListTestCase

- (void)setUp {
  [super setUp];
  ReadingListModel* model = GetReadingListModel();
  for (const GURL& url : model->Keys())
    model->RemoveEntryByURL(url);
}

- (void)tearDown {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  server.SetSuspend(NO);
  if (!server.IsRunning()) {
    server.StartOrDie();
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromSecondsD(kServerOperationDelay));
  }
  [super tearDown];
}

// Tests that the Reading List view is accessible.
- (void)testAccessibility {
  AddEntriesAndEnterEdit();
  // In edit mode.
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  TapToolbarButtonWithID(kReadingListToolbarCancelButtonID);
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version via context menu.
// TODO(crbug.com/796082): Re-enable this test on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSavingToReadingListAndLoadDistilled \
  testSavingToReadingListAndLoadDistilled
#else
#define MAYBE_testSavingToReadingListAndLoadDistilled \
  FLAKY_testSavingToReadingListAndLoadDistilled
#endif
- (void)MAYBE_testSavingToReadingListAndLoadDistilled {
  auto network_change_disabler =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  auto wifi_network = std::make_unique<WifiNetworkChangeNotifier>();
  web::test::SetUpSimpleHttpServer(ResponsesForDistillationServer());
  GURL distillablePageURL(web::test::HttpServer::MakeUrl(kDistillableURL));
  GURL nonDistillablePageURL(
      web::test::HttpServer::MakeUrl(kNonDistillableURL));
  std::string pageTitle(kDistillableTitle);
  // Open http://potato
  [ChromeEarlGrey loadURL:distillablePageURL];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:nonDistillablePageURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(pageTitle);

  WaitForDistillation();

  // Long press the entry, and open it offline.
  LongPressEntry(pageTitle);
  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_CONTENT_CONTEXT_OFFLINE);
  AssertIsShowingDistillablePage(false);

  // Tap the Omnibox' Info Bubble to open the Page Info.
  OpenPageSecurityInfoBubble();
  // Verify that the Page Info is about offline pages.
  id<GREYMatcher> pageInfoTitleMatcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_PAGE_INFO_OFFLINE_TITLE);
  [[EarlGrey selectElementWithMatcher:pageInfoTitleMatcher]
      assertWithMatcher:grey_notNil()];

  // Verify that the webState's title is correct.
  XCTAssertTrue(chrome_test_util::GetCurrentWebState()->GetTitle() ==
                base::ASCIIToUTF16(pageTitle.c_str()));
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads online version by tapping on entry.
// TODO(crbug.com/796082): Re-enable this test on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSavingToReadingListAndLoadNormal \
  testSavingToReadingListAndLoadNormal
#else
#define MAYBE_testSavingToReadingListAndLoadNormal \
  FLAKY_testSavingToReadingListAndLoadNormal
#endif
- (void)MAYBE_testSavingToReadingListAndLoadNormal {
  auto network_change_disabler =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  auto wifi_network = std::make_unique<WifiNetworkChangeNotifier>();
  web::test::SetUpSimpleHttpServer(ResponsesForDistillationServer());
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  std::string pageTitle(kDistillableTitle);

  // Open http://potato
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDistillableURL)];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(pageTitle);
  WaitForDistillation();

  // Long press the entry, and open it offline.
  TapEntry(pageTitle);

  AssertIsShowingDistillablePage(true);
  // Stop server to reload offline.
  server.SetSuspend(YES);
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));

  chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
      web::ReloadType::NORMAL, false);
  AssertIsShowingDistillablePage(false);
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version by tapping on entry without web server.
// TODO(crbug.com/796082): Re-enable this test on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSavingToReadingListAndLoadNoNetwork \
  testSavingToReadingListAndLoadNoNetwork
#else
#define MAYBE_testSavingToReadingListAndLoadNoNetwork \
  FLAKY_testSavingToReadingListAndLoadNoNetwork
#endif
- (void)MAYBE_testSavingToReadingListAndLoadNoNetwork {
  auto network_change_disabler =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  auto wifi_network = std::make_unique<WifiNetworkChangeNotifier>();
  web::test::SetUpSimpleHttpServer(ResponsesForDistillationServer());
  std::string pageTitle(kDistillableTitle);
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  // Open http://potato
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDistillableURL)];

  AddCurrentPageToReadingList();

  // Navigate to http://beans

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(pageTitle);
  WaitForDistillation();

  // Stop server to generate error.
  server.SetSuspend(YES);
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));
  // Long press the entry, and open it offline.
  TapEntry(pageTitle);

  AssertIsShowingDistillablePage(false);
  // Start server to reload online error.
  server.SetSuspend(NO);
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromSecondsD(kServerOperationDelay));
  web::test::SetUpSimpleHttpServer(ResponsesForDistillationServer());

  chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
      web::ReloadType::NORMAL, false);
  AssertIsShowingDistillablePage(true);
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version by tapping on entry with delayed web server.
// TODO(crbug.com/796082): Re-enable this test on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSavingToReadingListAndLoadBadNetwork \
  testSavingToReadingListAndLoadBadNetwork
#else
#define MAYBE_testSavingToReadingListAndLoadBadNetwork \
  FLAKY_testSavingToReadingListAndLoadBadNetwork
#endif
- (void)MAYBE_testSavingToReadingListAndLoadBadNetwork {
  auto network_change_disabler =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  auto wifi_network = std::make_unique<WifiNetworkChangeNotifier>();
  web::test::SetUpSimpleHttpServer(ResponsesForDistillationServer());
  std::string pageTitle(kDistillableTitle);
  // Open http://potato
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kDistillableURL)];

  AddCurrentPageToReadingList();

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kNonDistillableURL)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(pageTitle);
  WaitForDistillation();

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(ResponsesForDistillationServer()),
      kDelayForSlowWebServer));
  // Long press the entry, and open it offline.
  TapEntry(pageTitle);

  AssertIsShowingDistillablePage(false);

  // TODO(crbug.com/724555): Re-enable the reload checks.
  if (false) {
    // Reload should load online page.
    chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, false);
    AssertIsShowingDistillablePage(true);
    // Reload should load offline page.
    chrome_test_util::GetCurrentWebState()->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, false);
    AssertIsShowingDistillablePage(false);
  }
}

// Tests that only the "Edit" button is showing when not editing.
- (void)testVisibleButtonsNonEditingMode {
  GetReadingListModel()->AddEntry(GURL(kUnreadURL), std::string(kUnreadTitle),
                                  reading_list::ADDED_VIA_CURRENT_APP);
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
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries, GetReadingListModel()->unread_size());
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
  XCTAssertEqual((size_t)0, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries, GetReadingListModel()->unread_size());
}

// Marks all unread entries as read.
- (void)testMarkAllRead {
  AddEntriesAndEnterEdit();

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  // Tap the action sheet.
  TapContextMenuButtonWithA11yLabelID(
      IDS_IOS_READING_LIST_MARK_ALL_READ_ACTION);

  if (@available(iOS 11, *)) {
    // On iOS10, removing UITableView sections don't remove their header text
    // from the hierarchy.
    AssertHeaderNotVisible(kUnreadHeader);
  }

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberUnreadEntries + kNumberReadEntries,
                 ModelReadSize(GetReadingListModel()));
  XCTAssertEqual((size_t)0, GetReadingListModel()->unread_size());
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
  XCTAssertEqual(kNumberUnreadEntries + kNumberReadEntries,
                 GetReadingListModel()->unread_size());
  XCTAssertEqual((size_t)0, ModelReadSize(GetReadingListModel()));
}

// Selects an unread entry and mark it as read.
- (void)testMarkEntriesRead {
  AddEntriesAndEnterEdit();
  TapEntry(kUnreadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries + 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries - 1,
                 GetReadingListModel()->unread_size());
}

// Selects an read entry and mark it as unread.
- (void)testMarkEntriesUnread {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries + 1,
                 GetReadingListModel()->unread_size());
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
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries + 1,
                 GetReadingListModel()->unread_size());
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
  XCTAssertEqual(kNumberReadEntries + 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries - 1,
                 GetReadingListModel()->unread_size());
}

// Tests that you can delete multiple read items in the Reading List without
// creating a crash (crbug.com/701956).
- (void)testDeleteMultipleItems {
  // Add entries.
  ReadingListModel* model = GetReadingListModel();
  for (int i = 0; i < 11; i++) {
    std::string increment = std::to_string(i);
    model->AddEntry(GURL(kReadURL + increment),
                    std::string(kReadTitle + increment),
                    reading_list::ADDED_VIA_CURRENT_APP);
    model->SetReadStatus(GURL(kReadURL + increment), true);
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

@end

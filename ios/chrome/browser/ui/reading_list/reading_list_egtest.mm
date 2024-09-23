// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <functional>
#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#import "net/base/network_change_notifier.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::DeleteButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::ReadingListMarkAsReadButton;
using chrome_test_util::ReadingListMarkAsUnreadButton;
using reading_list_test_utils::AddedToLocalReadingListSnackbar;
using reading_list_test_utils::OpenReadingList;
using reading_list_test_utils::VisibleReadingListItem;

namespace {
const char kContentToRemove[] = "Text that distillation should remove.";
const char kContentToKeep[] = "Text that distillation should keep.";
NSString* const kDistillableTitle = @"Tomato";
const char kDistillableURL[] = "/potato";
const char kNonDistillableURL[] = "/beans";
const char kRedImageURL[] = "/redimage";
const char kGreenImageURL[] = "/greenimage";
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
constexpr base::TimeDelta kDelayForSlowWebServer = base::Seconds(4);
constexpr base::TimeDelta kLongPressDuration = base::Seconds(1);
constexpr base::TimeDelta kDistillationTimeout = base::Seconds(5);
constexpr base::TimeDelta kServerOperationDelay = base::Seconds(1);
NSString* const kReadHeader = @"Read";
NSString* const kUnreadHeader = @"Unread";

NSString* const kCheckImagesJS =
    @"function checkImages() {"
    @"  for (img of document.getElementsByTagName('img')) {"
    @"    s = img.src;"
    @"    data = s.startsWith('data:');"
    @"    loaded = img.complete && (img.naturalWidth > 0);"
    @"    if (data != loaded) return false;"
    @"  }"
    @"  return true;"
    @"}"
    @"checkImages();";

// Returns the string concatenated `n` times.
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
// `a11y_label_id`.
void AssertToolbarMarkButtonText(int a11y_label_id) {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(kReadingListToolbarMarkButtonID),
              grey_ancestor(grey_kindOfClassName(@"UIToolbar")),
              chrome_test_util::ButtonWithAccessibilityLabelId(a11y_label_id),
              nil)] assertWithMatcher:grey_sufficientlyVisible()];
}

// Asserts the `button_id` toolbar button is not visible.
void AssertToolbarButtonNotVisibleWithID(NSString* button_id) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(button_id),
                                          grey_ancestor(grey_kindOfClassName(
                                              @"UIToolbar")),
                                          nil)]
      assertWithMatcher:grey_notVisible()];
}

// Assert the `button_id` toolbar button is visible.
void AssertToolbarButtonVisibleWithID(NSString* button_id) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(button_id),
                                          grey_ancestor(grey_kindOfClassName(
                                              @"UIToolbar")),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Taps the `button_id` toolbar button.
void TapToolbarButtonWithID(NSString* button_id) {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(button_id)]
      performAction:grey_tap()];
}

// Taps the context menu button with the a11y label of `a11y_label_id`.
void TapContextMenuButtonWithA11yLabelID(int a11y_label_id) {
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     a11y_label_id)] performAction:grey_tap()];
}

// Performs `action` on the entry with the title `entryTitle`. The view can be
// scrolled down to find the entry.
void PerformActionOnEntry(NSString* entryTitle, id<GREYAction> action) {
  ScrollToTop();
  [[[EarlGrey selectElementWithMatcher:VisibleReadingListItem(entryTitle)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      performAction:action];
}

// Taps the entry with the title `entryTitle`.
void TapEntry(NSString* entryTitle) {
  PerformActionOnEntry(entryTitle, grey_tap());
}

// Long-presses the entry with the title `entryTitle`.
void LongPressEntry(NSString* entryTitle) {
  PerformActionOnEntry(entryTitle,
                       grey_longPressWithDuration(kLongPressDuration));
}

// Asserts that the entry with the title `entryTitle` is visible.
void AssertEntryVisible(NSString* entryTitle) {
  ScrollToTop();
  [[[EarlGrey selectElementWithMatcher:VisibleReadingListItem(entryTitle)]
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

// Asserts that the entry `title` is not visible.
void AssertEntryNotVisible(NSString* title) {
  [ChromeEarlGreyUI waitForAppToIdle];
  ScrollToTop();
  NSError* error;

  [[[EarlGrey selectElementWithMatcher:VisibleReadingListItem(title)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      assertWithMatcher:grey_notNil()
                  error:&error];
  GREYAssertNotNil(error, @"Entry is visible");
}

// Asserts `header` is visible.
void AssertHeaderNotVisible(NSString* header) {
  [ChromeEarlGreyUI waitForAppToIdle];
  ScrollToTop();
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(header)]
      assertWithMatcher:grey_notVisible()];
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

// Adds 2 read and 2 unread entries to the model, opens the reading list menu.
void AddEntriesAndOpenReadingList() {
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
}

void AddEntriesAndEnterEdit() {
  AddEntriesAndOpenReadingList();
  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
}

// Adds the current page to the Reading List.
void AddCurrentPageToReadingList() {
  // Add the page to the reading list.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:chrome_test_util::ButtonWithAccessibilityLabelId(
                             IDS_IOS_SHARE_MENU_READING_LIST_ACTION)];
  id<GREYMatcher> matcher =
      grey_allOf(reading_list_test_utils::AddedToLocalReadingListSnackbar(),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  [ReadingListAppInterface notifyWifiConnection];
}

// Wait until one element is distilled.
void WaitForDistillation() {
  ConditionBlock wait_for_distillation_date = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityID(
                                         kTableViewURLCellFaviconBadgeViewID),
                                     grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kDistillationTimeout, wait_for_distillation_date),
             @"Item was not distilled.");
}

// Serves URLs. Response can be delayed by `delay` second or return an error if
// `responds_with_content` is false.
// If `distillable`, result is can be distilled for offline display.
std::unique_ptr<net::test_server::HttpResponse> HandleQueryOrCloseSocket(
    const bool& responds_with_content,
    const base::TimeDelta& delay,
    bool distillable,
    const net::test_server::HttpRequest& request) {
  if (!responds_with_content) {
    return std::make_unique<net::test_server::RawHttpResponse>(
        /*headers=*/"", /*contents=*/"");
  }
  auto response =
      std::make_unique<net::test_server::DelayedHttpResponse>(delay);

  if (base::StartsWith(request.relative_url, kDistillableURL)) {
    response->set_content_type("text/html");
    std::string page_title = "Tomato";

    std::string content_to_remove(kContentToRemove);
    std::string content_to_keep(kContentToKeep);
    std::string green_image_url(kGreenImageURL);
    std::string red_image_url(kRedImageURL);

    response->set_content("<html><head><title>" + page_title +
                          "</title></head>" + content_to_remove * 20 +
                          "<article>" + content_to_keep * 20 + "<img src='" +
                          green_image_url +
                          "'/>"
                          "<img src='" +
                          red_image_url +
                          "'/>"
                          "</article>" +
                          content_to_remove * 20 + "</html>");
    return std::move(response);
  }
  if (base::StartsWith(request.relative_url, kNonDistillableURL)) {
    response->set_content_type("text/html");
    response->set_content("<html><head><title>greens</title></head></html>");
    return std::move(response);
  }
  NOTREACHED_IN_MIGRATION();
  return std::move(response);
}

// Serves image URLs.
// If `serve_red_image` is false, 404 error is returned when red image is
// requested.
// `served_red_image` will be set to true whenever red image is requested.
std::unique_ptr<net::test_server::HttpResponse> HandleImageQueryOrCloseSocket(
    const bool& serve_red_image,
    bool& served_red_image,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (base::StartsWith(request.relative_url, kGreenImageURL)) {
    response->set_content_type("image/png");
    UIImage* image = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(10, 10), [UIColor greenColor]);
    NSData* image_data = UIImagePNGRepresentation(image);
    response->set_content(std::string(
        static_cast<const char*>(image_data.bytes), image_data.length));
    return std::move(response);
  }
  if (base::StartsWith(request.relative_url, kRedImageURL)) {
    served_red_image = true;
    if (!serve_red_image) {
      response->set_code(net::HTTP_NOT_FOUND);
      return std::move(response);
    }
    response->set_content_type("image/png");
    UIImage* image = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(10, 10), [UIColor redColor]);
    NSData* image_data = UIImagePNGRepresentation(image);
    response->set_content(std::string(
        static_cast<const char*>(image_data.bytes), image_data.length));
    return std::move(response);
  }
  NOTREACHED_IN_MIGRATION();
  return std::move(response);
}

// Opens the page security info bubble.
void OpenPageSecurityInfoBubble() {
  // In UI Refresh, the security info is accessed through the tools menu.
  [ChromeEarlGreyUI openToolsMenu];
  // Tap on the Page Info button.
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::SiteInfoDestinationButton()];
}

// Tests that the correct version of kDistillableURL is displayed.
void AssertIsShowingDistillablePage(bool online, const GURL& distillable_url) {
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
  UIImage* symbol =
      DefaultSymbolTemplateWithPointSize(kDownloadPromptFillSymbol, 10);

  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::PageSecurityInfoIndicator(),
                            chrome_test_util::ImageViewWithImage(symbol), nil)]
      assertWithMatcher:online ? grey_nil() : grey_notNil()];
}

}  // namespace

// Test class for the Reading List menu.
@interface ReadingListTestCase : ChromeTestCase
// YES if test server is replying with valid HTML content (URL query). NO if
// test server closes the socket.
@property(nonatomic, assign) bool serverRespondsWithContent;
// YES if test server is replying with valid read image. NO if it responds with
// 404 error.
@property(nonatomic, assign) bool serverServesRedImage;
// Server sets this to true when it is requested the red image.
@property(nonatomic, assign) bool serverServedRedImage;

// The delay after which self.testServer will send a response.
@property(nonatomic, assign) base::TimeDelta serverResponseDelay;
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
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kGreenImageURL,
      base::BindRepeating(&HandleImageQueryOrCloseSocket,
                          std::cref(_serverServesRedImage),
                          std::ref(_serverServedRedImage))));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kRedImageURL,
      base::BindRepeating(&HandleImageQueryOrCloseSocket,
                          std::cref(_serverServesRedImage),
                          std::ref(_serverServedRedImage))));
  self.serverRespondsWithContent = true;
  self.serverServesRedImage = true;
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey stopWatcher];
}

- (void)tearDown {
  [ChromeEarlGrey stopWatcher];
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

// Tests that navigating back to an offline page is still displaying the error
// page and don't mess the navigation stack.
- (void)testNavigateBackToDistilledPage {
  [ReadingListAppInterface forceConnectionToWifi];
  GURL distillablePageURL(self.testServer->GetURL(kDistillableURL));
  GURL nonDistillablePageURL(self.testServer->GetURL(kNonDistillableURL));
  // Open http://potato
  [ChromeEarlGrey loadURL:distillablePageURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  AddCurrentPageToReadingList();

  // Verify that an entry with the correct title is present in the reading list.
  OpenReadingList();
  AssertEntryVisible(kDistillableTitle);

  WaitForDistillation();

  // Long press the entry, and open it offline.
  LongPressEntry(kDistillableTitle);

  int offlineStringId = IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON;

  TapContextMenuButtonWithA11yLabelID(offlineStringId);
  [ChromeEarlGrey waitForPageToFinishLoading];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  AssertIsShowingDistillablePage(false, distillablePageURL);

  // Navigate to http://beans
  [ChromeEarlGrey loadURL:nonDistillablePageURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  [ChromeEarlGrey goBack];

  [ChromeEarlGrey waitForPageToFinishLoading];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  // Check that the online version is now displayed.
  AssertIsShowingDistillablePage(true, distillablePageURL);
  GREYAssertEqual(1, [ChromeEarlGrey navigationBackListItemsCount],
                  @"The NTP page should be the first committed URL.");

  // Check that navigating forward navigates to the correct page.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          nonDistillablePageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
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

  int offlineStringId = IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON;

  TapContextMenuButtonWithA11yLabelID(offlineStringId);
  [ChromeEarlGrey waitForPageToFinishLoading];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  AssertIsShowingDistillablePage(false, distillablePageURL);

  // Tap the Omnibox' Info Bubble to open the Page Info.
  OpenPageSecurityInfoBubble();
  // Verify that the Page Info is about offline pages.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_PAGE_INFO_OFFLINE_PAGE_LABEL))]
      assertWithMatcher:grey_notNil()];

  // Verify that the webState's title is correct.
  GREYAssertEqualObjects([ChromeEarlGrey currentTabTitle], kDistillableTitle,
                         @"Wrong page name");
}

// Tests that URL can be added in the incognito mode and that a snackbar
// appears after the item is added. See https://crbug.com/1428055.
- (void)testSavingToReadingListInIncognito {
  GURL pageURL(self.testServer->GetURL(kDistillableURL));
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  AddCurrentPageToReadingList();
}

// Tests that offline page does not request online resources.
- (void)testSavingToReadingListAndLoadDistilledNoOnlineResource {
  self.serverServesRedImage = false;
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
  self.serverServesRedImage = true;
  self.serverServedRedImage = false;

  // Long press the entry, and open it offline.
  LongPressEntry(kDistillableTitle);

  int offlineStringId = IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON;

  TapContextMenuButtonWithA11yLabelID(offlineStringId);
  [ChromeEarlGrey waitForPageToFinishLoading];
  AssertIsShowingDistillablePage(false, distillablePageURL);
  GREYAssertFalse(self.serverServedRedImage,
                  @"Offline page accessed online resource.");

  base::Value checkImage = [ChromeEarlGrey evaluateJavaScript:kCheckImagesJS];

  GREYAssertTrue(checkImage.is_bool(), @"CheckImage is not a boolean.");
  GREYAssert(checkImage.GetBool(), @"Incorrect image loading.");

  // Verify that the webState's title is correct.
  GREYAssertEqualObjects([ChromeEarlGrey currentTabTitle], kDistillableTitle,
                         @"Wrong page name");
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
  base::test::ios::SpinRunLoopWithMinDelay(kServerOperationDelay);

  [ChromeEarlGrey startReloading];
  AssertIsShowingDistillablePage(false, distillableURL);
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version by tapping on entry without web server.
// TODO(crbug.com/40840653): Fix flakiness.
- (void)DISABLED_testSavingToReadingListAndLoadNoNetwork {
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
  base::test::ios::SpinRunLoopWithMinDelay(kServerOperationDelay);
  // Long press the entry, and open it offline.
  TapEntry(kDistillableTitle);
  AssertIsShowingDistillablePage(false, distillableURL);

  // Reload. As server is still down, the offline page should show again.
  [ChromeEarlGrey startReloading];
  AssertIsShowingDistillablePage(false, distillableURL);

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey goForward];
  AssertIsShowingDistillablePage(false, distillableURL);

  // Start server to reload online error.
  self.serverRespondsWithContent = YES;
  base::test::ios::SpinRunLoopWithMinDelay(kServerOperationDelay);

  [ChromeEarlGrey startReloading];
  AssertIsShowingDistillablePage(true, distillableURL);
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
// Loads offline version by tapping on entry with delayed web server.
// crbug.com/1382372: Reenable this test.
- (void)DISABLED_testSavingToReadingListAndLoadBadNetwork {
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
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  AssertIsShowingDistillablePage(false, distillableURL);

  // Reload should load online page.
  [ChromeEarlGrey startReloading];
  AssertIsShowingDistillablePage(true, distillableURL);
  // Reload should load offline page.
  [ChromeEarlGrey startReloading];
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

// Tests that the "Cancel", "Edit" and "Mark Unread" buttons are not visible
// after delete (using swipe).
- (void)testVisibleButtonsAfterSwipeDeletion {
  AddEntriesAndOpenReadingList();

  [[[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kReadTitle)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100)
      onElementWithMatcher:grey_accessibilityID(kReadingListViewID)]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  id<GREYMatcher> deleteButtonMatcher =
      grey_allOf(grey_accessibilityLabel(@"Delete"),
                 grey_kindOfClassName(@"UISwipeActionStandardButton"), nil);
  // Depending on the device, the swipe may have deleted the element or just
  // displayed the "Delete" button. Check if the delete button is still on
  // screen and tap it if it is the case.
  GREYCondition* waitForDeleteToDisappear = [GREYCondition
      conditionWithName:@"Element is already deleted"
                  block:^{
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:deleteButtonMatcher]
                        assertWithMatcher:grey_nil()
                                    error:&error];
                    return error == nil;
                  }];

  bool matchedElement = [waitForDeleteToDisappear
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];

  if (!matchedElement) {
    // Delete button is still on screen, tap it
    [[EarlGrey selectElementWithMatcher:deleteButtonMatcher]
        performAction:grey_tap()];
  }

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarMarkButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarEditButtonID);
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

  AssertToolbarButtonVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarEditButtonID);

  TapToolbarButtonWithID(kReadingListToolbarDeleteButtonID);

  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarMarkButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarDeleteButtonID);
  AssertToolbarButtonNotVisibleWithID(kReadingListToolbarCancelButtonID);
  AssertToolbarButtonVisibleWithID(kReadingListToolbarEditButtonID);

  AssertEntryVisible(kReadTitle);
  AssertEntryNotVisible(kReadTitle2);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);
  GREYAssertEqual([ReadingListAppInterface readEntriesCount],
                  static_cast<long>(kNumberReadEntries - 1),
                  @"Wrong number of read entry after delete.");
  GREYAssertEqual([ReadingListAppInterface unreadEntriesCount],
                  kNumberUnreadEntries,
                  @"Wrong number of unread entry after delete.");

  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
  TapEntry(kReadTitle);
  TapToolbarButtonWithID(kReadingListToolbarDeleteButtonID);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_text(@"Read"),
                                   grey_ancestor(grey_kindOfClassName(
                                       @"_UITableViewHeaderFooterContentView")),
                                   nil)] assertWithMatcher:grey_nil()];

  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
  TapEntry(kUnreadTitle);
  TapEntry(kUnreadTitle2);
  TapToolbarButtonWithID(kReadingListToolbarDeleteButtonID);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_text(@"Unread"),
                                   grey_ancestor(grey_kindOfClassName(
                                       @"_UITableViewHeaderFooterContentView")),
                                   nil)] assertWithMatcher:grey_nil()];
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
  GREYAssertEqual(0l, [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of unread entry.");
  GREYAssertEqual(kNumberUnreadEntries,
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entries.");
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
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries + kNumberReadEntries),
                  [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of read entries after marking all read.");
  GREYAssertEqual(0l, [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entries after marking all read.");
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
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries + kNumberReadEntries),
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entries after marking all unread.");
  GREYAssertEqual(0l, [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of read entries after marking all unread.");
}

// Marks all read entries as unread, when there is a lot of entries. This is to
// prevent crbug.com/1013708 and crbug.com/1246283 from regressing.
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
  GREYAssertEqual(static_cast<long>(kNumberReadEntries + 1),
                  [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of read entries after marking read.");
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries - 1),
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entries after marking read.");
}

// Selects an read entry and mark it as unread.
- (void)testMarkEntriesUnread {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);

  AssertToolbarMarkButtonText(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  TapToolbarButtonWithID(kReadingListToolbarMarkButtonID);

  AssertAllEntriesVisible();
  GREYAssertEqual(static_cast<long>(kNumberReadEntries - 1),
                  [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of read entries after marking unread.");
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries + 1),
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entries after marking unread.");
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
  GREYAssertEqual(static_cast<long>(kNumberReadEntries - 1),
                  [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of unread entry after marking unread.");
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries + 1),
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of read entry after marking unread.");
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
  GREYAssertEqual(static_cast<long>(kNumberReadEntries + 1),
                  [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of read entry after marking read.");
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries - 1),
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entry after marking read.");
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

  OpenReadingList();

  // Make sure the Reading List view is not empty. Therefore, the illustration,
  // title and subtitles shoud not be present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewIllustratedEmptyViewID)]
      assertWithMatcher:grey_nil()];

  id<GREYMatcher> noReadingListTitleMatcher = grey_allOf(
      grey_text(l10n_util::GetNSString(IDS_IOS_READING_LIST_NO_ENTRIES_TITLE)),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:noReadingListTitleMatcher]
      assertWithMatcher:grey_nil()];

  id<GREYMatcher> noReadingListMessageMatcher = grey_allOf(
      grey_text(
          l10n_util::GetNSString(IDS_IOS_READING_LIST_NO_ENTRIES_MESSAGE)),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:noReadingListMessageMatcher]
      assertWithMatcher:grey_nil()];

  // Delete them from the Reading List view.
  TapToolbarButtonWithID(kReadingListToolbarEditButtonID);
  TapToolbarButtonWithID(kReadingListToolbarDeleteAllReadButtonID);

  // Verify the background string is displayed.
  [self verifyReadingListIsEmpty];
}

// Tests that the VC can be dismissed by swiping down.
- (void)testSwipeDownDismiss {
  // TODO(crbug.com/40149458): Test disabled on iOS14 iPhones.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS14 iPhones.");
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

// Tests the Copy Link context menu action for a reading list entry.
- (void)testContextMenuCopyLink {
  AddEntriesAndOpenReadingList();
  LongPressEntry(kReadTitle);

  // Tap "Copy URL" and wait for the URL to be copied to the pasteboard.
  [ChromeEarlGrey verifyCopyLinkActionWithText:kReadURL];
}

// Tests the Open in New Tab context menu action for a reading list entry.
- (void)testContextMenuOpenInNewTab {
  GURL distillablePageURL(self.testServer->GetURL(kDistillableURL));
  [self addURLToReadingList:distillablePageURL];
  LongPressEntry(kDistillableTitle);

  // Select "Open in New Tab" and confirm that new tab is opened with selected
  // URL.
  [ChromeEarlGrey
      verifyOpenInNewTabActionWithURL:distillablePageURL.GetContent()];
}

// Tests display and selection of 'Open in New Incognito Tab' in a context menu
// on a history entry.
- (void)testContextMenuOpenInIncognito {
  GURL distillablePageURL(self.testServer->GetURL(kDistillableURL));
  [self addURLToReadingList:distillablePageURL];
  LongPressEntry(kDistillableTitle);

  // Select "Open in Incognito" and confirm that new tab is opened with selected
  // URL.
  [ChromeEarlGrey
      verifyOpenInIncognitoActionWithURL:distillablePageURL.GetContent()];
}

// Tests the Mark as Read/Unread context menu action for a reading list entry.
- (void)testContextMenuMarkAsReadAndBack {
  AddEntriesAndOpenReadingList();

  AssertAllEntriesVisible();
  GREYAssertEqual(static_cast<long>(kNumberReadEntries),
                  [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of read entry.");
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries),
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entry.");

  // Mark an unread entry as read.
  LongPressEntry(kUnreadTitle);

  [[EarlGrey selectElementWithMatcher:ReadingListMarkAsReadButton()]
      performAction:grey_tap()];

  AssertAllEntriesVisible();
  GREYAssertEqual(static_cast<long>(kNumberReadEntries + 1),
                  [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of read entry after marking read.");
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries - 1),
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entry after marking read.");

  // Now mark it back as unread.
  LongPressEntry(kUnreadTitle);

  [[EarlGrey selectElementWithMatcher:ReadingListMarkAsUnreadButton()]
      performAction:grey_tap()];

  AssertAllEntriesVisible();
  GREYAssertEqual(static_cast<long>(kNumberReadEntries),
                  [ReadingListAppInterface readEntriesCount],
                  @"Wrong number of read entry after marking unread.");
  GREYAssertEqual(static_cast<long>(kNumberUnreadEntries),
                  [ReadingListAppInterface unreadEntriesCount],
                  @"Wrong number of unread entry after marking unread.");
}

// Tests the Share context menu action for a reading list entry.
- (void)testContextMenuShare {
  GURL distillablePageURL(self.testServer->GetURL(kDistillableURL));
  [self addURLToReadingList:distillablePageURL];
  LongPressEntry(kDistillableTitle);

  [ChromeEarlGrey verifyShareActionWithURL:distillablePageURL
                                 pageTitle:kDistillableTitle];
}

// Tests the Delete context menu action for a reading list entry.
- (void)testContextMenuDelete {
  GURL distillablePageURL(self.testServer->GetURL(kDistillableURL));
  [self addURLToReadingList:distillablePageURL];
  LongPressEntry(kDistillableTitle);

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  [self verifyReadingListIsEmpty];
}

// Tests that review account settings promo is shown if the user is signed in
// only but reading list account storage is off and gets removed after enabling
// the toggle.
- (void)testReviewAccountSettingsPromoWithReadingListToggleDisabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // By default, `signinWithFakeIdentity` above enables reading list data type,
  // so turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kReadingList)
                          enabled:NO];

  OpenReadingList();
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSignedInWithPrimaryAccount];

  // Open the settings using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Turn Reading List On.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncReadingListIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/YES)];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the promo disappears.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SettingsDoneButton(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that review account settings promo is not shown if the user is signed
// in only and reading list account storage is already enabled.
- (void)testAccountSettingsPromoIfSyncToSigninEnabledWithReadingListOn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  OpenReadingList();
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that account settings are viewed from the reading list manager and
// account gets removed.
- (void)testAccountSettingsViewedFroReadingListManager {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables reading list data type,
  // so turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kReadingList)
                          enabled:NO];
  OpenReadingList();
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSignedInWithPrimaryAccount];

  // Open the settings using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Remove identity from device.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];

  // Verify that Account Settings is closed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
  // Sign in promo shows.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Tests that account settings promo is displayed when the reading list view
// is opened from an incognito tab.
// See: crbug.com/339472472.
- (void)testAccountSettingsHiddenFromIncognitoTab {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  [ChromeEarlGrey openNewIncognitoTab];
  // By default, `signinWithFakeIdentity` above enables reading list data type,
  // so turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kReadingList)
                          enabled:NO];
  OpenReadingList();
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSignedInWithPrimaryAccount];

  // Open the settings using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests review account settings promo changes to a sign-in promo after signing
// out from account settings.
- (void)testSignOutFromAccountSettingsFromReadingListManager {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables reading list data type,
  // so turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kReadingList)
                          enabled:NO];
  OpenReadingList();
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSignedInWithPrimaryAccount];

  // Open the settings using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll to the bottom to view the signout button.
  id<GREYMatcher> scroll_view_matcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scroll_view_matcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Sign out" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];

  // Verify that Account Settings is closed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Dismiss sign out snackbar.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(l10n_util::GetNSString(
              IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE))]
      performAction:grey_tap()];

  // Sign in promo shows and try to sign in succeeds.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     (IDS_IOS_SIGNIN_PROMO_REVIEW_READING_LIST_SETTINGS)))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
}

// Tests the review account settings promo does not show after signing in as
// reading list gets enabled by default on sign-in.
- (void)testNoReviewAccountSettingsPromo {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables reading list data type,
  // so turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kReadingList)
                          enabled:NO];

  // Sign out.
  [SigninEarlGreyUI signOut];

  // Sign in from Reading List promo.
  OpenReadingList();
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSStringF(
                                   IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
                                   base::SysNSStringToUTF16(
                                       fakeIdentity1.userEmail)))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify Account Settings promo does not show.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Verify that the reading list type is now enabled.
  GREYAssertTrue(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kReadingList],
      @"Reading list should be enabled.");
}

// Tests that reading list type gets disabled as it was before signing in when
// the snackbar undo is tapped.
- (void)testUndoSignInTypeDisabled {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables reading list data type,
  // so turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kReadingList)
                          enabled:NO];

  // Sign out.
  [SigninEarlGreyUI signOut];

  // Sign in from Reading List promo.
  OpenReadingList();
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap undo button from the snackbar.
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
      base::SysNSStringToUTF16(fakeIdentity1.userEmail));
  [[EarlGrey selectElementWithMatcher:grey_text(snackbarMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninSnackbarUndo),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  // Sign back in without using the promo.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // Verify that the reading list type is disabled as it was before signing in.
  GREYAssertFalse(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kReadingList],
      @"Reading list should be disabled.");
}

// Tests that reading list type remains enabled as it was before signing in even
// when the snackbar undo is tapped.
- (void)testUndoSignInTypeEnabled {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // Make sure reading list type is enabled.
  GREYAssertTrue(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kReadingList],
      @"Reading list should be enabled.");

  // Sign out.
  [SigninEarlGreyUI signOut];

  // Sign in from Reading List promo.
  OpenReadingList();
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap undo button from the snackbar.
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
      base::SysNSStringToUTF16(fakeIdentity1.userEmail));
  [[EarlGrey selectElementWithMatcher:grey_text(snackbarMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninSnackbarUndo),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  // Sign back in without using the promo.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // Verify that the reading list type remains enabled as it was before signing
  // in.
  GREYAssertTrue(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kReadingList],
      @"Reading list should be enabled.");
}

#pragma mark - Multiwindow

// Tests the Open in New Window context menu action for a reading list entry.
// TODO(crbug.com/40807345): Test is flaky
- (void)testContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  GURL distillablePageURL(self.testServer->GetURL(kDistillableURL));
  [self addURLToReadingList:distillablePageURL];
  LongPressEntry(kDistillableTitle);

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:kContentToKeep];
}

#pragma mark - Helper Methods

- (void)verifyReadingListIsEmpty {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kTableViewIllustratedEmptyViewID)]
        assertWithMatcher:grey_notNil()];

    // The dimiss animation takes 2 steps, and without the two waits below this
    // test will flake.
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        grey_text(l10n_util::GetNSString(
                            IDS_IOS_READING_LIST_NO_ENTRIES_TITLE))];

    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        grey_text(l10n_util::GetNSString(
                            IDS_IOS_READING_LIST_NO_ENTRIES_MESSAGE))];
}

- (void)addURLToReadingList:(const GURL&)URL {
  [ReadingListAppInterface forceConnectionToWifi];

  // Open http://potato
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  AddCurrentPageToReadingList();

  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey openNewTab];
  OpenReadingList();
}

@end

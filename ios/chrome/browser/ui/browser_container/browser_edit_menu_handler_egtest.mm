// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_app_interface.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kElementToLongPress[] = "selectid";

// An HTML template that puts some text in a simple span element.
const char kBasicSelectionUrl[] = "/basic";
const char kBasicSelectionHtmlTemplate[] =
    "<html>"
    "  <head>"
    "    <meta name='viewport' content='width=device-width, "
    "      initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "    />"
    "  </head>"
    "  <body>"
    "    Page Loaded <br/><br/>"
    "    This text contains a <span id='selectid'>SELECTION_TEXT</span>."
    "  </body>"
    "</html>";

// An HTML template that puts some text in a readonly input.
// Using a readonly input helps selecting the whole text without interfering
// with the context menu.
const char kInputSelectionUrl[] = "/input";
const char kInputSelectionHtmlTemplate[] =
    "<html>"
    "  <head>"
    "    <meta name='viewport' content='width=device-width, "
    "      initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "    />"
    "  </head>"
    "  <body>"
    "    Page Loaded <br/><br/>"
    "    This text contains a "
    "    <input id='selectid' "
    "           value='SELECTION_TEXT' "
    "           readonly size=SELECTION_SIZE>."
    "  </body>"
    "</html>";

// An HTML template that puts some text in an editable input.
const char kEditableInputSelectionUrl[] = "/editableinput";
const char kEditableInputSelectionHtmlTemplate[] =
    "<html>"
    "  <head>"
    "    <meta name='viewport' content='width=device-width, "
    "      initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "    />"
    "  </head>"
    "  <body>"
    "    Page Loaded <br/><br/>"
    "    This text contains a "
    "    <input id='selectid' "
    "           value='SELECTION_TEXT' "
    "           size=SELECTION_SIZE>."
    "  </body>"
    "</html>";

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  GURL request_url = request.GetURL();
  std::string html;

  if (request_url.path_piece() == kBasicSelectionUrl) {
    html = kBasicSelectionHtmlTemplate;
  } else if (request_url.path_piece() == kInputSelectionUrl) {
    html = kInputSelectionHtmlTemplate;
  } else if (request_url.path_piece() == kEditableInputSelectionUrl) {
    html = kEditableInputSelectionHtmlTemplate;
  } else {
    return nullptr;
  }

  std::string text;
  bool has_text = net::GetValueForKeyInQuery(request_url, "text", &text);
  if (has_text) {
    base::ReplaceFirstSubstringAfterOffset(&html, 0, "SELECTION_TEXT", text);
    base::ReplaceFirstSubstringAfterOffset(&html, 0, "SELECTION_SIZE",
                                           base::NumberToString(text.size()));
  }
  http_response->set_content(html);

  return std::move(http_response);
}

// Convenient function to wait for element disappearance
// Note: chrome_test_util::WaitForUIElementToDisappear waits for element to
// match grey_nil which is different.
void WaitForUIElementToDisappear(id<GREYMatcher> matcher) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error != nil;
  };

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"did not disappear");
}

// Go through the pages and Retrieve the visible Edit Menu actions.
// Note: The function must be called when the Edit Menu is visible and on
// page 1.
NSArray* GetEditMenuActions() {
  // The menu should be visible.
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Start on first screen (previous not visible or disabled).
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface
                                          editMenuPreviousButtonMatcher]]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)
                  error:&error];
  GREYAssert(error, @"GetEditMenuActions not called on the first page.");

  NSMutableArray* items = [NSMutableArray array];
  while (true) {
    NSArray* page_items = [EditMenuAppInterface editMenuActions];
    // `page_items` is an EDO object so it cannot be used directly.
    // Copy its items instead.
    for (NSString* item in page_items) {
      [items addObject:item];
    }
    error = nil;
    // If a next button is present and enabled, press on it to access next page.
    [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface
                                            editMenuNextButtonMatcher]]
        assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                     nil)
                    error:&error];
    if (error) {
      // There is no next button enabled, the last page has been reached.
      // break out of the loop.
      break;
    }
    [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface
                                            editMenuNextButtonMatcher]]
        performAction:grey_tap()];

    // Wait until the last element disappeared.
    WaitForUIElementToDisappear([EditMenuAppInterface
        editMenuActionWithAccessibilityLabel:[items lastObject]]);
  }
  return items;
}

// Long presses on `element_id`.
void LongPressElement(const char* element_id) {
  // Use triggers_context_menu = true as this is really "triggers_browser_menu".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:element_id],
                        true)];
}

// Convenient function to trigger the Edit Menu on `kElementToLongPress`.
void TriggerEditMenu() {
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_notVisible()];
  LongPressElement(kElementToLongPress);

  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  if (error) {
    // If edit is not visible, try to tap the element again.
    // This is possible on inputs when the first long press just selects the
    // input.
    LongPressElement(kElementToLongPress);
    [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Convenient function to trigger the Edit Menu on `kElementToLongPress` and
// tap "Select All".
// Long pressing will only select a portion of the string in the input. Pressing
// "Select all" ensures that the whole string is selected.
// Note: if the whole string is already selected, pressing "Select all" will
// make the Edit Menu disappear. So depending on the length and structure of
// the string to select, use TriggerEditMenu or SelectAllInput.
void SelectAllInput() {
  TriggerEditMenu();
  [[EarlGrey selectElementWithMatcher:
                 [EditMenuAppInterface
                     editMenuActionWithAccessibilityLabel:@"Select All"]]
      performAction:grey_tap()];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// The accessibility label of the translate button depending on whether it is
// replaced by partial translate.
NSString* TranslateAccessibilityLabel() {
  return [PartialTranslateAppInterface installedPartialTranslate]
             ? @"Google Translate"
             : @"Translate";
}

// A convenience enum for the different types of menu.
enum EditMenuAdditionType {
  kNoneMenu = 0,
  kBasicMenu = 1,
  kEditableMenu = 2,
  kEmailMenu = 4,
  kPhoneMenu = 8,
  kDateMenu = 16,
  kUnitMenu = 32,
  kReadOnlyMenu = kBasicMenu | kEmailMenu | kPhoneMenu | kDateMenu | kUnitMenu,
  kAllMenu = kBasicMenu | kEditableMenu | kEmailMenu | kPhoneMenu | kDateMenu |
             kUnitMenu
};

// Helper functions to build expected menu depending on the OS and the type.
NSArray* BuildExpectedMenu(EditMenuAdditionType additions) {
  NSMutableArray* items = [NSMutableArray array];
  struct EditMenuEntry {
    NSString* accessibility_identifier;
    int min_ios_version;
    int entry_type;
  };

  bool device_has_camera = [UIImagePickerController
      isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera];

  std::vector<EditMenuEntry> entries{
      // com.apple.menu.standard-edit
      {@"Cut", 14, kEditableMenu},
      {@"Copy", 14, kAllMenu},
      // Paste is only present if the pasteboard has items.
      // {@"Paste", 16, kEditableMenu},
      {@"Select All", 16, kAllMenu},
      // com.apple.menu.replace
      {@"Replace\u2026", 14, kEditableMenu},
      {@"Scan Text", 16, device_has_camera ? kEditableMenu : kNoneMenu},
      // com.apple.menu.lookup
      {@"Look Up", 14, kBasicMenu | kEditableMenu | kDateMenu | kUnitMenu},
      {TranslateAccessibilityLabel(), 15,
       kBasicMenu | kEditableMenu | kDateMenu | kUnitMenu},
      // WebView additions
      {@"Search Web", 16, kBasicMenu | kEditableMenu | kDateMenu},
      {@"New Mail Message", 16, kEmailMenu},
      {@"Send Message", 16, kEmailMenu | kPhoneMenu},
      {@"Add to Contacts", 16, kEmailMenu | kPhoneMenu},
      {@"Create Event", 16, kDateMenu},
      {@"Create Reminder", 16, kDateMenu},
      {@"Show in Calendar", 16, kDateMenu},
      {@"Convert Meters", 16, kUnitMenu},
      {@"Convert Minutes", 16, kUnitMenu},
      // com.apple.menu.share
      {@"Share\u2026", 14, kAllMenu},
      // Chrome actions
      {@"Create Link", 16, kReadOnlyMenu},
  };

  for (EditMenuEntry entry : entries) {
    if (base::ios::IsRunningOnOrLater(entry.min_ios_version, 0, 0) &&
        additions & entry.entry_type) {
      [items addObject:entry.accessibility_identifier];
    }
  }

  return items;
}

}  // namespace

// Tests for the Edit Menu customization.
@interface BrowserEditMenuHandlerTestCase : ChromeTestCase

@end

@implementation BrowserEditMenuHandlerTestCase
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kIOSEditMenuPartialTranslate);
  config.features_enabled.push_back(kIOSCustomBrowserEditMenu);
  return config;
}

- (void)setUp {
  [super setUp];
  // Clear the pasteboard to be in a consistent state regarding the "Paste"
  // action.
  [[UIPasteboard generalPasteboard] setItems:@[]];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Conveniently load a page that has "text" in a selectable field.
- (void)loadPageWithType:(const std::string&)type
                 forText:(const std::string&)text {
  GURL url = self.testServer->GetURL(type);
  url = net::AppendQueryParameter(url, "text", text);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Page Loaded"];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];
}

// Tests the menu on a normal word.
- (void)testBasicMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // There is a EG syncing issue on iOS14 when displaying Edit Menu that makes
    // the test flaky.
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS15-");
  }
  std::string pageText = "text";
  [self loadPageWithType:kBasicSelectionUrl forText:pageText];
  TriggerEditMenu();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kBasicMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
}

- (void)testEditableMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // There is a EG syncing issue on iOS14 when displaying Edit Menu that makes
    // the test flaky.
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS15-");
  }
  std::string pageText = "mmmm";
  [self loadPageWithType:kEditableInputSelectionUrl forText:pageText];
  TriggerEditMenu();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kEditableMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
}

- (void)testEmailMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  std::string pageText = "test@chromium.org";
  [self loadPageWithType:kInputSelectionUrl forText:pageText];
  SelectAllInput();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kEmailMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
}

- (void)testPhoneMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  std::string pageText = "(123)456-7890";
  [self loadPageWithType:kInputSelectionUrl forText:pageText];
  SelectAllInput();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kPhoneMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
}

- (void)testDateMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  // A date in one week in the future.
  NSDate* date = [NSDate dateWithTimeIntervalSinceNow:7 * 86400];
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"yyyy-MM-dd"];
  NSString* dateString = [formatter stringFromDate:date];
  std::string pageText = base::SysNSStringToUTF8(dateString);
  [self loadPageWithType:kInputSelectionUrl forText:pageText];
  SelectAllInput();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kDateMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
}

- (void)testUnitMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  std::string pageText = "1m";
  [self loadPageWithType:kInputSelectionUrl forText:pageText];
  TriggerEditMenu();

  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kUnitMenu);

  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
}

@end

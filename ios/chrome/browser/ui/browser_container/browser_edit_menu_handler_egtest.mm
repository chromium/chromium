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

const char kElementToSelect[] = "selectid";
const char kElementToSelectFirst[] = "dummyid";

// An HTML template that puts some text in a readonly input.
// Using a readonly input helps selecting the whole text without interfering
// with the context menu.
// -webkit-user-select is not always arctive immediately in the page.
// This page provides a dummy target that can be long pressed to trigger a
// 'normal' edit menu that seems to fix the issue.
// The label element prevents the annotation of the text that needs to be
// selected.
const char kDataSelectionUrl[] = "/data";
const char kDataSelectionHtmlTemplate[] =
    "<html>"
    "  <head>"
    "    <meta name='viewport' content='width=device-width, "
    "      initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "    />"
    "    <style>"
    "      .selectable {"
    "        -webkit-user-select: all;"
    "      }"
    "    </style>"
    "  </head>"
    "  <body>"
    "    Page Loaded<br/><br/>"
    "    Target for <span id='dummyid'>menu</span><br/><br/>"
    "    Data: <label><span class='selectable'>SELECTION_TEXT</label>"
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

  std::string text;
  bool has_text = net::GetValueForKeyInQuery(request_url, "text", &text);
  if (!has_text) {
    return nullptr;
  }

  if (request_url.path_piece() == kDataSelectionUrl) {
    html = kDataSelectionHtmlTemplate;
    text =
        "<span id='selectid'>" + text.substr(0, 1) + "</span>" + text.substr(1);
    base::ReplaceFirstSubstringAfterOffset(&html, 0, "SELECTION_TEXT", text);
  } else if (request_url.path_piece() == kEditableInputSelectionUrl) {
    html = kEditableInputSelectionHtmlTemplate;
    base::ReplaceFirstSubstringAfterOffset(&html, 0, "SELECTION_TEXT", text);
    base::ReplaceFirstSubstringAfterOffset(&html, 0, "SELECTION_SIZE",
                                           base::NumberToString(text.size()));
  } else {
    return nullptr;
  }
  http_response->set_content(html);

  return std::move(http_response);
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
    [ChromeEarlGrey
        waitForNotSufficientlyVisibleElementWithMatcher:
            [EditMenuAppInterface
                editMenuActionWithAccessibilityLabel:[items lastObject]]];
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

// Taps on `element_id`.
void TapElement(const char* element_id) {
  // Use unverified version as the event is handled by the browser.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementUnverified(
                        [ElementSelector selectorWithElementID:element_id])];
}

// Dismisses edit menu by tapping on the element.
void DismissEditMenuByTapping() {
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()];
  TapElement(kElementToSelect);
  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:[EditMenuAppInterface
                                                          editMenuMatcher]];
}

// Convenient function to trigger the Edit Menu on `kElementToLongPress`.
void TriggerEditMenuByTapping() {
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_notVisible()];

  TapElement(kElementToSelect);
  if ([ChromeEarlGrey
          testUIElementAppearanceWithMatcher:[EditMenuAppInterface
                                                 editMenuMatcher]]) {
    return;
  }

  // It seems that -webkit-user-select elements are not always active before
  // an edit menu is first triggered. Select a dummy word first, then press on
  // the real element that needs to be selected.
  LongPressElement(kElementToSelectFirst);
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[EditMenuAppInterface
                                                       editMenuMatcher]];
  TapElement(kElementToSelectFirst);
  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:[EditMenuAppInterface
                                                          editMenuMatcher]];

  // After long pressing another element, the first tap may fail.
  // Try a few times.
  for (int i = 0; i < 5; i++) {
    TapElement(kElementToSelect);
    if ([ChromeEarlGrey
            testUIElementAppearanceWithMatcher:[EditMenuAppInterface
                                                   editMenuMatcher]]) {
      return;
    }
  }
}

// Convenient function to trigger the Edit Menu on an input.
// Tap twice on the input, the first tap only selecting it, the second triggers
// the menu. Press "Select all" to select the content of the element.
void SelectAllInput() {
  TapElement(kElementToSelect);
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  TapElement(kElementToSelect);
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[EditMenuAppInterface
                                                       editMenuMatcher]];

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
  kLinkMenu = 64,
  kReadOnlyMenu =
      kBasicMenu | kEmailMenu | kPhoneMenu | kDateMenu | kUnitMenu | kLinkMenu,
  kAllMenu = kReadOnlyMenu | kEditableMenu
};

// Helper functions to build expected menu depending on the OS and the type.
NSArray* BuildExpectedMenu(EditMenuAdditionType additions) {
  NSMutableArray* items = [NSMutableArray array];
  struct EditMenuEntry {
    NSString* accessibility_identifier;
    int min_ios_version;
    int deprecation_ios_version;
    int entry_type;
  };

// There is no "Scan text" entry on simulator.
#if !TARGET_IPHONE_SIMULATOR
  bool device_has_camera = [UIImagePickerController
      isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera];
#endif

  std::vector<EditMenuEntry> entries{
      // com.apple.menu.standard-edit
    {@"Cut", 1400, 9900, kEditableMenu}, {@"Copy", 1400, 9900, kAllMenu},
        // Paste is only present if the pasteboard has items until iOS16.4.
        {@"Paste", 1604, 9900, kEditableMenu},
        {@"Select All", 1600, 1604, kAllMenu},
        // com.apple.menu.replace
        {@"Replace\u2026", 1400, 9900, kEditableMenu},
#if !TARGET_IPHONE_SIMULATOR
        {@"Scan Text", 1600, 9900,
         device_has_camera ? kEditableMenu : kNoneMenu},
#endif
        // com.apple.menu.lookup
        {@"Look Up", 1400, 9900,
         kBasicMenu | kEditableMenu | kDateMenu | kUnitMenu},
        {TranslateAccessibilityLabel(), 1500, 9900,
         kBasicMenu | kEditableMenu | kDateMenu | kUnitMenu},
        // WebView additions
        {@"Search Web", 1600, 9900, kBasicMenu | kEditableMenu | kDateMenu},
        {@"New Mail Message", 1600, 9900, kEmailMenu},
        {@"Send Message", 1600, 9900, kEmailMenu | kPhoneMenu},
        {@"Add to Contacts", 1600, 9900, kEmailMenu | kPhoneMenu},
        {@"Open Link", 1600, 9900, kLinkMenu},
        {@"Add to Reading List", 1600, 9900, kLinkMenu},
        {@"Create Event", 1600, 9900, kDateMenu},
        {@"Create Reminder", 1600, 9900, kDateMenu},
        {@"Show in Calendar", 1600, 9900, kDateMenu},
        {@"Convert Meters", 1600, 9900, kUnitMenu},
        {@"Convert Minutes", 1600, 9900, kUnitMenu},
        // com.apple.menu.share
        {@"Share\u2026", 1400, 9900, kAllMenu},
        // Chrome actions
        {@"Create Link", 1600, 9900, kReadOnlyMenu},
  };

  for (EditMenuEntry entry : entries) {
    if (base::ios::IsRunningOnOrLater(entry.min_ios_version / 100,
                                      entry.min_ios_version % 100, 0) &&
        !base::ios::IsRunningOnOrLater(entry.deprecation_ios_version / 100,
                                       entry.deprecation_ios_version % 100,
                                       0) &&
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
  // Note: on iOS16.4+, paste button is always visible.
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
  std::string pageText = "text";
  [self loadPageWithType:kDataSelectionUrl forText:pageText];
  TriggerEditMenuByTapping();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kBasicMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
  DismissEditMenuByTapping();
}

- (void)testEditableMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Test is flaky on iOS15- and there is no menu customization, so the test
    // is not needed. Just disable it.
    EARL_GREY_TEST_SKIPPED(@"Test is flaky on iOS15-.");
  }
  std::string pageText = "mmmm";
  [self loadPageWithType:kEditableInputSelectionUrl forText:pageText];
  SelectAllInput();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kEditableMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
  DismissEditMenuByTapping();
}

- (void)testURLMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  std::string pageText = "https://www.chromium.org/";
  [self loadPageWithType:kDataSelectionUrl forText:pageText];
  TriggerEditMenuByTapping();

  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kLinkMenu);

  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
  DismissEditMenuByTapping();
}

- (void)testEmailMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  std::string pageText = "test@chromium.org";
  [self loadPageWithType:kDataSelectionUrl forText:pageText];
  TriggerEditMenuByTapping();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kEmailMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
  DismissEditMenuByTapping();
}

- (void)testPhoneMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  std::string pageText = "(123)456-7890";
  [self loadPageWithType:kDataSelectionUrl forText:pageText];
  TriggerEditMenuByTapping();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kPhoneMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
  DismissEditMenuByTapping();
}

- (void)testDateMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  [self loadPageWithType:kDataSelectionUrl forText:"tomorrow"];
  TriggerEditMenuByTapping();
  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kDateMenu);
  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
  DismissEditMenuByTapping();
}

- (void)testUnitMenu {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"No contextual edit action on iOS15-");
  }
  std::string pageText = "1m";
  [self loadPageWithType:kDataSelectionUrl forText:pageText];
  TriggerEditMenuByTapping();

  NSArray* items = GetEditMenuActions();
  NSArray* expected = BuildExpectedMenu(EditMenuAdditionType::kUnitMenu);

  GREYAssertEqualObjects(items, expected, @"Edit Menu item don't match");
  DismissEditMenuByTapping();
}

@end

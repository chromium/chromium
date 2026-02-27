// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <vector>

#import "base/i18n/message_formatter.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/omnibox/browser/aim_eligibility_service_features.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_app_interface.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/tabs_egtest_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Web page. Used to create different tabs for the test.
const char kPageURL[] = "/page%d.html";
const char kPageTitle[] = "Title %d";
const char kPageContent[] = "Content %d";

// Matcher for the Composebox.
id<GREYMatcher> ComposeboxMatcher() {
  return grey_accessibilityID(kComposeboxAccessibilityIdentifier);
}

// Matcher for the clear button in the Composebox.
id<GREYMatcher> ComposeboxClearButtonMatcher() {
  return grey_allOf(
      grey_accessibilityID(kOmniboxClearButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

// A long text used to ensure the composebox is expanded when it is on a compact
// mode.
NSString* kLongText =
    @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    @"tempor incididunt ut labore et dolore magna aliqua.";

// Creates a simple HTTP response for `request`.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  // Extract page number from URL path (e.g., /page1.html -> 1)
  int page_number = 0;
  std::string url_path = request.relative_url;
  size_t start = url_path.find("page");
  size_t end = url_path.find(".html");
  if (start != std::string::npos && end != std::string::npos) {
    start += strlen("page");
    std::string number_str = url_path.substr(start, end - start);
    base::StringToInt(number_str, &page_number);
  }

  http_response->set_content(base::StringPrintf(
      "<html><head><title>Title "
      "%d</title></head><body class='page-%d'>Content %d</body></html>",
      page_number, page_number, page_number));
  return http_response;
}

// Opens the tab picker from the composebox.
void OpenTabPicker() {
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()
              error:&error];
  if (error) {
    [ChromeEarlGreyUI focusOmnibox];
  }

  // Wait for the composebox to be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:ComposeboxMatcher()];

  // Tap the plus button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Tap the select tabs button.
  id<GREYMatcher> selectTabsMatcher =
      grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:selectTabsMatcher]
      performAction:grey_tap()];
}

// Selects a tab in the tab picker with the given `title`.
void SelectTabWithTitle(NSString* title) {
  id<GREYMatcher> tabMatcher = TabWithTitle(title);
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:tabMatcher]
      assertWithMatcher:grey_minimumVisiblePercent(0.8)
                  error:&error];

  // Attempt to make the item visible by scrolling.
  if (error) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                kComposeboxTabPickerCollectionViewAccessibilityIdentifier)]
        performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
    [[EarlGrey selectElementWithMatcher:tabMatcher]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  [[EarlGrey selectElementWithMatcher:tabMatcher] performAction:grey_tap()];
}

id<GREYMatcher> TabCellAttachmentMatcherWithTitle(NSString* title) {
  std::u16string title_u16 = base::SysNSStringToUTF16(title);
  std::u16string pattern = l10n_util::GetStringUTF16(
      IDS_IOS_COMPOSEBOX_ATTACHMENT_TAB_ACCESSIBILITY_LABEL);
  std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
      pattern, "title", title_u16);
  return grey_allOf(grey_accessibilityLabel(base::SysUTF16ToNSString(message)),
                    grey_hidden(NO), nil);
}

// Verifies that a tab with the given `title` is attached in the composebox
// carousel. `firstTitle` is the title of the first element of the list, used
// to avoid scrolling when we are already at the edge.
void VerifyTabIsAttachedWithTitle(NSString* title) {
  id<GREYMatcher> cellMatcher = TabCellAttachmentMatcherWithTitle(title);
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:cellMatcher]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];

  // If not found, first try scrolling. Stop if the scroll failed.
  while (error) {
    NSError* scrollError = nil;
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(kComposeboxCarouselAccessibilityIdentifier)]
        performAction:grey_scrollInDirection(kGREYDirectionLeft, 100)
                error:&scrollError];
    if (scrollError) {
      break;
    }

    error = nil;
    [[EarlGrey selectElementWithMatcher:cellMatcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
  }

  // Final assertion to ensure the item is found.
  [[EarlGrey selectElementWithMatcher:cellMatcher]
      assertWithMatcher:grey_notNil()];
}

// Matcher for the close button of an item in the carousel.
id<GREYMatcher> ComposeboxInputItemCellCloseButtonMatcher() {
  return grey_allOf(
      grey_accessibilityID(
          kComposeboxInputItemCellCloseButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Removes an attachment with the given `title` from the composebox carousel.
void RemoveAttachmentWithTitle(NSString* title) {
  id<GREYMatcher> cellMatcher = TabCellAttachmentMatcherWithTitle(title);
  id<GREYMatcher> closeButtonMatcher =
      grey_allOf(ComposeboxInputItemCellCloseButtonMatcher(),
                 grey_ancestor(cellMatcher), nil);
  [[EarlGrey selectElementWithMatcher:closeButtonMatcher]
      performAction:grey_tap()];
}

}  // namespace

@interface ComposeboxTestCase : ChromeTestCase
@end

@implementation ComposeboxTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kComposeboxIOS);
  config.features_enabled.push_back(kComposeboxIpad);
  // Only rely on local conditions for AIM eligibility, so disable the
  // server-side checks.
  config.features_disabled.push_back(omnibox::kAimServerEligibilityEnabled);
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the Composebox is visible when tapping the omnibox.
- (void)testComposeboxVisibility {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Clear the omnibox.
  [[EarlGrey selectElementWithMatcher:ComposeboxClearButtonMatcher()]
      performAction:grey_tap()];

  // Check for Composebox elements.
  // Plus button is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Mic button is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxMicButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Send button is not visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxSendButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Composebox is hidden when not eligible.
- (void)testComposeboxHiddenWhenNotEligible {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ComposeboxAppInterface setAimEligible:NO];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Check that Composebox elements are NOT visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that typing in the Composebox shows the Send button.
- (void)testComposeboxSendButtonVisibility {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Wait for the composebox to be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:ComposeboxMatcher()];

  // Type some long text that expands the composebox.
  [[EarlGrey selectElementWithMatcher:ComposeboxMatcher()]
      performAction:grey_typeText(kLongText)];

  // Send button is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxSendButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Plus button is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Mic button is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxMicButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that image generation action is present when eligible.
- (void)testComposeboxCreateImageEligible {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ComposeboxAppInterface setCreateImagesEligible:YES];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Wait for the composebox to be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:ComposeboxMatcher()];

  // Tap the plus button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Tap the "Create image" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxImageGenerationActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify that the image generation button is visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              kComposeboxImageGenerationButtonAccessibilityIdentifier)];
}

// Tests that the image generation action is not available when not eligible.
- (void)testComposeboxCreateImageNotEligible {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ComposeboxAppInterface setCreateImagesEligible:NO];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Wait for the composebox to be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:ComposeboxMatcher()];

  // Tap the plus button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify that the "Create image" action is NOT visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxImageGenerationActionAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the AI mode action works as expected.
- (void)testComposeboxAIModeAction {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ComposeboxAppInterface setAimEligible:YES];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Wait for the composebox to be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:ComposeboxMatcher()];

  // Tap the plus button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Tap the "AI Mode" action.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxAIMActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify that the AI mode button is visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kComposeboxAIMButtonAccessibilityIdentifier)];

  // Tap the AI mode button to disable AI mode.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxAIMButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify that the AI mode button disappears.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxAIMButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that all buttons in the plus menu are enabled.
- (void)testPlusMenuButtonsEnabled {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Wait for the composebox to be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:ComposeboxMatcher()];

  // Tap the plus button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the buttons are enabled.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxImageGenerationActionAccessibilityIdentifier)]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxAIMActionAccessibilityIdentifier)]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxSelectTabsActionAccessibilityIdentifier)]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxAttachFileActionAccessibilityIdentifier)]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxGalleryActionAccessibilityIdentifier)]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxCameraActionAccessibilityIdentifier)]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxAttachCurrentTabActionAccessibilityIdentifier)]
      assertWithMatcher:grey_enabled()];
}

// Tests that tapping the attach tabs button opens the tab picker. Ensures that
// the title is set correctly and buttons are correctly enabled or disabled.
- (void)testTabPickerUI {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  OpenTabPicker();

  // Check that the tab picker is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxTabPickerCollectionViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the current tab cell is visible.
  NSString* pageTitle = [ChromeEarlGrey currentTabTitle];
  id<GREYMatcher> tabMatcher = TabWithTitle(pageTitle);
  [[EarlGrey selectElementWithMatcher:tabMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Done button is disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];

  // Tap the tab to select it.
  [[EarlGrey selectElementWithMatcher:tabMatcher] performAction:grey_tap()];

  // Check that the Done button is enabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      assertWithMatcher:grey_enabled()];

  // Ensure the tab picker title is updated correctly.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetPluralNSStringF(
                                   IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE, 1))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the tab again to deselect it.
  [[EarlGrey selectElementWithMatcher:tabMatcher] performAction:grey_tap()];

  // Check that the Done button is disabled again and the tab picker's title is
  // back to its initial state.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the empty state view is displayed when only NTPs are available
// (User should not be able to attach NTPs to the composebox). It also ensure
// that the user can dismiss the view.
- (void)testTabPickerEmptyStateView {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];

  OpenTabPicker();

  // Check that the empty state view is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxTabPickerEmptyStateViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the Done button is disabled and Cancel is enabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Check that the tab picker is not visible anymore.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxTabPickerCollectionViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that multiple tabs selected from the tab picker are displayed in the
// carousel, the attachment limit is respected, and the AIM button is visible.
- (void)testAttachMultipleTabsAndLimit {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  std::vector<GURL> URLS;
  NSUInteger totalNumberOfTabs = kAttachmentLimit + 1;
  [ChromeEarlGrey closeAllNormalTabs];
  for (NSUInteger i = 0; i < totalNumberOfTabs; ++i) {
    URLS.push_back(
        self.testServer->GetURL(base::StringPrintf(kPageURL, i + 1)));
    [ChromeEarlGrey openNewTab];
    [ChromeEarlGrey loadURL:URLS[i]];
    [ChromeEarlGrey
        waitForWebStateContainingText:base::StringPrintf(kPageContent, i + 1)];
    // Ensure title is also updated.
    ConditionBlock titleCondition = ^{
      return [[ChromeEarlGrey currentTabTitle]
          containsString:base::SysUTF8ToNSString(
                             base::StringPrintf(kPageTitle, i + 1))];
    };
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                   base::test::ios::kWaitForPageLoadTimeout, titleCondition),
               @"Page title failed to update.");
  }
  [ChromeEarlGrey waitForMainTabCount:totalNumberOfTabs];
  OpenTabPicker();

  // Ensure the current selected tab is visible as the tab picker requires to
  // scroll to the bottom to see the current tab.
  NSString* currentPageTitle = base::SysUTF8ToNSString(
      base::StringPrintf(kPageTitle, totalNumberOfTabs));
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:TabWithTitle(currentPageTitle)]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  if (error) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                kComposeboxTabPickerCollectionViewAccessibilityIdentifier)]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
    [[EarlGrey selectElementWithMatcher:TabWithTitle(currentPageTitle)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Select all tabs up to the limit (aka from tab 11 to tab 2 included).
  for (NSUInteger i = totalNumberOfTabs; i > 1; --i) {
    NSString* pageTitle =
        base::SysUTF8ToNSString(base::StringPrintf(kPageTitle, i));
    SelectTabWithTitle(pageTitle);
    [[EarlGrey
        selectElementWithMatcher:grey_text(l10n_util::GetPluralNSStringF(
                                     IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE,
                                     totalNumberOfTabs - i + 1))]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Attempt to select one more tab than the limit and verify the error snackbar
  // is displayed.
  NSString* firstPageTitle =
      base::SysUTF8ToNSString(base::StringPrintf(kPageTitle, 1));
  SelectTabWithTitle(firstPageTitle);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetPluralNSStringF(
                     IDS_IOS_COMPOSEBOX_MAXIMUM_ATTACHMENTS_REACHED,
                     kAttachmentLimit))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that kAttachmentLimit tabs are selected.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetPluralNSStringF(
                                          IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE,
                                          kAttachmentLimit))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Attach the selected tabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Verify the AIM button is visible in the composebox.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxAIMButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check all attachments.
  for (NSUInteger i = totalNumberOfTabs; i > 1; --i) {
    NSString* pageTitle =
        base::SysUTF8ToNSString(base::StringPrintf(kPageTitle, i));
    VerifyTabIsAttachedWithTitle(pageTitle);
  }

  // Ensure there is not another item attached.
  [[EarlGrey selectElementWithMatcher:TabCellAttachmentMatcherWithTitle(
                                          firstPageTitle)]
      assertWithMatcher:grey_nil()];
}

// Tests that a tab cannot be attached when in image generation mode, and that
// image generation mode can be entered after attachments are removed.
- (void)testNoTabAttachmentsInImageGeneration {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ComposeboxAppInterface setCreateImagesEligible:YES];

  // Add a tab and attach it.
  [ChromeEarlGrey closeAllNormalTabs];
  GURL URL = self.testServer->GetURL(base::StringPrintf(kPageURL, 1));
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey
      waitForWebStateContainingText:base::StringPrintf(kPageContent, 1)];
  OpenTabPicker();
  NSString* pageTitle =
      base::SysUTF8ToNSString(base::StringPrintf(kPageTitle, 1));
  SelectTabWithTitle(pageTitle);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  VerifyTabIsAttachedWithTitle(pageTitle);

  // Display the plus menu to verify that the image generation action is
  // disabled. This mode only supports a single image attachment, making it
  // incompatible with the currently attached tab.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxImageGenerationActionAccessibilityIdentifier)]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];

  // Tap somewhere to dismiss the plus button menu.
  [[EarlGrey selectElementWithMatcher:ComposeboxMatcher()]
      performAction:grey_tap()];

  // Remove the attachment and switch to image generation mode.
  RemoveAttachmentWithTitle(pageTitle);
  [[EarlGrey
      selectElementWithMatcher:TabCellAttachmentMatcherWithTitle(pageTitle)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxImageGenerationActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify that the image generation button is visible in the composebox.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              kComposeboxImageGenerationButtonAccessibilityIdentifier)];

  // Verify that the "Select tabs" action is disabled and the "Create Image"
  // action is selected.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kComposeboxSelectTabsActionAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kComposeboxImageGenerationActionAccessibilityIdentifier),
              grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitSelected),
                                   nil)];
}

@end

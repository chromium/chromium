// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "components/dom_distiller/core/dom_distiller_features.h"
#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#import "components/dom_distiller/core/pref_names.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "testing/gmock/include/gmock/gmock-matchers.h"

using testing::HasSubstr;

namespace {
// Verifies that the theme and font have been set as expected in the document
// body.
void ExpectBodyHasThemeAndFont(const std::string& theme,
                               const std::string& font) {
  NSString* js = @"(function() { return document.body.className; })();";
  base::Value result = [ChromeEarlGrey evaluateJavaScript:js];
  ASSERT_TRUE(result.is_string());

  const std::string resultStr = result.GetString();
  EXPECT_THAT(resultStr, HasSubstr(theme));
  EXPECT_THAT(resultStr, HasSubstr(font));
}

// Returns a matcher for a visible context menu item with the given
// `message_id`.
id<GREYMatcher> VisibleContextMenuItem(int message_id) {
  return grey_allOf(
      chrome_test_util::ContextMenuItemWithAccessibilityLabelId(message_id),
      grey_sufficientlyVisible(), nil);
}
}  // namespace

// Tests interactions with Reader Mode on a web page.
@interface ReaderModeTestCase : ChromeTestCase
@end

@implementation ReaderModeTestCase

- (void)setUp {
  [super setUp];
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kEnableReaderMode);

  if ([self isRunningTest:@selector
            (testNotEligibleReaderModePageEnabledInToolsMenu)]) {
    config.features_disabled.push_back(
        kEnableReaderModePageEligibilityForToolsMenu);
  } else {
    config.features_enabled.push_back(
        kEnableReaderModePageEligibilityForToolsMenu);
  }
  if ([self isRunningTest:@selector(testReadabilityEnabled)]) {
    config.features_enabled.push_back(dom_distiller::kReaderModeUseReadability);
  }
  return config;
}

#pragma mark - Helpers

// Asserts that the matcher has the expected content in the Tools Menu.
- (void)assertReaderModeInToolsMenuWithMatcher:(id<GREYMatcher>)matcher {
  [ChromeEarlGreyUI openToolsMenu];

  id<GREYMatcher> tableViewMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
  id<GREYMatcher> readerModeButtonMatcher =
      grey_allOf(grey_accessibilityID(kToolsMenuReaderMode),
                 grey_accessibilityTrait(UIAccessibilityTraitButton),
                 grey_sufficientlyVisible(), nil);

  [[[EarlGrey selectElementWithMatcher:readerModeButtonMatcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:tableViewMatcher] assertWithMatcher:matcher];

  [ChromeEarlGreyUI closeToolsMenu];
}

#pragma mark - Tests

// Tests that the user can show / hide Reader Mode from the tools menu
// entrypoint on an eligible web page.
- (void)testToggleReaderModeInToolsMenuForDistillablePage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Close Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  // The Reader Mode UI is not visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
}

// Test that a page that is not eligible for Reader Mode shows as a disabled
// option in the Tools menu.
- (void)testNotEligibleReaderModePageDisabledInToolsMenu {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  [self assertReaderModeInToolsMenuWithMatcher:
            grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)];
}

// Test that a page that is not eligible for Reader Mode shows as an enabled
// option in the Tools menu when there is no page eligibility criteria.
- (void)testNotEligibleReaderModePageEnabledInToolsMenu {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  [self assertReaderModeInToolsMenuWithMatcher:
            grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))];
}

// Tests that swiping between a Reader Mode web state and a normal web
// state shows the expected view.
- (void)testSideSwipeReaderMode {
  const GURL readerModeURL = self.testServer->GetURL("/article.html");
  [ChromeEarlGrey loadURL:readerModeURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Open a new Tab with an article to have a tab to switch to.
  [ChromeEarlGreyUI openNewTab];
  const GURL nonReaderModeURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:nonReaderModeURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Side swipe on the toolbar.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"PrimaryToolbarView")]
      performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];

  // Reader Mode view is visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];
  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(readerModeURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to Reader Mode url.");

  // Side swipe back to the non-Reader mode page on the toolbar.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"PrimaryToolbarView")]
      performAction:grey_swipeSlowInDirection(kGREYDirectionLeft)];

  // Non-Reader Mode view is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(nonReaderModeURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to non-Reader Mode url.");
}

// Tests that the user can show Reader Mode from the contextual panel entrypoint
// on an eligible web page.
- (void)testToggleReaderModeInContextualPanelEntrypointForDistillablePage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(@"ContextualPanelEntrypointImageViewAXID")];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"ContextualPanelEntrypointImageViewAXID")]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];
}

// Tests that theme change is applied to the Reading Mode web page.
- (void)testUpdateReaderModeTheme {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> tableViewMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuReaderMode),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:tableViewMatcher] performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  ExpectBodyHasThemeAndFont("light", "sans-serif");

  [ChromeEarlGrey setIntegerValue:(int)dom_distiller::mojom::Theme::kDark
                      forUserPref:dom_distiller::prefs::kTheme];

  ExpectBodyHasThemeAndFont("dark", "sans-serif");
}

// Tests that font change is applied to the Reading Mode web page.
- (void)testUpdateReaderModeFont {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> tableViewMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          ? grey_accessibilityID(kPopupMenuToolsMenuActionListId)
          : grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuReaderMode),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:tableViewMatcher] performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  ExpectBodyHasThemeAndFont("light", "sans-serif");

  [ChromeEarlGrey
      setIntegerValue:(int)dom_distiller::mojom::FontFamily::kMonospace
          forUserPref:dom_distiller::prefs::kFont];

  ExpectBodyHasThemeAndFont("light", "monospace");
}

// Tests that tapping the reader mode chip shows the Reader mode options view.
- (void)testTapReaderModeChipShowsOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Tap the chip.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The options view should be visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];
}

// Tests that font family can be changed from the options view.
- (void)testChangeReaderModeFontFamilyFromOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Tap the chip to open the options view.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The options view should be visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];

  ExpectBodyHasThemeAndFont("light", "sans-serif");

  // Change the font family to Serif.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kReaderModeOptionsFontFamilyButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 VisibleContextMenuItem(
                     IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SERIF_LABEL)]
      performAction:grey_tap()];
  GREYAssertEqual([ChromeEarlGrey userIntegerPref:dom_distiller::prefs::kFont],
                  static_cast<int>(dom_distiller::mojom::FontFamily::kSerif),
                  @"Pref should be updated to Serif");
  ExpectBodyHasThemeAndFont("light", "serif");

  // Change the font family to Monospace.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kReaderModeOptionsFontFamilyButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 VisibleContextMenuItem(
                     IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_MONOSPACE_LABEL)]
      performAction:grey_tap()];
  GREYAssertEqual(
      [ChromeEarlGrey userIntegerPref:dom_distiller::prefs::kFont],
      static_cast<int>(dom_distiller::mojom::FontFamily::kMonospace),
      @"Pref should be updated to Monospace");
  ExpectBodyHasThemeAndFont("light", "monospace");

  // Change the font family back to Sans-Serif.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kReaderModeOptionsFontFamilyButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 VisibleContextMenuItem(
                     IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SANS_SERIF_LABEL)]
      performAction:grey_tap()];
  GREYAssertEqual(
      [ChromeEarlGrey userIntegerPref:dom_distiller::prefs::kFont],
      static_cast<int>(dom_distiller::mojom::FontFamily::kSansSerif),
      @"Pref should be updated to Sans-Serif");
  ExpectBodyHasThemeAndFont("light", "sans-serif");
}

// Tests that Reading Mode UI continues to be functional when changing the
// underlying distillation architecture.
- (void)testReadabilityEnabled {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Close Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  // The Reader Mode UI is not visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
}

@end

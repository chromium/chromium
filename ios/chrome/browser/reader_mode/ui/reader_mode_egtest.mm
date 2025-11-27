// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/stringprintf.h"
#import "components/dom_distiller/core/dom_distiller_features.h"
#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#import "components/dom_distiller/core/pref_names.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/test/reader_mode_app_interface.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "testing/gmock/include/gmock/gmock-matchers.h"
#import "ui/base/l10n/l10n_util.h"

using testing::HasSubstr;

namespace {

// Base font size for Reader Mode, in pixels. This is the font size that is
// multiplied by the font scale multipliers. This value is defined in
// components/dom_distiller/core/javascript/dom_distiller_viewer.js.
constexpr double kReaderModeBaseFontSize = 16.0;

// Return the number of links on the page.
double CountNumLinks() {
  NSString* js =
      @"(function() { return document.querySelectorAll('a').length; })();";
  base::Value result = [ChromeEarlGrey evaluateJavaScript:js];
  GREYAssertTrue(result.is_double(),
                 @"The javascript result should be a double");

  return result.GetDouble();
}

// Verifies that the theme and font have been set as expected in the document
// body.
void ExpectBodyHasThemeAndFont(const std::string& theme,
                               const std::string& font) {
  NSString* js = @"(function() { return document.body.className; })();";
  base::Value result = [ChromeEarlGrey evaluateJavaScript:js];
  GREYAssertTrue(result.is_string(), @"The class name should be a string");

  const std::string resultStr = result.GetString();
  GREYAssertTrue(resultStr.find(theme) != std::string::npos,
                 @"Expected theme '%s' not found in className '%s'",
                 theme.c_str(), resultStr.c_str());
  GREYAssertTrue(resultStr.find(font) != std::string::npos,
                 @"Expected font '%s' not found in className '%s'",
                 font.c_str(), resultStr.c_str());
}

// Verifies that the font size has been set as expected on the document element.
void ExpectFontSize(double expected_size) {
  NSString* js =
      @"(function() { return document.documentElement.style.fontSize; })();";
  base::Value result = [ChromeEarlGrey evaluateJavaScript:js];
  GREYAssertTrue(result.is_string(), @"JS result should be a string");
  std::string expected_font_size = base::StringPrintf("%gpx", expected_size);
  GREYAssertEqual(result.GetString(), expected_font_size,
                  @"Font size should be %s, but it is %s",
                  expected_font_size.c_str(), result.GetString().c_str());
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
@property(nonatomic, strong) FakeSystemIdentity* fakeIdentity;
@end

@implementation ReaderModeTestCase

@synthesize fakeIdentity = _fakeIdentity;

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:translate::prefs::kOfferTranslateEnabled];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kIOSBwgConsent];

  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);

  self.fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:self.fakeIdentity
                 withCapabilities:@{
                   @(kCanUseModelExecutionFeaturesName) : @YES,
                 }];
  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
}

- (void)tearDownHelper {
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [ChromeEarlGrey
      clearUserPrefWithName:translate::prefs::kOfferTranslateEnabled];
  [ChromeEarlGrey clearUserPrefWithName:prefs::kIOSBwgConsent];

  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector
            (testNotEligibleReaderModePageEnabledInToolsMenu)]) {
    config.features_disabled.push_back(
        kEnableReaderModePageEligibilityForToolsMenu);
  } else {
    config.features_enabled_and_params.push_back(
        {kEnableReaderModePageEligibilityForToolsMenu, {}});
  }
  if ([self isRunningTest:@selector(testReadabilityEnabled)]) {
    config.features_enabled_and_params.push_back(
        {dom_distiller::kReaderModeUseReadability, {}});
  }
  if ([self isRunningTest:@selector(testReaderModeDistillationTimeout)]) {
    config.features_enabled_and_params.push_back(
        {kEnableReaderMode,
         {{{kReaderModeDistillationTimeoutDurationStringName, "0s"}}}});
    config.features_enabled_and_params.push_back({kEnableReaderModeInUS, {}});
  } else {
    config.features_enabled_and_params.push_back({kEnableReaderMode, {}});
    config.features_enabled_and_params.push_back({kEnableReaderModeInUS, {}});
  }
  if ([self isRunningTest:@selector(testTurnOnReaderModeViaPageActionMenu)] ||
      [self isRunningTest:@selector(testReaderModeChipShowsAIHubIfAvailable)]) {
    config.features_enabled_and_params.push_back({kPageActionMenu, {}});
    config.features_enabled_and_params.push_back(
        {kLensOverlayEnableIPadCompatibility, {}});
  } else {
    config.features_disabled.push_back(kPageActionMenu);
  }
  if ([self isRunningTest:@selector(testOmniboxEntryPointDisabled)]) {
    config.features_disabled.push_back(kEnableReaderModeOmniboxEntryPoint);
  } else {
    config.features_enabled_and_params.push_back(
        {kEnableReaderModeOmniboxEntryPoint, {}});
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
// TODO(crbug.com/438763264): Failing on device and flaky on simulator.
- (void)DISABLED_testToggleReaderModeInToolsMenuForDistillablePage {
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

// Tests that the user can show / hide Reader Mode from the tools menu
// entrypoint on an eligible web page in Incognito Mode.
// TODO(crbug.com/438763264): Failing on iPhone device, and flaky otherwise.
- (void)
    DISABLED_testToggleReaderModeInToolsMenuForDistillablePageInIncognitoMode {
  // Open a web page in Incognito.
  [ChromeEarlGrey openNewIncognitoTab];
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
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kBadgeButtonIncognitoAccessibilityIdentifier)];

  // Close Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  // The Reader Mode UI is not visible, but Incognito badge continues to be
  // visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kBadgeButtonIncognitoAccessibilityIdentifier)];
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
  // TODO(crbug.com/460745280): Test is flaky.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Side swipe transitions are flaky on iPad.");
  }
  const GURL readerModeURL = self.testServer->GetURL("/article.html");
  [ChromeEarlGrey loadURL:readerModeURL];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
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
  // TODO(crbug.com/438763264): Failing on iPad device.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad device");
  }
#endif
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // TODO(crbug.com/457880049): Clean up when feature is enabled by default.
  NSString* imageViewIdentifier =
      [ChromeEarlGrey isAskGeminiChipEnabled]
          ? kLocationBarBadgeImageViewIdentifier
          : @"ContextualPanelEntrypointImageViewAXID";
  // Open Reader Mode UI.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityID(
                                                       imageViewIdentifier)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(imageViewIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Check that the chip is a button with the expected accessibility label.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                            IDS_IOS_READER_MODE_CHIP_ACCESSIBILITY_LABEL)];
}

// Tests that theme change is applied to the Reading Mode web page.
- (void)testUpdateReaderModeTheme {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  ExpectBodyHasThemeAndFont("light", "sans-serif");

  [ChromeEarlGrey setIntegerValue:(int)dom_distiller::mojom::Theme::kDark
                      forUserPref:dom_distiller::prefs::kTheme];

  ExpectBodyHasThemeAndFont("dark", "sans-serif");
}

// Tests that font change is applied to the Reading Mode web page.
// TODO(crbug.com/438265943): Re-enable.
- (void)DISABLED_testUpdateReaderModeFont {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [ChromeEarlGrey waitForPageToFinishLoading];

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
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SANS_SERIF_LABEL)]
      assertWithMatcher:grey_notNil()];

  // Change the font family to Monospace.
  [ChromeEarlGrey
      setIntegerValue:(int)dom_distiller::mojom::FontFamily::kMonospace
          forUserPref:dom_distiller::prefs::kFont];
  ExpectBodyHasThemeAndFont("light", "monospace");
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_MONOSPACE_LABEL)]
      assertWithMatcher:grey_notNil()];

  // Change the font family to Serif.
  [ChromeEarlGrey setIntegerValue:(int)dom_distiller::mojom::FontFamily::kSerif
                      forUserPref:dom_distiller::prefs::kFont];
  ExpectBodyHasThemeAndFont("light", "serif");
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_READER_MODE_OPTIONS_FONT_FAMILY_SERIF_LABEL)]
      assertWithMatcher:grey_notNil()];
}

// Tests that tapping the reader mode chip shows the Reader mode options view.
- (void)testTapReaderModeChipShowsOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

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
  // TODO(crbug.com/438763264): Failing on iPad device.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad device");
  }
#endif
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

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
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Close Reader Mode UI.
  [ChromeEarlGrey hideReaderMode];

  // The Reader Mode UI is not visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
}

// Tests that font size can be changed from the options view.
- (void)testChangeReaderModeFontSizeFromOptionsView {
  // TODO(crbug.com/438763264): Failing on iPad device.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad device");
  }
#endif
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  [ChromeEarlGrey setDoubleValue:multipliers[0]
                     forUserPref:dom_distiller::prefs::kFontScale];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Tap the chip to open the options view.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The options view should be visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];

  // Decrease button should be disabled at the minimum font size.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kReaderModeOptionsDecreaseFontSizeButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_not(grey_enabled())];

  // Increase the font size.
  for (int i = 1; i < static_cast<int>(multipliers.size()); ++i) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                kReaderModeOptionsIncreaseFontSizeButtonAccessibilityIdentifier)]
        performAction:grey_tap()];
    GREYAssertEqual(
        [ChromeEarlGrey userDoublePref:dom_distiller::prefs::kFontScale],
        multipliers[i], @"Pref should be updated to next multiplier");
    ExpectFontSize(multipliers[i] * kReaderModeBaseFontSize);
    // Decrease button should be enabled.
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                kReaderModeOptionsDecreaseFontSizeButtonAccessibilityIdentifier)]
        assertWithMatcher:grey_enabled()];
  }

  // Increase button should be disabled at the maximum font size.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kReaderModeOptionsIncreaseFontSizeButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_not(grey_enabled())];

  // Decrease the font size.
  for (int i = static_cast<int>(multipliers.size()) - 2; i >= 0; --i) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                kReaderModeOptionsDecreaseFontSizeButtonAccessibilityIdentifier)]
        performAction:grey_tap()];
    GREYAssertEqual(
        [ChromeEarlGrey userDoublePref:dom_distiller::prefs::kFontScale],
        multipliers[i], @"Pref should be updated to previous multiplier");
    ExpectFontSize(multipliers[i] * kReaderModeBaseFontSize);
    // Increase button should be enabled.
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                kReaderModeOptionsIncreaseFontSizeButtonAccessibilityIdentifier)]
        assertWithMatcher:grey_enabled()];
  }
}

// Tests that color theme can be changed from the options view.
- (void)testChangeReaderModeThemeFromOptionsView {
  // TODO(crbug.com/438763264): Failing on iPad device.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad device");
  }
#endif
  [ChromeEarlGrey setIntegerValue:(int)dom_distiller::mojom::Theme::kLight
                      forUserPref:dom_distiller::prefs::kTheme];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

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

  // Change the theme to dark.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kReaderModeOptionsDarkThemeButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  GREYAssertEqual([ChromeEarlGrey userIntegerPref:dom_distiller::prefs::kTheme],
                  static_cast<int>(dom_distiller::mojom::Theme::kDark),
                  @"Pref should be updated to dark");
  ExpectBodyHasThemeAndFont("dark", "sans-serif");

  // Change the theme to sepia.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kReaderModeOptionsSepiaThemeButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  GREYAssertEqual([ChromeEarlGrey userIntegerPref:dom_distiller::prefs::kTheme],
                  static_cast<int>(dom_distiller::mojom::Theme::kSepia),
                  @"Pref should be updated to sepia");
  ExpectBodyHasThemeAndFont("sepia", "sans-serif");

  // Change the theme to light.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kReaderModeOptionsLightThemeButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  GREYAssertEqual([ChromeEarlGrey userIntegerPref:dom_distiller::prefs::kTheme],
                  static_cast<int>(dom_distiller::mojom::Theme::kLight),
                  @"Pref should be updated to light");
  ExpectBodyHasThemeAndFont("light", "sans-serif");
}

// Tests that tapping the close button in the options view dismisses the view.
- (void)testTapCloseButtonInOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

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

  // Tap the close button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kReaderModeOptionsCloseButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The options view should be hidden.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];
}

// Tests that tapping the "Turn Off" button in the options view dismisses the
// view and deactivates Reader mode.
- (void)testTapTurnOffButtonInOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

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

  // Tap the hide button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kReaderModeOptionsTurnOffButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The options view should be hidden.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];

  // The Reader Mode UI is not visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
}

// Tests that tapping outside of the options view dismisses it.
- (void)testTapOutsideOptionsViewDismissesIt {
  // TODO(crbug.com/438763264): Failing on iPad device.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad device");
  }
#endif
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

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

  // Tap on the top left corner of the screen to dismiss the options view.
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tapAtPoint(CGPointMake(5, 5))];

  // The options view should be hidden.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];
}

// Test accessibility for Reader mode options screen.
- (void)testReaderModeOptionsAccessibility {
  // TODO(crbug.com/438763264): Failing on iPad device.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad device");
  }
#endif
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Tap the chip to open the options view.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the contextual panel entrypoint disappears and a failure snackbar
// is presented when distillation fails.
- (void)testReaderModeDistillationFailure {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Wait for the contextual panel entrypoint to appear.
  id<GREYMatcher> entrypoint = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:entrypoint];

  // Make the page not distillable.
  [ChromeEarlGrey
      evaluateJavaScriptForSideEffect:@"document.body.outerHTML = ''"];

  // Tap the entrypoint to trigger distillation.
  [[EarlGrey selectElementWithMatcher:entrypoint] performAction:grey_tap()];

  // The entrypoint should disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:entrypoint];

  // A snackbar should be displayed with a failure message.
  NSString* failureMessage =
      l10n_util::GetNSString(IDS_IOS_READER_MODE_SNACKBAR_FAILURE_MESSAGE);
  id<GREYMatcher> snackbarMatcher =
      grey_allOf(chrome_test_util::SnackbarViewMatcher(),
                 grey_descendant(grey_accessibilityLabel(failureMessage)), nil);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:snackbarMatcher];
}

// Tests that the contextual panel entrypoint disappears and a failure snackbar
// is presented when distillation times out.
- (void)testReaderModeDistillationTimeout {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Wait for the contextual panel entrypoint to appear.
  id<GREYMatcher> entrypoint = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:entrypoint];

  // Tap the entrypoint to trigger distillation.
  [[EarlGrey selectElementWithMatcher:entrypoint] performAction:grey_tap()];

  // The entrypoint should disappear.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:entrypoint];

  // A snackbar should be displayed with a failure message.
  NSString* failureMessage =
      l10n_util::GetNSString(IDS_IOS_READER_MODE_SNACKBAR_FAILURE_MESSAGE);
  id<GREYMatcher> snackbarMatcher =
      grey_allOf(chrome_test_util::SnackbarViewMatcher(),
                 grey_descendant(grey_accessibilityLabel(failureMessage)), nil);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:snackbarMatcher];
}

// Tests that non-http links are removed from Reading mode.
- (void)testNonHttpsLinksRemovedFromReadingMode {
  // TODO(crbug.com/438763264): Failing on iPad device.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad device");
  }
#endif
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  EXPECT_EQ(4, CountNumLinks());

  // Wait for the contextual panel entrypoint to appear.
  id<GREYMatcher> entrypoint = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:entrypoint];

  // Tap the entrypoint to trigger distillation.
  [[EarlGrey selectElementWithMatcher:entrypoint] performAction:grey_tap()];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  EXPECT_EQ(1, CountNumLinks());
}

// Tests that the user can turn on Reader Mode from the page action menu.
- (void)testTurnOnReaderModeViaPageActionMenu {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Wait for the contextual chip to appear and then disappear.
  id<GREYMatcher> entrypoint = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:entrypoint];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:entrypoint
                                                 timeout:base::Seconds(10)];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAIHubEntrypointAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify the bottom sheet appears.
  id<GREYMatcher> bottomSheet =
      grey_accessibilityID(kAIHubBottomSheetAccessibilityIdentifier);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:bottomSheet];

  // Tap the "Reading mode" button.
  id<GREYMatcher> readingModeButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_AI_HUB_READER_MODE_LABEL);
  [[EarlGrey selectElementWithMatcher:readingModeButton]
      performAction:grey_tap()];

  // Verify bottom sheet disappears and Reader Mode UI is shown.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:bottomSheet];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];
}

// Tests that tapping the Reader mode chip shows the AI hub bottom sheet if AI
// hub is available.
- (void)testReaderModeChipShowsAIHubIfAvailable {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Tap the Reader mode chip.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify the bottom sheet appears.
  id<GREYMatcher> bottomSheet =
      grey_accessibilityID(kAIHubBottomSheetAccessibilityIdentifier);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:bottomSheet];

  // Tap the "Reading mode" button to go to the options view.
  id<GREYMatcher> readingModeOptionsButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_AI_HUB_READER_MODE_OPTIONS_BUTTON_TITLE);
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_ancestor(bottomSheet),
                                                 readingModeOptionsButton, nil)]
      performAction:grey_tap()];

  // The options view should be visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];

  // Tap the back button.
  [[EarlGrey selectElementWithMatcher:testing::NavigationBarBackButton()]
      performAction:grey_tap()];

  // The options view should be hidden.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];

  // Tap the hide button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_AI_HUB_HIDE_BUTTON_LABEL)]
      performAction:grey_tap()];

  // The Reader Mode UI is not visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
}

// Tests that the text zoom UI can be opened from reader mode and that it
// controls the reader mode font size.
- (void)testTextZoomInReaderMode {
  // This test is not relevant on iPads because the text zoom UI is unavailable
  // on iPad, even outside of Reader mode.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }

  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  [ChromeEarlGrey setDoubleValue:multipliers[2]
                     forUserPref:dom_distiller::prefs::kFontScale];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Open the tools menu and tap the text zoom button.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuTextZoom)];

  // The text zoom UI should be visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kTextZoomViewAccessibilityIdentifier)];

  // Increase the font size.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kTextZoomIncreaseButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  GREYAssertEqual(
      [ChromeEarlGrey userDoublePref:dom_distiller::prefs::kFontScale],
      multipliers[3], @"Pref should be updated to next multiplier");
  ExpectFontSize(multipliers[3] * kReaderModeBaseFontSize);

  // Decrease the font size.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kTextZoomDecreaseButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  GREYAssertEqual(
      [ChromeEarlGrey userDoublePref:dom_distiller::prefs::kFontScale],
      multipliers[2], @"Pref should be updated to previous multiplier");
  ExpectFontSize(multipliers[2] * kReaderModeBaseFontSize);

  // Reset the font size.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kTextZoomResetButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  GREYAssertEqual(
      [ChromeEarlGrey userDoublePref:dom_distiller::prefs::kFontScale], 1.0,
      @"Pref should be updated to 1.0");
  ExpectFontSize(1.0 * kReaderModeBaseFontSize);
}

// Tests that the contextual chip is visible in Incognito.
// TODO(crbug.com/438763264): Failing on device and flaky on simulator.
- (void)DISABLED_testContextualChipVisibleInIncognito {
  // Open a web page in Incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // TODO(crbug.com/457880049): Clean up when feature is enabled by default.
  NSString* imageViewIdentifier =
      [ChromeEarlGrey isAskGeminiChipEnabled]
          ? kLocationBarBadgeImageViewIdentifier
          : @"ContextualPanelEntrypointImageViewAXID";
  // Tap on the contextual panel entrypoint.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(imageViewIdentifier)]
      performAction:grey_tap()];

  // Reader mode and the incognito badge should be visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kBadgeButtonIncognitoAccessibilityIdentifier)];
}

// Tests that overscroll actions can be used to refresh dismisses Reader mode.
// TODO(crbug.com/446692216): Re-enable this test.
- (void)DISABLED_testOverscrollToRefresh {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Overscroll Actions are only on iPhone.");
  }
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Pull down to reload.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::OverscrollSwipe(kGREYDirectionDown)];

  // Wait for the page to reload.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // The Reader Mode UI is not visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
}

// Tests that Reader Mode can be toggled on and off for a URL with an empty
// fragment.
- (void)testToggleReaderModeWithEmptyRef {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html#")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Turn on Reader Mode.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  // Turn off Reader Mode.
  [ChromeEarlGrey hideReaderMode];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];

  // Turn on Reader Mode again.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];
}

// Tests that the killswitch to disable the omnibox entrypoint does not
// interfere with other Reading Mode entrypoints.
- (void)testOmniboxEntryPointDisabled {
  // TODO(crbug.com/445861550): Re-enable the test on device.
#if !TARGET_OS_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Test disabled on device.");
#endif
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that the omnibox entrypoint is disabled and the tools menu
  // entrypoint is still available.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kReaderModeChipViewAccessibilityIdentifier)]
      assertWithMatcher:grey_hidden(YES)];
  [self assertReaderModeInToolsMenuWithMatcher:
            grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))];
}

// Tests that the share menu is accessible via Reader Mode and records the
// expected metrics.
- (void)testShareMenuInReaderMode {
#if !TARGET_OS_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Test disabled on device.");
#endif
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  [ChromeEarlGreyUI openShareMenu];

  // Verify that the share menu is up and select the Copy action.
  [ChromeEarlGrey verifyActivitySheetVisible];
  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];
  [ChromeEarlGrey verifyActivitySheetNotVisible];

  // Ensure that UMA was logged correctly.
  NSError* error =
      [MetricsAppInterface expectCount:1
                             forBucket:14  // Number refering to
                                           // SharingScenario::ShareInReaderMode
                          forHistogram:@"Mobile.Share.EntryPoints"];
  if (error) {
    GREYFail([error description]);
  }

  error = [MetricsAppInterface
       expectCount:1
         forBucket:3  // Number refering to ShareActionType::Copy
      forHistogram:@"Mobile.Share.ShareInReaderMode.Actions"];
  if (error) {
    GREYFail([error description]);
  }
}

@end

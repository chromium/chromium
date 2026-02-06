// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/dom_distiller/core/dom_distiller_features.h"
#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#import "components/dom_distiller/core/pref_names.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_app_interface.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_constants.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/test/reader_mode_app_interface.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
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

// Returns the badge used to open Reader Mode customization UI.
id<GREYMatcher> ReaderModeCustomizationBadge() {
  NSString* const badgeIdentifier =
      [ChromeEarlGrey isProactiveSuggestionsFrameworkEnabled]
          ? kBadgeButtonReaderModeAccessibilityIdentifier
          : kReaderModeChipViewAccessibilityIdentifier;
  return grey_allOf(grey_accessibilityID(badgeIdentifier), grey_interactable(),
                    nil);
}

// Base font size for Reader Mode, in pixels. This is the font size that is
// multiplied by the font scale multipliers. This value is defined in
// components/dom_distiller/core/javascript/font_size_slider.js.
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

// Returns the Contextual Panel's entrypoint view GREY matcher.
id<GREYMatcher> ContextualPanelEntrypointImageViewMatcher() {
  // TODO(crbug.com/457880049): Clean up when feature is enabled by default.
  if ([ChromeEarlGrey isAskGeminiChipEnabled] ||
      [ChromeEarlGrey isProactiveSuggestionsFrameworkEnabled]) {
    return grey_allOf(
        grey_accessibilityID(kLocationBarBadgeImageViewIdentifier),
        grey_interactable(), nil);
  }
  return grey_allOf(
      grey_accessibilityID(@"ContextualPanelEntrypointImageViewAXID"),
      grey_interactable(), nil);
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

  // The user should be signed out at the beginning of Reading Mode tests.
  [SigninEarlGrey verifySignedOut];

  self.fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:self.fakeIdentity
                 withCapabilities:@{
                   @(kCanUseModelExecutionFeaturesName) : @YES,
                 }];
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

  config.iph_feature_enabled =
      feature_engagement::kIPHiOSReaderModeLargeOmniboxEntrypointFeature.name;
  config.features_enabled_and_params.push_back({kEnableReaderModeInUS, {}});

  if ([self isRunningTest:@selector(testTurnOnReaderModeViaPageActionMenu)] ||
      [self isRunningTest:@selector(testReaderModeChipShowsAIHubIfAvailable)] ||
#if TARGET_OS_SIMULATOR
      [self isRunningTest:@selector
            (testSampleContextualChipVisibleInReaderMode)] ||
#else
      [self isRunningTest:@selector
            (FLAKY_testSampleContextualChipVisibleInReaderMode)] ||
#endif
      [self isRunningTest:@selector(testReaderModeChipHiddenInReaderMode)]) {
    config.features_enabled_and_params.push_back({kPageActionMenu, {}});
    config.features_enabled_and_params.push_back(
        {kProactiveSuggestionsFramework, {}});
  } else {
    // Force an app restart before any tests that require the Gemini kill
    // switch. This is required to ensure that
    // BWGServiceFactory::BuildBwgService is re-evaluated for the new flag
    // configuration, otherwise a cached BWGService instance may be used.
    config.relaunch_policy = ForceRelaunchByCleanShutdown;
    config.features_disabled.push_back(kPageActionMenu);
    config.features_enabled_and_params.push_back({kGeminiKillSwitch, {}});
  }
  if ([self isRunningTest:@selector(testOmniboxEntryPointDisabled)]) {
    config.features_disabled.push_back(kEnableReaderModeOmniboxEntryPointInUS);
  } else {
    config.features_enabled_and_params.push_back(
        {kEnableReaderModeOmniboxEntryPointInUS, {}});
  }

  if ([self isRunningTest:@selector(testReaderModeContentSettingsOldToggle)]) {
    config.features_disabled.push_back(kEnableContentSettingsOptionForLinks);
  }
  if ([self isRunningTest:@selector(testReaderModeContentSettingsNewOptions)]) {
    config.features_enabled_and_params.push_back(
        {kEnableContentSettingsOptionForLinks, {}});
  }

  if ([self isRunningTest:@selector(testReaderModeDistillationTimeout)]) {
    config.additional_args.push_back(
        "--" + std::string(switches::kForceReaderModeDistillationTimeout));
  }
  if ([self isRunningTest:@selector(testReaderModeChipHiddenInReaderMode)] ||
#if TARGET_OS_SIMULATOR
      [self
          isRunningTest:@selector(testSampleContextualChipVisibleInReaderMode)]
#else
      [self isRunningTest:@selector
            (FLAKY_testSampleContextualChipVisibleInReaderMode)]
#endif
  ) {
    config.features_enabled_and_params.push_back(
        {kProactiveSuggestionsFramework, {}});
    config.features_enabled_and_params.push_back({kAskGeminiChip, {}});
  }
#if TARGET_OS_SIMULATOR
  if ([self isRunningTest:@selector
            (testReaderModeChipVisibleWhenLeavingReaderModeWithPSFDisabled)]) {
#else
  if ([self
          isRunningTest:@selector
          (FLAKY_testReaderModeChipVisibleWhenLeavingReaderModeWithPSFDisabled)]) {
#endif
    config.features_disabled.push_back(kProactiveSuggestionsFramework);
    config.features_disabled.push_back(kAskGeminiChip);
  }
#if TARGET_OS_SIMULATOR
  if ([self isRunningTest:@selector
            (testSampleContextualChipVisibleInReaderMode)]) {
#else
  if ([self isRunningTest:@selector
            (FLAKY_testSampleContextualChipVisibleInReaderMode)]) {
#endif
    config.features_enabled_and_params.push_back(
        {kContextualPanelForceShowEntrypoint, {}});
  }
  return config;
}

#pragma mark - Helpers

// Opens Reader mode via the contextual panel badge entrypoint UI.
- (void)openReaderModeWithBadgeEntrypoint {
  if ([ChromeEarlGrey isProactiveSuggestionsFrameworkEnabled]) {
    // Tap the AI hub entrypoint.
    id<GREYMatcher> entrypointMatcher = grey_allOf(
        grey_accessibilityID(kAIHubEntrypointAccessibilityIdentifier),
        grey_sufficientlyVisible(), nil);

    [[EarlGrey selectElementWithMatcher:entrypointMatcher]
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

    // Verify bottom sheet disappears.
    [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:bottomSheet];
  } else {
    // Wait for the contextual panel entrypoint to appear and tap it.
    id<GREYMatcher> entrypoint =
        chrome_test_util::ButtonWithAccessibilityLabelId(
            IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:entrypoint];
    [[EarlGrey selectElementWithMatcher:entrypoint] performAction:grey_tap()];
  }
}

// Loads the URL with optimization guide hints for Reader Mode eligibility
// set to true.
- (void)loadURLWithOptimizationGuideHints:(const GURL&)URL {
  optimization_guide::proto::Any any_metadata;
  NSData* metadata_data =
      [NSData dataWithBytes:any_metadata.SerializeAsString().c_str()
                     length:any_metadata.ByteSizeLong()];

  [OptimizationGuideTestAppInterface
      addHintForTesting:base::SysUTF8ToNSString(URL.spec())
                   type:optimization_guide::proto::READER_MODE_ELIGIBLE
         serialized_any:metadata_data
               type_url:@"type.googleapis.com/"
                        @"optimization_guide.proto.Any"];

  [ChromeEarlGrey loadURL:URL];
}

// Asserts that the matcher has the expected content in the Tools Menu.
- (void)assertReaderModeInToolsMenuWithMatcher:(id<GREYMatcher>)matcher {
  [ChromeEarlGreyUI openToolsMenu];

  id<GREYMatcher> tableViewMatcher =
      grey_accessibilityID(kPopupMenuToolsMenuActionListId);
  id<GREYMatcher> readerModeButtonMatcher =
      grey_allOf(grey_accessibilityID(kToolsMenuReaderMode),
                 grey_accessibilityTrait(UIAccessibilityTraitButton),
                 grey_sufficientlyVisible(), nil);

  [[[EarlGrey selectElementWithMatcher:readerModeButtonMatcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:tableViewMatcher] assertWithMatcher:matcher];

  [ChromeEarlGreyUI closeToolsMenu];
}

// Asserts that the Reader Mode UI attributes, the distilled page and the
// reading icon, are visible.
- (void)assertReaderModePageIsVisible {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:ReaderModeCustomizationBadge()];
}

// Asserts that the Reader Mode UI attributes, the distilled page and the
// reading icon, are not visible.
- (void)assertReaderModePageIsHidden {
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
  [[EarlGrey selectElementWithMatcher:ReaderModeCustomizationBadge()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Open the customization options UI via the Reader Mode badge.
- (void)openReaderModeCustomizationOptions {
  if ([ChromeEarlGrey isProactiveSuggestionsFrameworkEnabled]) {
    // Tap the Reader Mode chip.
    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(grey_accessibilityID(
                           kBadgeButtonReaderModeAccessibilityIdentifier),
                       grey_interactable(), nil)] performAction:grey_tap()];

    // Verify the bottom sheet appears.
    id<GREYMatcher> bottomSheet =
        grey_accessibilityID(kAIHubBottomSheetAccessibilityIdentifier);
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:bottomSheet];

    // Tap the "Reading mode" button to go to the options view.
    id<GREYMatcher> readingModeOptionsButton =
        chrome_test_util::ButtonWithAccessibilityLabelId(
            IDS_IOS_AI_HUB_READER_MODE_OPTIONS_BUTTON_TITLE);
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_ancestor(bottomSheet),
                                            readingModeOptionsButton, nil)]
        performAction:grey_tap()];
  } else {
    // Tap the Reader Mode chip.
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)]
        performAction:grey_tap()];
  }

  // The options view should be visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];
}

#pragma mark - Tests

// Tests that Reader Mode is shown / hidden for a distillable page.
- (void)testToggleReaderModeForDistillablePage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Close Reader Mode UI.
  [ChromeEarlGrey hideReaderMode];
  [self assertReaderModePageIsHidden];
}

// Tests that Reader Mode is shown / hidden for a distillable page in
// incognito mode.
- (void)testToggleReaderModeForDistillablePageInIncognitoMode {
  // Open a web page in Incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kBadgeButtonIncognitoAccessibilityIdentifier)];

  // Close Reader Mode UI.
  [ChromeEarlGrey hideReaderMode];
  [self assertReaderModePageIsHidden];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kBadgeButtonIncognitoAccessibilityIdentifier)];
}

// Test that a page that is not eligible for Reader Mode shows as an enabled
// option in the Tools menu.
- (void)testNotEligibleReaderModePageEnabledInToolsMenu {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];

  [self assertReaderModeInToolsMenuWithMatcher:
            grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))];
}

// Tests that swiping between a Reader Mode web state and a normal web
// state shows the expected view.
- (void)testSideSwipeReaderMode {
  const GURL readerModeURL = self.testServer->GetURL("/article.html");
  [ChromeEarlGrey loadURL:readerModeURL];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Open a new Tab with an article to have a tab to switch to.
  [ChromeEarlGreyUI openNewTab];
  const GURL nonReaderModeURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:nonReaderModeURL];

  // Side swipe on the toolbar.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"PrimaryToolbarView")]
      performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];

  // Reader Mode view is visible.
  [self assertReaderModePageIsVisible];

  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(readerModeURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to Reader Mode url.");

  // Side swipe back to the non-Reader mode page on the toolbar.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"PrimaryToolbarView")]
      performAction:grey_swipeSlowInDirection(kGREYDirectionLeft)];

  // Non-Reader Mode view is visible.
  [self assertReaderModePageIsHidden];

  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(nonReaderModeURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to non-Reader Mode url.");
}

// Tests that the user can show Reader Mode from the contextual panel entrypoint
// on an eligible web page.
- (void)testToggleReaderModeInContextualPanelEntrypointForDistillablePage {
  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];

  // Open Reader Mode UI.
  [self openReaderModeWithBadgeEntrypoint];
  [self assertReaderModePageIsVisible];

  // Check that the chip is a button with the expected accessibility label.
  [[EarlGrey selectElementWithMatcher:ReaderModeCustomizationBadge()]
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

// Tests that unselecting the theme in Reading Mode options will apply the
// default theme on the Reading Mode web page.
- (void)testUnselectReaderModeTheme {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Set the user-selected preference to Dark theme.
  [ChromeEarlGrey setIntegerValue:(int)dom_distiller::mojom::Theme::kDark
                      forUserPref:dom_distiller::prefs::kTheme];

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];

  ExpectBodyHasThemeAndFont("dark", "sans-serif");

  // Tap the Dark theme button.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ButtonWithAccessibilityLabelId(
              IDS_IOS_READER_MODE_OPTIONS_COLOR_THEME_BUTTON_ACCESSIBILITY_LABEL_DARK)]
      performAction:grey_tap()];

  // Expect the theme to return to the default theme preference and the button
  // to be unselected.
  ExpectBodyHasThemeAndFont("light", "sans-serif");
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithAccessibilityLabelId(
                  IDS_IOS_READER_MODE_OPTIONS_COLOR_THEME_BUTTON_ACCESSIBILITY_LABEL_DARK),
              grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected)),
              nil)] assertWithMatcher:grey_notNil()];

  // Tap the Light theme button.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ButtonWithAccessibilityLabelId(
              IDS_IOS_READER_MODE_OPTIONS_COLOR_THEME_BUTTON_ACCESSIBILITY_LABEL_LIGHT)]
      performAction:grey_tap()];

  // Expect the theme to register light theme as the user preference and the
  // button to be selected.
  ExpectBodyHasThemeAndFont("light", "sans-serif");
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithAccessibilityLabelId(
                  IDS_IOS_READER_MODE_OPTIONS_COLOR_THEME_BUTTON_ACCESSIBILITY_LABEL_LIGHT),
              grey_accessibilityTrait(UIAccessibilityTraitSelected), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that font change is applied to the Reading Mode web page.
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testUpdateReaderModeFont testUpdateReaderModeFont
#else
#define MAYBE_testUpdateReaderModeFont FLAKY_testUpdateReaderModeFont
#endif
- (void)MAYBE_testUpdateReaderModeFont {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];

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
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testTapReaderModeChipShowsOptionsView \
  testTapReaderModeChipShowsOptionsView
#else
#define MAYBE_testTapReaderModeChipShowsOptionsView \
  FLAKY_testTapReaderModeChipShowsOptionsView
#endif
- (void)MAYBE_testTapReaderModeChipShowsOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];
}

// Tests that font family can be changed from the options view.
- (void)testChangeReaderModeFontFamilyFromOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];

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

// Tests that font size can be changed from the options view.
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testChangeReaderModeFontSizeFromOptionsView \
  testChangeReaderModeFontSizeFromOptionsView
#else
#define MAYBE_testChangeReaderModeFontSizeFromOptionsView \
  FLAKY_testChangeReaderModeFontSizeFromOptionsView
#endif
- (void)MAYBE_testChangeReaderModeFontSizeFromOptionsView {
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  [ChromeEarlGrey setDoubleValue:multipliers[0]
                     forUserPref:dom_distiller::prefs::kFontScale];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];

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
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testChangeReaderModeThemeFromOptionsView \
  testChangeReaderModeThemeFromOptionsView
#else
#define MAYBE_testChangeReaderModeThemeFromOptionsView \
  FLAKY_testChangeReaderModeThemeFromOptionsView
#endif
- (void)MAYBE_testChangeReaderModeThemeFromOptionsView {
  [ChromeEarlGrey setIntegerValue:(int)dom_distiller::mojom::Theme::kLight
                      forUserPref:dom_distiller::prefs::kTheme];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];

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
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testTapCloseButtonInOptionsView testTapCloseButtonInOptionsView
#else
#define MAYBE_testTapCloseButtonInOptionsView \
  FLAKY_testTapCloseButtonInOptionsView
#endif
- (void)MAYBE_testTapCloseButtonInOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];

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
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testTapTurnOffButtonInOptionsView \
  testTapTurnOffButtonInOptionsView
#else
#define MAYBE_testTapTurnOffButtonInOptionsView \
  FLAKY_testTapTurnOffButtonInOptionsView
#endif
- (void)MAYBE_testTapTurnOffButtonInOptionsView {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];

  // Tap the "Turn off" button.
  if ([ChromeEarlGrey isProactiveSuggestionsFrameworkEnabled]) {
    [[EarlGrey selectElementWithMatcher:testing::NavigationBarBackButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_AI_HUB_HIDE_BUTTON_LABEL)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kReaderModeOptionsTurnOffButtonAccessibilityIdentifier)]
        performAction:grey_tap()];
  }

  // The options view should be hidden.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeOptionsViewAccessibilityIdentifier)];

  // The Reader Mode UI is not visible.
  [self assertReaderModePageIsHidden];
}

// Tests that tapping outside of the options view dismisses it.
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testTapOutsideOptionsViewDismissesIt \
  testTapOutsideOptionsViewDismissesIt
#else
#define MAYBE_testTapOutsideOptionsViewDismissesIt \
  FLAKY_testTapOutsideOptionsViewDismissesIt
#endif
- (void)MAYBE_testTapOutsideOptionsViewDismissesIt {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Open Reading Mode options view.
  [self openReaderModeCustomizationOptions];

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
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Tap the chip to open the options view.
  [[EarlGrey selectElementWithMatcher:ReaderModeCustomizationBadge()]
      performAction:grey_tap()];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the contextual panel entrypoint disappears and a failure snackbar
// is presented when distillation fails.
- (void)testReaderModeDistillationFailure {
  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];

  // Wait for the contextual panel entrypoint to appear.
  id<GREYMatcher> entrypoint = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:entrypoint];

  // Make the page not distillable.
  [ChromeEarlGrey
      evaluateJavaScriptForSideEffect:@"document.body.outerHTML = ''"];

  // Tap the entrypoint to trigger distillation.
  [self openReaderModeWithBadgeEntrypoint];

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
  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];

  [self openReaderModeWithBadgeEntrypoint];

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
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  EXPECT_EQ(4, CountNumLinks());

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  [self assertReaderModePageIsVisible];
  EXPECT_EQ(1, CountNumLinks());
}

// Tests that a sample contextual chip stays visible inside Reader mode if
// kAskGeminiChip is enabled.
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testSampleContextualChipVisibleInReaderMode \
  testSampleContextualChipVisibleInReaderMode
#else
#define MAYBE_testSampleContextualChipVisibleInReaderMode \
  FLAKY_testSampleContextualChipVisibleInReaderMode
#endif
- (void)MAYBE_testSampleContextualChipVisibleInReaderMode {
  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Check that the sample contextual chip is visible.
  [[EarlGrey
      selectElementWithMatcher:ContextualPanelEntrypointImageViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Reader mode contextual chip is hidden inside Reader mode if
// kAskGeminiChip is enabled.
- (void)testReaderModeChipHiddenInReaderMode {
  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];

  // Wait for the Reader Mode contextual entrypoint to appear.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ContextualPanelEntrypointImageViewMatcher()];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // The Reader Mode contextual entrypoint should be hidden.
  [[EarlGrey
      selectElementWithMatcher:ContextualPanelEntrypointImageViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  // The Reader mode badge button should be visible instead.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBadgeButtonReaderModeAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the user can turn on Reader Mode from the page action menu.
- (void)testTurnOnReaderModeViaPageActionMenu {
  GREYAssertTrue([ChromeEarlGrey isProactiveSuggestionsFrameworkEnabled],
                 @"Proactive suggestions framework feature must be enabled");

  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];

  // Wait for the contextual chip to appear and then disappear.
  id<GREYMatcher> entrypoint = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:entrypoint];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:entrypoint
                                                 timeout:base::Seconds(10)];

  [self openReaderModeWithBadgeEntrypoint];

  [self assertReaderModePageIsVisible];
}

// Tests that tapping the Reader mode chip shows the AI hub bottom sheet if AI
// hub is available.
- (void)testReaderModeChipShowsAIHubIfAvailable {
  [SigninEarlGrey signinWithFakeIdentity:self.fakeIdentity];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Tap the Reader mode customization badge.
  [[EarlGrey selectElementWithMatcher:grey_allOf(ReaderModeCustomizationBadge(),
                                                 grey_interactable(), nil)]
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
  [self assertReaderModePageIsHidden];
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

// Tests that the Reader mode badge is visible in Incognito.
- (void)testReaderModeBadgeVisibleInIncognito {
  // Open a web page in Incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Reader mode and incognito badge should be visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kReaderModeChipViewAccessibilityIdentifier)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kBadgeButtonIncognitoAccessibilityIdentifier)];
}

// Tests that a reload action dismisses Reader mode.
- (void)testReloadDismissesReaderMode {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  [ChromeEarlGrey reload];

  // The Reader Mode UI is not visible.
  [self assertReaderModePageIsHidden];
}

// Tests that Reader Mode can be toggled on and off for a URL with an empty
// fragment.
- (void)testToggleReaderModeWithEmptyRef {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html#")];

  // Turn on Reader Mode.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Turn off Reader Mode.
  [ChromeEarlGrey hideReaderMode];
  [self assertReaderModePageIsHidden];

  // Turn on Reader Mode again.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];
}

// Tests that the killswitch to disable the omnibox entrypoint does not
// interfere with other Reading Mode entrypoints.
- (void)testOmniboxEntryPointDisabled {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Verify that the omnibox entrypoint is disabled and the tools menu
  // entrypoint is still available.
  [self assertReaderModePageIsHidden];
  [self assertReaderModeInToolsMenuWithMatcher:
            grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))];
}

// Tests that the share menu is accessible via Reader Mode and records the
// expected metrics.
- (void)testShareMenuInReaderMode {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

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

// Tests that the Reader mode chip is visible when leaving Reader mode if
// PSF is disabled.
// TODO(crbug.com/467908483): Remove this test once PSF is launched with
// Reading Mode.
// TODO(crbug.com/481633359): Deflake this test.
#if TARGET_OS_SIMULATOR
#define MAYBE_testReaderModeChipVisibleWhenLeavingReaderModeWithPSFDisabled \
  testReaderModeChipVisibleWhenLeavingReaderModeWithPSFDisabled
#else
#define MAYBE_testReaderModeChipVisibleWhenLeavingReaderModeWithPSFDisabled \
  FLAKY_testReaderModeChipVisibleWhenLeavingReaderModeWithPSFDisabled
#endif
- (void)MAYBE_testReaderModeChipVisibleWhenLeavingReaderModeWithPSFDisabled {
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];

  // Wait for the Reader mode contextual panel entry point chip to be visible.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ContextualPanelEntrypointImageViewMatcher()];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  // Close Reader Mode UI.
  [ChromeEarlGrey hideReaderMode];

  [self assertReaderModePageIsHidden];

  // Wait for the Reader mode contextual panel entry point chip to be visible.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ContextualPanelEntrypointImageViewMatcher()];
}

// Tests that disabling kEnableContentSettingsOptionForLinks shows the old
// Reading Mode toggle in Content Settings.
- (void)testReaderModeContentSettingsOldToggle {
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];

  // Wait for the contextual panel entrypoint to appear.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ContextualPanelEntrypointImageViewMatcher()];

  // Open Content Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ContentSettingsButton()];

  // Check that the Reading Mode toggle is visible.
  id<GREYMatcher> readingModeToggleMatcher =
      chrome_test_util::TableViewSwitchCell(
          kSettingsShowReadingModeAvailableCellId, YES);
  [[EarlGrey selectElementWithMatcher:readingModeToggleMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toggle it OFF.
  [[EarlGrey selectElementWithMatcher:readingModeToggleMatcher]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Go back to the page.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Reload the page to ensure the contextual panel entrypoint is updated.
  [ChromeEarlGrey reload];

  // The contextual panel entrypoint should be hidden.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      ContextualPanelEntrypointImageViewMatcher()];
}

// Tests that enabling kEnableContentSettingsOptionForLinks shows the new
// Reading Mode section in Content Settings with multiple options.
- (void)testReaderModeContentSettingsNewOptions {
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];

  // Wait for the contextual panel entrypoint to appear.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ContextualPanelEntrypointImageViewMatcher()];

  // Open Content Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ContentSettingsButton()];

  // Check that the Reading Mode section is visible.
  id<GREYMatcher> readingModeSectionMatcher =
      grey_accessibilityID(kSettingsReaderModeCellId);
  [[EarlGrey selectElementWithMatcher:readingModeSectionMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the Reading Mode section.
  [[EarlGrey selectElementWithMatcher:readingModeSectionMatcher]
      performAction:grey_tap()];

  // Check that "Show suggestion" toggle is visible and toggle it OFF.
  id<GREYMatcher> showSuggestionToggleMatcher =
      chrome_test_util::TableViewSwitchCell(
          kReaderModeSettingsShowSuggestionAccessibilityIdentifier, YES);
  [[EarlGrey selectElementWithMatcher:showSuggestionToggleMatcher]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Check that "Show hyperlinks" toggle is visible and toggle it OFF.
  id<GREYMatcher> showHyperlinksToggleMatcher =
      chrome_test_util::TableViewSwitchCell(
          kReaderModeSettingsShowHyperlinksAccessibilityIdentifier, YES);
  [[EarlGrey selectElementWithMatcher:showHyperlinksToggleMatcher]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Go back to the page.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Reload the page to ensure the contextual panel entrypoint is updated.
  [ChromeEarlGrey reload];

  // The contextual panel entrypoint should be hidden.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      ContextualPanelEntrypointImageViewMatcher()];

  // Open Reader Mode UI to check for links-hidden class.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Check for 'links-hidden' class on body.
  NSString* js = @"document.body.classList.contains('links-hidden')";
  GREYAssertTrue([ChromeEarlGrey evaluateJavaScript:js].GetBool(),
                 @"The body should have the 'links-hidden' class");
}

// Tests that Reader mode UI stays visible when clearing the presented state.
- (void)testReaderModeUIStaysWhenClearingPresentedState {
  [self loadURLWithOptimizationGuideHints:self.testServer->GetURL(
                                              "/article.html")];
  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");
  [self assertReaderModePageIsVisible];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  [self assertReaderModePageIsVisible];
}

@end

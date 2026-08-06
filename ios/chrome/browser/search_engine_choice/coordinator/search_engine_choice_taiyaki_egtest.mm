// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/variations/variations_switches.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/search_engine_choice/test/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface SearchEngineChoiceTaiyakiTestCase : ChromeTestCase
@end

@implementation SearchEngineChoiceTaiyakiTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
  // Make sure the search engine has been reset, to avoid any issues if it was
  // not by a previous test.
  [SettingsAppInterface resetSearchEngine];
}

- (void)tearDownHelper {
  // Reset the default search engine to Google
  [SettingsAppInterface resetSearchEngine];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(
      "--" + std::string(switches::kSearchEngineChoiceCountry) + "=" +
      switches::kTaiyakiProgramOverride);
  config.additional_args.push_back(
      "--" + std::string(variations::switches::kVariationsOverrideCountry) +
      "=jp");
  config.additional_args.push_back(
      "--" + std::string(switches::kForceSearchEngineChoiceScreen));
  config.features_enabled.push_back(switches::kTaiyakiAllSurfaces);
  config.features_enabled.push_back(
      switches::kSearchEngineChoiceScreenSnackbar);
  // Relaunches the app at each test to re-display the choice screen.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

// Tests that the Search Engine Choice screen is displayed with both subtitles.
- (void)testTaiyakiSearchEngineChoiceScreen {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // The feature is not available on iPad.
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kSearchEngineChoiceTitleAccessibilityIdentifier)]
        assertWithMatcher:grey_nil()];
    return;
  }
  // Check that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  // Verifies that the "More" button is not visible.
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_nil(), nil)];
  // Verifies that subtitle1 and subtitle2 visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSearchEngineChoiceSubtitle1AccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSearchEngineChoiceSubtitle2AccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Select a search engine.
  NSString* searchEngineToSelect =
      [SearchEngineChoiceEarlGreyUI searchEngineNameWithPrepopulatedEngine:
                                        TemplateURLPrepopulateData::yahoo_jp];
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionDown
                              amount:50];

  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::SnackbarViewMatcher()];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

// Tests that tapping the snackbar displayed after confirming the Search Engine
// Choice screen opens the Search Engine settings screen, and verifies changing
// the search engine setting from there.
- (void)testTaiyakiSearchEngineChoiceScreenSnackbarOpenSettings {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // The feature is not available on iPad.
    return;
  }
  // Check that the choice screen is shown.
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];

  // Select a search engine.
  NSString* searchEngineToSelect =
      [SearchEngineChoiceEarlGreyUI searchEngineNameWithPrepopulatedEngine:
                                        TemplateURLPrepopulateData::yahoo_jp];
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionDown
                              amount:50];

  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];

  // Wait for the snackbar to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::SnackbarViewMatcher()];

  // Tap the snackbar action button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSnackbarButtonAccessibilityId)]
      performAction:grey_tap()];

  // Verify that the Search Engine Settings screen is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSearchEngineTableViewControllerId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that Yahoo! JAPAN is displayed in the settings.
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsWithPrepopulatedSearchEngine:
          TemplateURLPrepopulateData::yahoo_jp]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select Google search engine in settings.
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsWithPrepopulatedSearchEngine:
          TemplateURLPrepopulateData::google] performAction:grey_tap()];

  // Close Settings screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Open settings again and verify that Google search engine is selected.
  NSString* googleSearchEngineName =
      [SearchEngineChoiceEarlGreyUI searchEngineNameWithPrepopulatedEngine:
                                        TemplateURLPrepopulateData::google];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:googleSearchEngineName];
}

// Tests that tapping "Learn More" in Taiyaki program displays the Learn More
// screen with the Taiyaki third paragraph instructive text.
- (void)testTaiyakiLearnMore {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // The feature is not available on iPad.
    return;
  }
  // Check that the choice screen is shown.
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  // Open the Learn More dialog.
  id<GREYMatcher> learnMoreLinkMatcher =
      grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK)),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:learnMoreLinkMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  // Verify the Learn More view was presented.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kSearchEngineChoiceLearnMoreAccessibilityIdentifier)];

  // Expand the bottom sheet to largeDetent by swiping up on the sheet view.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kSearchEngineChoiceLearnMoreAccessibilityIdentifier)]
      performAction:grey_swipeSlowInDirection(kGREYDirectionUp)];

  // Verify the Taiyaki third paragraph instructive text is present and visible.
  NSString* taiyakiText = l10n_util::GetNSString(
      IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_THIRD_PARAGRAPH_INSTRUCTIVE);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ContainsPartialText(
                                          taiyakiText)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the Learn More dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

@end

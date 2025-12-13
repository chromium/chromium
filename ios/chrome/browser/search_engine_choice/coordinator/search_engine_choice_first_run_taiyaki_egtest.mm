// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/variations/variations_switches.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_test_case_base.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/search_engine_choice/test/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_app_interface.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

@interface SearchEngineChoiceFirstRunTaiyakiTestCase : FirstRunTestCaseBase
@end

@implementation SearchEngineChoiceFirstRunTaiyakiTestCase

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
  config.features_enabled.push_back(switches::kTaiyaki);
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
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:chrome_test_util::
                                               ButtonStackSecondaryButton()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
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
  [[self class] dismissDefaultBrowserAndRemainingScreens];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

@end

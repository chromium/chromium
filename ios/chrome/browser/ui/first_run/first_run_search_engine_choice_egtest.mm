// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/ui/first_run/first_run_test_case_base.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Tests first run stages with search engine choice
@interface FirstRunSearchEngineChoiceTestCase : FirstRunTestCaseBase
@end

@implementation FirstRunSearchEngineChoiceTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Need to use `switches::kEeaListCountryOverride` as the country to list all
  // the search engines. This is to make sure the more button appears.
  config.additional_args.push_back(
      "--" + std::string(switches::kSearchEngineChoiceCountry) + "=" +
      switches::kEeaListCountryOverride);
  config.features_enabled.push_back(switches::kSearchEngineChoiceTrigger);
  config.additional_args.push_back(
      "--" + std::string(switches::kForceSearchEngineChoiceScreen));
  config.additional_args.push_back("true");
  return config;
}

- (void)setUp {
  [super setUp];
  // Make sure the search engine has been reset, to avoid any issues if it was
  // not by a previous test.
  [SettingsAppInterface resetSearchEngine];
}

- (void)tearDown {
  // Reset the search engine for any other tests.
  [SettingsAppInterface resetSearchEngine];
  [super tearDown];
}

#pragma mark - Tests

// Tests that the Search Engine Choice screen is displayed, that the primary
// button is correctly updated when the user selects a search engine then
// scrolls down and that it correctly sets the default search engine.
- (void)testSearchEngineChoiceScreenSelectThenScroll {
  // Skips sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Checks that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];

  // Verifies that the primary button is initially the "More" button.
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];

  // Selects a search engine.
  NSString* searchEngineToSelect = [SearchEngineChoiceEarlGreyUI
      searchEngineNameWithPrepopulatedEngine:TemplateURLPrepopulateData::bing];
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionDown
                              amount:50];
  // Taps the primary button. This scrolls the table down to the bottom.
  [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  // Verify that the "More" button has been removed.
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_nil()];
  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];
  [[self class] dismissDefaultBrowserAndOmniboxPositionSelectionScreens];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

// Tests that the Search Engine Choice screen is displayed, that the
// primary button is correctly updated when the user scrolls down then selects a
// search engine and that it correctly sets the default search engine.
- (void)testSearchEngineChoiceScreenScrollThenSelect {
  // Skips sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Checks that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];

  // Verifies that the primary button is initially the "More" button.
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];

  // Taps the primary button. This scrolls the table down to the bottom.
  [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];

  // Verifies that the primary button is now the disabled "Set as Default"
  // button.
  id<GREYMatcher> primaryActionButtonMatcher =
      grey_accessibilityID(kSetAsDefaultSearchEngineIdentifier);
  [[EarlGrey selectElementWithMatcher:primaryActionButtonMatcher]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()), grey_notNil(),
                                   nil)];

  // Selects a search engine.
  NSString* searchEngineToSelect = [SearchEngineChoiceEarlGreyUI
      searchEngineNameWithPrepopulatedEngine:TemplateURLPrepopulateData::bing];
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionUp
                              amount:300];
  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];

  [[self class] dismissDefaultBrowserAndOmniboxPositionSelectionScreens];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}
@end

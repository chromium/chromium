// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/policy/policy_constants.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_test_case_base.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

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
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  // Verify that the primary button is initially the "More" pill button.
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];
  // Select a search engine.
  NSString* searchEngineToSelect = [SearchEngineChoiceEarlGreyUI
      searchEngineNameWithPrepopulatedEngine:TemplateURLPrepopulateData::bing];
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionDown
                              amount:50];
  // Verify that the "More" button has been removed.
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_nil()];
  // Tap on the Continue button. This scrolls the table down to the bottom.
  id<GREYMatcher> continueButtonMatcher =
      grey_accessibilityID(kSearchEngineContinueButtonIdentifier);
  [[[EarlGrey selectElementWithMatcher:continueButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  // Verify that the "Contine" button has been removed.
  [[EarlGrey selectElementWithMatcher:continueButtonMatcher]
      assertWithMatcher:grey_nil()];

  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];
  [[self class] dismissDefaultBrowser];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

// Tests that the Search Engine Choice screen is displayed, that the
// primary button is correctly updated when the user scrolls down then selects a
// search engine and that it correctly sets the default search engine.
- (void)testSearchEngineChoiceScreenScrollThenSelect {
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];

  // Verify that the primary button is initially the "More" button.
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];

  // Tap the primary button. This scrolls the table down to the bottom.
  [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];

  // Verify that the primary button is now the disabled "Set as Default"
  // button.
  id<GREYMatcher> primaryActionButtonMatcher =
      grey_accessibilityID(kSetAsDefaultSearchEngineIdentifier);
  [[EarlGrey selectElementWithMatcher:primaryActionButtonMatcher]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()), grey_notNil(),
                                   nil)];

  // Select a search engine.
  NSString* searchEngineToSelect = [SearchEngineChoiceEarlGreyUI
      searchEngineNameWithPrepopulatedEngine:TemplateURLPrepopulateData::bing];
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionUp
                              amount:300];
  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];

  [[self class] dismissDefaultBrowser];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

// Tests that the search engine screen is skipped if the enterprise policies
// are loaded after the sign-in screen is presented.
- (void)testLoadingEnterprisePoliciesAfterPresentingFRE {
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Load enterprise policies to skip the search engine screen.
  NSString* const enterpriseSearchEngineName = @"TestEngine";
  policy_test_utils::MergePolicyWithStringValue(
      base::SysNSStringToUTF8(enterpriseSearchEngineName),
      policy::key::kDefaultSearchProviderName);
  policy_test_utils::MergePolicyWithStringValue(
      "http://www.google.com/search?q={searchTerms}",
      policy::key::kDefaultSearchProviderSearchURL);
  policy_test_utils::MergePolicy(true,
                                 policy::key::kDefaultSearchProviderEnabled);
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Dismiss the default browser screen.
  [[self class] dismissDefaultBrowser];
  // Open the default search engine settings menu.
  [ChromeEarlGreyUI openSettingsMenu];
  // Verify that the correct search engine is selected. The enterprise search
  // engine's name appears in the name of the selected row.
  id<GREYMatcher> settingsSearchEngineButton =
      grey_accessibilityID(kSettingsManagedSearchEngineCellId);
  [[EarlGrey selectElementWithMatcher:settingsSearchEngineButton]
      assertWithMatcher:grey_allOf(
                            grey_accessibilityValue(enterpriseSearchEngineName),
                            grey_sufficientlyVisible(), nil)];
}

// Tests kSearchEngineChoiceScreenEventsHistogram during the FRE, with the
// following scenario:
// + Skip sign-in screen in the FRE
// + Verify search engine choice screen is presented
//    Verify SearchEngineChoiceScreenEvents::kFreChoiceScreenWasDisplayed
// + Open the Learn More dialog
//    Verfiy SearchEngineChoiceScreenEvents::kFreLearnMoreWasDisplayed
// + Close the Learn More dialog
// + Choose Bing search engine
// + Validate search engine choice screen dialog
//    Verify search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet
- (void)testSearchEngineChoiceScreenEventsHistogram {
  NSString* const eventHistogram =
      @(search_engines::kSearchEngineChoiceScreenEventsHistogram);
  // Skip sign-in.
  GREYAssertNil([MetricsAppInterface expectTotalCount:0
                                         forHistogram:eventHistogram],
                @"Failed to record event histogram");
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  GREYAssertNil([MetricsAppInterface expectTotalCount:1
                                         forHistogram:eventHistogram],
                @"Failed to record event histogram");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           search_engines::SearchEngineChoiceScreenEvents::
                               kFreChoiceScreenWasDisplayed)
          forHistogram:eventHistogram],
      @"Failed to record event histogram");
  // Scroll down and open the Learn More dialog.
  id<GREYMatcher> learnMoreLinkMatcher = grey_allOf(
      grey_accessibilityLabel(@"Learn more"), grey_sufficientlyVisible(), nil);
  [[self
      elementInteractionWithGreyMatcher:learnMoreLinkMatcher
                   scrollViewIdentifier:kSearchEngineChoiceScrollViewIdentifier]
      performAction:grey_tap()];
  // Verify the Learn More view was presented.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kSearchEngineChoiceLearnMoreAccessibilityIdentifier)];
  GREYAssertNil([MetricsAppInterface expectTotalCount:2
                                         forHistogram:eventHistogram],
                @"Failed to record event histogram");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           search_engines::SearchEngineChoiceScreenEvents::
                               kFreLearnMoreWasDisplayed)
          forHistogram:eventHistogram],
      @"Failed to record event histogram");
  // Close the Learn More dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Select a search engine.
  NSString* searchEngineToSelect = [SearchEngineChoiceEarlGreyUI
      searchEngineNameWithPrepopulatedEngine:TemplateURLPrepopulateData::bing];
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionDown
                              amount:50];
  // Tap on the Continue button. This scrolls the table down to the bottom.
  id<GREYMatcher> continueButtonMatcher =
      grey_accessibilityID(kSearchEngineContinueButtonIdentifier);
  [[[EarlGrey selectElementWithMatcher:continueButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];
  GREYAssertNil([MetricsAppInterface expectTotalCount:3
                                         forHistogram:eventHistogram],
                @"Failed to record event histogram");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           search_engines::SearchEngineChoiceScreenEvents::
                               kFreDefaultWasSet)
          forHistogram:eventHistogram],
      @"Failed to record event histogram");
  [[self class] dismissDefaultBrowser];
}

@end

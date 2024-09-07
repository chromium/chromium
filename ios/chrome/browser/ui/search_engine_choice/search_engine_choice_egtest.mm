// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface SearchEngineTestCase : ChromeTestCase
@end

@implementation SearchEngineTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
  // Make sure the search engine has been reset, to avoid any issues if it was
  // not by a previous test.
  [SettingsAppInterface resetSearchEngine];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
}

- (void)tearDown {
  // Reset the default search engine to Google
  [SettingsAppInterface resetSearchEngine];
  // Release the histogram tester.
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Need to use `switches::kEeaListCountryOverride` as the country to list all
  // the search engines. This is to make sure the more button appears.
  config.additional_args.push_back(
      "--" + std::string(switches::kSearchEngineChoiceCountry) + "=" +
      switches::kEeaListCountryOverride);
  config.additional_args.push_back(
      "--" + std::string(switches::kForceSearchEngineChoiceScreen));
  // Relaunches the app at each test to re-display the choice screen.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

// Tests that the search engine choice dialog is always visible when the app
// goes to background and foreground.
// TODO(crbug.com/356534232): Re-enable after fixing flakiness.
- (void)FLAKY_testMoveToBackgroundAndToForeground {
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
}

// Tests that search engine choice dialog is moved to the other active scene
// when the current scene is removed.
- (void)testOpenSecondWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey closeWindowWithNumber:0];
  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
}

// Tests that the Search Engine Choice screen is displayed, that the primary
// button is correctly updated when the user selects a search engine then
// scrolls down and that it correctly sets the default search engine.
- (void)testSearchEngineChoiceScreenSelectThenScroll {
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
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

// Tests that the Search Engine Choice screen is displayed, that the
// primary button is correctly updated when the user scrolls down then selects a
// search engine and that it correctly sets the default search engine.
- (void)testSearchEngineChoiceScreenScrollThenSelect {
  // Checks that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  // Verifies that the "More" button is visible.
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];
  // Taps the more button. This scrolls the table down to the bottom.
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
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

// Test that the snippet can be expanded or collapsed.
// TODO(crbug.com/333519732): Fix this flaky test.
- (void)FLAKY_testUserActionWhenExpandingSnippetChevron {
  // Checks that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  if ([ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  NSString* googleSearchEngineIdentifier =
      [SearchEngineChoiceEarlGreyUI searchEngineNameWithPrepopulatedEngine:
                                        TemplateURLPrepopulateData::google];
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:googleSearchEngineIdentifier
                     scrollDirection:kGREYDirectionDown
                              amount:50];
  id<GREYMatcher> oneLineChevronMatcher = grey_accessibilityID([NSString
      stringWithFormat:@"%@%@",
                       kSnippetSearchEngineOneLineChevronIdentifierPrefix,
                       googleSearchEngineIdentifier]);
  [[[EarlGrey selectElementWithMatcher:oneLineChevronMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  id<GREYMatcher> expandedChevronMatcher = grey_accessibilityID([NSString
      stringWithFormat:@"%@%@",
                       kSnippetSearchEngineExpandedChevronIdentifierPrefix,
                       googleSearchEngineIdentifier]);
  [[[EarlGrey selectElementWithMatcher:expandedChevronMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
}

// Tests kSearchEngineChoiceScreenEventsHistogram during the search engine
// choice dialog, with the following the scenario
// + Verify search engine choice screen is presented
// + Open the Learn More dialog
//    Verfiy SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed
// + Close the Learn More dialog
// + Choose Bing search engine
// + Validate search engine choice screen dialog
//    Verify search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet
// Note that SearchEngineChoiceScreenEvents::kChoiceScreenWasDisplayed cannot
// be tested since `[MetricsAppInterface setupHistogramTester] is called after
// the search engine choice dialog is presented.
- (void)testSearchEngineChoiceScreenEventsHistogram {
  NSString* const eventHistogram =
      @(search_engines::kSearchEngineChoiceScreenEventsHistogram);
  // Check that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  // Scroll down and open the Learn More dialog.
  id<GREYMatcher> learnMoreLinkMatcher = grey_allOf(
      grey_accessibilityLabel(@"Learn more"), grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:learnMoreLinkMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  // Verify the Learn More view was presented.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kSearchEngineChoiceLearnMoreAccessibilityIdentifier)];
  GREYAssertNil([MetricsAppInterface expectTotalCount:1
                                         forHistogram:eventHistogram],
                @"Failed to record event histogram");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           search_engines::SearchEngineChoiceScreenEvents::
                               kLearnMoreWasDisplayed)
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
  GREYAssertNil([MetricsAppInterface expectTotalCount:2
                                         forHistogram:eventHistogram],
                @"Failed to record event histogram");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(
                           search_engines::SearchEngineChoiceScreenEvents::
                               kDefaultWasSet)
          forHistogram:eventHistogram],
      @"Failed to record event histogram");
}

@end

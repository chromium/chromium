// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/search_engines_switches.h"
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
}

- (void)tearDown {
  // Reset the default search engine to Google
  [SettingsAppInterface resetSearchEngine];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Set the country to one that is eligible for the choice screen (in this
  // case, France).
  config.additional_args.push_back(
      "--" + std::string(switches::kSearchEngineChoiceCountry) + "=FR");
  // Force the dialog to trigger also for existing users.
  config.additional_args.push_back(
      "--enable-features=SearchEngineChoiceTrigger:for_tagged_profiles_only/"
      "false");
  config.additional_args.push_back(
      "--" + std::string(switches::kForceSearchEngineChoiceScreen));
  // Relaunches the app at each test to re-display the choice screen.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

// Tests that the search engine choice dialog is always visible when the app
// goes to background and foreground.
- (void)testMoveToBackgroundAndToForeground {
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
// TODO(crbug.com/329210226): Re-enable the test.
- (void)FLAKY_testSearchEngineChoiceScreenSelectThenScroll {
  // Checks that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  // The more button is not visible on iPads, only on iPhones.
  // TODO(crbug.com/329579023): We need have a more reliable way to know if
  // there is a more button or not instead of checking if the test is running
  // on iPad or iPhone.
  BOOL moreButtonVisible = ![ChromeEarlGrey isIPadIdiom];
  if (moreButtonVisible) {
    // Verifies that the primary button is initially the "More" button.
    [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
        assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];
  }
  // Selects a search engine.
  NSString* searchEngineToSelect = @"Bing";
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionDown
                              amount:50];
  if (moreButtonVisible) {
    // Taps the primary button. This scrolls the table down to the bottom.
    [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
        assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  }
  // Verify that the "More" button has been removed.
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_nil()];
  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];

  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

// Tests that the Search Engine Choice screen is displayed, that the
// primary button is correctly updated when the user scrolls down then selects a
// search engine and that it correctly sets the default search engine.
// TODO(crbug.com/329210226): Re-enable the test.
- (void)FLAKY_testSearchEngineChoiceScreenScrollThenSelect {
  // Checks that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  // The more button is not visible on iPads, only on iPhones.
  // TODO(crbug.com/329579023): We need have a more reliable way to know if
  // there is a more button or not instead of checking if the test is running
  // on iPad or iPhone.
  BOOL moreButtonVisible = ![ChromeEarlGrey isIPadIdiom];
  if (moreButtonVisible) {
    // Verifies that the primary button is initially the "More" button.
    [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
        assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];
    // Taps the primary button. This scrolls the table down to the bottom.
    [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
        assertWithMatcher:grey_notNil()] performAction:grey_tap()];
  } else {
    // Verify that the more button is not visible.
    [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
        assertWithMatcher:grey_nil()];
  }
  // Verifies that the primary button is now the disabled "Set as Default"
  // button.
  id<GREYMatcher> primaryActionButtonMatcher =
      grey_accessibilityID(kSetAsDefaultSearchEngineIdentifier);
  [[EarlGrey selectElementWithMatcher:primaryActionButtonMatcher]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()), grey_notNil(),
                                   nil)];

  // Selects a search engine.
  NSString* searchEngineToSelect = @"Bing";
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
  NSString* googleSearchEngineIdentifier = @"Google";
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

@end

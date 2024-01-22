// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
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
}

- (void)tearDown {
  // Clear the "choice was made" timestamp pref.
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:
          base::SysUTF8ToNSString(
              prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)];
  // Reset the default search engine to Google
  [SettingsAppInterface resetSearchEngine];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Set the country to one that is eligible for the choice screen (in this
  // case, France).
  config.additional_args.push_back("--search-engine-choice-country=FR");
  // Force the dialog to trigger also for existing users.
  config.additional_args.push_back(
      "--enable-features=SearchEngineChoiceTrigger:for_tagged_profiles_only/"
      "false");
  config.additional_args.push_back("-SearchEngineForceEnabled");
  config.additional_args.push_back("true");
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
- (void)testSearchEngineChoiceScreenSelectThenScroll {
  // Checks that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  // Checks that the fake omnibox illustration is displayed and is initially
  // empty
  [SearchEngineChoiceEarlGreyUI verifyFakeOmniboxIllustrationState:kEmpty];
  // Verifies that the primary button is initially the "More" button.
  id<GREYMatcher> moreButtonMatcher =
      grey_accessibilityID(kSearchEngineMoreButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_notNil(), nil)];

  // Selects a search engine.
  NSString* searchEngineToSelect = @"Bing";
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionDown
                              amount:50];
  // Checks that the fake omnibox illustration is still displayed but is no
  // longer empty
  [SearchEngineChoiceEarlGreyUI verifyFakeOmniboxIllustrationState:kFull];
  // Taps the primary button. This scrolls the table down to the bottom.
  [[[EarlGrey selectElementWithMatcher:moreButtonMatcher]
      assertWithMatcher:grey_notNil()] performAction:grey_tap()];
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
- (void)testSearchEngineChoiceScreenScrollThenSelect {
  // Checks that the choice screen is shown
  [SearchEngineChoiceEarlGreyUI verifySearchEngineChoiceScreenIsDisplayed];
  // Checks that the fake omnibox illustration is displayed and is initially
  // empty
  [SearchEngineChoiceEarlGreyUI verifyFakeOmniboxIllustrationState:kEmpty];
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
  NSString* searchEngineToSelect = @"Bing";
  [SearchEngineChoiceEarlGreyUI
      selectSearchEngineCellWithName:searchEngineToSelect
                     scrollDirection:kGREYDirectionUp
                              amount:300];
  // Checks that the fake omnibox illustration is still displayed but is no
  // longer empty
  [SearchEngineChoiceEarlGreyUI verifyFakeOmniboxIllustrationState:kFull];
  [SearchEngineChoiceEarlGreyUI confirmSearchEngineChoiceScreen];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:searchEngineToSelect];
}

@end

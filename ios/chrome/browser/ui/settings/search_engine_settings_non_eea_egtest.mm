// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/containers/contains.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/search_engines/prepopulated_engines.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/search_engine_settings_test_case_base.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

@interface SearchEngineSettingsNonEEATestCase : SearchEngineSettingsTestCaseBase
@end

@implementation SearchEngineSettingsNonEEATestCase

// Tests that when changing the default search engine, the URL used for the
// search is updated.
- (void)testChangeSearchEngine {
  [self startHTTPServer];
  [self addURLRewriter];

  // Search on Google.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"firstsearch")];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  const std::string googleSearchEngineKeyword(
      base::UTF16ToUTF8(TemplateURLPrepopulateData::google.keyword));
  [ChromeEarlGrey waitForWebStateContainingText:googleSearchEngineKeyword];

  // Change default search engine to the Second prepopulated search engine.
  const TemplateURLPrepopulateData::PrepopulatedEngine*
      secondPrepopulatedSearchEngine =
          [self.class secondPrepopulatedSearchEngine];
  [SearchEngineChoiceEarlGreyUI openSearchEngineSettings];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsWithPrepopulatedSearchEngine:
          *secondPrepopulatedSearchEngine] performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  [self addURLRewriter];

  // Search on selected search engine.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];

  // Search something different than the first search to make sure the omnibox
  // doesn't use the history instead of really searching.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"secondsearch")];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  const std::string secondSearchEngineKeyword(
      base::UTF16ToUTF8(secondPrepopulatedSearchEngine->keyword));
  [ChromeEarlGrey waitForWebStateContainingText:secondSearchEngineKeyword];
}

// Deletes a custom search engine by swiping and tapping on the "Delete" button.
- (void)testDeleteCustomSearchEngineSwipeAndTap {
  [self enterSettingsWithCustomSearchEngine];

  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsSearchEngineWithName:kCustomSearchEngineName]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Swipe all the way to the left, to delete the custom search engine.
  id<GREYMatcher> searchEngineCellMatcher = [SearchEngineChoiceEarlGreyUI
      settingsSearchEngineMatcherWithName:kCustomSearchEngineName];
  [[EarlGrey selectElementWithMatcher:searchEngineCellMatcher]
      performAction:grey_swipeSlowInDirectionWithStartPoint(kGREYDirectionLeft,
                                                            0.9, 0.5)];

  [[EarlGrey selectElementWithMatcher:searchEngineCellMatcher]
      assertWithMatcher:grey_nil()];
}

// Deletes a custom engine by swiping it.
- (void)testDeleteCustomSearchEngineSwipe {
  [self enterSettingsWithCustomSearchEngine];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsSearchEngineWithName:kCustomSearchEngineName]
      performAction:grey_swipeSlowInDirectionWithStartPoint(kGREYDirectionLeft,
                                                            0.9, 0.5)];
  id<GREYMatcher> searchEngineCellMatcher = [SearchEngineChoiceEarlGreyUI
      settingsSearchEngineMatcherWithName:kCustomSearchEngineName];
  [[EarlGrey selectElementWithMatcher:searchEngineCellMatcher]
      assertWithMatcher:grey_nil()];
}

// Tests that the selected custom search engine cannot be deleted.
- (void)testRefuseToDeleteSelectedCustomSearchEngineBySwipe {
  [self enterSettingsWithCustomSearchEngine];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsSearchEngineWithName:kCustomSearchEngineName]
      performAction:grey_tap()];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsSearchEngineWithName:kCustomSearchEngineName]
      performAction:grey_swipeSlowInDirectionWithStartPoint(kGREYDirectionLeft,
                                                            0.9, 0.5)];
  id<GREYMatcher> searchEngineCellMatcher = [SearchEngineChoiceEarlGreyUI
      settingsSearchEngineMatcherWithName:kCustomSearchEngineName];
  [[EarlGrey selectElementWithMatcher:searchEngineCellMatcher]
      assertWithMatcher:grey_notNil()];
}

// Deletes a non-selected custom search engine by entering edit mode.
- (void)testDeleteNotSelectedSearchEngineEdit {
  [self enterSettingsWithCustomSearchEngine];

  id<GREYMatcher> editButton = [[self class] editButtonMatcherWithEnabled:YES];
  [[EarlGrey selectElementWithMatcher:editButton] performAction:grey_tap()];

  id<GREYMatcher> searchEngineCellMatcher = [SearchEngineChoiceEarlGreyUI
      settingsSearchEngineMatcherWithName:kCustomSearchEngineName];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsSearchEngineWithName:kCustomSearchEngineName]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:searchEngineCellMatcher]
      performAction:grey_tap()];

  id<GREYMatcher> deleteButton = grey_allOf(
      grey_accessibilityLabel(@"Delete"),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
  [[EarlGrey selectElementWithMatcher:deleteButton] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:searchEngineCellMatcher]
      assertWithMatcher:grey_nil()];
  // Verify the default search engine is still Google.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  NSString* googleSearchEngineName =
      [SearchEngineChoiceEarlGreyUI searchEngineNameWithPrepopulatedEngine:
                                        TemplateURLPrepopulateData::google];
  [SearchEngineChoiceEarlGreyUI
      verifyDefaultSearchEngineSetting:googleSearchEngineName];
}

// Tests that the edit button is disabled when a custom search engine is
// elected. And tests that the edit button stays disabled after restarting
// Chrome.
- (void)testEditButtonDisabledWhenCustomSearchEngineSelected {
  // Add and select a custom search engine.
  [self enterSettingsWithCustomSearchEngine];
  // Verify the edit button is disabled.
  id<GREYMatcher> searchEngineCellMatcher = [SearchEngineChoiceEarlGreyUI
      settingsSearchEngineMatcherWithName:kCustomSearchEngineName];
  [[SearchEngineChoiceEarlGreyUI
      interactionForSettingsSearchEngineWithName:kCustomSearchEngineName]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:searchEngineCellMatcher]
      performAction:grey_tap()];
  id<GREYMatcher> editButton = [[self class] editButtonMatcherWithEnabled:NO];
  [[EarlGrey selectElementWithMatcher:editButton]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Restart Chrome.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  // Open the search engine settings.
  [SearchEngineChoiceEarlGreyUI openSearchEngineSettings];
  // Verify the edit button is still disabled.
  // Since it is a new instance of Chrome, we need to recreate the matcher.
  editButton = [[self class] editButtonMatcherWithEnabled:NO];
  [[EarlGrey selectElementWithMatcher:editButton]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - SearchEngineSettingsTestCaseBase

+ (const char*)countryForTestCase {
  return "US";
}

+ (const TemplateURLPrepopulateData::PrepopulatedEngine*)
    secondPrepopulatedSearchEngine {
  return &TemplateURLPrepopulateData::bing;
}

@end

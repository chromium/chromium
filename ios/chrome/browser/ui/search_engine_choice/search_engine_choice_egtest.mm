// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"

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
  config.features_enabled.push_back(switches::kSearchEngineChoice);
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

@end

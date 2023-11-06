// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

// Test tabs settings which handle all tabs settings.
@interface InactiveTabsSettingsTestCase : ChromeTestCase
@end

@implementation InactiveTabsSettingsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kTabInactivityThreshold.name) + "<" +
      std::string(kTabInactivityThreshold.name));
  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kTabInactivityThreshold.name) +
      "/Test");
  config.additional_args.push_back(
      "--force-fieldtrial-params=" + std::string(kTabInactivityThreshold.name) +
      ".Test:" + std::string(kTabInactivityThresholdParameterName) + "/" +
      kTabInactivityThresholdTwoWeeksParam);
  return config;
}

- (void)setUp {
  [super setUp];
  // Ensure that inactive tabs preference settings is set to its default state.
  [ChromeEarlGrey setIntegerValue:0
                forLocalStatePref:prefs::kInactiveTabsTimeThreshold];
  GREYAssertEqual(
      0,
      [ChromeEarlGrey localStateIntegerPref:prefs::kInactiveTabsTimeThreshold],
      @"Inactive tabs preference is not set to default value.");
}

- (void)tearDown {
  // Reset preferences back to default values.
  [ChromeEarlGrey setIntegerValue:0
                forLocalStatePref:prefs::kInactiveTabsTimeThreshold];
  [super tearDown];
}

// Ensures that the inactive tabs settings open.
- (void)testOpenInactiveTabsSettings {
  // This test is not relevant on iPads because there is no inactive tabs in
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }
  [self openInactiveTabsSettings];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsInactiveTabsTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testInactiveTabsPreferenceChange {
  // This test is not relevant on iPads because there is no inactive tabs in
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }
  [self openInactiveTabsSettings];

  NSArray<NSString*>* inactiveTabsThresholdOptions = @[
    l10n_util::GetNSString(IDS_IOS_OPTIONS_INACTIVE_TABS_DISABLED),
    base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_IOS_OPTIONS_INACTIVE_TABS_THRESHOLD),
            7)),
    base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_IOS_OPTIONS_INACTIVE_TABS_THRESHOLD),
            14)),
    base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(IDS_IOS_OPTIONS_INACTIVE_TABS_THRESHOLD),
            21))

  ];

  std::vector<int> expectedPreference = {-1, 7, 14, 21};

  GREYAssertEqual([inactiveTabsThresholdOptions count],
                  expectedPreference.size(),
                  @"Missing option or expected value.");

  for (NSUInteger i = 0; i < [inactiveTabsThresholdOptions count]; i++) {
    [[EarlGrey
        selectElementWithMatcher:grey_text(inactiveTabsThresholdOptions[i])]
        performAction:grey_tap()];
    GREYAssertEqual(
        [ChromeEarlGrey
            localStateIntegerPref:prefs::kInactiveTabsTimeThreshold],
        expectedPreference[i],
        @"The inactive tabs settings selected option did not change the "
        @"preference as expected.");
  }
}

#pragma mark - Helpers

// Opens inactive tabs settings.
- (void)openInactiveTabsSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::TabsSettingsButton()];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::InactiveTabsSettingsButton()];
}

@end

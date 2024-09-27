// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file can be empty. Its purpose is to contain the relatively short lived
// definitions required for experimental flags.

#import "ios/chrome/browser/shared/public/features/system_flags.h"

#import <Foundation/Foundation.h>

#import "base/command_line.h"
#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_switches.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/variations/variations_associated_data.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/memory/model/features.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

NSString* const kAlternateDiscoverFeedServerURL =
    @"AlternateDiscoverFeedServerURL";
NSString* const kEnableStartupCrash = @"EnableStartupCrash";
NSString* const kFirstRunForceEnabled = @"FirstRunForceEnabled";
NSString* const kUpgradePromoForceEnabled = @"UpgradePromoForceEnabled";
NSString* const kOriginServerHost = @"AlternateOriginServerHost";
NSString* const kWhatsNewPromoStatus = @"WhatsNewPromoStatus";
NSString* const kClearApplicationGroup = @"ClearApplicationGroup";
NSString* const kNextPromoForDisplayOverride = @"NextPromoForDisplayOverride";
NSString* const kFirstRunRecency = @"FirstRunRecency";
NSString* const kForceExperienceForDeviceSwitcherExperimentalSettings =
    @"ForceExperienceForDeviceSwitcher";
NSString* const kForceExperienceForShopperExperimentalSettings =
    @"ForceExperienceForShopper";
NSString* const kSafetyCheckUpdateChromeStateOverride =
    @"SafetyCheckUpdateChromeStateOverride";
NSString* const kSafetyCheckPasswordStateOverride =
    @"SafetyCheckPasswordStateOverride";
NSString* const kSafetyCheckSafeBrowsingStateOverride =
    @"SafetyCheckSafeBrowsingStateOverride";
NSString* const kSafetyCheckWeakPasswordsCountOverride =
    @"SafetyCheckWeakPasswordsCountOverride";
NSString* const kSafetyCheckReusedPasswordsCountOverride =
    @"SafetyCheckReusedPasswordsCountOverride";
NSString* const kSafetyCheckCompromisedPasswordsCountOverride =
    @"SafetyCheckCompromisedPasswordsCountOverride";
NSString* const kSimulatePostDeviceRestore = @"SimulatePostDeviceRestore";
NSString* const kShouldIgnoreHistorySyncDeclineLimits =
    @"ShouldIgnoreHistorySyncDeclineLimits";
NSString* const kSafetyCheckNotificationsInactivityThreshold =
    @"SafetyCheckNotificationsInactivityThreshold";
BASE_FEATURE(kEnableThirdPartyKeyboardWorkaround,
             "EnableThirdPartyKeyboardWorkaround",
             base::FEATURE_ENABLED_BY_DEFAULT);
NSString* const kTabResumptionDecorationOverride =
    @"TabResumptionDecorationOverride";
NSString* const kTipsMagicStackLensShopWithImage =
    @"TipsMagicStackLensShopWithImage";
NSString* const kTipsMagicStackStateOverride = @"TipsMagicStackStateOverride";
}  // namespace

namespace experimental_flags {

NSString* const kDisplaySwitchProfile = @"DisplaySwitchProfile";

bool AlwaysDisplayFirstRun() {
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:kFirstRunForceEnabled];
}

bool AlwaysDisplayUpgradePromo() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kUpgradePromoForceEnabled];
}

NSString* GetOriginServerHost() {
  return [[NSUserDefaults standardUserDefaults] stringForKey:kOriginServerHost];
}

NSString* GetAlternateDiscoverFeedServerURL() {
  return [[NSUserDefaults standardUserDefaults]
      stringForKey:kAlternateDiscoverFeedServerURL];
}

bool ShouldResetNoticeCardOnFeedStart() {
  return [[NSUserDefaults standardUserDefaults] boolForKey:@"ResetNoticeCard"];
}

bool ShouldResetFirstFollowCount() {
  return [[NSUserDefaults standardUserDefaults] boolForKey:@"ResetFirstFollow"];
}

bool ShouldForceContentNotificationsPromo() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"ForceContentNotificationsPromo"];
}

bool ShouldForceFeedSigninPromo() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"ForceFeedSigninPromo"];
}

bool ShouldIgnoreTileAblationConditions() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"IgnoreTileAblationConditions"];
}

void DidResetFirstFollowCount() {
  [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"ResetFirstFollow"];
}

bool ShouldAlwaysShowFirstFollow() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"AlwaysShowFirstFollow"];
}

bool ShouldAlwaysShowFollowIPH() {
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:@"AlwaysShowFollowIPH"];
}

bool IsMemoryDebuggingEnabled() {
#if BUILDFLAG(IOS_ENABLE_MEMORY_DEBUGGING)
  return true;
#else
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"EnableMemoryDebugging"];
#endif
}

bool IsOmniboxDebuggingEnabled() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"EnableOmniboxDebugging"];
}

bool IsSpotlightDebuggingEnabled() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"EnableSpotlightDebugging"];
}

bool IsStartupCrashEnabled() {
  return [[NSUserDefaults standardUserDefaults] boolForKey:kEnableStartupCrash];
}

bool MustClearApplicationGroupSandbox() {
  bool value =
      [[NSUserDefaults standardUserDefaults] boolForKey:kClearApplicationGroup];
  [[NSUserDefaults standardUserDefaults] setBool:NO
                                          forKey:kClearApplicationGroup];
  return value;
}

bool IsThirdPartyKeyboardWorkaroundEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableThirdPartyKeyboardWorkaround)) {
    return true;
  } else if (command_line->HasSwitch(
                 switches::kDisableThirdPartyKeyboardWorkaround)) {
    return false;
  }

  // Check if the Finch experiment is turned on.
  return base::FeatureList::IsEnabled(kEnableThirdPartyKeyboardWorkaround);
}

NSString* GetForcedPromoToDisplay() {
  return [[NSUserDefaults standardUserDefaults]
      stringForKey:kNextPromoForDisplayOverride];
}

std::optional<UpdateChromeSafetyCheckState> GetUpdateChromeSafetyCheckState() {
  std::string state =
      base::SysNSStringToUTF8([[NSUserDefaults standardUserDefaults]
          stringForKey:kSafetyCheckUpdateChromeStateOverride]);

  return UpdateChromeSafetyCheckStateForName(state);
}

std::optional<PasswordSafetyCheckState> GetPasswordSafetyCheckState() {
  std::string state =
      base::SysNSStringToUTF8([[NSUserDefaults standardUserDefaults]
          stringForKey:kSafetyCheckPasswordStateOverride]);

  return PasswordSafetyCheckStateForName(state);
}

std::optional<SafeBrowsingSafetyCheckState> GetSafeBrowsingSafetyCheckState() {
  std::string state =
      base::SysNSStringToUTF8([[NSUserDefaults standardUserDefaults]
          stringForKey:kSafetyCheckSafeBrowsingStateOverride]);

  return SafeBrowsingSafetyCheckStateForName(state);
}

std::optional<int> GetSafetyCheckWeakPasswordsCount() {
  int weakPasswordsCount = [[NSUserDefaults standardUserDefaults]
      integerForKey:kSafetyCheckWeakPasswordsCountOverride];

  if (weakPasswordsCount == 0) {
    return std::nullopt;
  }

  return weakPasswordsCount;
}

std::optional<int> GetFirstRunRecency() {
  int first_run_recency =
      [[NSUserDefaults standardUserDefaults] integerForKey:kFirstRunRecency];
  if (first_run_recency == 0) {
    return std::nullopt;
  }
  return first_run_recency;
}

std::optional<int> GetSafetyCheckReusedPasswordsCount() {
  int reusedPasswordsCount = [[NSUserDefaults standardUserDefaults]
      integerForKey:kSafetyCheckReusedPasswordsCountOverride];

  if (reusedPasswordsCount == 0) {
    return std::nullopt;
  }

  return reusedPasswordsCount;
}

std::optional<int> GetSafetyCheckCompromisedPasswordsCount() {
  int compromisedPasswordsCount = [[NSUserDefaults standardUserDefaults]
      integerForKey:kSafetyCheckCompromisedPasswordsCountOverride];

  if (compromisedPasswordsCount == 0) {
    return std::nullopt;
  }

  return compromisedPasswordsCount;
}

std::string GetSegmentForForcedDeviceSwitcherExperience() {
  // Checks iOS Experimental Settings.
  std::string segment =
      base::SysNSStringToUTF8([[NSUserDefaults standardUserDefaults]
          stringForKey:kForceExperienceForDeviceSwitcherExperimentalSettings]);
  if (segment.empty()) {
    // Checks command line flag.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(
            switches::kForceDeviceSwitcherExperienceCommandLineFlag)) {
      segment = command_line->GetSwitchValueNative(
          switches::kForceDeviceSwitcherExperienceCommandLineFlag);
    }
  }
  return segment;
}

std::string GetSegmentForForcedShopperExperience() {
  // Checks iOS Experimental Settings.
  std::string segment =
      [[NSUserDefaults standardUserDefaults]
          boolForKey:kForceExperienceForShopperExperimentalSettings]
          ? segmentation_platform::kShoppingUserUmaName
          : segmentation_platform::kLegacyNegativeLabel;
  if (segment.empty()) {
    // Checks command line flag.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kForceShopperExperience)) {
      segment =
          command_line->GetSwitchValueNative(switches::kForceShopperExperience);
    }
  }
  return segment;
}

bool SimulatePostDeviceRestore() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kSimulatePostDeviceRestore];
}

bool ShouldIgnoreHistorySyncDeclineLimits() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kShouldIgnoreHistorySyncDeclineLimits];
}

std::optional<int> DisplaySwitchProfile() {
  int switchProfileCount = [[NSUserDefaults standardUserDefaults]
      integerForKey:kDisplaySwitchProfile];

  if (switchProfileCount == 0) {
    return std::nullopt;
  }

  return switchProfileCount;
}

std::optional<int> GetForcedInactivityThresholdForSafetyCheckNotifications() {
  int threshold = [[NSUserDefaults standardUserDefaults]
      integerForKey:kSafetyCheckNotificationsInactivityThreshold];

  if (threshold == 0) {
    return std::nullopt;
  }

  return threshold;
}

std::optional<int> GetForcedTipsMagicStackState() {
  int tipsIdentifier = [[NSUserDefaults standardUserDefaults]
      integerForKey:kTipsMagicStackStateOverride];

  if (tipsIdentifier == 0) {
    return std::nullopt;
  }

  return tipsIdentifier;
}

bool ShouldDisplayLensShopTipWithImage() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kTipsMagicStackLensShopWithImage];
}

NSString* GetTabResumptionDecorationOverride() {
  NSString* override_value = [[NSUserDefaults standardUserDefaults]
      stringForKey:kTabResumptionDecorationOverride];
  if ([override_value length]) {
    return override_value;
  }
  return nil;
}

}  // namespace experimental_flags

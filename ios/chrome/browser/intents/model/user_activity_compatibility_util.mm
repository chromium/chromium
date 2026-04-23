// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intents/model/user_activity_compatibility_util.h"

#import <CoreSpotlight/CoreSpotlight.h>

#import "base/debug/dump_without_crashing.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/handoff/handoff_utility.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/intents/model/intents_constants.h"
#import "ios/chrome/browser/policy/model/policy_util.h"

namespace {

// Enumeration corresponding to a bit flag corresponding to the mode (regular,
// incognito or both) supported by the activity.
enum ActivityCompatibilityMode {
  kNothing = 0,
  kRegularMode = 1 << 0,
  kIncognitoMode = 1 << 1,
  kRegularAndIncognito = kRegularMode | kIncognitoMode,
};

// Returns the compatible mode for an user activity.
ActivityCompatibilityMode CompatibleModeForActivityType(
    NSString* activity_type) {
  if ([activity_type isEqualToString:CSSearchableItemActionType] ||
      [activity_type isEqualToString:handoff::kChromeHandoffActivityType] ||
      [activity_type isEqualToString:kSiriShortcutAddBookmarkToChrome] ||
      [activity_type isEqualToString:kSiriShortcutAddReadingListItemToChrome] ||
      [activity_type isEqualToString:kSiriShortcutSearchInChrome] ||
      [activity_type isEqualToString:NSUserActivityTypeBrowsingWeb] ||
      [activity_type isEqualToString:kSiriOpenLatestTab] ||
      [activity_type isEqualToString:kSiriOpenReadingList] ||
      [activity_type isEqualToString:kSiriOpenBookmarks] ||
      [activity_type isEqualToString:kSiriOpenTabGrid] ||
      [activity_type isEqualToString:kSiriVoiceSearch] ||
      [activity_type isEqualToString:kSiriOpenNewTab] ||
      [activity_type isEqualToString:kSiriPlayDinoGame] ||
      [activity_type isEqualToString:kSiriSetChromeDefaultBrowser] ||
      [activity_type isEqualToString:kSiriManagePaymentMethods] ||
      [activity_type isEqualToString:kSiriRunSafetyCheck] ||
      [activity_type isEqualToString:kSiriManagePasswords] ||
      [activity_type isEqualToString:kSiriManageSettings] ||
      [activity_type isEqualToString:kSiriOpenLensFromIntents]) {
    return ActivityCompatibilityMode::kRegularAndIncognito;
  }
  if (@available(iOS 26, *)) {
    if (CredentialExchangeEnabled() &&
        [activity_type isEqualToString:[CredentialImportManager
                                           credentialExchangeActivity]]) {
      return ActivityCompatibilityMode::kRegularAndIncognito;
    }
  }
  if ([activity_type isEqualToString:kSiriShortcutOpenInChrome] ||
      [activity_type isEqualToString:kSiriOpenRecentTabs] ||
      [activity_type isEqualToString:kSiriViewHistory] ||
      [activity_type isEqualToString:kSiriClearBrowsingData]) {
    return ActivityCompatibilityMode::kRegularMode;
  }
  if ([activity_type isEqualToString:kSiriShortcutOpenInIncognito] ||
      [activity_type isEqualToString:kSiriOpenNewIncognitoTab]) {
    return ActivityCompatibilityMode::kIncognitoMode;
  }

  // Use 32 as the maximum length of the reported value for this key (31
  // characters + '\0'). See NSUserActivityTypes in Info.plist for the list of
  // expected values.
  static crash_reporter::CrashKeyString<32> key("activity");
  crash_reporter::ScopedCrashKeyString crash_key(
      &key, base::SysNSStringToUTF8(activity_type));
  base::debug::DumpWithoutCrashing();
  return ActivityCompatibilityMode::kNothing;
}

}  // namespace

BOOL ProceedWithUserActivity(NSUserActivity* user_activity,
                             PrefService* pref_service) {
  ActivityCompatibilityMode compatibility =
      CompatibleModeForActivityType(user_activity.activityType);
  if (IsIncognitoModeDisabled(pref_service)) {
    return compatibility & ActivityCompatibilityMode::kRegularMode;
  }
  if (IsIncognitoModeForced(pref_service)) {
    return compatibility & ActivityCompatibilityMode::kIncognitoMode;
  }
  // Return YES if the compatible mode array is not nil.
  return compatibility != ActivityCompatibilityMode::kNothing;
}

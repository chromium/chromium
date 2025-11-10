// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/public/synced_set_up_metrics.h"

#import <optional>

#import "base/metrics/histogram_functions.h"
#import "components/commerce/core/pref_names.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/safety_check/safety_check_pref_names.h"

namespace {

// UMA histogram name for recording the `SyncedSetUpTriggerSource`.
static constexpr char kSyncedSetUpTriggerSource[] =
    "IOS.SyncedSetUp.TriggerSource";

// UMA histogram name for recording the `SyncedSetUpSnackbarInteraction`.
static constexpr char kSyncedSetUpSnackbarInteraction[] =
    "IOS.SyncedSetUp.Snackbar.Interaction";

// UMA histogram name for recording the number of applied remote prefs.
static constexpr char kSyncedSetUpRemoteAppliedPrefCount[] =
    "IOS.SyncedSetUp.RemoteAppliedPrefCount";

// UMA histogram name for recording `SyncedSetUpAppliedPref`.
static constexpr char kSyncedSetUpPrefApplied[] = "IOS.SyncedSetUp.PrefApplied";

// Maps a pref `name` to its corresponding metric enum.
std::optional<SyncedSetUpAppliedPref> PrefNameToEnum(
    std::string_view pref_name) {
  if (pref_name == omnibox::kIsOmniboxInBottomPosition) {
    return SyncedSetUpAppliedPref::kOmniboxPosition;
  }
  if (pref_name == ntp_tiles::prefs::kMagicStackHomeModuleEnabled) {
    return SyncedSetUpAppliedPref::kMagicStackHomeModule;
  }
  if (pref_name == ntp_tiles::prefs::kMostVisitedHomeModuleEnabled) {
    return SyncedSetUpAppliedPref::kMostVisitedHomeModule;
  }
  if (pref_name == commerce::kPriceTrackingHomeModuleEnabled) {
    return SyncedSetUpAppliedPref::kPriceTrackingHomeModule;
  }
  if (pref_name == safety_check::prefs::kSafetyCheckHomeModuleEnabled) {
    return SyncedSetUpAppliedPref::kSafetyCheckHomeModule;
  }
  if (pref_name == ntp_tiles::prefs::kTabResumptionHomeModuleEnabled) {
    return SyncedSetUpAppliedPref::kTabResumptionHomeModule;
  }
  if (pref_name == ntp_tiles::prefs::kTipsHomeModuleEnabled) {
    return SyncedSetUpAppliedPref::kTipsHomeModule;
  }
  return std::nullopt;
}

}  // namespace

void LogSyncedSetUpTriggerSource(SyncedSetUpTriggerSource source) {
  base::UmaHistogramEnumeration(kSyncedSetUpTriggerSource, source);
}

void LogSyncedSetUpSnackbarInteraction(SyncedSetUpSnackbarInteraction event) {
  base::UmaHistogramEnumeration(kSyncedSetUpSnackbarInteraction, event);
}

void LogSyncedSetUpRemoteAppliedPrefCount(int count) {
  // Uses `Counts100` as the number of applied prefs for the Synced Set Up
  // feature is small (< 10).
  base::UmaHistogramCounts100(kSyncedSetUpRemoteAppliedPrefCount, count);
}

void LogSyncedSetUpPrefApplied(std::string_view pref_name) {
  SyncedSetUpAppliedPref pref_enum =
      PrefNameToEnum(pref_name).value_or(SyncedSetUpAppliedPref::kUnknown);
  base::UmaHistogramEnumeration(kSyncedSetUpPrefApplied, pref_enum);
}

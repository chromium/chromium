// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/install_attribution/install_attribution_helper.h"

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/model/install_attribution/gmo_sko_acceptance_data.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace install_attribution {

namespace {

// The different time windows used for considering an install to be
// attributable to the acceptance of an external promo.
const base::TimeDelta kShortAttributionWindow = base::Days(1);
const base::TimeDelta kLongAttributionWindow = base::Days(15);

// Helper function to calculate the start of the next calendar month. If the
// operation fails (due to underlying time manipulation errors), returns false.
// The content of `result` is unspecified in that scenario.
bool GetNextCalendarMonthStart(base::Time* result) {
  base::Time::Exploded exploded;
  base::Time::Now().UTCExplode(&exploded);
  CHECK(exploded.HasValidValues(), base::NotFatalUntil::M150);

  exploded.day_of_week = 0;
  exploded.day_of_month = 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  if (exploded.month == 12) {
    exploded.year++;
    exploded.month = 1;
  } else {
    exploded.month++;
  }

  bool success = base::Time::FromUTCExploded(exploded, result);
  return success;
}

}  // namespace

void LogInstallAttribution() {
  if (!IsInstallAttributionLoggingEnabled()) {
    return;
  }

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  if (!sharedDefaults || !local_state) {
    return;
  }

  bool shouldClearAcceptanceData = true;

  // First, check for previously stored attribution data that needs to be
  // logged.
  base::Time next_log_date =
      local_state->GetTime(prefs::kIOSGMOSKOPlacementIDNextLogDate);
  int placement_id =
      local_state->GetInteger(prefs::kIOSGMOSKOLastAttributionPlacementID);
  int attribution_window_type =
      local_state->GetInteger(prefs::kIOSGMOSKOLastAttributionWindowType);

  if (placement_id != 0 && attribution_window_type > 0 &&
      base::Time::Now() > next_log_date) {
    // The histogram for the long window also includes placement IDs from the
    // short window, otherwise it will undercount placement ID results and
    // misrepresent the performance of the different promo variants.
    base::UmaHistogramSparse(
        "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", placement_id);
    if (attribution_window_type ==
        static_cast<int>(InstallAttributionType::Within24Hours)) {
      base::UmaHistogramSparse(
          "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow",
          placement_id);
    }

    local_state->ClearPref(prefs::kIOSGMOSKOPlacementIDNextLogDate);
    local_state->ClearPref(prefs::kIOSGMOSKOLastAttributionPlacementID);
    local_state->ClearPref(prefs::kIOSGMOSKOLastAttributionWindowType);
  } else {
    // Otherwise, check for new acceptance data from the shared defaults.
    NSData* archivedData =
        [sharedDefaults dataForKey:app_group::kGMOSKOInstallAttribution];
    if (archivedData) {
      NSError* unarchiveError = nil;
      GMOSKOAcceptanceData* acceptanceData = [NSKeyedUnarchiver
          unarchivedObjectOfClass:[GMOSKOAcceptanceData class]
                         fromData:archivedData
                            error:&unarchiveError];

      if (acceptanceData != nil && acceptanceData.placementID != nil &&
          acceptanceData.timestamp != nil) {
        base::Time acceptanceTime =
            base::Time::FromNSDate(acceptanceData.timestamp);
        base::TimeDelta elapsedSinceAcceptance =
            base::Time::Now() - acceptanceTime;

        if (elapsedSinceAcceptance < kLongAttributionWindow) {
          // Record coarse attribution data as it is received.
          InstallAttributionType window_type =
              elapsedSinceAcceptance < kShortAttributionWindow
                  ? InstallAttributionType::Within24Hours
                  : InstallAttributionType::Within15Days;

          base::UmaHistogramEnumeration("IOS.GMOSKOInstallAttribution",
                                        window_type);

          // The more specific placement ID must be stored and recorded in the
          // next time bucket (next calendar month).
          base::Time next_month_start;
          if (GetNextCalendarMonthStart(&next_month_start)) {
            local_state->SetTime(prefs::kIOSGMOSKOPlacementIDNextLogDate,
                                 next_month_start);
            local_state->SetInteger(prefs::kIOSGMOSKOLastAttributionPlacementID,
                                    [acceptanceData.placementID intValue]);
            local_state->SetInteger(prefs::kIOSGMOSKOLastAttributionWindowType,
                                    static_cast<int>(window_type));
          } else {
            // Could not determine next month start. Keep the acceptance data
            // and try again next launch.
            shouldClearAcceptanceData = false;
          }
        }
      }
    }
  }

  if (shouldClearAcceptanceData) {
    [sharedDefaults removeObjectForKey:app_group::kGMOSKOInstallAttribution];
  }
}

}  // namespace install_attribution

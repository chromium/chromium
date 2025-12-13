// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/install_attribution/install_attribution_helper.h"

#import <string>

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/model/install_attribution/install_attribution_acceptance_data.h"
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

void LogInstallAttributionForSource(NSString* incoming_class_name,
                                    NSString* acceptance_data_key,
                                    const std::string& next_log_date_pref,
                                    const std::string& last_placement_id_pref,
                                    const std::string& last_window_type_pref,
                                    const std::string& long_window_histogram,
                                    const std::string& short_window_histogram,
                                    const std::string& attribution_histogram) {
  NSUserDefaults* shared_defaults = app_group::GetCommonGroupUserDefaults();
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  if (!shared_defaults || !local_state) {
    return;
  }

  bool should_clear_acceptance_data = true;

  // First, check for previously stored attribution data that needs to be
  // logged.
  base::Time next_log_date = local_state->GetTime(next_log_date_pref);
  int placement_id = local_state->GetInteger(last_placement_id_pref);
  int attribution_window_type = local_state->GetInteger(last_window_type_pref);

  if (placement_id != 0 && attribution_window_type > 0 &&
      base::Time::Now() > next_log_date) {
    // The histogram for the long window also includes placement IDs from the
    // short window, otherwise it will undercount placement ID results and
    // misrepresent the performance of the different promo variants.
    base::UmaHistogramSparse(long_window_histogram, placement_id);
    if (attribution_window_type ==
        static_cast<int>(InstallAttributionType::Within24Hours)) {
      base::UmaHistogramSparse(short_window_histogram, placement_id);
    }

    local_state->ClearPref(next_log_date_pref);
    local_state->ClearPref(last_placement_id_pref);
    local_state->ClearPref(last_window_type_pref);
  } else {
    // Otherwise, check for new acceptance data from the shared defaults.
    NSData* archivedData = [shared_defaults dataForKey:acceptance_data_key];
    if (archivedData) {
      NSError* unarchiveError = nil;
      [NSKeyedUnarchiver setClass:[InstallAttributionAcceptanceData class]
                     forClassName:incoming_class_name];
      InstallAttributionAcceptanceData* acceptanceData = [NSKeyedUnarchiver
          unarchivedObjectOfClass:[InstallAttributionAcceptanceData class]
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

          base::UmaHistogramEnumeration(attribution_histogram, window_type);

          // The more specific placement ID must be stored and recorded in the
          // next time bucket (next calendar month).
          base::Time next_month_start;
          if (GetNextCalendarMonthStart(&next_month_start)) {
            local_state->SetTime(next_log_date_pref, next_month_start);
            local_state->SetInteger(last_placement_id_pref,
                                    [acceptanceData.placementID intValue]);
            local_state->SetInteger(last_window_type_pref,
                                    static_cast<int>(window_type));
          } else {
            // Could not determine next month start. Keep the acceptance data
            // and try again next launch.
            should_clear_acceptance_data = false;
          }
        }
      }
    }
  }

  if (should_clear_acceptance_data) {
    [shared_defaults removeObjectForKey:acceptance_data_key];
  }
}

}  // namespace

void LogInstallAttribution() {
  if (IsInstallAttributionLoggingEnabled()) {
    LogInstallAttributionForSource(
        @"GMOSKOAcceptanceData", app_group::kGMOSKOInstallAttribution,
        prefs::kIOSGMOSKOPlacementIDNextLogDate,
        prefs::kIOSGMOSKOLastAttributionPlacementID,
        prefs::kIOSGMOSKOLastAttributionWindowType,
        "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow",
        "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow",
        "IOS.GMOSKOInstallAttribution");
  }

  if (IsAppPreviewInstallAttributionLoggingEnabled()) {
    LogInstallAttributionForSource(
        @"GCRAppPreviewAcceptanceData",
        app_group::kAppPreviewInstallAttribution,
        prefs::kIOSAppPreviewPlacementIDNextLogDate,
        prefs::kIOSAppPreviewLastAttributionPlacementID,
        prefs::kIOSAppPreviewLastAttributionWindowType,
        "IOS.AppPreviewAttributionPlacementID.LongAttributionWindow",
        "IOS.AppPreviewAttributionPlacementID.ShortAttributionWindow",
        "IOS.AppPreviewInstallAttribution");
  }
}

}  // namespace install_attribution

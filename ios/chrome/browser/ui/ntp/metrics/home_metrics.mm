// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_constants.h"

void RecordHomeAction(IOSHomeActionType type, bool isStartSurface) {
  if (isStartSurface) {
    UMA_HISTOGRAM_ENUMERATION(kActionOnStartHistogram, type);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kActionOnNTPHistogram, type);
  }
}

void RecordModuleFreshnessSignal(ContentSuggestionsModuleType module_type) {
  switch (module_type) {
    case ContentSuggestionsModuleType::kShortcuts: {
      PrefService* local_state = GetApplicationContext()->GetLocalState();
      local_state->SetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
          0);
      base::RecordAction(
          base::UserMetricsAction("IOSMagicStackShortcutsFreshSignal"));
      break;
    }
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow: {
      PrefService* local_state = GetApplicationContext()->GetLocalState();
      local_state->SetInteger(
          prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
          0);
      base::RecordAction(
          base::UserMetricsAction("IOSMagicStackSafetyCheckFreshSignal"));
      break;
    }
    case ContentSuggestionsModuleType::kTabResumption: {
      PrefService* local_state = GetApplicationContext()->GetLocalState();
      local_state->SetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
          0);
      base::RecordAction(
          base::UserMetricsAction("IOSMagicStackTabResumptionFreshSignal"));
      break;
    }
    case ContentSuggestionsModuleType::kParcelTracking: {
      PrefService* local_state = GetApplicationContext()->GetLocalState();
      local_state->SetInteger(
          prefs::
              kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness,
          0);
      base::RecordAction(
          base::UserMetricsAction("IOSMagicStackParcelTrackingFreshSignal"));
      break;
    }
    default:
      break;
  }
}

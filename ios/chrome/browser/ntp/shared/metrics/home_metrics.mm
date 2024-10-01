// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"

const char kMagicStackTopModuleImpressionHistogram[] =
    "IOS.MagicStack.Module.TopImpression";
const char kMagicStackModuleEngagementOnStartHistogram[] =
    "IOS.MagicStack.Module.Click.OnStart";
const char kMagicStackModuleEngagementOnNTPHistogram[] =
    "IOS.MagicStack.Module.Click.OnNTP";

// Maximum index of module.
const float kMaxModuleImpressionIndex = 50;

namespace {
std::string TabResumptionHistogramName(bool is_click,
                                       bool is_start_surface,
                                       bool is_local) {
  std::string histogram_name = "IOS.MagicStack.Module";
  histogram_name += is_click ? ".Click" : ".Impression";
  histogram_name += ".TabResumption";
  histogram_name += is_start_surface ? ".OnStart" : ".OnNTP";
  histogram_name += is_local ? ".Recent" : ".Sync";
  return histogram_name;
}
}  // namespace

void RecordHomeAction(IOSHomeActionType type, bool isStartSurface) {
  if (isStartSurface) {
    UMA_HISTOGRAM_ENUMERATION(kActionOnStartHistogram, type);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kActionOnNTPHistogram, type);
  }
}

void RecordMagicStackClick(ContentSuggestionsModuleType type,
                           bool isStartSurface) {
  if (isStartSurface) {
    UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleEngagementOnStartHistogram,
                              type);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleEngagementOnNTPHistogram, type);
  }
}

void RecordMagicStackTabResumptionClick(bool isLocal,
                                        bool isStartSurface,
                                        NSUInteger index) {
  base::UmaHistogramExactLinear(
      TabResumptionHistogramName(/*is_click*/ true, isStartSurface, isLocal),
      index, kMaxModuleImpressionIndex);
}

void RecordModuleFreshnessSignal(ContentSuggestionsModuleType module_type) {
  switch (module_type) {
    case ContentSuggestionsModuleType::kMostVisited: {
      PrefService* local_state = GetApplicationContext()->GetLocalState();
      local_state->SetInteger(
          prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, 0);
      break;
    }
    case ContentSuggestionsModuleType::kShortcuts: {
      PrefService* local_state = GetApplicationContext()->GetLocalState();
      local_state->SetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
          0);
      base::RecordAction(
          base::UserMetricsAction("IOSMagicStackShortcutsFreshSignal"));
      break;
    }
    case ContentSuggestionsModuleType::kSafetyCheck: {
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

void LogTopModuleImpressionForType(ContentSuggestionsModuleType module_type) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  switch (module_type) {
    case ContentSuggestionsModuleType::kMostVisited: {
      // Increment freshness pref since it is an impression of
      // the latest Most Visited Sites as the top module, but only if there has
      // been a freshness signal.
      int freshness_impression_count = local_state->GetInteger(
          prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        local_state->SetInteger(
            prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness,
            freshness_impression_count + 1);
      }
      break;
    }
    case ContentSuggestionsModuleType::kShortcuts: {
      // Increment freshness pref since it is an impression of
      // the latest Shortcuts as the top module, but only if there has been a
      // freshness signal.
      int freshness_impression_count = local_state->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        local_state->SetInteger(
            prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
            freshness_impression_count + 1);
      }
      break;
    }

    case ContentSuggestionsModuleType::kSafetyCheck: {
      // Increment freshness pref since it is an impression of
      // the latest Safety Check results as the top module, but only if there
      // has been a freshness signal.
      int freshness_impression_count = local_state->GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        local_state->SetInteger(
            prefs::
                kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
            freshness_impression_count + 1);
      }
      break;
    }
    case ContentSuggestionsModuleType::kTabResumption: {
      // Increment freshness pref since it is an impression of
      // the latest Tab Resumption results as the top module, but only if there
      // has been a freshness signal.
      int freshness_impression_count = local_state->GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        local_state->SetInteger(
            prefs::
                kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
            freshness_impression_count + 1);
      }
      break;
    }
    case ContentSuggestionsModuleType::kParcelTracking: {
      // Increment freshness pref since it is an impression of
      // the latest Tab Resumption results as the top module, but only if there
      // has been a freshness signal.
      int freshness_impression_count = local_state->GetInteger(
          prefs::
              kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        local_state->SetInteger(
            prefs::
                kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness,
            freshness_impression_count + 1);
      }
      break;
    }
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
    case ContentSuggestionsModuleType::kPlaceholder:
      // Ephemeral Card
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
    case ContentSuggestionsModuleType::kInvalid:
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
      break;
  }
  UMA_HISTOGRAM_ENUMERATION(kMagicStackTopModuleImpressionHistogram,
                            module_type);
}

void LogTabResumptionImpression(bool isLocal,
                                bool isStartSurface,
                                NSUInteger index) {
  base::UmaHistogramExactLinear(
      TabResumptionHistogramName(/*is_click*/ false, isStartSurface, isLocal),
      index, kMaxModuleImpressionIndex);
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

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

void RecordModuleFreshnessSignal(ContentSuggestionsModuleType module_type,
                                 PrefService* profile_pref_service) {
  switch (module_type) {
    case ContentSuggestionsModuleType::kMostVisited: {
      profile_pref_service->SetInteger(
          prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, 0);
      break;
    }
    case ContentSuggestionsModuleType::kShortcuts: {
      profile_pref_service->SetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
          0);
      base::RecordAction(
          base::UserMetricsAction("IOSMagicStackShortcutsFreshSignal"));
      break;
    }
    case ContentSuggestionsModuleType::kSafetyCheck: {
      profile_pref_service->SetInteger(
          prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
          0);
      base::RecordAction(
          base::UserMetricsAction("IOSMagicStackSafetyCheckFreshSignal"));
      break;
    }
    case ContentSuggestionsModuleType::kTabResumption: {
      profile_pref_service->SetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
          0);
      base::RecordAction(
          base::UserMetricsAction("IOSMagicStackTabResumptionFreshSignal"));
      break;
    }
    default:
      break;
  }
}

void LogTopModuleImpressionForType(ContentSuggestionsModuleType module_type,
                                   PrefService* profile_pref_service) {
  switch (module_type) {
    case ContentSuggestionsModuleType::kMostVisited: {
      // Increment freshness pref since it is an impression of
      // the latest Most Visited Sites as the top module, but only if there has
      // been a freshness signal.
      int freshness_impression_count = profile_pref_service->GetInteger(
          prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        profile_pref_service->SetInteger(
            prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness,
            freshness_impression_count + 1);
      }
      break;
    }
    case ContentSuggestionsModuleType::kShortcuts: {
      // Increment freshness pref since it is an impression of
      // the latest Shortcuts as the top module, but only if there has been a
      // freshness signal.
      int freshness_impression_count = profile_pref_service->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        profile_pref_service->SetInteger(
            prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness,
            freshness_impression_count + 1);
      }
      break;
    }
    case ContentSuggestionsModuleType::kSafetyCheck: {
      // Increment freshness pref since it is an impression of
      // the latest Safety Check results as the top module, but only if there
      // has been a freshness signal.
      int freshness_impression_count = profile_pref_service->GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        profile_pref_service->SetInteger(
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
      int freshness_impression_count = profile_pref_service->GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness);
      if (freshness_impression_count >= 0) {
        profile_pref_service->SetInteger(
            prefs::
                kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
            freshness_impression_count + 1);
      }
      break;
    }
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
    case ContentSuggestionsModuleType::kPlaceholder:
    case ContentSuggestionsModuleType::kShopCard:
      // Ephemeral Card
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
    case ContentSuggestionsModuleType::kSendTabPromo:
    case ContentSuggestionsModuleType::kInvalid:
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
    case ContentSuggestionsModuleType::kAppBundlePromo:
    case ContentSuggestionsModuleType::kDefaultBrowser:
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

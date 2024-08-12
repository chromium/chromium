// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_position_util.h"

namespace {

/// Returns the metric name variation for classification `result`.
std::string MetricNameForDeviceSwitcherResult(
    const std::string& name,
    const segmentation_platform::ClassificationResult& result) {
  if (!omnibox::IsNewUser()) {
    return name + ".NotNew";
  }

  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    return name + ".Unavailable";
  }

  if (omnibox::IsSafariSwitcher(result)) {
    return name + ".IsSwitcher";
  } else {
    return name + ".NotSwitcher";
  }
}

}  // namespace

void RecordScreenEvent(OmniboxPositionChoiceScreenEvent event) {
  // Record histogram.
  std::string histogram = "IOS.Omnibox.Promo.Events.Startup";
  base::UmaHistogramEnumeration(histogram, event);

  // Record action.
  switch (event) {
    case OmniboxPositionChoiceScreenEvent::kScreenDisplayed: {
        base::RecordAction(
            base::UserMetricsAction("IOS.Omnibox.Promo.Presented.Startup"));
      break;
    }
    case OmniboxPositionChoiceScreenEvent::kPositionValidated: {
        base::RecordAction(base::UserMetricsAction(
            "IOS.Omnibox.Promo.PositionValidated.Startup"));
      break;
    }
    case OmniboxPositionChoiceScreenEvent::kPositionDiscarded: {
        base::RecordAction(base::UserMetricsAction(
            "IOS.Omnibox.Promo.PositionDiscarded.Startup"));
      break;
    }
    case OmniboxPositionChoiceScreenEvent::kScreenSkipped: {
        base::RecordAction(
            base::UserMetricsAction("IOS.Omnibox.Promo.Skipped.Startup"));
      break;
    }
    default:
      // Some events are not recorded in user-actions.
      break;
  }
}

void RecordSelectedPosition(
    ToolbarType toolbar_type,
    BOOL is_default,
    segmentation_platform::DeviceSwitcherResultDispatcher*
        device_switcher_result_dispatcher) {
  std::string histogram = "IOS.Omnibox.Promo.SelectedPosition.Startup";
  OmniboxPromoSelectedPosition position =
      OmniboxPromoSelectedPosition::kTopDefault;
  if (toolbar_type == ToolbarType::kPrimary) {
    position = is_default ? OmniboxPromoSelectedPosition::kTopDefault
                          : OmniboxPromoSelectedPosition::kTopNotDefault;
  } else {
    position = is_default ? OmniboxPromoSelectedPosition::kBottomDefault
                          : OmniboxPromoSelectedPosition::kBottomNotDefault;
  }
  base::UmaHistogramEnumeration(histogram, position);

  // Record selected position grouped by device switcher result.
  if (device_switcher_result_dispatcher) {
    segmentation_platform::ClassificationResult result =
        device_switcher_result_dispatcher->GetCachedClassificationResult();
    std::string histogram_with_classification =
        MetricNameForDeviceSwitcherResult(histogram, result);
    base::UmaHistogramEnumeration(histogram_with_classification, position);
  }
}

void RecordTimeOpen(base::TimeDelta elapsed) {
  std::string histogram = "IOS.Omnibox.Promo.TimeOpen.Startup";
  base::UmaHistogramMediumTimes(histogram, elapsed);
}

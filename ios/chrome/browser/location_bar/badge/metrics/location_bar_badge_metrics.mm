// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/metrics/location_bar_badge_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/stringprintf.h"
#import "base/time/time.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"

namespace {

// Which type of loud entrypoint was displayed to be used in metric names.
std::string LoudEntrypointTypeStringForMetrics(
    ContextualPanelTabHelper::EntrypointMetricsData& metricsData) {
  if (metricsData.iphWasShown) {
    return "IPH";
  } else if (metricsData.largeEntrypointWasShown) {
    return "Large";
  } else {
    return "";
  }
}

}  // namespace

@implementation LocationBarBadgeMetrics

+ (void)logFirstDisplayContextualPanelEntrypointMetrics:
    (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)
        optionalMetricsData {
  if (!optionalMetricsData ||
      optionalMetricsData->entrypoint_regular_display_metrics_fired) {
    return;
  }

  ContextualPanelTabHelper::EntrypointMetricsData& metricsData =
      optionalMetricsData.value();

  metricsData.entrypoint_regular_display_metrics_fired = true;

  base::UmaHistogramEnumeration("IOS.ContextualPanel.EntrypointDisplayed",
                                metricsData.entrypoint_item_type);

  std::string entrypointTypeHistogramName =
      "IOS.ContextualPanel.Entrypoint.Regular";
  base::UmaHistogramEnumeration(entrypointTypeHistogramName,
                                EntrypointInteractionType::Displayed);

  std::string blockTypeEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.Regular.%s",
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramEnumeration(blockTypeEntrypointTypeHistogramName,
                                EntrypointInteractionType::Displayed);
}

+ (void)logLoudDisplayContextualPanelEntrypointMetrics:
    (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)
        optionalMetricsData {
  if (!optionalMetricsData ||
      optionalMetricsData->entrypoint_loud_display_metrics_fired) {
    return;
  }

  ContextualPanelTabHelper::EntrypointMetricsData& metricsData =
      optionalMetricsData.value();

  std::string entrypointTypeString =
      LoudEntrypointTypeStringForMetrics(metricsData);

  // Either the IPH or Large entrypoint should have been shown by now.
  if (entrypointTypeString == "") {
    return;
  }

  metricsData.entrypoint_loud_display_metrics_fired = true;

  std::string entrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.%s", entrypointTypeString.c_str());
  base::UmaHistogramEnumeration(entrypointTypeHistogramName,
                                EntrypointInteractionType::Displayed);

  std::string blockTypeEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.%s.%s", entrypointTypeString.c_str(),
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramEnumeration(blockTypeEntrypointTypeHistogramName,
                                EntrypointInteractionType::Displayed);
}

+ (void)logFirstTapMetricsContextualPanelEntrypointMetrics:
    (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)
        optionalMetricsData {
  if (!optionalMetricsData ||
      optionalMetricsData->entrypoint_tap_metrics_fired) {
    return;
  }

  ContextualPanelTabHelper::EntrypointMetricsData& metricsData =
      optionalMetricsData.value();

  base::TimeDelta visibleTimeThisIteration =
      (metricsData.appearance_time)
          ? (base::Time::Now() - metricsData.appearance_time.value())
          : base::Seconds(0);
  base::TimeDelta visibleTime =
      metricsData.time_visible + visibleTimeThisIteration;

  metricsData.entrypoint_tap_metrics_fired = true;

  // Fire metrics saying the entrypoint was tapped.
  base::UmaHistogramEnumeration("IOS.ContextualPanel.EntrypointTapped",
                                metricsData.entrypoint_item_type);

  // Always fire the regular tap events because the regular display events are
  // also always fired.
  base::UmaHistogramEnumeration("IOS.ContextualPanel.Entrypoint.Regular",
                                EntrypointInteractionType::Tapped);

  std::string blockTypeEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.Regular.%s",
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramEnumeration(blockTypeEntrypointTypeHistogramName,
                                EntrypointInteractionType::Tapped);

  // Fire metrics for the time to tap.
  base::UmaHistogramTimes(
      "IOS.ContextualPanel.Entrypoint.Regular.UptimeBeforeTap", visibleTime);

  std::string blockTypeEntrypointTypeUptimeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.Regular.%s.UptimeBeforeTap",
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramTimes(blockTypeEntrypointTypeUptimeHistogramName,
                          visibleTime);

  // Additionally fire metrics for the loud entrypoint variant, if one was
  // shown.
  std::string entrypointTypeString =
      LoudEntrypointTypeStringForMetrics(metricsData);
  if (entrypointTypeString == "") {
    return;
  }
  std::string loudEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.%s", entrypointTypeString.c_str());
  base::UmaHistogramEnumeration(loudEntrypointTypeHistogramName,
                                EntrypointInteractionType::Tapped);

  std::string blockTypeLoudEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.%s.%s", entrypointTypeString.c_str(),
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramEnumeration(blockTypeLoudEntrypointTypeHistogramName,
                                EntrypointInteractionType::Tapped);

  // Time to tap metrics:
  std::string loudEntrypointTypeUptimeHistogramName =
      base::StringPrintf("IOS.ContextualPanel.Entrypoint.%s.UptimeBeforeTap",
                         entrypointTypeString.c_str());
  base::UmaHistogramTimes(loudEntrypointTypeUptimeHistogramName, visibleTime);

  std::string blockTypeLoudEntrypointTypeUptimeHistogramName =
      base::StringPrintf(
          "IOS.ContextualPanel.Entrypoint.%s.%s.UptimeBeforeTap",
          entrypointTypeString.c_str(),
          StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramTimes(blockTypeLoudEntrypointTypeUptimeHistogramName,
                          visibleTime);
}

+ (void)logContextualPanelEntrypointDismissMetrics:
    (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)
        metricsData {
  base::UmaHistogramEnumeration("IOS.ContextualPanel.DismissedReason",
                                ContextualPanelDismissedReason::UserDismissed);
}

@end

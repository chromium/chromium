// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_METRICS_LOCATION_BAR_BADGE_METRICS_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_METRICS_LOCATION_BAR_BADGE_METRICS_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

// Helper class for logging metrics related to the location bar badge.
@interface LocationBarBadgeMetrics : NSObject

// Logs metrics that should be fired when the entrypoint is displayed for the
// first time.
+ (void)logFirstDisplayContextualPanelEntrypointMetrics:
    (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)
        metricsData;

// Log any metrics that should be logged when a loud entrypoint is displayed.
+ (void)logLoudDisplayContextualPanelEntrypointMetrics:
    (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)
        metricsData;

// Logs any metrics fired the first time a given entrypoint is opened via
// tapping.
+ (void)logFirstTapMetricsContextualPanelEntrypointMetrics:
    (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)
        metricsData;

// Logs any metrics fired the first time a given entrypoint is opened via
// tapping.
+ (void)logContextualPanelEntrypointDismissMetrics:
    (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)
        metricsData;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_METRICS_LOCATION_BAR_BADGE_METRICS_H_

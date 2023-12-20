// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_UTIL_H_

#include "base/time/time.h"

namespace web_performance_metrics {

struct FirstContentfulPaint {
  // The time at which the frame started loading.
  double navigation_start_time;
  // The first contentful paint time relative to the frame's
  // navigation start time.
  double relative_time;
  // The sum of the frame's navigation start time and its
  // first contentful paint.
  double absolute_time;

  FirstContentfulPaint(double start_time = std::numeric_limits<double>::max(),
                       double r_time = std::numeric_limits<double>::max(),
                       double a_time = std::numeric_limits<double>::max())
      : navigation_start_time(start_time),
        relative_time(r_time),
        absolute_time(a_time) {}
};

// The function calculates the First Contentful Paint
// across main and subframes that
// occurred at the earliest point in time relative to the
// main frame's navigation start time.
base::TimeDelta CalculateAggregateFirstContentfulPaint(
    double aggregate_absolute_first_contentful_paint,
    FirstContentfulPaint main_frame);

// The function calculates the absolute first contentful paint time
// relative to the given frame's navigation start time.
double CalculateAbsoluteFirstContentfulPaint(
    double navigation_start_time,
    double relative_first_contentful_paint);

}  // namespace web_performance_metrics

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_UTIL_H_

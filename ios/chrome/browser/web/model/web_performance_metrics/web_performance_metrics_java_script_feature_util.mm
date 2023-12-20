// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"

#import <limits>

namespace web_performance_metrics {

base::TimeDelta CalculateAggregateFirstContentfulPaint(
    double aggregate_absolute_first_contentful_paint,
    web_performance_metrics::FirstContentfulPaint main_frame) {
  if (aggregate_absolute_first_contentful_paint < main_frame.absolute_time) {
    // Converts the aggregate absolute iframe's first contentful paint
    // time to a relative first contenful paint time with respect to
    // the main frame's navigation start time.
    return base::Milliseconds(aggregate_absolute_first_contentful_paint -
                              main_frame.navigation_start_time);
  }
  return base::Milliseconds(main_frame.relative_time);
}

double CalculateAbsoluteFirstContentfulPaint(
    double navigation_start_time,
    double relative_first_contentful_paint) {
  return navigation_start_time + relative_first_contentful_paint;
}

}

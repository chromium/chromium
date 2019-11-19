// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_HISTORY_NAVIGATION_ABLATION_STUDY_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_FRAME_HOST_HISTORY_NAVIGATION_ABLATION_STUDY_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// This navigation throttle delays a navigation for a certain duration
// when an experiment is enabled to allow us to run A/B ablation study
// and measure the impact on the top-level user engagement metrics.
// TODO(altimin, crbug.com/954271): Clean up after the end of the study.
class HistoryNavigationAblationStudyNavigationThrottle
    : public NavigationThrottle,
      public base::SupportsWeakPtr<
          HistoryNavigationAblationStudyNavigationThrottle> {
 public:
  explicit HistoryNavigationAblationStudyNavigationThrottle(
      NavigationHandle* navigation_handle);
  ~HistoryNavigationAblationStudyNavigationThrottle() override = default;

  // Create a NavigationThrottle if the relevant experiment is enabled.
  static std::unique_ptr<HistoryNavigationAblationStudyNavigationThrottle>
  MaybeCreateForNavigation(NavigationHandle* navigation_handle);

  // NavigationThrottle methods:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 private:
  const double probability_;
  const base::TimeDelta delay_;
  base::OneShotTimer delay_timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_HISTORY_NAVIGATION_ABLATION_STUDY_NAVIGATION_THROTTLE_H_

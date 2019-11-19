// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/history_navigation_ablation_study_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

constexpr base::Feature kDelayHistoryNavigationsAblationStudy{
    "DelayHistoryNavigationsAblationStudy", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<double> kProbability{
    &kDelayHistoryNavigationsAblationStudy, "delay_probability", 0.0};
constexpr base::FeatureParam<int> kDelay{&kDelayHistoryNavigationsAblationStudy,
                                         "delay_ms", 0};

constexpr base::TimeDelta kMaxDelay = base::TimeDelta::FromSeconds(15);

}  // namespace

HistoryNavigationAblationStudyNavigationThrottle::
    HistoryNavigationAblationStudyNavigationThrottle(
        NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle),
      probability_(kProbability.Get()),
      delay_(std::min(base::TimeDelta::FromMilliseconds(kDelay.Get()),
                      kMaxDelay)) {}

// static
std::unique_ptr<HistoryNavigationAblationStudyNavigationThrottle>
HistoryNavigationAblationStudyNavigationThrottle::MaybeCreateForNavigation(
    NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(kDelayHistoryNavigationsAblationStudy))
    return nullptr;
  bool is_history_navigation =
      navigation_handle->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK;
  if (!is_history_navigation)
    return nullptr;
  return std::make_unique<HistoryNavigationAblationStudyNavigationThrottle>(
      navigation_handle);
}

NavigationThrottle::ThrottleCheckResult
HistoryNavigationAblationStudyNavigationThrottle::WillStartRequest() {
  if (base::RandDouble() >= probability_)
    return PROCEED;
  delay_timer_.Start(
      FROM_HERE, delay_,
      base::BindOnce(&HistoryNavigationAblationStudyNavigationThrottle::Resume,
                     AsWeakPtr()));
  return DEFER;
}

const char*
HistoryNavigationAblationStudyNavigationThrottle::GetNameForLogging() {
  return "HistoryNavigationAblationStudyNavigationThrottle";
}

}  // namespace content

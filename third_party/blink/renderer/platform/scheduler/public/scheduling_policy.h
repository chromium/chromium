// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_SCHEDULING_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_SCHEDULING_POLICY_H_

#include "base/traits_bag.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// A list of things a feature can opt out from on the behalf of the page
// if the page is using this feature.
// See FrameOrWorkerScheduler::RegisterFeature.
struct PLATFORM_EXPORT SchedulingPolicy {
  using Feature = scheduler::WebSchedulerTrackedFeature;

  // Sticky features can't be unregistered and remain active for the rest
  // of the lifetime of the page.
  static bool IsFeatureSticky(Feature feature);

  // List of opt-outs which form a policy.
  struct DisableAggressiveThrottling {};
  struct RecordMetricsForBackForwardCache {};

  struct ValidPolicies {
    ValidPolicies(DisableAggressiveThrottling);
    ValidPolicies(RecordMetricsForBackForwardCache);
  };

  template <class... ArgTypes,
            class CheckArgumentsAreValid = std::enable_if_t<
                base::trait_helpers::AreValidTraits<ValidPolicies,
                                                    ArgTypes...>::value>>
  constexpr SchedulingPolicy(ArgTypes... args)
      : disable_aggressive_throttling(
            base::trait_helpers::HasTrait<DisableAggressiveThrottling,
                                          ArgTypes...>()),
        disable_back_forward_cache(
            base::trait_helpers::HasTrait<RecordMetricsForBackForwardCache,
                                          ArgTypes...>()) {}

  SchedulingPolicy() {}

  bool disable_aggressive_throttling = false;
  bool disable_back_forward_cache = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_SCHEDULING_POLICY_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER_H_

#include <stddef.h>

#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/safety_checks.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/renderer_host/navigation_throttle_registry_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// This is the original implementation of the NavigationThrottleRunner, and now
// the essential interfaces are defined in the NavigationThrottleRunnerBase to
// introduce an alternative runner, called NavigationThrottleRunner2, which will
// eventually replace this class. See https://crbug.com/422003056 for more
// information.
class CONTENT_EXPORT NavigationThrottleRunner
    : public NavigationThrottleRunnerBase {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  // `registry` should outlive this instance.
  NavigationThrottleRunner(NavigationThrottleRegistryBase* registry,
                           int64_t navigation_id,
                           bool is_primary_main_frame);

  NavigationThrottleRunner(const NavigationThrottleRunner&) = delete;
  NavigationThrottleRunner& operator=(const NavigationThrottleRunner&) = delete;

  ~NavigationThrottleRunner() override;

  // Implements NavigationThrottleRunnerBase:
  void ProcessNavigationEvent(NavigationThrottleEvent event) override;
  void ResumeProcessingNavigationEvent(
      NavigationThrottle* resuming_throttle) override;

 private:
  void ProcessInternal();
  void InformRegistry(const NavigationThrottle::ThrottleCheckResult& result);

  // Records UKM about the deferring throttle when the navigation is resumed.
  void RecordDeferTimeUKM();

  void ReportStuckThrottle();

  const raw_ref<NavigationThrottleRegistryBase> registry_;

  // The index of the next throttle to check.
  size_t next_index_;

  // The unique id of the navigation which this throttle runner is associated
  // with.
  const int64_t navigation_id_;

  // The time a throttle started deferring the navigation.
  base::Time defer_start_time_;

  // The total duration time that throttles deferred the navigation.
  base::TimeDelta total_defer_duration_time_;
  base::TimeDelta total_defer_duration_time_for_request_;

  // The time this runner started ProcessInternal() for the current_event_.
  // Should be reset when the processing is done.
  std::optional<base::Time> event_process_start_time_;

  // The accumulated time duration this runner took to execute throttles for the
  // current_event_.
  base::TimeDelta event_process_execution_time_;

  // The total count to know how many times a throttle defer the navigation.
  size_t defer_count_ = 0;
  size_t defer_count_for_request_ = 0;

  // The event currently being processed.
  NavigationThrottleEvent current_event_ = NavigationThrottleEvent::kNoEvent;

  // Whether the navigation is in the primary main frame.
  bool is_primary_main_frame_ = false;

  // A timer to detect throttles blocking the navigation for extra long time.
  std::unique_ptr<base::OneShotTimer> report_stuck_throttle_timer_;

  base::WeakPtrFactory<NavigationThrottleRunner> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER_H_

#include <stddef.h>

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/safety_checks.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/navigation_throttle_registry_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// This class collaborates with NavigationThrottleRegistry that owns the set of
// NavigationThrottles added to an underlying navigation, and is responsible for
// calling the various sets of events on its NavigationThrottles, and notifying
// its delegate of the results of said events.
class CONTENT_EXPORT NavigationThrottleRunner {
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

  ~NavigationThrottleRunner();

  // Will call the appropriate NavigationThrottle function based on |event| on
  // all NavigationThrottles owned by this NavigationThrottleRunner.
  void ProcessNavigationEvent(NavigationThrottleEvent event);

  // Resumes calling the appropriate NavigationThrottle functions for |event_|
  // on all NavigationThrottles that have not yet been notified.
  // |resuming_throttle| is the NavigationThrottle that asks for navigation
  // event processing to be resumed; it should be the one currently deferring
  // the navigation.
  void ResumeProcessingNavigationEvent(NavigationThrottle* resuming_throttle);

  // Simulates the navigation resuming. Most callers should just let the
  // deferring NavigationThrottle do the resuming.
  void CallResumeForTesting();

  // Returns the throttle that is currently deferring the navigation (i.e. the
  // throttle at index |next_index_ -1|). If the handle is not deferred, returns
  // nullptr;
  NavigationThrottle* GetDeferringThrottle() const;

  void set_first_deferral_callback_for_testing(base::OnceClosure callback) {
    first_deferral_callback_for_testing_ = std::move(callback);
  }

 private:
  void ProcessInternal();
  void InformRegistry(const NavigationThrottle::ThrottleCheckResult& result);

  // Records UKM about the deferring throttle when the navigation is resumed.
  void RecordDeferTimeUKM();

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

  // This test-only callback will be run the first time a NavigationThrottle
  // defers this navigation.
  base::OnceClosure first_deferral_callback_for_testing_;

  // The event currently being processed.
  NavigationThrottleEvent current_event_ =
      NavigationThrottleEvent::kNoEvent;

  // Whether the navigation is in the primary main frame.
  bool is_primary_main_frame_ = false;

  base::WeakPtrFactory<NavigationThrottleRunner> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER_H_

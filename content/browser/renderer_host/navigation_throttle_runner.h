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
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// This class owns the set of NavigationThrottles added to a NavigationHandle.
// It is responsible for calling the various sets of events on its
// NavigationThrottle, and notifying its delegate of the results of said events.
class CONTENT_EXPORT NavigationThrottleRunner {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  // The different event types that can be processed by NavigationThrottles.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // This type is also used in the UKM as set in the RecordDeferTimeUKM().
  //
  // LINT.IfChange(Event)
  enum class Event {
    kNoEvent = 0,
    kWillStartRequest = 1,
    kWillRedirectRequest = 2,
    kWillFailRequest = 3,
    kWillProcessResponse = 4,
    kWillCommitWithoutUrlLoader = 5,
    kMaxValue = kWillCommitWithoutUrlLoader,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:NavigationThrottleEvent)

  class Delegate {
   public:
    // Called when the NavigationThrottleRunner is done processing the
    // navigation event of type |event|. |result| is the final
    // NavigationThrottle::ThrottleCheckResult for this event.
    virtual void OnNavigationEventProcessed(
        Event event,
        NavigationThrottle::ThrottleCheckResult result) = 0;
  };

  NavigationThrottleRunner(Delegate* delegate,
                           int64_t navigation_id,
                           bool is_primary_main_frame);

  NavigationThrottleRunner(const NavigationThrottleRunner&) = delete;
  NavigationThrottleRunner& operator=(const NavigationThrottleRunner&) = delete;

  ~NavigationThrottleRunner();

  // Will call the appropriate NavigationThrottle function based on |event| on
  // all NavigationThrottles owned by this NavigationThrottleRunner.
  void ProcessNavigationEvent(Event event);

  // Resumes calling the appropriate NavigationThrottle functions for |event_|
  // on all NavigationThrottles that have not yet been notified.
  // |resuming_throttle| is the NavigationThrottle that asks for navigation
  // event processing to be resumed; it should be the one currently deferring
  // the navigation.
  void ResumeProcessingNavigationEvent(NavigationThrottle* resuming_throttle);

  // Simulates the navigation resuming. Most callers should just let the
  // deferring NavigationThrottle do the resuming.
  void CallResumeForTesting();

  // Registers the appropriate NavigationThrottles are added for a "standard"
  // navigation (i.e., one with a URLLoader that goes through the
  // WillSendRequest/WillProcessResponse callback sequence).
  void RegisterNavigationThrottles();

  // Registers the appropriate NavigationThrottles for a navigation that can
  // immediately commit because no URLLoader is required (about:blank,
  // about:srcdoc, and most same-document navigations).
  void RegisterNavigationThrottlesForCommitWithoutUrlLoader();

  // Returns the throttle that is currently deferring the navigation (i.e. the
  // throttle at index |next_index_ -1|). If the handle is not deferred, returns
  // nullptr;
  NavigationThrottle* GetDeferringThrottle() const;

  // Takes ownership of |navigation_throttle|. Following this call, any event
  // processed by the NavigationThrottleRunner will be called on
  // |navigation_throttle|.
  void AddThrottle(std::unique_ptr<NavigationThrottle> navigation_throttle);

  void set_first_deferral_callback_for_testing(base::OnceClosure callback) {
    first_deferral_callback_for_testing_ = std::move(callback);
  }

 private:
  void ProcessInternal();
  void InformDelegate(const NavigationThrottle::ThrottleCheckResult& result);

  // Records UKM about the deferring throttle when the navigation is resumed.
  void RecordDeferTimeUKM();

  const raw_ptr<Delegate> delegate_;

  // A list of Throttles registered for this navigation.
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_;

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
  Event current_event_ = Event::kNoEvent;

  // Whether the navigation is in the primary main frame.
  bool is_primary_main_frame_ = false;

  base::WeakPtrFactory<NavigationThrottleRunner> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_RUNNER_H_

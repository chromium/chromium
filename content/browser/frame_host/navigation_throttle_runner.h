// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_THROTTLE_RUNNER_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_THROTTLE_RUNNER_H_

#include <stddef.h>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// This class owns the set of NavigationThrottles added to a NavigationHandle.
// It is responsible for calling the various sets of events on its
// NavigationThrottle, and notifying its delegate of the results of said events.
class CONTENT_EXPORT NavigationThrottleRunner {
 public:
  // The different event types that can be processed by NavigationThrottles.
  enum class Event {
    WillStartRequest,
    WillRedirectRequest,
    WillFailRequest,
    WillProcessResponse,
    NoEvent,
  };

  class Delegate {
   public:
    // Called when the NavigationThrottleRunner is done processing the
    // navigation event of type |event|. |result| is the final
    // NavigationThrottle::ThrottleCheckResult for this event.
    virtual void OnNavigationEventProcessed(
        Event event,
        NavigationThrottle::ThrottleCheckResult result) = 0;
  };

  NavigationThrottleRunner(Delegate* delegate);
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

  void CallResumeForTesting();

  void RegisterNavigationThrottles();

  // Returns the throttle that is currently deferring the navigation (i.e. the
  // throttle at index |next_index_ -1|). If the handle is not deferred, returns
  // nullptr;
  NavigationThrottle* GetDeferringThrottle() const;

  // Takes ownership of |navigation_throttle|. Following this call, any event
  // processed by the NavigationThrottleRunner will be called on
  // |navigation_throttle|.
  void AddThrottle(std::unique_ptr<NavigationThrottle> navigation_throttle);

 private:
  void ProcessInternal();
  void InformDelegate(const NavigationThrottle::ThrottleCheckResult& result);

  Delegate* delegate_;

  // A list of Throttles registered for this navigation.
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_;

  // The index of the next throttle to check.
  size_t next_index_;

  // The event currently being processed.
  Event current_event_ = Event::NoEvent;
  base::WeakPtrFactory<NavigationThrottleRunner> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NavigationThrottleRunner);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_THROTTLE_RUNNER_H_

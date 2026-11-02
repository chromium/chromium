// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_METRICS_H_

#import <optional>

#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_controller.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"

namespace actor {

// Tracks and records state durations and outcomes across the
// `ObservationDelayController` state machine.
// Mirrors `chrome/browser/actor/tools/observation_delay_metrics.h`.
class ObservationDelayMetrics {
 public:
  ObservationDelayMetrics();
  ~ObservationDelayMetrics();

  ObservationDelayMetrics(const ObservationDelayMetrics&) = delete;
  ObservationDelayMetrics& operator=(const ObservationDelayMetrics&) = delete;

  // Records the start timestamp of the observation delay wait.
  void Start();

  // Called when the controller is about to transition from `current_state` to
  // `state`.
  void WillMoveToState(ObservationDelayController::State new_state,
                       ObservationDelayController::State current_state);

  // Called when `ObservationDelayController::DelayForLcp()` actively waits for
  // Largest Contentful Paint (LCP) on the target frame.
  void OnLcpDelayApplied();

 private:
  struct StateDuration {
    bool IsValid() const {
      return !start_time.is_null() && !end_time.is_null();
    }
    base::TimeTicks start_time;
    base::TimeTicks end_time;
  };

  // The time at which `ObservationDelayController::Wait` started.
  base::TimeTicks wait_start_time_;

  // The duration waiting for page stability.
  StateDuration wait_for_page_stability_;

  // The duration waiting for page load completion.
  StateDuration wait_for_load_completion_;

  // The duration waiting for Largest Contentful Paint (LCP).
  StateDuration delay_for_lcp_;

  // The duration waiting for Autofill predictions.
  StateDuration wait_for_autofill_predictions_;

  // Whether an active LCP delay was applied during `State::kDelayForLcp`.
  // Remains `std::nullopt` until `State::kDelayForLcp` is entered.
  std::optional<bool> lcp_delay_needed_;

  // Whether the observation delay completed due to timeout.
  bool did_timeout_ = false;

  // Whether the observation delay completed due to navigation.
  bool did_navigate_ = false;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_METRICS_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_metrics.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"

namespace actor {

ObservationDelayMetrics::ObservationDelayMetrics() = default;

ObservationDelayMetrics::~ObservationDelayMetrics() = default;

void ObservationDelayMetrics::Start() {
  wait_start_time_ = base::TimeTicks::Now();
}

void ObservationDelayMetrics::WillMoveToState(
    ObservationDelayController::State new_state,
    ObservationDelayController::State current_state) {
  const base::TimeTicks now = base::TimeTicks::Now();

  // End the previous state's timer.
  switch (current_state) {
    case ObservationDelayController::State::kWaitForPageStability:
      wait_for_page_stability_.end_time = now;
      break;
    case ObservationDelayController::State::kWaitForLoadCompletion:
      wait_for_load_completion_.end_time = now;
      break;
    case ObservationDelayController::State::kDelayForLcp:
      delay_for_lcp_.end_time = now;
      break;
    case ObservationDelayController::State::kWaitForAutofillPredictions:
      wait_for_autofill_predictions_.end_time = now;
      break;
    case ObservationDelayController::State::kInitial:
    case ObservationDelayController::State::kPageNavigated:
    case ObservationDelayController::State::kDidTimeout:
    case ObservationDelayController::State::kDone:
      break;
  }

  // Start the new state's timer or handle terminal states.
  switch (new_state) {
    case ObservationDelayController::State::kInitial:
      NOTREACHED();
    case ObservationDelayController::State::kWaitForPageStability:
      wait_for_page_stability_.start_time = now;
      break;
    case ObservationDelayController::State::kWaitForLoadCompletion:
      wait_for_load_completion_.start_time = now;
      break;
    case ObservationDelayController::State::kDelayForLcp:
      delay_for_lcp_.start_time = now;
      lcp_delay_needed_ = false;
      break;
    case ObservationDelayController::State::kWaitForAutofillPredictions:
      wait_for_autofill_predictions_.start_time = now;
      break;
    case ObservationDelayController::State::kDidTimeout:
      did_timeout_ = true;
      break;
    case ObservationDelayController::State::kPageNavigated:
      did_navigate_ = true;
      break;
    case ObservationDelayController::State::kDone:
      base::UmaHistogramBoolean(kIOSActorObservationDelayDidTimeoutMetricName,
                                did_timeout_);
      base::UmaHistogramBoolean(kIOSActorObservationDelayDidNavigateMetricName,
                                did_navigate_);

      if (!did_timeout_ && !did_navigate_) {
        CHECK(!wait_start_time_.is_null());
        base::UmaHistogramTimes(
            kIOSActorObservationDelayTotalWaitDurationMetricName,
            now - wait_start_time_);

        if (wait_for_page_stability_.IsValid()) {
          base::UmaHistogramTimes(
              kIOSActorObservationDelayStateDurationWaitForPageStabilityMetricName,
              wait_for_page_stability_.end_time -
                  wait_for_page_stability_.start_time);
        }
        if (wait_for_load_completion_.IsValid()) {
          base::UmaHistogramTimes(
              kIOSActorObservationDelayStateDurationWaitForLoadCompletionMetricName,
              wait_for_load_completion_.end_time -
                  wait_for_load_completion_.start_time);
        }
        if (lcp_delay_needed_.has_value()) {
          base::UmaHistogramBoolean(
              kIOSActorObservationDelayLcpDelayNeededMetricName,
              *lcp_delay_needed_);
          if (*lcp_delay_needed_ && delay_for_lcp_.IsValid()) {
            base::UmaHistogramTimes(
                kIOSActorObservationDelayStateDurationDelayForLcpMetricName,
                delay_for_lcp_.end_time - delay_for_lcp_.start_time);
          }
        }
        if (wait_for_autofill_predictions_.IsValid()) {
          base::UmaHistogramTimes(
              kIOSActorObservationDelayStateDurationWaitForAutofillPredictionsMetricName,
              wait_for_autofill_predictions_.end_time -
                  wait_for_autofill_predictions_.start_time);
        }
      }
      break;
  }
}

void ObservationDelayMetrics::OnLcpDelayApplied() {
  lcp_delay_needed_ = true;
}

}  // namespace actor

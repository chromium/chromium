// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading_attempt_impl.h"

#include "content/common/state_transitions.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

namespace {

void DCHECKTriggeringOutcomeTransitions(PreloadingTriggeringOutcome old_state,
                                        PreloadingTriggeringOutcome new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<StateTransitions<PreloadingTriggeringOutcome>>
      allowed_transitions(StateTransitions<PreloadingTriggeringOutcome>({
          {PreloadingTriggeringOutcome::kUnspecified,
           {PreloadingTriggeringOutcome::kNotTriggered,
            PreloadingTriggeringOutcome::kDuplicate,
            PreloadingTriggeringOutcome::kRunning,
            PreloadingTriggeringOutcome::kReady,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingTriggeringOutcome::kFailure}},

          {PreloadingTriggeringOutcome::kNotTriggered, {}},

          {PreloadingTriggeringOutcome::kDuplicate, {}},

          {PreloadingTriggeringOutcome::kRunning,
           {PreloadingTriggeringOutcome::kReady,
            PreloadingTriggeringOutcome::kFailure}},

          // It can be possible that the preloading attempt may end up failing
          // after being ready to use, for cases where we have to cancel the
          // attempt for performance and security reasons.
          {PreloadingTriggeringOutcome::kReady,
           {PreloadingTriggeringOutcome::kSuccess,
            PreloadingTriggeringOutcome::kFailure}},

          {PreloadingTriggeringOutcome::kSuccess, {}},

          {PreloadingTriggeringOutcome::kFailure, {}},
      }));
  DCHECK_STATE_TRANSITION(allowed_transitions,
                          /*old_state=*/old_state,
                          /*new_state=*/new_state);
#endif  // DCHECK_IS_ON()
}

}  // namespace

void PreloadingAttemptImpl::SetEligibility(PreloadingEligibility eligibility) {
  eligibility_ = eligibility;
}

void PreloadingAttemptImpl::SetTriggeringOutcome(
    PreloadingTriggeringOutcome triggering_outcome) {
  // Check that we do the correct transition before setting
  // `triggering_outcome_`.
  DCHECKTriggeringOutcomeTransitions(/*old_state=*/triggering_outcome_,
                                     /*new_state=*/triggering_outcome);
  triggering_outcome_ = triggering_outcome;
}

void PreloadingAttemptImpl::SetFailureReason(int64_t reason) {
  // Value of failure reason should be greater than zero, as zero in
  // FailureReason represents success.
  DCHECK_GT(reason, 0);
  failure_reason_ = reason;
}

PreloadingAttemptImpl::PreloadingAttemptImpl(
    PreloadingPredictor predictor,
    PreloadingType preloading_type,
    ukm::SourceId triggered_primary_page_source_id,
    base::RepeatingCallback<bool(const GURL&)> url_match_predicate)
    : predictor_type_(predictor),
      preloading_type_(preloading_type),
      triggered_primary_page_source_id_(triggered_primary_page_source_id),
      url_match_predicate_(std::move(url_match_predicate)) {}

PreloadingAttemptImpl::~PreloadingAttemptImpl() = default;

void PreloadingAttemptImpl::RecordPreloadingAttemptUKMs(
    ukm::SourceId navigated_page_source_id,
    const GURL& navigated_url) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();

  DCHECK(url_match_predicate_);
  // Use the predicate to match the URLs as the matching logic varies for each
  // predictor.
  bool accurate_triggering = url_match_predicate_.Run(navigated_url);

  // Ensure that when the `triggering_outcome_` is kSuccess, then the
  // accurate_triggering should be true.
  if (triggering_outcome_ == PreloadingTriggeringOutcome::kSuccess) {
    DCHECK(accurate_triggering)
        << "TriggeringOutcome set to kSuccess without correct prediction\n";
  }

  // Don't log when the source id is invalid.
  if (navigated_page_source_id != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Attempt(navigated_page_source_id)
        .SetPreloadingType(static_cast<int64_t>(preloading_type_))
        .SetPreloadingPredictor(static_cast<int64_t>(predictor_type_))
        .SetEligibility(static_cast<int64_t>(eligibility_))
        .SetTriggeringOutcome(static_cast<int64_t>(triggering_outcome_))
        .SetFailureReason(failure_reason_)
        .SetAccurateTriggering(accurate_triggering)
        .Record(ukm_recorder);
  }

  if (triggered_primary_page_source_id_ != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Attempt_PreviousPrimaryPage(
        triggered_primary_page_source_id_)
        .SetPreloadingType(static_cast<int64_t>(preloading_type_))
        .SetPreloadingPredictor(static_cast<int64_t>(predictor_type_))
        .SetEligibility(static_cast<int64_t>(eligibility_))
        .SetTriggeringOutcome(static_cast<int64_t>(triggering_outcome_))
        .SetFailureReason(failure_reason_)
        .SetAccurateTriggering(accurate_triggering)
        .Record(ukm_recorder);
  }
}

// Used for StateTransitions matching.
std::ostream& operator<<(std::ostream& os,
                         const PreloadingTriggeringOutcome& outcome) {
  switch (outcome) {
    case PreloadingTriggeringOutcome::kUnspecified:
      os << "Unspecified";
      break;
    case PreloadingTriggeringOutcome::kNotTriggered:
      os << "NotTriggered";
      break;
    case PreloadingTriggeringOutcome::kDuplicate:
      os << "Duplicate";
      break;
    case PreloadingTriggeringOutcome::kRunning:
      os << "Running";
      break;
    case PreloadingTriggeringOutcome::kReady:
      os << "Ready";
      break;
    case PreloadingTriggeringOutcome::kSuccess:
      os << "Success";
      break;
    case PreloadingTriggeringOutcome::kFailure:
      os << "Failure";
      break;
  }
  return os;
}

}  // namespace content

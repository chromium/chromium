// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_attempt_impl.h"

#include "base/state_transitions.h"
#include "content/public/browser/preloading.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

namespace {

void DCHECKTriggeringOutcomeTransitions(PreloadingTriggeringOutcome old_state,
                                        PreloadingTriggeringOutcome new_state) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<
      base::StateTransitions<PreloadingTriggeringOutcome>>
      allowed_transitions(base::StateTransitions<PreloadingTriggeringOutcome>({
          {PreloadingTriggeringOutcome::kUnspecified,
           {PreloadingTriggeringOutcome::kDuplicate,
            PreloadingTriggeringOutcome::kRunning,
            PreloadingTriggeringOutcome::kReady,
            PreloadingTriggeringOutcome::kSuccess,
            PreloadingTriggeringOutcome::kFailure,
            PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown,
            PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender,
            PreloadingTriggeringOutcome::kTriggeredButPending}},

          {PreloadingTriggeringOutcome::kDuplicate, {}},

          {PreloadingTriggeringOutcome::kRunning,
           {PreloadingTriggeringOutcome::kReady,
            PreloadingTriggeringOutcome::kFailure,
            PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender}},

          // It can be possible that the preloading attempt may end up failing
          // after being ready to use, for cases where we have to cancel the
          // attempt for performance and security reasons.
          {PreloadingTriggeringOutcome::kReady,
           {PreloadingTriggeringOutcome::kSuccess,
            PreloadingTriggeringOutcome::kFailure,
            PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender}},

          {PreloadingTriggeringOutcome::kSuccess, {}},

          {PreloadingTriggeringOutcome::kFailure, {}},

          {PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown, {}},

          {PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender,
           {PreloadingTriggeringOutcome::kFailure}},

          {PreloadingTriggeringOutcome::kTriggeredButPending,
           {PreloadingTriggeringOutcome::kRunning,
            PreloadingTriggeringOutcome::kFailure}},
      }));
  DCHECK_STATE_TRANSITION(allowed_transitions,
                          /*old_state=*/old_state,
                          /*new_state=*/new_state);
#endif  // DCHECK_IS_ON()
}

}  // namespace

void PreloadingAttemptImpl::SetEligibility(PreloadingEligibility eligibility) {
  // Ensure that eligiblity is only set once and that it's set before the
  // holdback status and the triggering outcome.
  DCHECK_EQ(eligibility_, PreloadingEligibility::kUnspecified);
  DCHECK_EQ(holdback_status_, PreloadingHoldbackStatus::kUnspecified);
  DCHECK_EQ(triggering_outcome_, PreloadingTriggeringOutcome::kUnspecified);
  DCHECK_NE(eligibility, PreloadingEligibility::kUnspecified);
  eligibility_ = eligibility;
}

void PreloadingAttemptImpl::SetHoldbackStatus(
    PreloadingHoldbackStatus holdback_status) {
  // Ensure that the holdback status is only set once and that it's set for
  // eligible attempts and before the triggering outcome.
  DCHECK_EQ(eligibility_, PreloadingEligibility::kEligible);
  DCHECK_EQ(holdback_status_, PreloadingHoldbackStatus::kUnspecified);
  DCHECK_EQ(triggering_outcome_, PreloadingTriggeringOutcome::kUnspecified);
  DCHECK_NE(holdback_status, PreloadingHoldbackStatus::kUnspecified);
  holdback_status_ = holdback_status;
}

void PreloadingAttemptImpl::SetTriggeringOutcome(
    PreloadingTriggeringOutcome triggering_outcome) {
  // Ensure that the triggering outcome is only set for eligible and
  // non-holdback attempts.
  DCHECK_EQ(eligibility_, PreloadingEligibility::kEligible);
  DCHECK_EQ(holdback_status_, PreloadingHoldbackStatus::kAllowed);
  // Check that we do the correct transition before setting
  // `triggering_outcome_`.
  DCHECKTriggeringOutcomeTransitions(/*old_state=*/triggering_outcome_,
                                     /*new_state=*/triggering_outcome);
  triggering_outcome_ = triggering_outcome;
}

void PreloadingAttemptImpl::SetFailureReason(PreloadingFailureReason reason) {
  // Ensure that the failure reason is only set once and is only set for
  // eligible and non-holdback attempts.
  DCHECK_EQ(eligibility_, PreloadingEligibility::kEligible);
  DCHECK_EQ(holdback_status_, PreloadingHoldbackStatus::kAllowed);
  DCHECK_EQ(failure_reason_, PreloadingFailureReason::kUnspecified);
  DCHECK_NE(reason, PreloadingFailureReason::kUnspecified);

  // It could be possible that the TriggeringOutcome is already kFailure, when
  // we try to set FailureReason after setting TriggeringOutcome to kFailure.
  if (triggering_outcome_ != PreloadingTriggeringOutcome::kFailure)
    SetTriggeringOutcome(PreloadingTriggeringOutcome::kFailure);
  failure_reason_ = reason;
}

base::WeakPtr<PreloadingAttempt> PreloadingAttemptImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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
    ukm::SourceId navigated_page_source_id) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();

  // Ensure that when the `triggering_outcome_` is kSuccess, then the
  // accurate_triggering should be true.
  if (triggering_outcome_ == PreloadingTriggeringOutcome::kSuccess) {
    DCHECK(is_accurate_triggering_)
        << "TriggeringOutcome set to kSuccess without correct prediction\n";
  }

  // Don't log when the source id is invalid.
  if (navigated_page_source_id != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Attempt(navigated_page_source_id)
        .SetPreloadingType(static_cast<int64_t>(preloading_type_))
        .SetPreloadingPredictor(static_cast<int64_t>(predictor_type_))
        .SetEligibility(static_cast<int64_t>(eligibility_))
        .SetHoldbackStatus(static_cast<int64_t>(holdback_status_))
        .SetTriggeringOutcome(static_cast<int64_t>(triggering_outcome_))
        .SetFailureReason(static_cast<int64_t>(failure_reason_))
        .SetAccurateTriggering(is_accurate_triggering_)
        .Record(ukm_recorder);
  }

  if (triggered_primary_page_source_id_ != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Attempt_PreviousPrimaryPage(
        triggered_primary_page_source_id_)
        .SetPreloadingType(static_cast<int64_t>(preloading_type_))
        .SetPreloadingPredictor(static_cast<int64_t>(predictor_type_))
        .SetEligibility(static_cast<int64_t>(eligibility_))
        .SetHoldbackStatus(static_cast<int64_t>(holdback_status_))
        .SetTriggeringOutcome(static_cast<int64_t>(triggering_outcome_))
        .SetFailureReason(static_cast<int64_t>(failure_reason_))
        .SetAccurateTriggering(is_accurate_triggering_)
        .Record(ukm_recorder);
  }
}

void PreloadingAttemptImpl::SetIsAccurateTriggering(const GURL& navigated_url) {
  DCHECK(url_match_predicate_);

  // Use the predicate to match the URLs as the matching logic varies for each
  // predictor.
  is_accurate_triggering_ |= url_match_predicate_.Run(navigated_url);
}

// Used for StateTransitions matching.
std::ostream& operator<<(std::ostream& os,
                         const PreloadingTriggeringOutcome& outcome) {
  switch (outcome) {
    case PreloadingTriggeringOutcome::kUnspecified:
      os << "Unspecified";
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
    case PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown:
      os << "TriggeredButOutcomeUnknown";
      break;
    case PreloadingTriggeringOutcome::kTriggeredButUpgradedToPrerender:
      os << "TriggeredButUpgradedToPrerender";
      break;
    case PreloadingTriggeringOutcome::kTriggeredButPending:
      os << "TriggeredButPending";
      break;
  }
  return os;
}

}  // namespace content

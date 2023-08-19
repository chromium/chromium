// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_ATTEMPT_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_ATTEMPT_IMPL_H_

#include "base/timer/elapsed_timer.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"
#include "url/gurl.h"

namespace content {

namespace test {
class PreloadingAttemptAccessor;
}

class CONTENT_EXPORT PreloadingAttemptImpl : public PreloadingAttempt {
 public:
  ~PreloadingAttemptImpl() override;

  // PreloadingAttempt implementation:
  void SetEligibility(PreloadingEligibility eligibility) override;
  void SetHoldbackStatus(PreloadingHoldbackStatus holdback_status) override;
  bool ShouldHoldback() override;
  void SetTriggeringOutcome(
      PreloadingTriggeringOutcome triggering_outcome) override;
  void SetFailureReason(PreloadingFailureReason reason) override;
  base::WeakPtr<PreloadingAttempt> GetWeakPtr() override;

  // Records UKMs for both Preloading_Attempt and
  // Preloading_Attempt_PreviousPrimaryPage. Metrics for both these are same.
  // Only difference is that the Preloading_Attempt_PreviousPrimaryPage UKM is
  // associated with the WebContents primary page that triggered the preloading
  // attempt. This is done to easily analyze the impact of the preloading
  // attempt on the primary visible page. Here `navigated_page` represent the
  // ukm::SourceId of the navigated page. If the navigation doesn't happen this
  // could be invalid. This must be called after the page load ends and we know
  // if the attempt was accurate.
  void RecordPreloadingAttemptMetrics(ukm::SourceId navigated_page);

  PreloadingType preloading_type() { return preloading_type_; }

  PreloadingPredictor predictor() { return predictor_type_; }

  ukm::SourceId triggered_primary_page_source_id() {
    return triggered_primary_page_source_id_;
  }

  // Sets `is_accurate_triggering_` to true if `navigated_url` matches the
  // predicate URL logic. It also records `time_to_next_navigation_`.
  void SetIsAccurateTriggering(const GURL& navigated_url);

  bool IsAccurateTriggering() const { return is_accurate_triggering_; }

  PreloadingAttemptImpl(PreloadingPredictor predictor,
                        PreloadingType preloading_type,
                        ukm::SourceId triggered_primary_page_source_id,
                        PreloadingURLMatchCallback url_match_predicate,
                        uint32_t sampling_seed);

  // Called by the `PreloadingDataImpl` that owns this attempt, to check the
  // validity of `predictor_type_`.
  PreloadingPredictor predictor_type() const { return predictor_type_; }

  PreloadingType preloading_type() const { return preloading_type_; }

  void SetSpeculationEagerness(blink::mojom::SpeculationEagerness eagerness);

 private:
  friend class test::PreloadingAttemptAccessor;

  void RecordPreloadingAttemptUMA();

  // Reason why the preloading attempt failed, this is similar to specific
  // preloading logging reason. Zero as a failure reason signifies no reason is
  // specified. This value is casted from preloading specific enum to int64_t
  // instead of having an enum declaration for each case.
  PreloadingFailureReason failure_reason_ =
      PreloadingFailureReason::kUnspecified;

  // Specifies the eligibility status for this PreloadingAttempt.
  PreloadingEligibility eligibility_ = PreloadingEligibility::kUnspecified;

  PreloadingHoldbackStatus holdback_status_ =
      PreloadingHoldbackStatus::kUnspecified;

  // Specifies the triggering outcome for this PreloadingAttempt.
  PreloadingTriggeringOutcome triggering_outcome_ =
      PreloadingTriggeringOutcome::kUnspecified;

  // Preloading predictor of this PreloadingAttempt.
  const PreloadingPredictor predictor_type_;

  // PreloadingType this attempt is associated with.
  const PreloadingType preloading_type_;

  // Holds the ukm::SourceId of the triggered primary page of this preloading
  // attempt.
  const ukm::SourceId triggered_primary_page_source_id_;

  // Triggers can specify their own predicate for judging whether two URLs are
  // considered as pointing to the same destination.
  const PreloadingURLMatchCallback url_match_predicate_;

  // Set to true if this PreloadingAttempt was used for the next navigation.
  bool is_accurate_triggering_ = false;

  // Records when the preloading attempt began, for computing times.
  const base::ElapsedTimer elapsed_timer_;

  // The time between the creation of the attempt and the start of the next
  // navigation, whether accurate or not. The latency is reported as standard
  // buckets, of 1.15 spacing.
  absl::optional<base::TimeDelta> time_to_next_navigation_;

  // Represents the duration between the attempt creation and its
  // `triggering_outcome_` becoming `kReady`. The latency is reported as
  // standard buckets, of 1.15 spacing.
  absl::optional<base::TimeDelta> ready_time_;

  // The random seed used to determine if a preloading attempt should be sampled
  // in UKM logs. We use a different random seed for each session (that is the
  // source of randomness for sampling) and then hash that seed with the UKM
  // source ID so that all attempts for a given source ID use the same random
  // value to determine sampling. This allows all PreloadingAttempt for a given
  // (preloading_type, predictor) in a page load to be sampled in or out
  // together.
  uint32_t sampling_seed_;

  // Eagerness of this preloading attempt (specified by a speculation rule).
  // This is only set for attempts that are triggered by speculation rules.
  absl::optional<blink::mojom::SpeculationEagerness> eagerness_ = absl::nullopt;

  base::WeakPtrFactory<PreloadingAttemptImpl> weak_factory_{this};
};

// Used when DCHECK_STATE_TRANSITION triggers.
CONTENT_EXPORT std::ostream& operator<<(std::ostream& o,
                                        const PreloadingTriggeringOutcome& s);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_ATTEMPT_IMPL_H_

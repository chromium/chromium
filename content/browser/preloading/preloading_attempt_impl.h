// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_ATTEMPT_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_ATTEMPT_IMPL_H_

#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"

#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace content {

class CONTENT_EXPORT PreloadingAttemptImpl : public PreloadingAttempt {
 public:
  ~PreloadingAttemptImpl() override;

  // PreloadingAttempt implementation:
  void SetEligibility(PreloadingEligibility eligibility) override;
  void SetHoldbackStatus(PreloadingHoldbackStatus holdback_status) override;
  void SetTriggeringOutcome(
      PreloadingTriggeringOutcome triggering_outcome) override;

  // Records both UKMs Preloading_Attempt and
  // Preloading_Attempt_PreviousPrimaryPage. Metrics for both these are same.
  // Only difference is that the Preloading_Attempt_PreviousPrimaryPage UKM is
  // associated with the WebContents primary page that triggered the preloading
  // attempt. This is done to easily analyze the impact of the preloading
  // attempt on the primary visible page. Here `navigated_page` and
  // `navigated_url` represent the ukm::SourceId and URL of the navigated page.
  // If the navigation doesn't happen these could be invalid/ empty.
  void RecordPreloadingAttemptUKMs(ukm::SourceId navigated_page,
                                   const GURL& navigated_url);

  // Sets the specific failure reason specific to the PreloadingType. This also
  // sets the PreloadingTriggeringOutcome to kFailure.
  void SetFailureReason(PreloadingFailureReason reason);

  explicit PreloadingAttemptImpl(
      PreloadingPredictor predictor,
      PreloadingType preloading_type,
      ukm::SourceId triggered_primary_page_source_id,
      PreloadingURLMatchCallback url_match_predicate);

 private:
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
};

// Used when DCHECK_STATE_TRANSITION triggers.
CONTENT_EXPORT std::ostream& operator<<(std::ostream& o,
                                        const PreloadingTriggeringOutcome& s);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_ATTEMPT_IMPL_H_

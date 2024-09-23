// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRELOADING_DATA_H_
#define CONTENT_PUBLIC_BROWSER_PRELOADING_DATA_H_

#include <optional>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/preloading.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace content {

class WebContents;
class NavigationHandle;
using PreloadingURLMatchCallback = base::RepeatingCallback<bool(const GURL&)>;
using PredictorDomainCallback =
    base::RepeatingCallback<bool(NavigationHandle*)>;

// PreloadingPrediction and PreloadingAttempt are the preloading logging APIs
// which allows us to set various metrics and log the values.

// All these metrics are logged into the UKM after the page navigates or when
// the WebContents is being destroyed. This API will be used by features both
// inside and outside //content.

// Both PreloadingPrediction and PreloadingAttempt are owned by PreloadingData
// class and that is associated with WebContentsUserData. PreloadingAttempt is
// cleared when either
// - A WebContents is deleted/ destroyed.
// - Primary page of the WebContents changes.

// PreloadingAttempt keeps track of every preloading attempt associated with
// various preloading features defined in preloading.h (please see the comments
// there for more details); whether it is eligible, whether it is triggered or
// not, specifies the failure reason on failing and others.
class CONTENT_EXPORT PreloadingAttempt {
 public:
  // Sets whether preloading is eligible to be triggered. This should only be
  // called once per preloading attempt.
  virtual void SetEligibility(PreloadingEligibility eligibility) = 0;

  // Whether this preloading attempt should be held back in order to
  // counterfactually evaluate how well this type of preloading is performing.
  // This is controlled via field trial configuration. Users of preloading
  // attempt should always call this before triggering an eligible preloading
  // attempt. Note that individual features can also use their own field
  // trial-controlled holdbacks, in which case preloading should be disabled if
  // either the feature's holdback is enabled or if ShouldHoldback returns true.
  // Features implementing their own holdback to should record held back
  // preloading attempts by calling SetHoldbackStatus (declared below).
  virtual bool ShouldHoldback() = 0;

  // Sets the outcome of the holdback check used to implement counterfactual
  // experiments. This is not part of eligibility status to clarify that this
  // check needs to happen after we are done verifying the eligibility of a
  // preloading attempt. In general, eligibility checks can be reordered, but
  // the holdback check always needs to come after verifying that the preloading
  // attempt was eligible. This must only be called after calling
  // SetEligibility(kEligible) and should not be called more than once.
  virtual void SetHoldbackStatus(PreloadingHoldbackStatus holdback_status) = 0;

  // Updates the preload outcome after it was triggered. This should only be
  // called for eligible attempts with a kAllowed holdback status.
  // - Initially set to kUnspecified.
  // - After triggering this if there is already a preloading attempt available
  // for the same URL we set to kDuplicate, or
  // - kRunning (for preloading methods with given enough time, we expect to
  //   update with kReady/ kSuccess/ kFailure).
  virtual void SetTriggeringOutcome(
      PreloadingTriggeringOutcome triggering_outcome) = 0;

  // Sets the specific failure reason specific to the PreloadingType. This also
  // sets the PreloadingTriggeringOutcome to kFailure.
  virtual void SetFailureReason(PreloadingFailureReason failure_reason) = 0;

  virtual base::WeakPtr<PreloadingAttempt> GetWeakPtr() = 0;

 protected:
  virtual ~PreloadingAttempt() = default;
};

// PreloadingData holds the data associated with all the PreloadingAttempts
// and PreloadingPredictions. This class is responsible for notifying all the
// PreloadingAttempts and PreloadingPredictions about logging the UKMs and
// maintaining its lifetime.

// Lifetime of PreloadingData is associated with WebContentsUserData.
class CONTENT_EXPORT PreloadingData {
 public:
  // This static function is implemented in PreloadingDataImpl.
  // Please see content/browser/preloading/preloading_data_impl.cc for more
  // details.
  static PreloadingData* GetOrCreateForWebContents(WebContents* web_contents);
  static PreloadingData* GetForWebContents(WebContents* web_contents);

  // Helper method to return the PreloadingURLMatchCallback for
  // `destination_url`. This method will return true only for exact matches to
  // `destination_url`.
  static PreloadingURLMatchCallback GetSameURLMatcher(
      const GURL& destination_url);

  // Creates a new PreloadingAttempt and returns a pointer associated with the
  // PreloadingAttempt class. Here callers pass the `url_predicate_callback` to
  // verify if the navigated and triggered URLs match based on callers logic.
  //
  // A caller can pass `planned_max_preloading_type` (defaults to
  // `preloading_type`) different from `preloading_type` if the caller expects
  // this preloading attempt will be consumed by other preloadings. For
  // example, a caller can start from triggering prefetch and then proceed
  // with triggering prerender. `preloading_type` must be upgradable to
  // `planned_max_preloading_type`. See `IsPreloadingTypeUpgradableTo()` in
  // //content/browser/preloading/preloading_attempt_impl.cc.
  //
  // `triggering_primary_page_source_id` is a UKM source ID of the page that
  // triggered preloading. This is used for recording the metrics for user
  // visible primary pages (Preloading_Attempt_PreviousPrimaryPage) to measure
  // the impact of PreloadingAttempt on the page user is viewing.
  // TODO(crbug.com/40227283): Extend this for non-primary page and inner
  // WebContents preloading attempts.
  virtual PreloadingAttempt* AddPreloadingAttempt(
      PreloadingPredictor predictor,
      PreloadingType preloading_type,
      PreloadingURLMatchCallback url_match_predicate,
      std::optional<PreloadingType> planned_max_preloading_type,
      ukm::SourceId triggering_primary_page_source_id) = 0;

  // Creates a new PreloadingPrediction. Same as above `url_predicate_callback`
  // is passed by the caller to verify that both predicted and navigated URLs
  // match. `confidence` signifies the confidence percentage of correct
  // predictor's preloading prediction.
  //
  // `triggering_primary_page_source_id` is a UKM source ID of the page that
  // triggered preloading. This is used for recording the metrics for user
  // visible primary pages (Preloading_Prediction_PreviousPrimaryPage) to
  // measure the impact of PreloadingPrediction on the page user is viewing.
  virtual void AddPreloadingPrediction(
      PreloadingPredictor predictor,
      int confidence,
      PreloadingURLMatchCallback url_match_predicate,
      ukm::SourceId triggering_primary_page_source_id) = 0;

  // To calculate the recall score of the `predictor`, we need to know if the
  // `predictor` is potentially responsible for predicting the next navigation
  // or not. Here, if caller provided `is_navigation_in_domain_callback`
  // callback returns true for the navigation, it will be considered in the
  // predictor's domain. The predictor's domain is the set of navigations a
  // predictor is meant to handle (predict). For example, for Omnibox DUI, this
  // is the set of navigations to history URLs started from the Omnibox
  // (navigations where the user ends up choosing a Search query or where the
  // user closes the Omnibox and clicks on a link in the page are not part of
  // the domain of the Omnibox DUI predictor).
  virtual void SetIsNavigationInDomainCallback(
      PreloadingPredictor predictor,
      PredictorDomainCallback is_navigation_in_domain_callback) = 0;

  // This flag will be true if there's been at least 1 attempt to do a
  // speculation-rules based prerender.
  virtual bool HasSpeculationRulesPrerender() = 0;

  // Called when the embedder is making a prediction with an ML model about
  // whether a navigation to `url` will occur. The provided callback will be
  // invoked on navigation with the result of whether the prediction would be
  // accurate. If downsampling occurs, some callbacks may not be invoked, and
  // the ones that are invoked will have the amount of sampling indicated in the
  // `sampling_likelihood`.
  virtual void OnPreloadingHeuristicsModelInput(
      const GURL& url,
      base::OnceCallback<void(std::optional<double> sampling_likelihood,
                              bool is_accurate_prediction)>
          on_record_outcome) = 0;

 protected:
  virtual ~PreloadingData() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRELOADING_DATA_H_

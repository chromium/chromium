// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRELOADING_DATA_H_
#define CONTENT_PUBLIC_BROWSER_PRELOADING_DATA_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/preloading.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace content {

class WebContents;
using PreloadingURLMatchCallback = base::RepeatingCallback<bool(const GURL&)>;

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

  // Helper method to return the PreloadingURLMatchCallback for
  // `destination_url`. This method will return true only for exact matches to
  // `destination_url`.
  static PreloadingURLMatchCallback GetSameURLMatcher(
      const GURL& destination_url);

  // Creates a new PreloadingAttempt and returns a pointer associated with the
  // PreloadingAttempt class. Here callers pass the `url_predicate_callback` to
  // verify if the navigated and triggered URLs match based on callers logic.
  virtual PreloadingAttempt* AddPreloadingAttempt(
      PreloadingPredictor predictor,
      PreloadingType preloading_type,
      PreloadingURLMatchCallback url_match_predicate) = 0;

  // Creates a new PreloadingPrediction. Same as above `url_predicate_callback`
  // is passed by the caller to verify that both predicted and navigated URLs
  // match. `confidence` signifies the confidence percentage of correct
  // predictor's preloading prediction.
  virtual void AddPreloadingPrediction(
      PreloadingPredictor predictor,
      int64_t confidence,
      PreloadingURLMatchCallback url_match_predicate) = 0;

 protected:
  virtual ~PreloadingData() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRELOADING_DATA_H_

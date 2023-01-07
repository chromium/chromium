// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_PREDICTION_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_PREDICTION_H_

#include "content/public/browser/preloading_data.h"

#include "base/callback.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

// PreloadingPrediction keeps track of every preloading prediction associated
// with various predictors as defined in content/public/preloading.h
// (please see for more details); whether the prediction is accurate or not;
// whether the prediction is confident enough or not.
class PreloadingPrediction {
 public:
  ~PreloadingPrediction();

  // Disallow copy and assign.
  PreloadingPrediction(const PreloadingPrediction& other) = delete;
  PreloadingPrediction& operator=(const PreloadingPrediction& other) = delete;

  // Records both UKMs Preloading_Prediction and
  // Preloading_Prediction_PreviousPrimaryPage. Metrics for both these are same.
  // Only difference is that the Preloading_Prediction_PreviousPrimaryPage UKM
  // is associated with the WebContents primary page that triggered the
  // preloading prediction. This is done to easily analyze the impact of the
  // preloading prediction on the primary visible page.
  void RecordPreloadingPredictionUKMs(ukm::SourceId navigated_page_source_id);

  // Sets `is_accurate_prediction_` to true if `navigated_url` matches the URL
  // predicate.
  void SetIsAccuratePrediction(const GURL& navigated_url);

  explicit PreloadingPrediction(
      PreloadingPredictor predictor,
      double confidence,
      ukm::SourceId triggered_primary_page_source_id,
      base::RepeatingCallback<bool(const GURL&)> url_match_predicate);

 private:
  // Preloading predictor of this preloading prediction.
  const PreloadingPredictor predictor_type_;

  // Confidence percentage of predictor's preloading prediction. This value
  // should be between 0 - 100.
  const int64_t confidence_;

  // Holds the triggered primary page of preloading operation ukm::SourceId.
  const ukm::SourceId triggered_primary_page_source_id_;

  // Triggers can specify their own predicate to judge whether two URLs are
  // considered as pointing to the same destination as this varies for different
  // predictors.
  const PreloadingURLMatchCallback url_match_predicate_;

  // Set to true when preloading prediction was correct i.e., when the
  // navigation happens to the same predicted URL.
  bool is_accurate_prediction_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_PREDICTION_H_

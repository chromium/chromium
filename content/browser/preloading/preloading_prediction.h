// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_PREDICTION_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_PREDICTION_H_

#include <optional>
#include <string_view>

#include "base/timer/elapsed_timer.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/public/browser/preloading_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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
  PreloadingPrediction(PreloadingPrediction&&);
  PreloadingPrediction& operator=(PreloadingPrediction&&);

  // Records both UKMs Preloading_Prediction and
  // Preloading_Prediction_PreviousPrimaryPage. Metrics for both these are same.
  // Only difference is that the Preloading_Prediction_PreviousPrimaryPage UKM
  // is associated with the WebContents primary page that triggered the
  // preloading prediction. This is done to easily analyze the impact of the
  // preloading prediction on the primary visible page.
  void RecordPreloadingPredictionUKMs(
      ukm::SourceId navigated_page_source_id,
      std::optional<double> sampling_likelihood);

  // Sets `is_accurate_prediction_` to true if `navigated_url` matches the URL
  // predicate. It also records `time_to_next_navigation_`.
  void SetIsAccuratePrediction(const GURL& navigated_url);

  bool IsAccuratePrediction() const { return is_accurate_prediction_; }

  PreloadingPrediction(
      PreloadingPredictor predictor,
      PreloadingConfidence confidence,
      ukm::SourceId triggered_primary_page_source_id,
      base::RepeatingCallback<bool(const GURL&)> url_match_predicate);

  // Called by the `PreloadingDataImpl` that owns this prediction, to check the
  // validity of `predictor_type_`.
  PreloadingPredictor predictor_type() const { return predictor_type_; }

 private:
  // Preloading predictor of this preloading prediction.
  PreloadingPredictor predictor_type_;

  // Holds the triggered primary page of preloading operation ukm::SourceId.
  ukm::SourceId triggered_primary_page_source_id_;

  // Triggers can specify their own predicate to judge whether two URLs are
  // considered as pointing to the same destination as this varies for different
  // predictors.
  PreloadingURLMatchCallback url_match_predicate_;

  // Confidence percentage of predictor's preloading prediction. This value
  // should be between 0 - 100.
  PreloadingConfidence confidence_;

  // Set to true when preloading prediction was correct i.e., when the
  // navigation happens to the same predicted URL.
  bool is_accurate_prediction_ = false;

  // Records when the preloading prediction was first recorded.
  base::ElapsedTimer elapsed_timer_;

  // The time between the creation of the prediction and the start of the next
  // navigation, whether accurate or not. The latency is reported as standard
  // buckets, of 1.15 spacing.
  std::optional<base::TimeDelta> time_to_next_navigation_;
};

// The output of many predictors is a logit/probability score. To use this score
// for binary classification, we compare it to a threshold. If the score is
// above the threshold, we classify the instance as positive; otherwise, we
// classify it as negative. Threshold choice affects classifier precision and
// recall. There is a trade-off between precision and recall. If we set the
// threshold too low, we will have high precision but low recall. If we set the
// threshold too high, we will have high recall but low precision. To choose the
// best threshold, we can use ROC curves, precision-recall curves, or
// logit-precision and logit-recall curves. `ExperimentalPreloadingPrediction`
// helps us collect the UMA data required to achieve this.
class ExperimentalPreloadingPrediction {
 public:
  ExperimentalPreloadingPrediction() = delete;
  ExperimentalPreloadingPrediction(
      std::string_view name,
      PreloadingURLMatchCallback url_match_predicate,
      float score,
      float min_score,
      float max_score,
      size_t buckets);
  ~ExperimentalPreloadingPrediction();

  ExperimentalPreloadingPrediction(
      const ExperimentalPreloadingPrediction& other) = delete;
  ExperimentalPreloadingPrediction& operator=(
      const ExperimentalPreloadingPrediction& other) = delete;
  ExperimentalPreloadingPrediction(ExperimentalPreloadingPrediction&&);
  ExperimentalPreloadingPrediction& operator=(
      ExperimentalPreloadingPrediction&&);

  std::string_view PredictorName() const { return name_; }
  bool IsAccuratePrediction() const { return is_accurate_prediction_; }

  void SetIsAccuratePrediction(const GURL& navigated_url);
  void RecordToUMA() const;

 private:
  // Experimental predictor's name
  std::string_view name_;
  // Set to true when preloading prediction was correct i.e., when the
  // navigation happens to the same predicted URL.
  bool is_accurate_prediction_ = false;
  // The number of buckets that will be used for UMA aggregation. It must be
  // less than 101.
  uint8_t buckets_;
  // The logit or probability score output of the predictor model.
  // Normalized based on the min and max score values.
  float normalized_score_;
  // The callback to verify that the navigated URL is a match.
  PreloadingURLMatchCallback url_match_predicate_;
};

// Stores data relating to a prediction made by the preloading ML model. Once
// the outcome of whether the prediction is accurate is known, the provided
// callback is invoked.
class ModelPredictionTrainingData {
 public:
  using OutcomeCallback =
      base::OnceCallback<void(std::optional<double> sampling_likelihood,
                              bool is_accurate_prediction)>;

  ModelPredictionTrainingData(OutcomeCallback on_record_outcome,
                              PreloadingURLMatchCallback url_match_predicate);

  ~ModelPredictionTrainingData();
  ModelPredictionTrainingData(const ModelPredictionTrainingData&) = delete;
  ModelPredictionTrainingData& operator=(const ModelPredictionTrainingData&) =
      delete;
  ModelPredictionTrainingData(ModelPredictionTrainingData&&);
  ModelPredictionTrainingData& operator=(ModelPredictionTrainingData&&);

  void SetIsAccuratePrediction(const GURL& navigated_url);
  void Record(std::optional<double> sampling_likelihood);

 private:
  OutcomeCallback on_record_outcome_;
  PreloadingURLMatchCallback url_match_predicate_;
  bool is_accurate_prediction_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_PREDICTION_H_

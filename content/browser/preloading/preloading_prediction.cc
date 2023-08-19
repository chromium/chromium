// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_prediction.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "content/public/browser/page.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

PreloadingPrediction::PreloadingPrediction(
    PreloadingPredictor predictor,
    double confidence,
    ukm::SourceId triggered_primary_page_source_id,
    PreloadingURLMatchCallback url_match_predicate)
    : predictor_type_(predictor),
      confidence_(confidence),
      triggered_primary_page_source_id_(triggered_primary_page_source_id),
      url_match_predicate_(std::move(url_match_predicate)) {}

PreloadingPrediction::~PreloadingPrediction() = default;

void PreloadingPrediction::RecordPreloadingPredictionUKMs(
    ukm::SourceId navigated_page_source_id) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();

  // Don't log when the source id is invalid.
  if (navigated_page_source_id != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Prediction builder(navigated_page_source_id);
    builder.SetPreloadingPredictor(predictor_type_.ukm_value())
        .SetConfidence(confidence_)
        .SetAccuratePrediction(is_accurate_prediction_);
    if (time_to_next_navigation_) {
      builder.SetTimeToNextNavigation(ukm::GetExponentialBucketMinForCounts1000(
          time_to_next_navigation_->InMilliseconds()));
    }
    builder.Record(ukm_recorder);
  }

  if (triggered_primary_page_source_id_ != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Prediction_PreviousPrimaryPage builder(
        triggered_primary_page_source_id_);
    builder.SetPreloadingPredictor(predictor_type_.ukm_value())
        .SetConfidence(confidence_)
        .SetAccuratePrediction(is_accurate_prediction_);
    if (time_to_next_navigation_) {
      builder.SetTimeToNextNavigation(ukm::GetExponentialBucketMinForCounts1000(
          time_to_next_navigation_->InMilliseconds()));
    }
    builder.Record(ukm_recorder);
  }
}

void PreloadingPrediction::SetIsAccuratePrediction(const GURL& navigated_url) {
  DCHECK(url_match_predicate_);

  // `PreloadingAttemptImpl::SetIsAccurateTriggering` is called during
  // `WCO::DidStartNavigation`.
  if (!time_to_next_navigation_) {
    time_to_next_navigation_ = elapsed_timer_.Elapsed();
  }

  // Use the predicate to match the URLs as the matching logic varies for each
  // predictor.
  is_accurate_prediction_ |= url_match_predicate_.Run(navigated_url);
}

ExperimentalPreloadingPrediction::ExperimentalPreloadingPrediction(
    base::StringPiece name,
    PreloadingURLMatchCallback url_match_predicate,
    float score,
    float min_score,
    float max_score,
    size_t buckets)
    : name_(name),
      score_(score),
      min_score_(min_score),
      max_score_(max_score),
      buckets_(buckets),
      url_match_predicate_(std::move(url_match_predicate)) {
  CHECK(max_score > min_score);
}

void ExperimentalPreloadingPrediction::SetIsAccuratePrediction(
    const GURL& navigated_url) {
  is_accurate_prediction_ = url_match_predicate_.Run(navigated_url);
}

ExperimentalPreloadingPrediction::~ExperimentalPreloadingPrediction() = default;

void ExperimentalPreloadingPrediction::RecordToUMA() const {
  const auto uma_experimental_prediction =
      base::StrCat({"Preloading.Experimental.", PredictorName(), ".",
                    IsAccuratePrediction() ? "Positive" : "Negative"});
  float normalized_param = (Score() - min_score_) / (max_score_ - min_score_);
  base::UmaHistogramExactLinear(uma_experimental_prediction,
                                normalized_param * buckets_, buckets_ + 1);
}

}  // namespace content

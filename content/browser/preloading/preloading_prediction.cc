// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_prediction.h"

#include "content/public/browser/page.h"
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
    ukm::builders::Preloading_Prediction(navigated_page_source_id)
        .SetPreloadingPredictor(static_cast<int64_t>(predictor_type_))
        .SetConfidence(confidence_)
        .SetAccuratePrediction(is_accurate_prediction_)
        .Record(ukm_recorder);
  }

  if (triggered_primary_page_source_id_ != ukm::kInvalidSourceId) {
    ukm::builders::Preloading_Prediction_PreviousPrimaryPage(
        triggered_primary_page_source_id_)
        .SetPreloadingPredictor(static_cast<int64_t>(predictor_type_))
        .SetConfidence(confidence_)
        .SetAccuratePrediction(is_accurate_prediction_)
        .Record(ukm_recorder);
  }
}

void PreloadingPrediction::SetIsAccuratePrediction(const GURL& navigated_url) {
  DCHECK(url_match_predicate_);

  // Use the predicate to match the URLs as the matching logic varies for each
  // predictor.
  is_accurate_prediction_ |= url_match_predicate_.Run(navigated_url);
}

}  // namespace content

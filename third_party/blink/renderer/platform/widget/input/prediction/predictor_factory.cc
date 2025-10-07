// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/prediction/predictor_factory.h"

#include "third_party/blink/public/common/features.h"
#include "ui/base/prediction/empty_predictor.h"
#include "ui/base/prediction/kalman_predictor.h"
#include "ui/base/prediction/least_squares_predictor.h"
#include "ui/base/prediction/linear_predictor.h"
#include "ui/base/prediction/linear_resampling.h"
#include "ui/base/ui_base_features.h"

namespace blink {

namespace {
using input_prediction::PredictorType;
}

PredictorType PredictorFactory::GetPredictorTypeFromName(
    const std::string& predictor_name) {
  if (predictor_name == ::features::kPredictorNameLinearResampling) {
    return PredictorType::kScrollPredictorTypeLinearResampling;
  } else if (predictor_name == ::features::kPredictorNameLsq) {
    return PredictorType::kScrollPredictorTypeLsq;
  } else if (predictor_name == ::features::kPredictorNameKalman) {
    return PredictorType::kScrollPredictorTypeKalman;
  } else if (predictor_name == ::features::kPredictorNameLinearFirst) {
    return PredictorType::kScrollPredictorTypeLinearFirst;
  } else if (predictor_name == ::features::kPredictorNameLinearSecond) {
    return PredictorType::kScrollPredictorTypeLinearSecond;
  } else {
    return PredictorType::kScrollPredictorTypeEmpty;
  }
}

std::unique_ptr<ui::InputPredictor> PredictorFactory::GetPredictor(
    PredictorType predictor_type) {
  if (predictor_type == PredictorType::kScrollPredictorTypeLinearResampling) {
    return std::make_unique<ui::LinearResampling>();
  } else if (predictor_type == PredictorType::kScrollPredictorTypeLsq) {
    return std::make_unique<ui::LeastSquaresPredictor>();
  } else if (predictor_type == PredictorType::kScrollPredictorTypeKalman) {
    return std::make_unique<ui::KalmanPredictor>();
  } else if (predictor_type == PredictorType::kScrollPredictorTypeLinearFirst) {
    return std::make_unique<ui::LinearPredictor>(
        ui::LinearPredictor::EquationOrder::kFirstOrder);
  } else if (predictor_type ==
             PredictorType::kScrollPredictorTypeLinearSecond) {
    return std::make_unique<ui::LinearPredictor>(
        ui::LinearPredictor::EquationOrder::kSecondOrder);
  } else {
    return std::make_unique<ui::EmptyPredictor>();
  }
}

}  // namespace blink

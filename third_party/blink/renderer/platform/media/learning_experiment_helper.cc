// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/learning_experiment_helper.h"

namespace blink {

using ::media::learning::FeatureDictionary;
using ::media::learning::FeatureVector;
using ::media::learning::LearningTaskController;
using ::media::learning::TargetValue;

LearningExperimentHelper::LearningExperimentHelper(
    std::unique_ptr<LearningTaskController> controller)
    : controller_(std::move(controller)) {}

LearningExperimentHelper::~LearningExperimentHelper() {
  CancelObservationIfNeeded();
}

void LearningExperimentHelper::BeginObservation(
    const FeatureDictionary& dictionary) {
  if (!controller_)
    return;

  CancelObservationIfNeeded();

  // Get the features that our task needs.
  FeatureVector features;
  dictionary.Lookup(controller_->GetLearningTask(), &features);

  observation_id_ = base::UnguessableToken::Create();
  controller_->BeginObservation(observation_id_, features);
}

void LearningExperimentHelper::CompleteObservationIfNeeded(
    const TargetValue& target) {
  if (!observation_id_)
    return;

  controller_->CompleteObservation(observation_id_, target);
  observation_id_ = base::UnguessableToken::Null();
}

void LearningExperimentHelper::CancelObservationIfNeeded() {
  if (!observation_id_)
    return;

  controller_->CancelObservation(observation_id_);
  observation_id_ = base::UnguessableToken::Null();
}

}  // namespace blink

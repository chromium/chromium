// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/mojo/public/cpp/mojo_learning_task_controller.h"

#include <utility>

namespace media {
namespace learning {

MojoLearningTaskController::MojoLearningTaskController(
    const LearningTask& task,
    mojo::Remote<mojom::LearningTaskController> controller)
    : task_(task), controller_(std::move(controller)) {}

MojoLearningTaskController::~MojoLearningTaskController() = default;

void MojoLearningTaskController::BeginObservation(
    base::UnguessableToken id,
    const FeatureVector& features,
    const std::optional<TargetValue>& default_target,
    const std::optional<ukm::SourceId>& source_id) {
  // We don't need to keep track of in-flight observations, since the service
  // side handles it for us.  Also note that |source_id| is ignored; the service
  // has no reason to trust it.  It will fill it in for us.  DCHECK in case
  // somebody actually tries to send us one, expecting it to be used.
  DCHECK(!source_id);
  controller_->BeginObservation(id, features, default_target);
}

void MojoLearningTaskController::CompleteObservation(
    base::UnguessableToken id,
    const ObservationCompletion& completion) {
  controller_->CompleteObservation(id, completion);
}

void MojoLearningTaskController::CancelObservation(base::UnguessableToken id) {
  controller_->CancelObservation(id);
}

void MojoLearningTaskController::UpdateDefaultTarget(
    base::UnguessableToken id,
    const std::optional<TargetValue>& default_target) {
  controller_->UpdateDefaultTarget(id, default_target);
}

const LearningTask& MojoLearningTaskController::GetLearningTask() {
  return task_;
}

void MojoLearningTaskController::PredictDistribution(
    const FeatureVector& features,
    PredictionCB callback) {
  controller_->PredictDistribution(features, std::move(callback));
}

}  // namespace learning
}  // namespace media

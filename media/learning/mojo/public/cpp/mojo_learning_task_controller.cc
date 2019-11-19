// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/mojo/public/cpp/mojo_learning_task_controller.h"

#include <utility>

#include "mojo/public/cpp/bindings/binding.h"

namespace media {
namespace learning {

MojoLearningTaskController::MojoLearningTaskController(
    const LearningTask& task,
    mojo::PendingRemote<mojom::LearningTaskController> controller)
    : task_(task), controller_(std::move(controller)) {}

MojoLearningTaskController::~MojoLearningTaskController() = default;

void MojoLearningTaskController::BeginObservation(
    base::UnguessableToken id,
    const FeatureVector& features,
    const base::Optional<TargetValue>& default_target) {
  // We don't need to keep track of in-flight observations, since the service
  // side handles it for us.
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
    const base::Optional<TargetValue>& default_target) {
  controller_->UpdateDefaultTarget(id, default_target);
}

const LearningTask& MojoLearningTaskController::GetLearningTask() {
  return task_;
}

}  // namespace learning
}  // namespace media

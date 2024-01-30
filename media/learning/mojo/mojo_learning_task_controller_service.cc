// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/mojo/mojo_learning_task_controller_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "media/learning/common/learning_task_controller.h"

namespace media {
namespace learning {

// Somewhat arbitrary upper limit on the number of in-flight observations that
// we'll allow a client to have.
static const size_t kMaxInFlightObservations = 16;

MojoLearningTaskControllerService::MojoLearningTaskControllerService(
    const LearningTask& task,
    ukm::SourceId source_id,
    std::unique_ptr<::media::learning::LearningTaskController> impl)
    : task_(task), source_id_(source_id), impl_(std::move(impl)) {}

MojoLearningTaskControllerService::~MojoLearningTaskControllerService() =
    default;

void MojoLearningTaskControllerService::BeginObservation(
    const base::UnguessableToken& id,
    const FeatureVector& features,
    const std::optional<TargetValue>& default_target) {
  // Drop the observation if it doesn't match the feature description size.
  if (features.size() != task_.feature_descriptions.size())
    return;

  // Don't allow the client to send too many in-flight observations.
  if (in_flight_observations_.size() >= kMaxInFlightObservations)
    return;
  in_flight_observations_.insert(id);

  // Since we own |impl_|, we don't need to keep track of in-flight
  // observations.  We'll release |impl_| on destruction, which cancels them.
  impl_->BeginObservation(id, features, default_target, source_id_);
}

void MojoLearningTaskControllerService::CompleteObservation(
    const base::UnguessableToken& id,
    const ObservationCompletion& completion) {
  auto iter = in_flight_observations_.find(id);
  if (iter == in_flight_observations_.end())
    return;
  in_flight_observations_.erase(iter);

  impl_->CompleteObservation(id, completion);
}

void MojoLearningTaskControllerService::CancelObservation(
    const base::UnguessableToken& id) {
  auto iter = in_flight_observations_.find(id);
  if (iter == in_flight_observations_.end())
    return;
  in_flight_observations_.erase(iter);

  impl_->CancelObservation(id);
}

void MojoLearningTaskControllerService::UpdateDefaultTarget(
    const base::UnguessableToken& id,
    const std::optional<TargetValue>& default_target) {
  auto iter = in_flight_observations_.find(id);
  if (iter == in_flight_observations_.end())
    return;

  impl_->UpdateDefaultTarget(id, default_target);
}

void MojoLearningTaskControllerService::PredictDistribution(
    const FeatureVector& features,
    PredictDistributionCallback callback) {
  impl_->PredictDistribution(features, std::move(callback));
}

}  // namespace learning
}  // namespace media

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_MOJO_MOJO_LEARNING_TASK_CONTROLLER_SERVICE_H_
#define MEDIA_LEARNING_MOJO_MOJO_LEARNING_TASK_CONTROLLER_SERVICE_H_

#include <memory>
#include <set>

#include "base/component_export.h"
#include "media/learning/mojo/public/mojom/learning_task_controller.mojom.h"

namespace media {
namespace learning {

class LearningTaskController;

// Mojo service that talks to a local LearningTaskController.
class COMPONENT_EXPORT(MEDIA_LEARNING_MOJO) MojoLearningTaskControllerService
    : public mojom::LearningTaskController {
 public:
  // |impl| is the underlying controller that we'll send requests to.
  // |source_id| will be used for all observations, since we don't trust the
  // client to provide the right one.
  explicit MojoLearningTaskControllerService(
      const LearningTask& task,
      ukm::SourceId source_id,
      std::unique_ptr<::media::learning::LearningTaskController> impl);

  MojoLearningTaskControllerService(const MojoLearningTaskControllerService&) =
      delete;
  MojoLearningTaskControllerService& operator=(
      const MojoLearningTaskControllerService&) = delete;

  ~MojoLearningTaskControllerService() override;

  // mojom::LearningTaskController
  void BeginObservation(
      const base::UnguessableToken& id,
      const FeatureVector& features,
      const std::optional<TargetValue>& default_target) override;
  void CompleteObservation(const base::UnguessableToken& id,
                           const ObservationCompletion& completion) override;
  void CancelObservation(const base::UnguessableToken& id) override;
  void UpdateDefaultTarget(
      const base::UnguessableToken& id,
      const std::optional<TargetValue>& default_target) override;
  void PredictDistribution(const FeatureVector& features,
                           PredictDistributionCallback callback) override;

 protected:
  const LearningTask task_;

  ukm::SourceId source_id_;

  // Underlying controller to which we proxy calls.
  std::unique_ptr<::media::learning::LearningTaskController> impl_;

  std::set<base::UnguessableToken> in_flight_observations_;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_MOJO_MOJO_LEARNING_TASK_CONTROLLER_SERVICE_H_

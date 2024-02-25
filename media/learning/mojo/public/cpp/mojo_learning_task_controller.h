// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_
#define MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_

#include "base/component_export.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/mojo/public/mojom/learning_task_controller.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
namespace learning {

// LearningTaskController implementation to forward to a remote impl via mojo.
class COMPONENT_EXPORT(MEDIA_LEARNING_MOJO) MojoLearningTaskController
    : public LearningTaskController {
 public:
  // |task| will be provided by GetLearningTask().  Hopefully, it matches
  // whatever |controller| uses.
  MojoLearningTaskController(
      const LearningTask& task,
      mojo::Remote<mojom::LearningTaskController> controller);

  MojoLearningTaskController(const MojoLearningTaskController&) = delete;
  MojoLearningTaskController& operator=(const MojoLearningTaskController&) =
      delete;

  ~MojoLearningTaskController() override;

  // LearningTaskController
  void BeginObservation(base::UnguessableToken id,
                        const FeatureVector& features,
                        const std::optional<TargetValue>& default_target,
                        const std::optional<ukm::SourceId>& source_id) override;
  void CompleteObservation(base::UnguessableToken id,
                           const ObservationCompletion& completion) override;
  void CancelObservation(base::UnguessableToken id) override;
  void UpdateDefaultTarget(
      base::UnguessableToken id,
      const std::optional<TargetValue>& default_target) override;
  const LearningTask& GetLearningTask() override;
  void PredictDistribution(const FeatureVector& features,
                           PredictionCB callback) override;

 private:
  LearningTask task_;
  mojo::Remote<mojom::LearningTaskController> controller_;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_
#define MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_

#include <utility>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/mojo/public/mojom/learning_task_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
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
      mojo::PendingRemote<mojom::LearningTaskController> controller);
  ~MojoLearningTaskController() override;

  // LearningTaskController
  void BeginObservation(
      base::UnguessableToken id,
      const FeatureVector& features,
      const base::Optional<TargetValue>& default_target) override;
  void CompleteObservation(base::UnguessableToken id,
                           const ObservationCompletion& completion) override;
  void CancelObservation(base::UnguessableToken id) override;
  void UpdateDefaultTarget(
      base::UnguessableToken id,
      const base::Optional<TargetValue>& default_target) override;
  const LearningTask& GetLearningTask() override;

 private:
  LearningTask task_;
  mojo::Remote<mojom::LearningTaskController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MojoLearningTaskController);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_MOJO_PUBLIC_CPP_MOJO_LEARNING_TASK_CONTROLLER_H_

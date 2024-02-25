// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_session_impl.h"

#include <set>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/learning/impl/distribution_reporter.h"
#include "media/learning/impl/learning_task_controller_impl.h"

namespace media {
namespace learning {

// Allow multiple clients to own an LTC that points to the same underlying LTC.
// Since we don't own the LTC, we also keep track of in-flight observations and
// explicitly cancel them on destruction, since dropping an LTC implies that.
class WeakLearningTaskController : public LearningTaskController {
 public:
  WeakLearningTaskController(
      base::WeakPtr<LearningSessionImpl> weak_session,
      base::SequenceBound<LearningTaskController>* controller,
      const LearningTask& task)
      : weak_session_(std::move(weak_session)),
        controller_(controller),
        task_(task) {}

  ~WeakLearningTaskController() override {
    if (!weak_session_)
      return;

    // Cancel any outstanding observation, unless they have a default value.  In
    // that case, complete them.
    for (auto& id : outstanding_observations_) {
      const std::optional<TargetValue>& default_value = id.second;
      if (default_value) {
        controller_->AsyncCall(&LearningTaskController::CompleteObservation)
            .WithArgs(id.first, *default_value);
      } else {
        controller_->AsyncCall(&LearningTaskController::CancelObservation)
            .WithArgs(id.first);
      }
    }
  }

  void BeginObservation(
      base::UnguessableToken id,
      const FeatureVector& features,
      const std::optional<TargetValue>& default_target,
      const std::optional<ukm::SourceId>& source_id) override {
    if (!weak_session_)
      return;

    outstanding_observations_[id] = default_target;
    // We don't send along the default value because LearningTaskControllerImpl
    // doesn't support it.  Since all client calls eventually come through us
    // anyway, it seems okay to handle it here.
    controller_->AsyncCall(&LearningTaskController::BeginObservation)
        .WithArgs(id, features, std::nullopt, source_id);
  }

  void CompleteObservation(base::UnguessableToken id,
                           const ObservationCompletion& completion) override {
    if (!weak_session_)
      return;
    outstanding_observations_.erase(id);
    controller_->AsyncCall(&LearningTaskController::CompleteObservation)
        .WithArgs(id, completion);
  }

  void CancelObservation(base::UnguessableToken id) override {
    if (!weak_session_)
      return;
    outstanding_observations_.erase(id);
    controller_->AsyncCall(&LearningTaskController::CancelObservation)
        .WithArgs(id);
  }

  void UpdateDefaultTarget(
      base::UnguessableToken id,
      const std::optional<TargetValue>& default_target) override {
    if (!weak_session_)
      return;

    outstanding_observations_[id] = default_target;
  }

  const LearningTask& GetLearningTask() override { return task_; }

  void PredictDistribution(const FeatureVector& features,
                           PredictionCB callback) override {
    if (!weak_session_)
      return;
    controller_->AsyncCall(&LearningTaskController::PredictDistribution)
        .WithArgs(features, std::move(callback));
  }

  base::WeakPtr<LearningSessionImpl> weak_session_;
  raw_ptr<base::SequenceBound<LearningTaskController>, DanglingUntriaged>
      controller_;
  LearningTask task_;

  // Set of ids that have been started but not completed / cancelled yet, and
  // any default target value.
  std::map<base::UnguessableToken, std::optional<TargetValue>>
      outstanding_observations_;
};

LearningSessionImpl::LearningSessionImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      controller_factory_(base::BindRepeating(
          [](scoped_refptr<base::SequencedTaskRunner> task_runner,
             const LearningTask& task,
             SequenceBoundFeatureProvider feature_provider)
              -> base::SequenceBound<LearningTaskController> {
            return base::SequenceBound<LearningTaskControllerImpl>(
                task_runner, task, DistributionReporter::Create(task),
                std::move(feature_provider));
          })) {}

LearningSessionImpl::~LearningSessionImpl() = default;

void LearningSessionImpl::SetTaskControllerFactoryCBForTesting(
    CreateTaskControllerCB cb) {
  controller_factory_ = std::move(cb);
}

std::unique_ptr<LearningTaskController> LearningSessionImpl::GetController(
    const std::string& task_name) {
  auto iter = controller_map_.find(task_name);
  if (iter == controller_map_.end())
    return nullptr;

  // If there were any way to replace / destroy a controller other than when we
  // destroy |this|, then this wouldn't be such a good idea.
  return std::make_unique<WeakLearningTaskController>(
      weak_factory_.GetWeakPtr(), &iter->second, task_map_[task_name]);
}

void LearningSessionImpl::RegisterTask(
    const LearningTask& task,
    SequenceBoundFeatureProvider feature_provider) {
  DCHECK(controller_map_.count(task.name) == 0);
  controller_map_.emplace(
      task.name,
      controller_factory_.Run(task_runner_, task, std::move(feature_provider)));

  task_map_.emplace(task.name, task);
}

}  // namespace learning
}  // namespace media

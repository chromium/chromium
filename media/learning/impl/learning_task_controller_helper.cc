// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_task_controller_helper.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace media {
namespace learning {

LearningTaskControllerHelper::LearningTaskControllerHelper(
    const LearningTask& task,
    AddExampleCB add_example_cb,
    SequenceBoundFeatureProvider feature_provider)
    : task_(task),
      feature_provider_(std::move(feature_provider)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      add_example_cb_(std::move(add_example_cb)) {}

LearningTaskControllerHelper::~LearningTaskControllerHelper() = default;

void LearningTaskControllerHelper::BeginObservation(
    base::UnguessableToken id,
    FeatureVector features,
    std::optional<ukm::SourceId> source_id) {
  auto& pending_example = pending_examples_[id];

  if (source_id)
    pending_example.source_id = *source_id;

  // Start feature prediction, so that we capture the current values.
  if (!feature_provider_.is_null()) {
    // TODO(dcheng): Convert this to use Then() helper.
    feature_provider_.AsyncCall(&FeatureProvider::AddFeatures)
        .WithArgs(std::move(features),
                  base::BindOnce(
                      &LearningTaskControllerHelper::OnFeaturesReadyTrampoline,
                      task_runner_, weak_ptr_factory_.GetWeakPtr(), id));
  } else {
    pending_example.example.features = std::move(features);
    pending_example.features_done = true;
  }
}

void LearningTaskControllerHelper::CompleteObservation(
    base::UnguessableToken id,
    const ObservationCompletion& completion) {
  auto iter = pending_examples_.find(id);
  if (iter == pending_examples_.end())
    return;

  iter->second.example.target_value = completion.target_value;
  iter->second.example.weight = completion.weight;
  iter->second.target_done = true;
  ProcessExampleIfFinished(std::move(iter));
}

void LearningTaskControllerHelper::CancelObservation(
    base::UnguessableToken id) {
  auto iter = pending_examples_.find(id);
  if (iter == pending_examples_.end())
    return;

  // This would have to check for pending predictions, if we supported them, and
  // defer destruction until the features arrive.
  pending_examples_.erase(iter);
}

// static
void LearningTaskControllerHelper::OnFeaturesReadyTrampoline(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<LearningTaskControllerHelper> weak_this,
    base::UnguessableToken id,
    FeatureVector features) {
  // TODO(liberato): this would benefit from promises / deferred data.
  auto cb = base::BindOnce(&LearningTaskControllerHelper::OnFeaturesReady,
                           std::move(weak_this), id, std::move(features));
  if (!task_runner->RunsTasksInCurrentSequence()) {
    task_runner->PostTask(FROM_HERE, std::move(cb));
  } else {
    std::move(cb).Run();
  }
}

void LearningTaskControllerHelper::OnFeaturesReady(base::UnguessableToken id,
                                                   FeatureVector features) {
  PendingExampleMap::iterator iter = pending_examples_.find(id);
  // It's possible that OnLabelCallbackDestroyed has already run.  That's okay
  // since we don't support prediction right now.
  if (iter == pending_examples_.end())
    return;

  iter->second.example.features = std::move(features);
  iter->second.features_done = true;
  ProcessExampleIfFinished(std::move(iter));
}

void LearningTaskControllerHelper::ProcessExampleIfFinished(
    PendingExampleMap::iterator iter) {
  if (!iter->second.features_done || !iter->second.target_done)
    return;

  add_example_cb_.Run(std::move(iter->second.example), iter->second.source_id);
  pending_examples_.erase(iter);

  // TODO(liberato): If we receive FeatureVector f1 then f2, and start filling
  // in features for a prediction, and if features become available in the order
  // f2, f1, and we receive a target value for f2 before f1's features are
  // complete, should we insist on deferring training with f2 until we start
  // prediction on f1?  I suppose that we could just insist that features are
  // provided in the same order they're received, and it's automatic.
}

}  // namespace learning
}  // namespace media

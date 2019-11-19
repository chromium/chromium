// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_task_controller_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "media/learning/impl/distribution_reporter.h"
#include "media/learning/impl/extra_trees_trainer.h"
#include "media/learning/impl/lookup_table_trainer.h"

namespace media {
namespace learning {

LearningTaskControllerImpl::LearningTaskControllerImpl(
    const LearningTask& task,
    std::unique_ptr<DistributionReporter> reporter,
    SequenceBoundFeatureProvider feature_provider)
    : task_(task),
      training_data_(std::make_unique<TrainingData>()),
      reporter_(std::move(reporter)),
      helper_(std::make_unique<LearningTaskControllerHelper>(
          task,
          base::BindRepeating(&LearningTaskControllerImpl::AddFinishedExample,
                              AsWeakPtr()),
          std::move(feature_provider))),
      expected_feature_count_(task_.feature_descriptions.size()) {
  // Note that |helper_| uses the full set of features.

  // TODO(liberato): Make this compositional.  FeatureSubsetTaskController?
  if (task_.feature_subset_size)
    DoFeatureSubsetSelection();

  switch (task_.model) {
    case LearningTask::Model::kExtraTrees:
      trainer_ = std::make_unique<ExtraTreesTrainer>();
      break;
    case LearningTask::Model::kLookupTable:
      trainer_ = std::make_unique<LookupTableTrainer>();
      break;
  }
}

LearningTaskControllerImpl::~LearningTaskControllerImpl() = default;

void LearningTaskControllerImpl::BeginObservation(
    base::UnguessableToken id,
    const FeatureVector& features,
    const base::Optional<TargetValue>& default_target) {
  // TODO(liberato): Should we enforce that the right number of features are
  // present here?  Right now, we allow it to be shorter, so that features from
  // a FeatureProvider may be omitted.  Of course, they have to be at the end in
  // that case.  If we start enforcing it here, make sure that LearningHelper
  // starts adding the placeholder features.
  if (!trainer_)
    return;

  // We don't support default targets, since we're the base learner and can't
  // easily do that.  However, defaults are handled by (weak) controllers
  // handed out by LearningSessionImpl.  So, we don't bother since they never
  // get here anyway.
  DCHECK(!default_target);

  helper_->BeginObservation(id, features);
}

void LearningTaskControllerImpl::CompleteObservation(
    base::UnguessableToken id,
    const ObservationCompletion& completion) {
  if (!trainer_)
    return;
  helper_->CompleteObservation(id, completion);
}

void LearningTaskControllerImpl::CancelObservation(base::UnguessableToken id) {
  if (!trainer_)
    return;
  helper_->CancelObservation(id);
}

void LearningTaskControllerImpl::UpdateDefaultTarget(
    base::UnguessableToken id,
    const base::Optional<TargetValue>& default_target) {
  NOTREACHED();
}

const LearningTask& LearningTaskControllerImpl::GetLearningTask() {
  return task_;
}

void LearningTaskControllerImpl::AddFinishedExample(LabelledExample example,
                                                    ukm::SourceId source_id) {
  // Verify that we have a trainer and that we got the right number of features.
  // We don't compare to |task_.feature_descriptions.size()| since that has been
  // adjusted to the subset size already.  We expect the original count.
  if (!trainer_ || example.features.size() != expected_feature_count_)
    return;

  // Now that we have the whole set of features, select the subset we want.
  FeatureVector new_features;
  if (task_.feature_subset_size) {
    for (auto& iter : feature_indices_)
      new_features.push_back(example.features[iter]);
    example.features = std::move(new_features);
  }  // else use them all.

  // The features should now match the task.
  DCHECK_EQ(example.features.size(), task_.feature_descriptions.size());

  if (training_data_->size() >= task_.max_data_set_size) {
    // Replace a random example.  We don't necessarily want to replace the
    // oldest, since we don't necessarily want to enforce an ad-hoc recency
    // constraint here.  That's a different issue.
    (*training_data_)[rng()->Generate(training_data_->size())] = example;
  } else {
    training_data_->push_back(example);
  }
  // Either way, we have one more example that we haven't used for training yet.
  num_untrained_examples_++;

  // Once we have a model, see if we'd get |example| correct.
  if (model_ && reporter_) {
    TargetHistogram predicted = model_->PredictDistribution(example.features);

    DistributionReporter::PredictionInfo info;
    info.observed = example.target_value;
    info.source_id = source_id;
    info.total_training_weight = last_training_weight_;
    info.total_training_examples = last_training_size_;
    reporter_->GetPredictionCallback(info).Run(predicted);
  }

  // Can't train more than one model concurrently.
  if (training_is_in_progress_)
    return;

  // Train every time we get enough new examples.  Note that this works even if
  // we are replacing old examples rather than adding new ones.
  double frac = ((double)num_untrained_examples_) / training_data_->size();
  if (frac < task_.min_new_data_fraction)
    return;

  num_untrained_examples_ = 0;

  // Record these for metrics.
  last_training_weight_ = training_data_->total_weight();
  last_training_size_ = training_data_->size();

  TrainedModelCB model_cb =
      base::BindOnce(&LearningTaskControllerImpl::OnModelTrained, AsWeakPtr(),
                     training_data_->total_weight(), training_data_->size());
  training_is_in_progress_ = true;
  // Note that this copies the training data, so it's okay if we add more
  // examples to our copy before this returns.
  // TODO(liberato): Post to a background task runner, and bind |model_cb| to
  // the current one.  Be careful about ownership if we invalidate |trainer_|
  // on this thread.  Be sure to post destruction to that sequence.
  trainer_->Train(task_, *training_data_, std::move(model_cb));
}

void LearningTaskControllerImpl::OnModelTrained(double training_weight,
                                                int training_size,
                                                std::unique_ptr<Model> model) {
  DCHECK(training_is_in_progress_);
  training_is_in_progress_ = false;
  model_ = std::move(model);
  // Record these for metrics.
  last_training_weight_ = training_weight;
  last_training_size_ = training_size;
}

void LearningTaskControllerImpl::SetTrainerForTesting(
    std::unique_ptr<TrainingAlgorithm> trainer) {
  trainer_ = std::move(trainer);
}

void LearningTaskControllerImpl::DoFeatureSubsetSelection() {
  // Choose a random feature, and trim the descriptions to match.
  std::vector<size_t> features;
  for (size_t i = 0; i < task_.feature_descriptions.size(); i++)
    features.push_back(i);

  for (int i = 0; i < *task_.feature_subset_size; i++) {
    // Pick an element from |i| to the end of the list, inclusive.
    // TODO(liberato): For tests, this will happen before any rng is provided
    // by the test; we'll use an actual rng.
    int r = rng()->Generate(features.size() - i) + i;
    // Swap them.
    std::swap(features[i], features[r]);
  }

  // Construct the feature subset from the first few elements.  Also adjust the
  // task's descriptions to match.  We do this in two steps so that the
  // descriptions are added via iterating over |feature_indices_|, so that the
  // enumeration order is the same as when we adjust the feature values of
  // incoming examples.  In both cases, we iterate over |feature_indicies_|,
  // which might (will) re-order them with respect to |features|.
  for (int i = 0; i < *task_.feature_subset_size; i++)
    feature_indices_.insert(features[i]);

  std::vector<LearningTask::ValueDescription> adjusted_descriptions;
  for (auto& iter : feature_indices_)
    adjusted_descriptions.push_back(task_.feature_descriptions[iter]);

  task_.feature_descriptions = adjusted_descriptions;

  if (reporter_)
    reporter_->SetFeatureSubset(feature_indices_);
}

}  // namespace learning
}  // namespace media

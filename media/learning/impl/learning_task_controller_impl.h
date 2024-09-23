// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_

#include <memory>
#include <set>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/impl/distribution_reporter.h"
#include "media/learning/impl/feature_provider.h"
#include "media/learning/impl/learning_task_controller_helper.h"
#include "media/learning/impl/random_number_generator.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

class DistributionReporter;
class LearningTaskControllerImplTest;

// Controller for a single learning task.  Takes training examples, and forwards
// them to the learner(s).  Responsible for things like:
//  - Managing underlying learner(s) based on the learning task
//  - Feature subset selection
//  - UMA reporting on accuracy / feature importance
//
// The idea is that one can create a LearningTask, give it to an LTCI, and the
// LTCI will do the work of building / evaluating the model based on training
// examples that are provided to it.
class COMPONENT_EXPORT(LEARNING_IMPL) LearningTaskControllerImpl final
    : public LearningTaskController,
      public HasRandomNumberGenerator {
 public:
  LearningTaskControllerImpl(
      const LearningTask& task,
      std::unique_ptr<DistributionReporter> reporter = nullptr,
      SequenceBoundFeatureProvider feature_provider =
          SequenceBoundFeatureProvider());
  ~LearningTaskControllerImpl() override;

  // LearningTaskController
  // Note that we don't support |default_target|, since destroying us destroys
  // everything.  One might make the argument that only the mojo client /
  // service should support default values, but it's much more convenient if
  // they're part of the base api.  So, since clients shouldn't be dealing with
  // us directly (see LearningSessionImpl), it's okay.
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
  // Add |example| to the training data, and process it.
  void AddFinishedExample(LabelledExample example, ukm::SourceId source_id);

  // Called by |training_cb_| when the model is trained.  |training_weight| and
  // |training_size| are the training set's total weight and number of examples.
  void OnModelTrained(double training_weight,
                      int training_size,
                      std::unique_ptr<Model> model);

  void SetTrainerForTesting(std::unique_ptr<TrainingAlgorithm> trainer);

  // Update |task_| to reflect a randomly chosen subset of features.
  void DoFeatureSubsetSelection();

  LearningTask task_;

  // Current batch of examples.
  std::unique_ptr<TrainingData> training_data_;

  // Most recently trained model, or null.
  std::unique_ptr<Model> model_;

  // We don't want to have multiple models in flight.
  bool training_is_in_progress_ = false;

  // Number of examples in |training_data_| that haven't been used for training.
  // This helps us decide when to train a new model.
  int num_untrained_examples_ = 0;

  // Total weight and number of examples in the most recently trained model.
  double last_training_weight_ = 0.;
  size_t last_training_size_ = 0u;

  // Training algorithm that we'll use.
  std::unique_ptr<TrainingAlgorithm> trainer_;

  // Optional reporter for training accuracy.
  std::unique_ptr<DistributionReporter> reporter_;

  // Helper that we use to handle deferred examples.
  std::unique_ptr<LearningTaskControllerHelper> helper_;

  // If the task specifies feature importance measurement, then this is the
  // randomly chosen subset of features.
  std::set<int> feature_indices_;

  // Number of features that we expect in each observation.
  size_t expected_feature_count_;

  base::WeakPtrFactory<LearningTaskControllerImpl> weak_ptr_factory_{this};

  friend class LearningTaskControllerImplTest;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_

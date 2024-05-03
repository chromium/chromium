// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_EXTRA_TREES_TRAINER_H_
#define MEDIA_LEARNING_IMPL_EXTRA_TREES_TRAINER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/impl/one_hot.h"
#include "media/learning/impl/random_number_generator.h"
#include "media/learning/impl/random_tree_trainer.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

// Bagged forest of extremely randomized trees.
//
// These are an ensemble of trees.  Each tree is constructed from the full
// training set.  The trees are constructed by selecting a random subset of
// features at each node.  For each feature, a uniformly random split point is
// chosen.  The feature with the best randomly chosen split point is used.
//
// These will automatically convert nominal values to one-hot vectors.
class COMPONENT_EXPORT(LEARNING_IMPL) ExtraTreesTrainer final
    : public TrainingAlgorithm,
      public HasRandomNumberGenerator {
 public:
  ExtraTreesTrainer();

  ExtraTreesTrainer(const ExtraTreesTrainer&) = delete;
  ExtraTreesTrainer& operator=(const ExtraTreesTrainer&) = delete;

  ~ExtraTreesTrainer() override;

  // TrainingAlgorithm
  void Train(const LearningTask& task,
             const TrainingData& training_data,
             TrainedModelCB model_cb) override;

 private:
  void OnRandomTreeModel(TrainedModelCB model_cb, std::unique_ptr<Model> model);

  std::unique_ptr<TrainingAlgorithm> tree_trainer_;

  // In-flight training.
  LearningTask task_;
  std::vector<std::unique_ptr<Model>> trees_;
  std::unique_ptr<OneHotConverter> converter_;
  TrainingData converted_training_data_;
  base::WeakPtrFactory<ExtraTreesTrainer> weak_ptr_factory_{this};
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_EXTRA_TREES_TRAINER_H_

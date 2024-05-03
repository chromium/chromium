// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/extra_trees_trainer.h"

#include <set>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "media/learning/impl/voting_ensemble.h"

namespace media {
namespace learning {

ExtraTreesTrainer::ExtraTreesTrainer() = default;

ExtraTreesTrainer::~ExtraTreesTrainer() = default;

void ExtraTreesTrainer::Train(const LearningTask& task,
                              const TrainingData& training_data,
                              TrainedModelCB model_cb) {
  // Make sure that there is no training in progress.
  DCHECK_EQ(trees_.size(), 0u);
  DCHECK_EQ(converter_.get(), nullptr);

  task_ = task;
  trees_.reserve(task.rf_number_of_trees);

  // Instantiate our tree trainer if we haven't already.  We do this now only
  // so that we can send it our rng, mostly for tests.
  // TODO(liberato): We should always take the rng in the ctor, rather than
  // via SetRngForTesting.  Then we can do this earlier.
  if (!tree_trainer_)
    tree_trainer_ = std::make_unique<RandomTreeTrainer>(rng());

  // We've modified RandomTree to handle nominals, so we don't need to do one-
  // hot conversion normally.  It's slow.  However, the changes to RandomTree
  // are only approximately the same thing.
  if (task_.use_one_hot_conversion) {
    converter_ = std::make_unique<OneHotConverter>(task, training_data);
    converted_training_data_ = converter_->Convert(training_data);
    task_ = converter_->converted_task();
  } else {
    converted_training_data_ = training_data;
  }

  // Start training.  Send in nullptr to start the process.
  OnRandomTreeModel(std::move(model_cb), nullptr);
}

void ExtraTreesTrainer::OnRandomTreeModel(TrainedModelCB model_cb,
                                          std::unique_ptr<Model> model) {
  // Allow a null Model to make it easy to start training.
  if (model)
    trees_.push_back(std::move(model));

  // If this is the last tree, then return the finished model.
  if (trees_.size() == task_.rf_number_of_trees) {
    std::unique_ptr<Model> finished_model =
        std::make_unique<VotingEnsemble>(std::move(trees_));
    // If we have a converter, then wrap everything in a ConvertingModel.
    if (converter_) {
      finished_model = std::make_unique<ConvertingModel>(
          std::move(converter_), std::move(finished_model));
    }

    std::move(model_cb).Run(std::move(finished_model));
    return;
  }

  // Train the next tree.
  auto cb = base::BindOnce(&ExtraTreesTrainer::OnRandomTreeModel,
                           weak_ptr_factory_.GetWeakPtr(), std::move(model_cb));
  tree_trainer_->Train(task_, converted_training_data_, std::move(cb));
}

}  // namespace learning
}  // namespace media

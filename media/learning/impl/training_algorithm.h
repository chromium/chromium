// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_TRAINING_ALGORITHM_H_
#define MEDIA_LEARNING_IMPL_TRAINING_ALGORITHM_H_

#include <memory>

#include "base/functional/callback.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/impl/model.h"

namespace media {
namespace learning {

// Returns a trained model.
using TrainedModelCB = base::OnceCallback<void(std::unique_ptr<Model>)>;

// Base class for training algorithms.
class TrainingAlgorithm {
 public:
  TrainingAlgorithm() = default;

  TrainingAlgorithm(const TrainingAlgorithm&) = delete;
  TrainingAlgorithm& operator=(const TrainingAlgorithm&) = delete;

  virtual ~TrainingAlgorithm() = default;

  virtual void Train(const LearningTask& task,
                     const TrainingData& training_data,
                     TrainedModelCB model_cb) = 0;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_TRAINING_ALGORITHM_H_

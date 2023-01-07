// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/lookup_table_trainer.h"

#include <map>


namespace media {
namespace learning {

class LookupTable : public Model {
 public:
  LookupTable(const TrainingData& training_data) {
    for (auto& example : training_data)
      buckets_[example.features] += example;
  }

  // Model
  TargetHistogram PredictDistribution(const FeatureVector& instance) override {
    auto iter = buckets_.find(instance);
    if (iter == buckets_.end())
      return TargetHistogram();

    return iter->second;
  }

 private:
  std::map<FeatureVector, TargetHistogram> buckets_;
};

LookupTableTrainer::LookupTableTrainer() = default;

LookupTableTrainer::~LookupTableTrainer() = default;

void LookupTableTrainer::Train(const LearningTask& task,
                               const TrainingData& training_data,
                               TrainedModelCB model_cb) {
  std::unique_ptr<LookupTable> lookup_table =
      std::make_unique<LookupTable>(training_data);

  // TODO(liberato): post?
  std::move(model_cb).Run(std::move(lookup_table));
}

}  // namespace learning
}  // namespace media

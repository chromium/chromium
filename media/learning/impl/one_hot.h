// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_ONE_HOT_H_
#define MEDIA_LEARNING_IMPL_ONE_HOT_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/common/value.h"
#include "media/learning/impl/model.h"

namespace media {
namespace learning {

// Converter class that memorizes a mapping from nominal features to numeric
// features with a one-hot encoding.
class COMPONENT_EXPORT(LEARNING_IMPL) OneHotConverter {
 public:
  // Build a one-hot converter for all nominal features |task|, using the values
  // found in |training_data|.
  OneHotConverter(const LearningTask& task, const TrainingData& training_data);

  OneHotConverter(const OneHotConverter&) = delete;
  OneHotConverter& operator=(const OneHotConverter&) = delete;

  ~OneHotConverter();

  // Return the LearningTask that has only nominal features.
  const LearningTask& converted_task() const { return converted_task_; }

  // Convert |training_data| to be a one-hot model.
  TrainingData Convert(const TrainingData& training_data) const;

  // Convert |feature_vector| to match the one-hot model.
  FeatureVector Convert(const FeatureVector& feature_vector) const;

 private:
  // Build a converter for original feature |index|.
  void ProcessOneFeature(
      size_t index,
      const LearningTask::ValueDescription& original_description,
      const TrainingData& training_data);

  // Learning task with the feature descriptions adjusted for the one-hot model.
  LearningTask converted_task_;

  // [value] == vector index that should be 1 in the one-hot vector.
  using ValueVectorIndexMap = std::map<Value, size_t>;

  // [original task feature index] = optional converter for it.  If the feature
  // was kNumeric to begin with, then there will be no converter.
  std::vector<std::optional<ValueVectorIndexMap>> converters_;
};

// Model that uses |Converter| to convert instances before sending them to the
// underlying model.
class COMPONENT_EXPORT(LEARNING_IMPL) ConvertingModel : public Model {
 public:
  ConvertingModel(std::unique_ptr<OneHotConverter> converter,
                  std::unique_ptr<Model> model);

  ConvertingModel(const ConvertingModel&) = delete;
  ConvertingModel& operator=(const ConvertingModel&) = delete;

  ~ConvertingModel() override;

  // Model
  TargetHistogram PredictDistribution(const FeatureVector& instance) override;

 private:
  std::unique_ptr<OneHotConverter> converter_;
  std::unique_ptr<Model> model_;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_ONE_HOT_H_

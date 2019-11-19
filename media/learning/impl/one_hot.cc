// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/one_hot.h"

#include <set>

namespace media {
namespace learning {

OneHotConverter::OneHotConverter(const LearningTask& task,
                                 const TrainingData& training_data)
    : converted_task_(task) {
  converted_task_.feature_descriptions.clear();

  // store
  converters_.resize(task.feature_descriptions.size());

  for (size_t i = 0; i < task.feature_descriptions.size(); i++) {
    const LearningTask::ValueDescription& feature =
        task.feature_descriptions[i];

    // If this is already a numeric feature, then we will copy it since
    // converters[i] will be unset.
    if (feature.ordering == LearningTask::Ordering::kNumeric) {
      converted_task_.feature_descriptions.push_back(feature);
      continue;
    }

    ProcessOneFeature(i, feature, training_data);
  }
}

OneHotConverter::~OneHotConverter() = default;

TrainingData OneHotConverter::Convert(const TrainingData& training_data) const {
  TrainingData converted_training_data;
  for (auto& example : training_data) {
    LabelledExample converted_example(example);
    converted_example.features = Convert(example.features);
    converted_training_data.push_back(converted_example);
  }

  return converted_training_data;
}

FeatureVector OneHotConverter::Convert(
    const FeatureVector& feature_vector) const {
  FeatureVector converted_feature_vector;
  converted_feature_vector.reserve(converted_task_.feature_descriptions.size());
  for (size_t i = 0; i < converters_.size(); i++) {
    auto& converter = converters_[i];
    if (!converter) {
      // There's no conversion needed for this feature, since it was numeric.
      converted_feature_vector.push_back(feature_vector[i]);
      continue;
    }

    // Convert this feature to a one-hot vector.
    const size_t vector_size = converter->size();

    // Start with a zero-hot vector.  Is that a thing?
    for (size_t v = 0; v < vector_size; v++)
      converted_feature_vector.push_back(FeatureValue(0));

    // Set the appropriate entry to 1, if any.  Otherwise, this is a
    // previously unseen value and all of them should be zero.
    auto iter = converter->find(feature_vector[i]);
    if (iter != converter->end())
      converted_feature_vector[iter->second] = FeatureValue(1);
  }

  return converted_feature_vector;
}

void OneHotConverter::ProcessOneFeature(
    size_t index,
    const LearningTask::ValueDescription& original_description,
    const TrainingData& training_data) {
  // Collect all the distinct values for |index|.
  std::set<Value> values;
  for (auto& example : training_data) {
    DCHECK_GE(example.features.size(), index);
    values.insert(example.features[index]);
  }

  // We let the set's ordering be the one-hot value.  It doesn't really matter
  // as long as we don't change it once we pick it.
  ValueVectorIndexMap value_map;
  // Vector index that should be set to one for each distinct value.  This will
  // start at the next feature in the adjusted task.
  size_t next_vector_index = converted_task_.feature_descriptions.size();

  // Add one feature for each value, and construct a map from value to the
  // feature index that should be 1 when the feature takes that value.
  for (auto& value : values) {
    LearningTask::ValueDescription adjusted_description = original_description;
    adjusted_description.ordering = LearningTask::Ordering::kNumeric;
    converted_task_.feature_descriptions.push_back(adjusted_description);
    // |value| will converted into a 1 in the |next_vector_index|-th feature.
    value_map[value] = next_vector_index++;
  }

  // Record |values| for the |index|-th original feature.
  converters_[index] = std::move(value_map);
}

ConvertingModel::ConvertingModel(std::unique_ptr<OneHotConverter> converter,
                                 std::unique_ptr<Model> model)
    : converter_(std::move(converter)), model_(std::move(model)) {}
ConvertingModel::~ConvertingModel() = default;

TargetHistogram ConvertingModel::PredictDistribution(
    const FeatureVector& instance) {
  FeatureVector converted_instance = converter_->Convert(instance);
  return model_->PredictDistribution(converted_instance);
}

}  // namespace learning
}  // namespace media

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/labelled_example.h"

#include "base/containers/flat_set.h"

namespace media {
namespace learning {

LabelledExample::LabelledExample() = default;

LabelledExample::LabelledExample(FeatureVector feature_vector,
                                 TargetValue target)
    : features(std::move(feature_vector)), target_value(target) {}

LabelledExample::LabelledExample(std::initializer_list<FeatureValue> init_list,
                                 TargetValue target)
    : features(init_list), target_value(target) {}

LabelledExample::LabelledExample(const LabelledExample& rhs) = default;

LabelledExample::LabelledExample(LabelledExample&& rhs) noexcept = default;

LabelledExample::~LabelledExample() = default;

std::ostream& operator<<(std::ostream& out, const LabelledExample& example) {
  out << example.features << " => " << example.target_value;

  return out;
}

std::ostream& operator<<(std::ostream& out, const FeatureVector& features) {
  for (const auto& feature : features)
    out << " " << feature;

  return out;
}

bool LabelledExample::operator==(const LabelledExample& rhs) const {
  // Do not check weight.
  return target_value == rhs.target_value && features == rhs.features;
}

bool LabelledExample::operator!=(const LabelledExample& rhs) const {
  // Do not check weight.
  return !((*this) == rhs);
}

bool LabelledExample::operator<(const LabelledExample& rhs) const {
  // Impose a somewhat arbitrary ordering.
  // Do not check weight.
  if (target_value != rhs.target_value)
    return target_value < rhs.target_value;

  // Note that we could short-circuit this if the feature vector lengths are
  // unequal, since we don't particularly care how they compare as long as it's
  // stable.  In particular, we don't have any notion of a "prefix".
  return features < rhs.features;
}

LabelledExample& LabelledExample::operator=(const LabelledExample& rhs) =
    default;

LabelledExample& LabelledExample::operator=(LabelledExample&& rhs) noexcept =
    default;

TrainingData::TrainingData() = default;

TrainingData::TrainingData(const TrainingData& rhs) = default;

TrainingData::TrainingData(TrainingData&& rhs) = default;

TrainingData::~TrainingData() = default;

TrainingData& TrainingData::operator=(const TrainingData& rhs) = default;

TrainingData& TrainingData::operator=(TrainingData&& rhs) = default;

TrainingData TrainingData::DeDuplicate() const {
  // flat_set has non-const iterators, while std::set does not.  const_cast is
  // not allowed by chromium style outside of getters, so flat_set it is.
  base::flat_set<LabelledExample> example_set;
  for (auto& example : examples_) {
    auto iter = example_set.find(example);
    if (iter != example_set.end())
      iter->weight += example.weight;
    else
      example_set.insert(example);
  }

  TrainingData deduplicated_data;
  for (auto& example : example_set)
    deduplicated_data.push_back(example);

  return deduplicated_data;
}

}  // namespace learning
}  // namespace media

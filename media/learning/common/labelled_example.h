// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_LABELLED_EXAMPLE_H_
#define MEDIA_LEARNING_COMMON_LABELLED_EXAMPLE_H_

#include <initializer_list>
#include <ostream>
#include <vector>

#include "base/check_op.h"
#include "base/component_export.h"
#include "media/learning/common/value.h"

namespace media {
namespace learning {

// Vector of features, for training or prediction.
// To interpret the features, one probably needs to check a LearningTask.  It
// provides a description for each index.  For example, [0]=="height",
// [1]=="url", etc.
using FeatureVector = std::vector<FeatureValue>;

using WeightType = size_t;

// One training example == group of feature values, plus the desired target.
struct COMPONENT_EXPORT(LEARNING_COMMON) LabelledExample {
  LabelledExample();
  LabelledExample(FeatureVector feature_vector, TargetValue target);
  LabelledExample(std::initializer_list<FeatureValue> init_list,
                  TargetValue target);
  LabelledExample(const LabelledExample& rhs);
  LabelledExample(LabelledExample&& rhs) noexcept;
  ~LabelledExample();

  // Comparisons ignore weight, because it's convenient.
  bool operator==(const LabelledExample& rhs) const;
  bool operator!=(const LabelledExample& rhs) const;
  bool operator<(const LabelledExample& rhs) const;

  LabelledExample& operator=(const LabelledExample& rhs);
  LabelledExample& operator=(LabelledExample&& rhs) noexcept;

  // Observed feature values.
  // Note that to interpret these values, you probably need to have the
  // LearningTask that they're supposed to be used with.
  FeatureVector features;

  // Observed output value, when given |features| as input.
  TargetValue target_value;

  WeightType weight = 1u;

  // Copy / assignment is allowed.
};

// TODO(liberato): This should probably move to impl/ .
class COMPONENT_EXPORT(LEARNING_COMMON) TrainingData {
 public:
  using ExampleVector = std::vector<LabelledExample>;
  using const_iterator = ExampleVector::const_iterator;

  TrainingData();
  TrainingData(const TrainingData& rhs);
  TrainingData(TrainingData&& rhs);

  TrainingData& operator=(const TrainingData& rhs);
  TrainingData& operator=(TrainingData&& rhs);

  ~TrainingData();

  // Add |example| with weight |weight|.
  void push_back(const LabelledExample& example) {
    DCHECK_GT(example.weight, 0u);
    examples_.push_back(example);
    total_weight_ += example.weight;
  }

  bool empty() const { return !total_weight_; }

  size_t size() const { return examples_.size(); }

  // Returns the number of instances, taking into account their weight.  For
  // example, if one adds an example with weight 2, then this will return two
  // more than it did before.
  WeightType total_weight() const { return total_weight_; }

  const_iterator begin() const { return examples_.begin(); }
  const_iterator end() const { return examples_.end(); }

  bool is_unweighted() const { return examples_.size() == total_weight_; }

  // Provide the |i|-th example, over [0, size()).
  const LabelledExample& operator[](size_t i) const { return examples_[i]; }
  LabelledExample& operator[](size_t i) { return examples_[i]; }

  // Return a copy of this data with duplicate entries merged.  Example weights
  // will be summed.
  TrainingData DeDuplicate() const;

 private:
  ExampleVector examples_;

  WeightType total_weight_ = 0u;

  // Copy / assignment is allowed.
};

COMPONENT_EXPORT(LEARNING_COMMON)
std::ostream& operator<<(std::ostream& out, const LabelledExample& example);

COMPONENT_EXPORT(LEARNING_COMMON)
std::ostream& operator<<(std::ostream& out, const FeatureVector& features);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_LABELLED_EXAMPLE_H_

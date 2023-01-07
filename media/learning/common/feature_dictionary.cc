// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/feature_dictionary.h"

namespace media {
namespace learning {

FeatureDictionary::FeatureDictionary() = default;

FeatureDictionary::~FeatureDictionary() = default;

void FeatureDictionary::Lookup(const LearningTask& task,
                               FeatureVector* features) const {
  const size_t num_features = task.feature_descriptions.size();

  if (features->size() < num_features)
    features->resize(num_features);

  for (size_t i = 0; i < num_features; i++) {
    const auto& name = task.feature_descriptions[i].name;
    auto entry = dictionary_.find(name);
    if (entry == dictionary_.end())
      continue;

    // |name| appears in the dictionary, so add its value to |features|.
    (*features)[i] = entry->second;
  }
}

void FeatureDictionary::Add(const std::string& name,
                            const FeatureValue& value) {
  dictionary_[name] = value;
}

}  // namespace learning
}  // namespace media

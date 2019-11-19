// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_FEATURE_DICTIONARY_H_
#define MEDIA_LEARNING_COMMON_FEATURE_DICTIONARY_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/learning_task.h"

namespace media {
namespace learning {

// Dictionary of feature name => value pairs.
//
// This is useful if one simply wants to snapshot some features, and apply them
// to more than one task without recomputing anything.
//
// While it's not required, FeatureLibrary is useful to provide the descriptions
// that a FeatureDictionary will provide, so that the LearningTask and the
// dictionary agree on names.
class COMPONENT_EXPORT(LEARNING_COMMON) FeatureDictionary {
 public:
  // [feature name] => snapshotted value.
  using Dictionary = std::map<std::string, FeatureValue>;

  FeatureDictionary();
  ~FeatureDictionary();

  // Add features for |task| to |features| from our dictionary.  Features that
  // aren't present in the dictionary will be ignored.  |features| will be
  // expanded if needed to match |task|.
  void Lookup(const LearningTask& task, FeatureVector* features) const;

  // Add |name| to the dictionary with value |value|.
  void Add(const std::string& name, const FeatureValue& value);

 private:
  Dictionary dictionary_;

  DISALLOW_COPY_AND_ASSIGN(FeatureDictionary);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_FEATURE_DICTIONARY_H_

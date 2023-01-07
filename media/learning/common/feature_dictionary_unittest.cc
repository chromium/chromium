// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/feature_dictionary.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class FeatureDictionaryTest : public testing::Test {};

TEST_F(FeatureDictionaryTest, FillsInFeatures) {
  FeatureDictionary dict;
  const std::string feature_name_1("feature 1");
  const FeatureValue feature_value_1("feature value 1");

  const std::string feature_name_2("feature 2");
  const FeatureValue feature_value_2("feature value 2");

  const std::string feature_name_3("feature 3");
  const FeatureValue feature_value_3("feature value 3");

  dict.Add(feature_name_1, feature_value_1);
  dict.Add(feature_name_2, feature_value_2);
  dict.Add(feature_name_3, feature_value_3);

  LearningTask task;
  task.feature_descriptions.push_back({"some other feature"});
  task.feature_descriptions.push_back({feature_name_3});
  task.feature_descriptions.push_back({feature_name_1});

  FeatureVector features;
  features.push_back(FeatureValue(0));  // some other feature

  dict.Lookup(task, &features);
  EXPECT_EQ(features.size(), 3u);
  EXPECT_EQ(features[0], FeatureValue(0));
  EXPECT_EQ(features[1], feature_value_3);
  EXPECT_EQ(features[2], feature_value_1);
}

}  // namespace learning
}  // namespace media

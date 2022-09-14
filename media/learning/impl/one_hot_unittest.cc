// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/one_hot.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class OneHotTest : public testing::Test {
 public:
  OneHotTest() {}
};

TEST_F(OneHotTest, EmptyLearningTaskWorks) {
  LearningTask empty_task("EmptyTask", LearningTask::Model::kExtraTrees, {},
                          LearningTask::ValueDescription({"target"}));
  TrainingData empty_training_data;
  OneHotConverter one_hot(empty_task, empty_training_data);
  EXPECT_EQ(one_hot.converted_task().feature_descriptions.size(), 0u);
}

TEST_F(OneHotTest, SimpleConversionWorks) {
  LearningTask task("SimpleTask", LearningTask::Model::kExtraTrees,
                    {{"feature1", LearningTask::Ordering::kUnordered}},
                    LearningTask::ValueDescription({"target"}));
  TrainingData training_data;
  training_data.push_back({{FeatureValue("abc")}, TargetValue(0)});
  training_data.push_back({{FeatureValue("def")}, TargetValue(1)});
  training_data.push_back({{FeatureValue("ghi")}, TargetValue(2)});
  // Push a duplicate as the last one.
  training_data.push_back({{FeatureValue("def")}, TargetValue(3)});
  OneHotConverter one_hot(task, training_data);
  // There should be one feature for each distinct value in features[0].
  const size_t adjusted_feature_size = 3u;
  EXPECT_EQ(one_hot.converted_task().feature_descriptions.size(),
            adjusted_feature_size);
  EXPECT_EQ(one_hot.converted_task().feature_descriptions[0].ordering,
            LearningTask::Ordering::kNumeric);
  EXPECT_EQ(one_hot.converted_task().feature_descriptions[1].ordering,
            LearningTask::Ordering::kNumeric);
  EXPECT_EQ(one_hot.converted_task().feature_descriptions[2].ordering,
            LearningTask::Ordering::kNumeric);

  TrainingData converted_training_data = one_hot.Convert(training_data);
  EXPECT_EQ(converted_training_data.size(), training_data.size());
  // Exactly one feature should be 1.
  for (size_t i = 0; i < converted_training_data.size(); i++) {
    EXPECT_EQ(converted_training_data[i].features[0].value() +
                  converted_training_data[i].features[1].value() +
                  converted_training_data[i].features[2].value(),
              1);
  }

  // Each of the first three training examples should have distinct vectors.
  for (size_t f = 0; f < adjusted_feature_size; f++) {
    int num_ones = 0;
    // 3u is the number of distinct examples.  [3] is a duplicate.
    for (size_t i = 0; i < 3u; i++)
      num_ones += converted_training_data[i].features[f].value();
    EXPECT_EQ(num_ones, 1);
  }

  // The features of examples 1 and 3 should be the same.
  for (size_t f = 0; f < adjusted_feature_size; f++) {
    EXPECT_EQ(converted_training_data[1].features[f],
              converted_training_data[3].features[f]);
  }

  // Converting each feature vector should result in the same one as before.
  for (size_t f = 0; f < adjusted_feature_size; f++) {
    FeatureVector converted_feature_vector =
        one_hot.Convert(training_data[f].features);
    EXPECT_EQ(converted_feature_vector, converted_training_data[f].features);
  }
}

TEST_F(OneHotTest, NumericsAreNotConverted) {
  LearningTask task("SimpleTask", LearningTask::Model::kExtraTrees,
                    {{"feature1", LearningTask::Ordering::kNumeric}},
                    LearningTask::ValueDescription({"target"}));
  OneHotConverter one_hot(task, TrainingData());
  EXPECT_EQ(one_hot.converted_task().feature_descriptions.size(), 1u);
  EXPECT_EQ(one_hot.converted_task().feature_descriptions[0].ordering,
            LearningTask::Ordering::kNumeric);

  TrainingData training_data;
  training_data.push_back({{FeatureValue(5)}, TargetValue(0)});
  TrainingData converted_training_data = one_hot.Convert(training_data);
  EXPECT_EQ(converted_training_data[0], training_data[0]);

  FeatureVector converted_feature_vector =
      one_hot.Convert(training_data[0].features);
  EXPECT_EQ(converted_feature_vector, training_data[0].features);
}

TEST_F(OneHotTest, UnknownValuesAreZeroHot) {
  LearningTask task("SimpleTask", LearningTask::Model::kExtraTrees,
                    {{"feature1", LearningTask::Ordering::kUnordered}},
                    LearningTask::ValueDescription({"target"}));
  TrainingData training_data;
  training_data.push_back({{FeatureValue("abc")}, TargetValue(0)});
  training_data.push_back({{FeatureValue("def")}, TargetValue(1)});
  training_data.push_back({{FeatureValue("ghi")}, TargetValue(2)});
  OneHotConverter one_hot(task, training_data);

  // Send in an unknown value, and see if it becomes {0, 0, 0}.
  FeatureVector converted_feature_vector =
      one_hot.Convert(FeatureVector({FeatureValue("jkl")}));
  EXPECT_EQ(converted_feature_vector.size(), 3u);
  for (size_t i = 0; i < converted_feature_vector.size(); i++)
    EXPECT_EQ(converted_feature_vector[i], FeatureValue(0));
}

}  // namespace learning
}  // namespace media

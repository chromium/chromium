// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "media/learning/impl/learning_task_controller_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningTaskControllerHelperTest : public testing::Test {
 public:
  class FakeFeatureProvider : public FeatureProvider {
   public:
    FakeFeatureProvider(FeatureVector* features_out,
                        FeatureProvider::FeatureVectorCB* cb_out)
        : features_out_(features_out), cb_out_(cb_out) {}

    // Do nothing, except note that we were called.
    void AddFeatures(FeatureVector features,
                     FeatureProvider::FeatureVectorCB cb) override {
      *features_out_ = std::move(features);
      *cb_out_ = std::move(cb);
    }

    raw_ptr<FeatureVector> features_out_;
    raw_ptr<FeatureProvider::FeatureVectorCB> cb_out_;
  };

  LearningTaskControllerHelperTest() {
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    task_.name = "example_task";

    example_.features.push_back(FeatureValue(1));
    example_.features.push_back(FeatureValue(2));
    example_.features.push_back(FeatureValue(3));
    example_.target_value = TargetValue(123);
    example_.weight = 100u;

    id_ = base::UnguessableToken::Create();
  }

  ~LearningTaskControllerHelperTest() override {
    // To prevent a memory leak, reset the helper.  This will post destruction
    // of other objects, so RunUntilIdle().
    helper_.reset();
    task_environment_.RunUntilIdle();
  }

  void CreateClient(bool include_fp) {
    // Create the fake feature provider, and get a pointer to it.
    base::SequenceBound<FakeFeatureProvider> sb_fp;
    if (include_fp) {
      sb_fp = base::SequenceBound<FakeFeatureProvider>(task_runner_,
                                                       &fp_features_, &fp_cb_);
      task_environment_.RunUntilIdle();
    }

    // TODO(liberato): make sure this works without a fp.
    helper_ = std::make_unique<LearningTaskControllerHelper>(
        task_,
        base::BindRepeating(
            &LearningTaskControllerHelperTest::OnLabelledExample,
            base::Unretained(this)),
        std::move(sb_fp));
  }

  void OnLabelledExample(LabelledExample example, ukm::SourceId source_id) {
    most_recent_example_ = std::move(example);
    most_recent_source_id_ = source_id;
  }

  // Since we're friends but the tests aren't.
  size_t pending_example_count() const {
    return helper_->pending_example_count_for_testing();
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<LearningTaskControllerHelper> helper_;

  // Most recent features / cb given to our FakeFeatureProvider.
  FeatureVector fp_features_;
  FeatureProvider::FeatureVectorCB fp_cb_;

  // Most recently added example via OnLabelledExample, if any.
  std::optional<LabelledExample> most_recent_example_;
  ukm::SourceId most_recent_source_id_;

  LearningTask task_;

  base::UnguessableToken id_;

  LabelledExample example_;
};

TEST_F(LearningTaskControllerHelperTest, AddingAnExampleWithoutFPWorks) {
  // A helper that doesn't use a FeatureProvider should forward examples as soon
  // as they're done.
  CreateClient(false);
  ukm::SourceId source_id = 2;
  helper_->BeginObservation(id_, example_.features, source_id);
  EXPECT_EQ(pending_example_count(), 1u);
  helper_->CompleteObservation(
      id_, ObservationCompletion(example_.target_value, example_.weight));
  EXPECT_TRUE(most_recent_example_);
  EXPECT_EQ(*most_recent_example_, example_);
  EXPECT_EQ(most_recent_example_->weight, example_.weight);
  EXPECT_EQ(most_recent_source_id_, source_id);
  EXPECT_EQ(pending_example_count(), 0u);
}

TEST_F(LearningTaskControllerHelperTest, DropTargetValueWithoutFPWorks) {
  // Verify that we can drop an example without labelling it.
  CreateClient(false);
  helper_->BeginObservation(id_, example_.features, std::nullopt);
  EXPECT_EQ(pending_example_count(), 1u);
  helper_->CancelObservation(id_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(most_recent_example_);
  EXPECT_EQ(pending_example_count(), 0u);
}

TEST_F(LearningTaskControllerHelperTest, AddTargetValueBeforeFP) {
  // Verify that an example is added if the target value arrives first.
  CreateClient(true);
  helper_->BeginObservation(id_, example_.features, std::nullopt);
  EXPECT_EQ(pending_example_count(), 1u);
  task_environment_.RunUntilIdle();
  // The feature provider should know about the example.
  EXPECT_EQ(fp_features_, example_.features);

  // Add the targe value and verify that the example wasn't added yet.
  helper_->CompleteObservation(
      id_, ObservationCompletion(example_.target_value, example_.weight));
  EXPECT_FALSE(most_recent_example_);
  EXPECT_EQ(pending_example_count(), 1u);

  // Add the features, and verify that they arrive at the AddExampleCB.
  example_.features[0] = FeatureValue(456);
  std::move(fp_cb_).Run(example_.features);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(pending_example_count(), 0u);
  EXPECT_TRUE(most_recent_example_);
  EXPECT_EQ(*most_recent_example_, example_);
  EXPECT_EQ(most_recent_example_->weight, example_.weight);
}

TEST_F(LearningTaskControllerHelperTest, DropTargetValueBeforeFP) {
  // Verify that an example is correctly dropped before the FP adds features.
  CreateClient(true);
  helper_->BeginObservation(id_, example_.features, std::nullopt);
  EXPECT_EQ(pending_example_count(), 1u);
  task_environment_.RunUntilIdle();
  // The feature provider should know about the example.
  EXPECT_EQ(fp_features_, example_.features);

  // Cancel the observation.
  helper_->CancelObservation(id_);
  // We don't care if the example is still queued or not, only that we can
  // add features and have it be zero by then.

  // Add the features, and verify that the pending example is removed and no
  // example was sent to us.
  example_.features[0] = FeatureValue(456);
  std::move(fp_cb_).Run(example_.features);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(pending_example_count(), 0u);
  EXPECT_FALSE(most_recent_example_);
}

TEST_F(LearningTaskControllerHelperTest, AddTargetValueAfterFP) {
  // Verify that an example is added if the target value arrives second.
  CreateClient(true);
  helper_->BeginObservation(id_, example_.features, std::nullopt);
  EXPECT_EQ(pending_example_count(), 1u);
  task_environment_.RunUntilIdle();
  // The feature provider should know about the example.
  EXPECT_EQ(fp_features_, example_.features);
  EXPECT_EQ(pending_example_count(), 1u);

  // Add the features, and verify that the example isn't sent yet.
  example_.features[0] = FeatureValue(456);
  std::move(fp_cb_).Run(example_.features);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(most_recent_example_);
  EXPECT_EQ(pending_example_count(), 1u);

  // Add the targe value and verify that the example is added.
  helper_->CompleteObservation(
      id_, ObservationCompletion(example_.target_value, example_.weight));
  EXPECT_TRUE(most_recent_example_);
  EXPECT_EQ(*most_recent_example_, example_);
  EXPECT_EQ(most_recent_example_->weight, example_.weight);
  EXPECT_EQ(pending_example_count(), 0u);
}

TEST_F(LearningTaskControllerHelperTest, DropTargetValueAfterFP) {
  // Verify that we can cancel the observationc after sending features.
  CreateClient(true);
  helper_->BeginObservation(id_, example_.features, std::nullopt);
  EXPECT_EQ(pending_example_count(), 1u);
  task_environment_.RunUntilIdle();
  // The feature provider should know about the example.
  EXPECT_EQ(fp_features_, example_.features);
  EXPECT_EQ(pending_example_count(), 1u);

  // Add the features, and verify that the example isn't sent yet.  We do care
  // that the example is still pending, since we haven't actually dropped the
  // callback yet; we might send a TargetValue.
  example_.features[0] = FeatureValue(456);
  std::move(fp_cb_).Run(example_.features);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(most_recent_example_);
  EXPECT_EQ(pending_example_count(), 1u);

  // Cancel the observation, and verify that the pending example has been
  // removed, and no example was sent to us.
  helper_->CancelObservation(id_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(most_recent_example_);
  EXPECT_EQ(pending_example_count(), 0u);
}

}  // namespace learning
}  // namespace media

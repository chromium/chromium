// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/impl/distribution_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class DistributionReporterTest : public testing::Test {
 public:
  DistributionReporterTest()
      : ukm_recorder_(std::make_unique<ukm::TestAutoSetUkmRecorder>()),
        source_id_(123) {
    task_.name = "TaskName";
    // UMA reporting requires a numeric target.
    task_.target_description.ordering = LearningTask::Ordering::kNumeric;
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  LearningTask task_;

  ukm::SourceId source_id_;

  std::unique_ptr<DistributionReporter> reporter_;

  TargetHistogram HistogramFor(double value) {
    TargetHistogram histogram;
    histogram += TargetValue(value);
    return histogram;
  }
};

TEST_F(DistributionReporterTest, DistributionReporterDoesNotCrash) {
  // Make sure that we request some sort of reporting.
  task_.uma_hacky_aggregate_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);

  // Observe an average of 2 / 3.
  DistributionReporter::PredictionInfo info;
  info.observed = TargetValue(2.0 / 3.0);
  auto cb = reporter_->GetPredictionCallback(info);

  TargetHistogram predicted;
  const TargetValue Zero(0);
  const TargetValue One(1);

  // Predict an average of 5 / 9.
  predicted[Zero] = 40;
  predicted[One] = 50;
  std::move(cb).Run(predicted);
}

TEST_F(DistributionReporterTest, CallbackRecordsRegressionPredictions) {
  // Make sure that |reporter_| records everything correctly for regressions.
  task_.target_description.ordering = LearningTask::Ordering::kNumeric;
  // Scale 1-2 => 0->100.
  task_.ukm_min_input_value = 1.;
  task_.ukm_max_input_value = 2.;
  task_.report_via_ukm = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);

  DistributionReporter::PredictionInfo info;
  info.observed = TargetValue(1.1);  // => 10
  info.source_id = source_id_;
  auto cb = reporter_->GetPredictionCallback(info);

  TargetHistogram predicted;
  const TargetValue One(1);
  const TargetValue Five(5);
  // Predict an average of 1.5 => 50 in the 0-100 scale.
  predicted[One] = 70;
  predicted[Five] = 10;
  ASSERT_EQ(predicted.Average(), 1.5);
  std::move(cb).Run(predicted);

  // The record should show the correct averages, scaled by |fixed_point_scale|.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder_->GetEntriesByName("Media.Learning.PredictionRecord");
  EXPECT_EQ(entries.size(), 1u);
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[0], "LearningTask",
                                          task_.GetId());
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[0], "ObservedValue", 10);
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[0], "PredictedValue", 50);
}

TEST_F(DistributionReporterTest, DistributionReporterNeedsUmaNameOrUkm) {
  // Make sure that we don't get a reporter if we don't request any reporting.
  task_.target_description.ordering = LearningTask::Ordering::kNumeric;
  task_.uma_hacky_aggregate_confusion_matrix = false;
  task_.uma_hacky_by_training_weight_confusion_matrix = false;
  task_.uma_hacky_by_feature_subset_confusion_matrix = false;
  task_.report_via_ukm = false;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_EQ(reporter_, nullptr);
}

TEST_F(DistributionReporterTest,
       DistributionReporterHackyConfusionMatrixNeedsRegression) {
  // Hacky confusion matrix reporting only works with regression.
  task_.target_description.ordering = LearningTask::Ordering::kUnordered;
  task_.uma_hacky_aggregate_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_EQ(reporter_, nullptr);
}

TEST_F(DistributionReporterTest, ProvidesAggregateReporter) {
  task_.uma_hacky_aggregate_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);
}

TEST_F(DistributionReporterTest, ProvidesByTrainingWeightReporter) {
  task_.uma_hacky_by_training_weight_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);
}

TEST_F(DistributionReporterTest, ProvidesByFeatureSubsetReporter) {
  task_.uma_hacky_by_feature_subset_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);
}

TEST_F(DistributionReporterTest, UkmBucketizesProperly) {
  task_.target_description.ordering = LearningTask::Ordering::kNumeric;
  // Scale [1000, 2000] => [0, 100]
  task_.ukm_min_input_value = 1000;
  task_.ukm_max_input_value = 2000;
  task_.report_via_ukm = true;

  reporter_ = DistributionReporter::Create(task_);
  DistributionReporter::PredictionInfo info;
  info.source_id = source_id_;

  // Add a few predictions / observations.  We rotate the predicted / observed
  // just to be sure they end up in the right UKM field.

  // Inputs less than min scale to 0.
  info.observed = TargetValue(900);
  reporter_->GetPredictionCallback(info).Run(HistogramFor(1500));

  // Inputs exactly at min scale to 0.
  info.observed = TargetValue(1000);
  reporter_->GetPredictionCallback(info).Run(HistogramFor(2000));

  // Inputs in the middle scale to 50.
  info.observed = TargetValue(1500);
  reporter_->GetPredictionCallback(info).Run(HistogramFor(2100));

  // Inputs at max scale to 100.
  info.observed = TargetValue(2000);
  reporter_->GetPredictionCallback(info).Run(HistogramFor(900));

  // Inputs greater than max scale to 100.
  info.observed = TargetValue(2100);
  reporter_->GetPredictionCallback(info).Run(HistogramFor(1000));

  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder_->GetEntriesByName("Media.Learning.PredictionRecord");
  EXPECT_EQ(entries.size(), 5u);

  ukm::TestUkmRecorder::ExpectEntryMetric(entries[0], "ObservedValue", 0);
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[0], "PredictedValue", 50);

  ukm::TestUkmRecorder::ExpectEntryMetric(entries[1], "ObservedValue", 0);
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[1], "PredictedValue", 100);

  ukm::TestUkmRecorder::ExpectEntryMetric(entries[2], "ObservedValue", 50);
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[2], "PredictedValue", 100);

  ukm::TestUkmRecorder::ExpectEntryMetric(entries[3], "ObservedValue", 100);
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[3], "PredictedValue", 0);

  ukm::TestUkmRecorder::ExpectEntryMetric(entries[4], "ObservedValue", 100);
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[4], "PredictedValue", 0);
}

}  // namespace learning
}  // namespace media

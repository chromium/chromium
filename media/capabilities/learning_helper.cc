// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/learning_helper.h"

#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "media/learning/common/feature_library.h"
#include "media/learning/common/learning_task.h"

namespace media {

using learning::FeatureLibrary;
using learning::FeatureProviderFactoryCB;
using learning::FeatureValue;
using learning::LabelledExample;
using learning::LearningSessionImpl;
using learning::LearningTask;
using learning::LearningTaskController;
using learning::ObservationCompletion;
using learning::SequenceBoundFeatureProvider;
using learning::TargetValue;

// Remember that these are used to construct UMA histogram names!  Be sure to
// update histograms.xml if you change them!

// Dropped frame ratio, default features, unweighted regression tree.
const char* const kDroppedFrameRatioBaseUnweightedTreeTaskName =
    "BaseUnweightedTree";

// Dropped frame ratio, default features, unweighted examples, lookup table.
const char* const kDroppedFrameRatioBaseUnweightedTableTaskName =
    "BaseUnweightedTable";

// Same as BaseUnweightedTree, but with 200 training examples max.
const char* const kDroppedFrameRatioBaseUnweightedTree200TaskName =
    "BaseUnweightedTree200";

// Dropped frame ratio, default+FeatureLibrary features, regression tree with
// unweighted examples and 200 training examples max.
const char* const kDroppedFrameRatioEnhancedUnweightedTree200TaskName =
    "EnhancedUnweightedTree200";

// Threshold for the dropped frame to total frame ratio, at which we'll decide
// that the playback was not smooth.
constexpr double kSmoothnessThreshold = 0.1;

LearningHelper::LearningHelper(FeatureProviderFactoryCB feature_factory) {
  // Create the LearningSession on a background task runner.  In the future,
  // it's likely that the session will live on the main thread, and handle
  // delegation of LearningTaskControllers to other threads.  However, for now,
  // do it here.
  learning_session_ = std::make_unique<LearningSessionImpl>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  // Register a few learning tasks.
  //
  // We only do this here since we own the session.  Normally, whatever creates
  // the session would register all the learning tasks.
  LearningTask dropped_frame_task(
      "no name", LearningTask::Model::kLookupTable,
      {
          {"codec_profile",
           ::media::learning::LearningTask::Ordering::kUnordered},
          {"width", ::media::learning::LearningTask::Ordering::kNumeric},
          {"height", ::media::learning::LearningTask::Ordering::kNumeric},
          {"frame_rate", ::media::learning::LearningTask::Ordering::kNumeric},
      },
      LearningTask::ValueDescription(
          {"dropped_ratio", LearningTask::Ordering::kNumeric}));

  // Report results hackily both in aggregate and by training data weight.
  dropped_frame_task.smoothness_threshold = kSmoothnessThreshold;
  dropped_frame_task.uma_hacky_aggregate_confusion_matrix = true;
  dropped_frame_task.uma_hacky_by_training_weight_confusion_matrix = true;

  // Buckets will have 10 examples each, or 20 for the 200-set tasks.
  const double data_set_size = 100;
  const double big_data_set_size = 200;

  // Unweighted table
  dropped_frame_task.name = kDroppedFrameRatioBaseUnweightedTableTaskName;
  dropped_frame_task.max_data_set_size = data_set_size;
  learning_session_->RegisterTask(dropped_frame_task,
                                  SequenceBoundFeatureProvider());
  base_unweighted_table_controller_ =
      learning_session_->GetController(dropped_frame_task.name);

  // Unweighted base tree.
  dropped_frame_task.name = kDroppedFrameRatioBaseUnweightedTreeTaskName;
  dropped_frame_task.model = LearningTask::Model::kExtraTrees;
  dropped_frame_task.max_data_set_size = data_set_size;
  learning_session_->RegisterTask(dropped_frame_task,
                                  SequenceBoundFeatureProvider());
  base_unweighted_tree_controller_ =
      learning_session_->GetController(dropped_frame_task.name);

  // Unweighted tree with a larger training set.
  dropped_frame_task.name = kDroppedFrameRatioBaseUnweightedTree200TaskName;
  dropped_frame_task.max_data_set_size = big_data_set_size;
  learning_session_->RegisterTask(dropped_frame_task,
                                  SequenceBoundFeatureProvider());
  base_unweighted_tree_200_controller_ =
      learning_session_->GetController(dropped_frame_task.name);

  // Add common features, if we have a factory.
  if (feature_factory) {
    dropped_frame_task.name =
        kDroppedFrameRatioEnhancedUnweightedTree200TaskName;
    dropped_frame_task.max_data_set_size = big_data_set_size;
    dropped_frame_task.feature_descriptions.push_back(
        {"origin", ::media::learning::LearningTask::Ordering::kUnordered});
    dropped_frame_task.feature_descriptions.push_back(
        FeatureLibrary::NetworkType());
    dropped_frame_task.feature_descriptions.push_back(
        FeatureLibrary::BatteryPower());
    learning_session_->RegisterTask(dropped_frame_task,
                                    feature_factory.Run(dropped_frame_task));
    enhanced_unweighted_tree_200_controller_ =
        learning_session_->GetController(dropped_frame_task.name);
  }
}

LearningHelper::~LearningHelper() = default;

void LearningHelper::AppendStats(
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    learning::FeatureValue origin,
    const VideoDecodeStatsDB::DecodeStatsEntry& new_stats) {
  // If no frames were recorded, then do nothing.
  if (new_stats.frames_decoded == 0)
    return;

  // Sanity.
  if (new_stats.frames_dropped > new_stats.frames_decoded)
    return;

  // Add a training example for |new_stats|.
  LabelledExample example;

  // Extract features from |video_key|.
  example.features.push_back(FeatureValue(video_key.codec_profile));
  example.features.push_back(FeatureValue(video_key.size.width()));
  example.features.push_back(FeatureValue(video_key.size.height()));
  example.features.push_back(FeatureValue(video_key.frame_rate));

  // Record the ratio of dropped frames to non-dropped frames.  Weight this
  // example by the total number of frames, since we want to predict the
  // aggregate dropped frames ratio.  That lets us compare with the current
  // implementation directly.
  //
  // It's also not clear that we want to do this; we might want to weight each
  // playback equally and predict the dropped frame ratio.  For example, if
  // there is a dependence on video length, then it's unclear that weighting
  // the examples is the right thing to do.
  example.target_value = TargetValue(
      static_cast<double>(new_stats.frames_dropped) / new_stats.frames_decoded);
  example.weight = 1u;

  // Add this example to all tasks.
  AddExample(base_unweighted_table_controller_.get(), example);
  AddExample(base_unweighted_tree_controller_.get(), example);
  AddExample(base_unweighted_tree_200_controller_.get(), example);

  if (enhanced_unweighted_tree_200_controller_) {
    example.features.push_back(origin);
    AddExample(enhanced_unweighted_tree_200_controller_.get(), example);
  }
}

void LearningHelper::AddExample(LearningTaskController* controller,
                                const LabelledExample& example) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  controller->BeginObservation(id, example.features);
  controller->CompleteObservation(
      id, ObservationCompletion(example.target_value, example.weight));
}

}  // namespace media

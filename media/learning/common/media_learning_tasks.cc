// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/media_learning_tasks.h"

#include "base/no_destructor.h"
#include "base/notreached.h"

namespace media {
namespace learning {
namespace {

const LearningTask& GetWillPlayTask() {
  static base::NoDestructor<LearningTask> task;
  if (!task->feature_descriptions.size()) {
    task->name = tasknames::kWillPlay;
    // TODO(liberato): fill in the rest here, once we have the features picked.
  }

  return *task;
}

// Add some features to |task| that WMPI knows how to add.
void PushWMPIFeatures(LearningTask& task) {
  // NOTE: Be sure to update web_media_player_impl.cc if you change these, since
  // it memorizes them.
  task.feature_descriptions.push_back(
      {"codec", LearningTask::Ordering::kUnordered});
  task.feature_descriptions.push_back(
      {"profile", LearningTask::Ordering::kUnordered});
  task.feature_descriptions.push_back(
      {"width", LearningTask::Ordering::kNumeric});
  task.feature_descriptions.push_back(
      {"fps", LearningTask::Ordering::kNumeric});
}

const LearningTask& GetConsecutiveBadWindowsTask() {
  static base::NoDestructor<LearningTask> task;
  if (!task->feature_descriptions.size()) {
    task->name = tasknames::kConsecutiveBadWindows;
    task->model = LearningTask::Model::kExtraTrees;

    // Target is max number of consecutive bad windows.
    task->target_description = {"max_bad_windows",
                                LearningTask::Ordering::kNumeric};

    PushWMPIFeatures(*task);

    // Report via UKM, but allow up to 100 bad windows, since it'll auto-scale
    // to two digits of precision.  Might as well use all of it, even if 100
    // consecutive bad windows is unlikely.
    task->report_via_ukm = true;
    task->ukm_min_input_value = 0.0;
    task->ukm_max_input_value = 100.0;
  }

  return *task;
}

const LearningTask& GetConsecutiveNNRsTask() {
  static base::NoDestructor<LearningTask> task;
  if (!task->feature_descriptions.size()) {
    task->name = tasknames::kConsecutiveNNRs;
    task->model = LearningTask::Model::kExtraTrees;

    // Target is max number of consecutive bad windows.
    task->target_description = {"total_playback_nnrs",
                                LearningTask::Ordering::kNumeric};

    PushWMPIFeatures(*task);

    task->report_via_ukm = true;
    task->ukm_min_input_value = 0.0;
    task->ukm_max_input_value = 100.0;
  }

  return *task;
}

}  // namespace

// static
const LearningTask& MediaLearningTasks::Get(const char* task_name) {
  if (strcmp(task_name, tasknames::kWillPlay) == 0)
    return GetWillPlayTask();
  if (strcmp(task_name, tasknames::kConsecutiveBadWindows) == 0)
    return GetConsecutiveBadWindowsTask();
  if (strcmp(task_name, tasknames::kConsecutiveNNRs) == 0)
    return GetConsecutiveNNRsTask();

  NOTREACHED() << " Unknown learning task:" << task_name;
}

// static
void MediaLearningTasks::Register(
    base::RepeatingCallback<void(const LearningTask&)> cb) {
  cb.Run(Get(tasknames::kWillPlay));
  cb.Run(Get(tasknames::kConsecutiveBadWindows));
  cb.Run(Get(tasknames::kConsecutiveNNRs));
}

}  // namespace learning
}  // namespace media

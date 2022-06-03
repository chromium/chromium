// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/media_learning_tasks.h"

#include <memory>

#include "base/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {
namespace learning {

class MediaLearningTasksTest : public testing::Test {};

TEST_F(MediaLearningTasksTest, WillPlayTask) {
  LearningTask task = MediaLearningTasks::Get(tasknames::kWillPlay);
  // Make sure the name is correct, mostly to reduce cut-and-paste errors.
  EXPECT_EQ(task.name, "MediaLearningWillPlay");
}

TEST_F(MediaLearningTasksTest, ConsecutiveBadWindowsTask) {
  LearningTask task =
      MediaLearningTasks::Get(tasknames::kConsecutiveBadWindows);
  // Make sure the name is correct, mostly to reduce cut-and-paste errors.
  EXPECT_EQ(task.name, "MediaLearningConsecutiveBadWindows");
}

TEST_F(MediaLearningTasksTest, ConsecutiveNNRsTask) {
  LearningTask task = MediaLearningTasks::Get(tasknames::kConsecutiveNNRs);
  // Make sure the name is correct, mostly to reduce cut-and-paste errors.
  EXPECT_EQ(task.name, "MediaLearningConsecutiveNNRs");
}

TEST_F(MediaLearningTasksTest, EnumeratesAllTasks) {
  int count = 0;
  auto cb = base::BindRepeating(
      [](int* count, const LearningTask& task) { (*count)++; },
      base::Unretained(&count));
  MediaLearningTasks::Register(std::move(cb));
  EXPECT_EQ(count, 3);
}

}  // namespace learning
}  // namespace media

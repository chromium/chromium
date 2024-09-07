// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_task_queue.h"

#include "components/webapps/browser/installable/installable_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

// A POD struct which holds booleans for creating and comparing against
// a (move-only) InstallableTask.
struct TaskParams {
  bool valid_manifest = false;
  bool valid_primary_icon = false;
};

// Constructs an InstallableTask, with the supplied bools stored in it.
std::unique_ptr<InstallableTask> CreateTask(const TaskParams& task_params) {
  InstallableParams params;
  params.installable_criteria =
      task_params.valid_manifest ? InstallableCriteria::kValidManifestWithIcons
                                 : InstallableCriteria::kDoNotCheck;
  params.valid_primary_icon = task_params.valid_primary_icon;

  InstallablePageData page_data;
  return std::make_unique<InstallableTask>(params, page_data);
}

bool IsEqual(const TaskParams& params, const InstallableTask& task) {
  return (task.params().installable_criteria ==
          InstallableCriteria::kValidManifestWithIcons) ==
             params.valid_manifest &&
         task.params().valid_primary_icon == params.valid_primary_icon;
}

class InstallableTaskQueueUnitTest : public testing::Test {};

TEST_F(InstallableTaskQueueUnitTest, NextDiscardsTask) {
  InstallableTaskQueue task_queue;
  TaskParams task1 = {.valid_manifest = false,
                      .valid_primary_icon = false};
  TaskParams task2 = {.valid_manifest = true, .valid_primary_icon = true};

  task_queue.Add(CreateTask(task1));
  task_queue.Add(CreateTask(task2));

  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));
  task_queue.Next();
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  task_queue.Next();
  // No more task exist.
  EXPECT_FALSE(task_queue.HasCurrent());
}

}  // namespace webapps

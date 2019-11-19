// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/task_pump.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "jingle/glue/mock_task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace jingle_glue {

namespace {

using ::testing::Return;

class TaskPumpTest : public testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(TaskPumpTest, Basic) {
  TaskPump task_pump;
  MockTask* task = new MockTask(&task_pump);
  // We have to do this since the state enum is protected in
  // rtc::Task.
  const int TASK_STATE_DONE = 2;
  EXPECT_CALL(*task, ProcessStart()).WillOnce(Return(TASK_STATE_DONE));
  task->Start();

  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskPumpTest, Stop) {
  TaskPump task_pump;
  MockTask* task = new MockTask(&task_pump);
  // We have to do this since the state enum is protected in
  // rtc::Task.
  const int TASK_STATE_ERROR = 3;
  ON_CALL(*task, ProcessStart()).WillByDefault(Return(TASK_STATE_ERROR));
  EXPECT_CALL(*task, ProcessStart()).Times(0);
  task->Start();

  task_pump.Stop();
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace jingle_glue

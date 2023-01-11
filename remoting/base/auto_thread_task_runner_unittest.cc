// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auto_thread_task_runner.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void SetFlagTask(bool* success) {
  *success = true;
}

}  // namespace

namespace remoting {

TEST(AutoThreadTaskRunnerTest, StartAndStop) {
  // Create a task runner.
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  scoped_refptr<AutoThreadTaskRunner> task_runner = new AutoThreadTaskRunner(
      task_environment.GetMainThreadTaskRunner(), run_loop.QuitClosure());

  // Post a task to make sure it is executed.
  bool success = false;
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetFlagTask, &success));

  task_runner.reset();
  run_loop.Run();
  EXPECT_TRUE(success);
}

}  // namespace remoting

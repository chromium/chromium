// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {
namespace {

// Number of tasks in each chain posted by `PostChain()`.
constexpr int kChainLength = 3;

// Increments `count`, then posts the next task of the chain while
// `remaining_tasks` is positive.
void RunChainTask(int* count, int remaining_tasks) {
  ++*count;
  if (remaining_tasks == 0) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RunChainTask, base::Unretained(count),
                                remaining_tasks - 1));
}

// Posts a chain of `kChainLength` tasks, each posting the next one, that each
// increment `count`.
void PostChain(int* count) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunChainTask, base::Unretained(count), kChainLength - 1));
}

}  // namespace

class ActorTestUtilsTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// Tests that `RunAlreadyPostedTasks()` runs every task already posted, but not
// the tasks they post.
TEST_F(ActorTestUtilsTest, RunAlreadyPostedTasksRunsOneLevel) {
  int count = 0;
  PostChain(&count);
  PostChain(&count);

  RunAlreadyPostedTasks();

  EXPECT_EQ(2, count);
}

}  // namespace actor

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

void Increment(int* x) {
  ++*x;
}

void GetIsActive(bool* is_active, TaskHandle* handle) {
  *is_active = handle->IsActive();
}

class CancellationTestHelper {
  DISALLOW_NEW();

 public:
  CancellationTestHelper() {}

  base::WeakPtr<CancellationTestHelper> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void RevokeWeakPtrs() { weak_ptr_factory_.InvalidateWeakPtrs(); }
  void IncrementCounter() { ++counter_; }
  int Counter() const { return counter_; }

 private:
  int counter_ = 0;
  base::WeakPtrFactory<CancellationTestHelper> weak_ptr_factory_{this};
};

}  // namespace

TEST(WebTaskRunnerTest, PostCancellableTaskTest) {
  scoped_refptr<scheduler::FakeTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();

  // Run without cancellation.
  int count = 0;
  TaskHandle handle =
      PostCancellableTask(*task_runner, FROM_HERE,
                          WTF::BindOnce(&Increment, WTF::Unretained(&count)));
  EXPECT_EQ(0, count);
  EXPECT_TRUE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, count);
  EXPECT_FALSE(handle.IsActive());

  count = 0;
  handle = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&Increment, WTF::Unretained(&count)),
      base::Milliseconds(1));
  EXPECT_EQ(0, count);
  EXPECT_TRUE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, count);
  EXPECT_FALSE(handle.IsActive());

  count = 0;
  handle = PostNonNestableCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&Increment, WTF::Unretained(&count)));
  EXPECT_EQ(0, count);
  EXPECT_TRUE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, count);
  EXPECT_FALSE(handle.IsActive());

  count = 0;
  handle = PostNonNestableDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&Increment, WTF::Unretained(&count)),
      base::Milliseconds(1));
  EXPECT_EQ(0, count);
  EXPECT_TRUE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, count);
  EXPECT_FALSE(handle.IsActive());

  // Cancel a task.
  count = 0;
  handle =
      PostCancellableTask(*task_runner, FROM_HERE,
                          WTF::BindOnce(&Increment, WTF::Unretained(&count)));
  handle.Cancel();
  EXPECT_EQ(0, count);
  EXPECT_FALSE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_EQ(0, count);

  count = 0;
  handle = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&Increment, WTF::Unretained(&count)),
      base::Milliseconds(1));
  handle.Cancel();
  EXPECT_EQ(0, count);
  EXPECT_FALSE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_EQ(0, count);

  count = 0;
  handle = PostNonNestableCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&Increment, WTF::Unretained(&count)));
  handle.Cancel();
  EXPECT_EQ(0, count);
  EXPECT_FALSE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_EQ(0, count);

  count = 0;
  handle = PostNonNestableDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&Increment, WTF::Unretained(&count)),
      base::Milliseconds(1));
  handle.Cancel();
  EXPECT_EQ(0, count);
  EXPECT_FALSE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_EQ(0, count);

  // The task should be cancelled when the handle is dropped.
  {
    count = 0;
    TaskHandle handle2 =
        PostCancellableTask(*task_runner, FROM_HERE,
                            WTF::BindOnce(&Increment, WTF::Unretained(&count)));
    EXPECT_TRUE(handle2.IsActive());
  }
  EXPECT_EQ(0, count);
  task_runner->RunUntilIdle();
  EXPECT_EQ(0, count);

  // The task should be cancelled when another TaskHandle is assigned on it.
  count = 0;
  handle =
      PostCancellableTask(*task_runner, FROM_HERE,
                          WTF::BindOnce(&Increment, WTF::Unretained(&count)));
  handle = PostCancellableTask(*task_runner, FROM_HERE, WTF::BindOnce([] {}));
  EXPECT_EQ(0, count);
  task_runner->RunUntilIdle();
  EXPECT_EQ(0, count);

  // Self assign should be nop.
  count = 0;
  handle =
      PostCancellableTask(*task_runner, FROM_HERE,
                          WTF::BindOnce(&Increment, WTF::Unretained(&count)));
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  handle = std::move(handle);
#pragma GCC diagnostic pop
#else
  handle = std::move(handle);
#endif  // defined(__clang__)
  EXPECT_EQ(0, count);
  task_runner->RunUntilIdle();
  EXPECT_EQ(1, count);

  // handle->isActive() should switch to false before the task starts running.
  bool is_active = false;
  handle = PostCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&GetIsActive, WTF::Unretained(&is_active),
                    WTF::Unretained(&handle)));
  EXPECT_TRUE(handle.IsActive());
  task_runner->RunUntilIdle();
  EXPECT_FALSE(is_active);
  EXPECT_FALSE(handle.IsActive());
}

TEST(WebTaskRunnerTest, CancellationCheckerTest) {
  scoped_refptr<scheduler::FakeTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();

  int count = 0;
  TaskHandle handle =
      PostCancellableTask(*task_runner, FROM_HERE,
                          WTF::BindOnce(&Increment, WTF::Unretained(&count)));
  EXPECT_EQ(0, count);

  // TaskHandle::isActive should detect the deletion of posted task.
  auto queue = task_runner->TakePendingTasksForTesting();
  ASSERT_EQ(1u, queue.size());
  EXPECT_FALSE(queue[0].first.IsCancelled());
  EXPECT_TRUE(handle.IsActive());
  queue.clear();
  EXPECT_FALSE(handle.IsActive());
  EXPECT_EQ(0, count);

  count = 0;
  CancellationTestHelper helper;
  handle = PostCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&CancellationTestHelper::IncrementCounter,
                    helper.GetWeakPtr()));
  EXPECT_EQ(0, helper.Counter());

  // The cancellation of the posted task should be propagated to TaskHandle.
  queue = task_runner->TakePendingTasksForTesting();
  ASSERT_EQ(1u, queue.size());
  EXPECT_FALSE(queue[0].first.IsCancelled());
  EXPECT_TRUE(handle.IsActive());
  helper.RevokeWeakPtrs();
  EXPECT_TRUE(queue[0].first.IsCancelled());
  EXPECT_FALSE(handle.IsActive());
}

}  // namespace blink

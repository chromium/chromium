// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/frame_queue.h"

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class FrameQueueTest : public testing::Test {
 public:
  FrameQueueTest() : io_task_runner_(Platform::Current()->GetIOTaskRunner()) {}

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};

TEST_F(FrameQueueTest, PushPopMatches) {
  const int kMaxSize = 5;
  scoped_refptr<FrameQueue<int>> queue =
      base::MakeRefCounted<FrameQueue<int>>(kMaxSize);
  for (int i = 0; i < kMaxSize; ++i)
    queue->Push(i);
  for (int i = 0; i < kMaxSize; ++i) {
    std::optional<int> element = queue->Pop();
    EXPECT_TRUE(element.has_value());
    EXPECT_EQ(*element, i);
  }
}

TEST_F(FrameQueueTest, PushReturnsReplacedElement) {
  const int kMaxSize = 2;
  scoped_refptr<FrameQueue<int>> queue =
      base::MakeRefCounted<FrameQueue<int>>(kMaxSize);
  std::optional<int> replaced = queue->Push(1);
  EXPECT_FALSE(replaced.has_value());

  replaced = queue->Push(2);
  EXPECT_FALSE(replaced.has_value());

  replaced = queue->Push(3);
  EXPECT_TRUE(replaced.has_value());
  EXPECT_EQ(replaced.value(), 1);

  replaced = queue->Push(4);
  EXPECT_TRUE(replaced.has_value());
  EXPECT_EQ(replaced.value(), 2);
}

TEST_F(FrameQueueTest, EmptyQueueReturnsNullopt) {
  scoped_refptr<FrameQueue<int>> queue =
      base::MakeRefCounted<FrameQueue<int>>(5);
  std::optional<int> element = queue->Pop();
  EXPECT_FALSE(element.has_value());
}

TEST_F(FrameQueueTest, QueueDropsOldElements) {
  const int kMaxSize = 5;
  const int kNumInsertions = 10;
  scoped_refptr<FrameQueue<int>> queue =
      base::MakeRefCounted<FrameQueue<int>>(kMaxSize);
  for (int i = 0; i < kNumInsertions; ++i)
    queue->Push(i);
  for (int i = 0; i < kMaxSize; ++i) {
    std::optional<int> element = queue->Pop();
    EXPECT_TRUE(element.has_value());
    EXPECT_EQ(*element, kNumInsertions - kMaxSize + i);
  }
  EXPECT_TRUE(queue->IsEmpty());
  EXPECT_FALSE(queue->Pop().has_value());
}

TEST_F(FrameQueueTest, FrameQueueHandle) {
  const int kMaxSize = 5;
  scoped_refptr<FrameQueue<int>> original_queue =
      base::MakeRefCounted<FrameQueue<int>>(kMaxSize);
  FrameQueueHandle<int> handle1(original_queue);
  FrameQueueHandle<int> handle2(std::move(original_queue));

  for (int i = 0; i < kMaxSize; ++i) {
    auto queue = handle1.Queue();
    EXPECT_TRUE(queue);
    queue->Push(i);
  }
  for (int i = 0; i < kMaxSize; ++i) {
    auto queue = handle2.Queue();
    EXPECT_TRUE(queue);
    std::optional<int> element = queue->Pop();
    EXPECT_TRUE(element.has_value());
    EXPECT_EQ(*element, i);
  }

  EXPECT_TRUE(handle1.Queue());
  handle1.Invalidate();
  EXPECT_FALSE(handle1.Queue());

  EXPECT_TRUE(handle2.Queue());
  handle2.Invalidate();
  EXPECT_FALSE(handle2.Queue());
}

TEST_F(FrameQueueTest, PushValuesInOrderOnSeparateThread) {
  const int kMaxSize = 3;
  const int kNumElements = 100;
  scoped_refptr<FrameQueue<int>> original_queue =
      base::MakeRefCounted<FrameQueue<int>>(kMaxSize);
  FrameQueueHandle<int> handle1(std::move(original_queue));
  FrameQueueHandle<int> handle2(handle1.Queue());

  base::WaitableEvent start_event;
  base::WaitableEvent end_event;
  PostCrossThreadTask(
      *io_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          [](FrameQueueHandle<int>* handle, base::WaitableEvent* start_event,
             base::WaitableEvent* end_event) {
            auto queue = handle->Queue();
            EXPECT_TRUE(queue);
            start_event->Signal();
            for (int i = 0; i < kNumElements; ++i)
              queue->Push(i);
            handle->Invalidate();
            end_event->Signal();
          },
          CrossThreadUnretained(&handle1), CrossThreadUnretained(&start_event),
          CrossThreadUnretained(&end_event)));

  auto queue = handle2.Queue();
  int last_value_read = -1;
  start_event.Wait();
  for (int i = 0; i < kNumElements; ++i) {
    std::optional<int> element = queue->Pop();
    if (element) {
      EXPECT_GE(*element, 0);
      EXPECT_LT(*element, kNumElements);
      EXPECT_GT(*element, last_value_read);
      last_value_read = *element;
    }
  }
  end_event.Wait();
  EXPECT_FALSE(handle1.Queue());
  EXPECT_TRUE(handle2.Queue());

  int num_read = 0;
  while (!queue->IsEmpty()) {
    std::optional<int> element = queue->Pop();
    EXPECT_TRUE(element.has_value());
    EXPECT_GE(*element, 0);
    EXPECT_LT(*element, kNumElements);
    EXPECT_GT(*element, last_value_read);
    last_value_read = *element;
    num_read++;
  }
  EXPECT_LE(num_read, kMaxSize);
}

TEST_F(FrameQueueTest, LockedOperations) {
  const int kMaxSize = 1;
  scoped_refptr<FrameQueue<int>> queue =
      base::MakeRefCounted<FrameQueue<int>>(kMaxSize);
  base::AutoLock locker(queue->GetLock());
  EXPECT_TRUE(queue->IsEmptyLocked());

  std::optional<int> peeked = queue->PeekLocked();
  EXPECT_FALSE(peeked.has_value());

  std::optional<int> popped = queue->PushLocked(1);
  EXPECT_FALSE(popped.has_value());
  EXPECT_FALSE(queue->IsEmptyLocked());

  peeked = queue->PeekLocked();
  EXPECT_TRUE(peeked.has_value());
  EXPECT_EQ(peeked.value(), 1);
  EXPECT_FALSE(queue->IsEmptyLocked());

  popped = queue->PushLocked(2);
  EXPECT_TRUE(popped.has_value());
  EXPECT_EQ(popped.value(), 1);

  peeked = queue->PeekLocked();
  EXPECT_TRUE(peeked.has_value());
  EXPECT_EQ(peeked.value(), 2);
  EXPECT_FALSE(queue->IsEmptyLocked());

  popped = queue->PopLocked();
  EXPECT_TRUE(popped.has_value());
  EXPECT_EQ(popped.value(), 2);
  EXPECT_TRUE(queue->IsEmptyLocked());

  peeked = queue->PeekLocked();
  EXPECT_FALSE(peeked.has_value());
}

}  // namespace blink

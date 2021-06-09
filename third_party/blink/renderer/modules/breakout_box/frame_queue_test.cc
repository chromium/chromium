// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/frame_queue.h"

#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class FrameQueueTest : public testing::Test {
 public:
  FrameQueueTest() : io_task_runner_(Platform::Current()->GetIOTaskRunner()) {}

 protected:
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
    absl::optional<int> element = queue->Pop();
    EXPECT_TRUE(element.has_value());
    EXPECT_EQ(*element, i);
  }
}

TEST_F(FrameQueueTest, EmptyQueueReturnsNullopt) {
  scoped_refptr<FrameQueue<int>> queue =
      base::MakeRefCounted<FrameQueue<int>>(5);
  absl::optional<int> element = queue->Pop();
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
    absl::optional<int> element = queue->Pop();
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
    absl::optional<int> element = queue->Pop();
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
    absl::optional<int> element = queue->Pop();
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
    absl::optional<int> element = queue->Pop();
    EXPECT_TRUE(element.has_value());
    EXPECT_GE(*element, 0);
    EXPECT_LT(*element, kNumElements);
    EXPECT_GT(*element, last_value_read);
    last_value_read = *element;
    num_read++;
  }
  EXPECT_LE(num_read, kMaxSize);
}

}  // namespace blink

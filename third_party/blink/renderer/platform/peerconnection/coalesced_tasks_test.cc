// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/coalesced_tasks.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::MockFunction;

TEST(CoalescedTasksTest, TaskRunInOrder) {
  std::vector<std::string> run_tasks;

  MockFunction<void()> first_callback;
  EXPECT_CALL(first_callback, Call()).WillOnce([&]() {
    run_tasks.emplace_back("first");
  });
  MockFunction<void()> second_callback;
  EXPECT_CALL(second_callback, Call()).WillOnce([&]() {
    run_tasks.emplace_back("second");
  });
  MockFunction<void()> third_callback;
  EXPECT_CALL(third_callback, Call()).WillOnce([&]() {
    run_tasks.emplace_back("third");
  });

  base::TimeTicks now;
  base::TimeTicks scheduled_time = now + base::Milliseconds(10);

  CoalescedTasks coalesced_tasks;
  coalesced_tasks.QueueDelayedTask(now + base::Milliseconds(5),
                                   second_callback.AsStdFunction(),
                                   scheduled_time);
  coalesced_tasks.QueueDelayedTask(now + base::Milliseconds(1),
                                   first_callback.AsStdFunction(),
                                   scheduled_time);
  coalesced_tasks.QueueDelayedTask(now + base::Milliseconds(9),
                                   third_callback.AsStdFunction(),
                                   scheduled_time);
  coalesced_tasks.RunScheduledTasks(scheduled_time);

  EXPECT_THAT(run_tasks, ElementsAre("first", "second", "third"));
}

TEST(CoalescedTasksTest, OnlyReadyTasksRun) {
  std::vector<std::string> run_tasks;

  MockFunction<void()> first_callback;
  EXPECT_CALL(first_callback, Call()).WillOnce([&]() {
    run_tasks.emplace_back("first");
  });
  MockFunction<void()> second_callback;
  EXPECT_CALL(second_callback, Call()).WillOnce([&]() {
    run_tasks.emplace_back("second");
  });
  MockFunction<void()> third_callback;
  EXPECT_CALL(third_callback, Call()).WillOnce([&]() {
    run_tasks.emplace_back("third");
  });

  base::TimeTicks now;
  base::TimeTicks first_scheduled_time = now + base::Milliseconds(10);
  base::TimeTicks second_scheduled_time = now + base::Milliseconds(20);

  CoalescedTasks coalesced_tasks;
  coalesced_tasks.QueueDelayedTask(now + base::Milliseconds(11),
                                   third_callback.AsStdFunction(),
                                   second_scheduled_time);
  coalesced_tasks.QueueDelayedTask(now + base::Milliseconds(9),
                                   first_callback.AsStdFunction(),
                                   first_scheduled_time);
  coalesced_tasks.QueueDelayedTask(now + base::Milliseconds(10),
                                   second_callback.AsStdFunction(),
                                   first_scheduled_time);

  coalesced_tasks.RunScheduledTasks(first_scheduled_time);
  EXPECT_THAT(run_tasks, ElementsAre("first", "second"));
  run_tasks.clear();

  coalesced_tasks.RunScheduledTasks(second_scheduled_time);
  EXPECT_THAT(run_tasks, ElementsAre("third"));
  run_tasks.clear();
}

TEST(CoalescedTasksTest, QueueDelayedTaskReturnsTrueWhenSchedulingIsNeeded) {
  MockFunction<void()> dummy_callback;
  EXPECT_CALL(dummy_callback, Call()).WillRepeatedly([]() {});

  base::TimeTicks now;
  base::TimeTicks first_scheduled_time = now + base::Milliseconds(1);
  base::TimeTicks second_scheduled_time = now + base::Milliseconds(2);

  CoalescedTasks coalesced_tasks;
  // `second_scheduled_time` needs to be scheduled.
  EXPECT_TRUE(coalesced_tasks.QueueDelayedTask(second_scheduled_time,
                                               dummy_callback.AsStdFunction(),
                                               second_scheduled_time));
  // `second_scheduled_time` does not need to be scheduled multiple times.
  EXPECT_FALSE(coalesced_tasks.QueueDelayedTask(second_scheduled_time,
                                                dummy_callback.AsStdFunction(),
                                                second_scheduled_time));
  // `first_scheduled_time` needs to be scheduled.
  EXPECT_TRUE(coalesced_tasks.QueueDelayedTask(first_scheduled_time,
                                               dummy_callback.AsStdFunction(),
                                               first_scheduled_time));
  // `first_scheduled_time` does not need to be scheduled multiple times.
  EXPECT_FALSE(coalesced_tasks.QueueDelayedTask(first_scheduled_time,
                                                dummy_callback.AsStdFunction(),
                                                first_scheduled_time));

  coalesced_tasks.RunScheduledTasks(first_scheduled_time);
  // `first_scheduled_time` is no longer scheduled, so this returns true.
  EXPECT_TRUE(coalesced_tasks.QueueDelayedTask(first_scheduled_time,
                                               dummy_callback.AsStdFunction(),
                                               first_scheduled_time));
  // `second_scheduled_time` is still scheduled.
  EXPECT_FALSE(coalesced_tasks.QueueDelayedTask(second_scheduled_time,
                                                dummy_callback.AsStdFunction(),
                                                second_scheduled_time));

  coalesced_tasks.RunScheduledTasks(second_scheduled_time);
  // `second_scheduled_time` is no longer scheduled, so this returns true.
  EXPECT_TRUE(coalesced_tasks.QueueDelayedTask(second_scheduled_time,
                                               dummy_callback.AsStdFunction(),
                                               second_scheduled_time));

  coalesced_tasks.Clear();
}

TEST(CoalescedTasksTest, PrepareAndFinalizeCallbacks) {
  std::vector<std::string> run_tasks;

  CoalescedTasks::PrepareRunTaskCallback prepare_callback = base::BindRepeating(
      [](std::vector<std::string>* run_tasks) {
        run_tasks->emplace_back("prepare");
        return std::optional<base::TimeTicks>(base::TimeTicks() +
                                              base::Milliseconds(1337));
      },
      base::Unretained(&run_tasks));
  MockFunction<void()> task_callback;
  EXPECT_CALL(task_callback, Call()).WillOnce([&]() {
    run_tasks.emplace_back("task");
  });
  CoalescedTasks::FinalizeRunTaskCallback finalize_callback =
      base::BindRepeating(
          [](std::vector<std::string>* run_tasks,
             std::optional<base::TimeTicks> ticks) {
            run_tasks->emplace_back("finalize");
            // Ticks should be the same value that `prepare_callback` returned.
            EXPECT_TRUE(ticks.has_value());
            EXPECT_EQ(ticks.value(),
                      base::TimeTicks() + base::Milliseconds(1337));
          },
          base::Unretained(&run_tasks));

  base::TimeTicks now;
  CoalescedTasks coalesced_tasks;
  coalesced_tasks.QueueDelayedTask(now, task_callback.AsStdFunction(), now);
  coalesced_tasks.RunScheduledTasks(now, prepare_callback, finalize_callback);

  EXPECT_THAT(run_tasks, ElementsAre("prepare", "task", "finalize"));
}

}  // namespace blink

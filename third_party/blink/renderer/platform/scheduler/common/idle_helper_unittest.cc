// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/idle_helper.h"

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Exactly;
using testing::Invoke;
using testing::Return;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace idle_helper_unittest {

using base::sequence_manager::TaskQueue;
using base::sequence_manager::SequenceManager;

void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(std::vector<std::string>* vector,
                                std::string value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void NullTask() {}

void NullIdleTask(base::TimeTicks deadline) {}

void AppendToVectorReentrantTask(base::SingleThreadTaskRunner* task_runner,
                                 std::vector<int>* vector,
                                 int* reentrant_count,
                                 int max_reentrant_count) {
  vector->push_back((*reentrant_count)++);
  if (*reentrant_count < max_reentrant_count) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(AppendToVectorReentrantTask,
                                         base::Unretained(task_runner), vector,
                                         reentrant_count, max_reentrant_count));
  }
}

void IdleTestTask(int* run_count,
                  base::TimeTicks* deadline_out,
                  base::TimeTicks deadline) {
  (*run_count)++;
  *deadline_out = deadline;
}

int g_max_idle_task_reposts = 2;

void RepostingIdleTestTask(SingleThreadIdleTaskRunner* idle_task_runner,
                           int* run_count,
                           base::TimeTicks* deadline_out,
                           base::TimeTicks deadline) {
  if ((*run_count + 1) < g_max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE, base::BindOnce(&RepostingIdleTestTask,
                                  base::Unretained(idle_task_runner), run_count,
                                  deadline_out));
  }
  *deadline_out = deadline;
  (*run_count)++;
}

void RepostingUpdateClockIdleTestTask(
    SingleThreadIdleTaskRunner* idle_task_runner,
    int* run_count,
    scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner,
    base::TimeDelta advance_time,
    std::vector<base::TimeTicks>* deadlines,
    base::TimeTicks deadline) {
  if ((*run_count + 1) < g_max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE, base::BindOnce(&RepostingUpdateClockIdleTestTask,
                                  base::Unretained(idle_task_runner), run_count,
                                  test_task_runner, advance_time, deadlines));
  }
  deadlines->push_back(deadline);
  (*run_count)++;
  test_task_runner->AdvanceMockTickClock(advance_time);
}

void RepeatingTask(base::SingleThreadTaskRunner* task_runner,
                   int num_repeats,
                   base::TimeDelta delay) {
  if (num_repeats > 1) {
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RepeatingTask, base::Unretained(task_runner),
                       num_repeats - 1, delay),
        delay);
  }
}

void UpdateClockIdleTestTask(
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
    int* run_count,
    base::TimeTicks set_time,
    base::TimeTicks deadline) {
  task_runner->AdvanceMockTickClock(set_time - task_runner->NowTicks());
  (*run_count)++;
}

void UpdateClockToDeadlineIdleTestTask(
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
    int* run_count,
    base::TimeTicks deadline) {
  UpdateClockIdleTestTask(task_runner, run_count, deadline, deadline);
}

void EndIdlePeriodIdleTask(IdleHelper* idle_helper, base::TimeTicks deadline) {
  idle_helper->EndIdlePeriod();
}

void ShutdownIdleTask(IdleHelper* helper,
                      bool* shutdown_task_run,
                      base::TimeTicks deadline) {
  *shutdown_task_run = true;
  helper->Shutdown();
}

class IdleHelperForTest : public IdleHelper, public IdleHelper::Delegate {
 public:
  explicit IdleHelperForTest(
      SchedulerHelper* scheduler_helper,
      base::TimeDelta required_quiescence_duration_before_long_idle_period,
      scoped_refptr<TaskQueue> idle_task_runner)
      : IdleHelper(scheduler_helper,
                   this,
                   "TestSchedulerIdlePeriod",
                   required_quiescence_duration_before_long_idle_period,
                   idle_task_runner) {}

  ~IdleHelperForTest() override = default;

  // IdleHelper::Delegate implementation:
  MOCK_METHOD2(CanEnterLongIdlePeriod,
               bool(base::TimeTicks now,
                    base::TimeDelta* next_long_idle_period_delay_out));

  MOCK_METHOD0(IsNotQuiescent, void());
  MOCK_METHOD0(OnIdlePeriodStarted, void());
  MOCK_METHOD0(OnIdlePeriodEnded, void());
  MOCK_METHOD1(OnPendingTasksChanged, void(bool has_tasks));
};

class BaseIdleHelperTest : public testing::Test {
 public:
  BaseIdleHelperTest(
      std::unique_ptr<base::MessageLoop> message_loop,
      base::TimeDelta required_quiescence_duration_before_long_idle_period)
      : message_loop_(std::move(message_loop)),
        test_task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kStandalone)) {
    std::unique_ptr<SequenceManager> sequence_manager;
    if (!message_loop_) {
      sequence_manager = base::sequence_manager::SequenceManagerForTest::Create(
          nullptr, test_task_runner_, test_task_runner_->GetMockTickClock());
    } else {
      // It's okay to use |test_task_runner_| just as a mock clock because
      // it isn't bound to thread and all tasks will go through a MessageLoop.
      sequence_manager = base::sequence_manager::SequenceManagerForTest::Create(
          message_loop_.get(), message_loop_->task_runner(),
          test_task_runner_->GetMockTickClock());
    }
    sequence_manager_ = sequence_manager.get();
    scheduler_helper_ = std::make_unique<NonMainThreadSchedulerHelper>(
        std::move(sequence_manager), nullptr, TaskType::kInternalTest);
    idle_helper_ = std::make_unique<IdleHelperForTest>(
        scheduler_helper_.get(),
        required_quiescence_duration_before_long_idle_period,
        scheduler_helper_->NewTaskQueue(TaskQueue::Spec("idle_test")));
    default_task_queue_ = scheduler_helper_->DefaultNonMainThreadTaskQueue();
    default_task_runner_ = default_task_queue_->CreateTaskRunner(0);
    idle_task_runner_ = idle_helper_->IdleTaskRunner();
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMicroseconds(5000));
  }

  ~BaseIdleHelperTest() override = default;

  void SetUp() override {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(_)).Times(AnyNumber());
  }

  void TearDown() override {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(AnyNumber());
    idle_helper_->Shutdown();
    test_task_runner_->FastForwardUntilNoTasksRemain();
  }

  SequenceManager* sequence_manager() const { return sequence_manager_; }

  template <typename E>
  static void CallForEachEnumValue(E first,
                                   E last,
                                   const char* (*function)(E)) {
    for (E val = first; val < last;
         val = static_cast<E>(static_cast<int>(val) + 1)) {
      (*function)(val);
    }
  }

  static void CheckAllTaskQueueIdToString() {
    CallForEachEnumValue<IdleHelper::IdlePeriodState>(
        IdleHelper::IdlePeriodState::kFirstIdlePeriodState,
        IdleHelper::IdlePeriodState::kIdlePeriodStateCount,
        &IdleHelper::IdlePeriodStateToString);
  }

  bool IsInIdlePeriod() const {
    return idle_helper_->IsInIdlePeriod(
        idle_helper_->SchedulerIdlePeriodState());
  }

 protected:
  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMaximumIdlePeriodMillis);
  }

  static base::TimeDelta retry_enable_long_idle_period_delay() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kRetryEnableLongIdlePeriodDelayMillis);
  }

  static base::TimeDelta minimum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMinimumIdlePeriodDurationMillis);
  }

  base::TimeTicks CurrentIdleTaskDeadline() {
    return idle_helper_->CurrentIdleTaskDeadline();
  }

  void CheckIdlePeriodStateIs(const char* expected) {
    EXPECT_STREQ(expected, IdleHelper::IdlePeriodStateToString(
                               idle_helper_->SchedulerIdlePeriodState()));
  }

  const scoped_refptr<TaskQueue>& idle_queue() const {
    return idle_helper_->idle_queue_;
  }

  std::unique_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  std::unique_ptr<NonMainThreadSchedulerHelper> scheduler_helper_;
  SequenceManager* sequence_manager_;  // Owned by scheduler_helper_.
  std::unique_ptr<IdleHelperForTest> idle_helper_;
  scoped_refptr<base::sequence_manager::TaskQueue> default_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BaseIdleHelperTest);
};

class IdleHelperTest : public BaseIdleHelperTest {
 public:
  IdleHelperTest() : BaseIdleHelperTest(nullptr, base::TimeDelta()) {}

  ~IdleHelperTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleHelperTest);
};

TEST_F(IdleHelperTest, TestPostIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->StartIdlePeriod(IdleHelper::IdlePeriodState::kInShortIdlePeriod,
                                test_task_runner_->NowTicks(),
                                expected_deadline);
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperTest, TestPostIdleTask_EndIdlePeriod) {
  int run_count = 0;
  base::TimeTicks deadline_in_task;

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  idle_helper_->EndIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);
}

TEST_F(IdleHelperTest, TestRepostingIdleTask) {
  base::TimeTicks actual_deadline;
  int run_count = 0;

  g_max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&RepostingIdleTestTask,
                                base::RetainedRef(idle_task_runner_),
                                &run_count, &actual_deadline));
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Reposted tasks shouldn't run until next idle period.
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(IdleHelperTest, TestIdleTaskExceedsDeadline) {
  int run_count = 0;

  // Post two UpdateClockToDeadlineIdleTestTask tasks.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&UpdateClockToDeadlineIdleTestTask,
                                test_task_runner_, &run_count));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&UpdateClockToDeadlineIdleTestTask,
                                test_task_runner_, &run_count));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  test_task_runner_->RunUntilIdle();
  // Only the first idle task should execute since it's used up the deadline.
  EXPECT_EQ(1, run_count);

  idle_helper_->EndIdlePeriod();
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  test_task_runner_->RunUntilIdle();
  // Second task should be run on the next idle period.
  EXPECT_EQ(2, run_count);
}

class IdleHelperTestWithIdlePeriodObserver : public BaseIdleHelperTest {
 public:
  IdleHelperTestWithIdlePeriodObserver()
      : BaseIdleHelperTest(nullptr, base::TimeDelta()) {}

  ~IdleHelperTestWithIdlePeriodObserver() override = default;

  void SetUp() override {
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(_)).Times(AnyNumber());
  }

  void ExpectIdlePeriodStartsButNeverEnds() {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(1);
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(0);
  }

  void ExpectIdlePeriodStartsAndEnds(const testing::Cardinality& cardinality) {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(cardinality);
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(cardinality);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleHelperTestWithIdlePeriodObserver);
};

TEST_F(IdleHelperTestWithIdlePeriodObserver, TestEnterButNotExitIdlePeriod) {
  ExpectIdlePeriodStartsButNeverEnds();

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
}

TEST_F(IdleHelperTestWithIdlePeriodObserver, TestEnterAndExitIdlePeriod) {
  BaseIdleHelperTest* fixture = this;
  ON_CALL(*idle_helper_, OnIdlePeriodStarted())
      .WillByDefault(
          Invoke([fixture]() { EXPECT_TRUE(fixture->IsInIdlePeriod()); }));
  ON_CALL(*idle_helper_, OnIdlePeriodEnded()).WillByDefault(Invoke([fixture]() {
    EXPECT_FALSE(fixture->IsInIdlePeriod());
  }));

  ExpectIdlePeriodStartsAndEnds(Exactly(1));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  idle_helper_->EndIdlePeriod();
}

class IdleHelperWithMessageLoopTest : public BaseIdleHelperTest {
 public:
  IdleHelperWithMessageLoopTest()
      : BaseIdleHelperTest(std::make_unique<base::MessageLoop>(),
                           base::TimeDelta()) {}
  ~IdleHelperWithMessageLoopTest() override = default;

  void PostFromNestedRunloop(
      std::vector<std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>>*
          tasks) {
    for (std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>& pair : *tasks) {
      if (pair.second) {
        idle_task_runner_->PostIdleTask(FROM_HERE, std::move(pair.first));
      } else {
        idle_task_runner_->PostNonNestableIdleTask(FROM_HERE,
                                                   std::move(pair.first));
      }
    }
    idle_helper_->StartIdlePeriod(
        IdleHelper::IdlePeriodState::kInShortIdlePeriod,
        test_task_runner_->NowTicks(),
        test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
    base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  }

  void SetUp() override {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(_)).Times(AnyNumber());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleHelperWithMessageLoopTest);
};

TEST_F(IdleHelperWithMessageLoopTest,
       NonNestableIdleTaskDoesntExecuteInNestedLoop) {
  std::vector<std::string> order;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("1")));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("2")));

  std::vector<std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("3")),
      false));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("4")),
      true));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("5")),
      true));

  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IdleHelperWithMessageLoopTest::PostFromNestedRunloop,
                     base::Unretained(this),
                     base::Unretained(&tasks_to_post_from_nested_loop)));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  base::RunLoop().RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(order, testing::ElementsAre(std::string("1"), std::string("2"),
                                          std::string("4"), std::string("5"),
                                          std::string("3")));
}

TEST_F(IdleHelperTestWithIdlePeriodObserver, TestLongIdlePeriod) {
  base::TimeTicks expected_deadline =
      test_task_runner_->NowTicks() + maximum_idle_period_duration();
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(1)
      .WillRepeatedly(Return(true));
  ExpectIdlePeriodStartsButNeverEnds();

  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperTest, TestLongIdlePeriodWithPendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(30);
  base::TimeTicks expected_deadline =
      test_task_runner_->NowTicks() + pending_task_delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        pending_task_delay);

  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperTest, TestLongIdlePeriodWithLatePendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        pending_task_delay);

  // Advance clock until after delayed task was meant to be run.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(20));

  // Post an idle task and then EnableLongIdlePeriod. Since there is a late
  // pending delayed task this shouldn't actually start an idle period.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // After the delayed task has been run we should trigger an idle period.
  test_task_runner_->AdvanceMockTickClock(maximum_idle_period_duration());
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperTestWithIdlePeriodObserver, TestLongIdlePeriodRepeating) {
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(4)
      .WillRepeatedly(Return(true));
  ExpectIdlePeriodStartsAndEnds(AtLeast(2));

  g_max_idle_task_reposts = 3;
  base::TimeTicks clock_before(test_task_runner_->NowTicks());
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingUpdateClockIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count,
                     test_task_runner_, idle_task_runtime, &actual_deadlines));

  // Check each idle task runs in their own idle period.
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(3, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(clock_before + maximum_idle_period_duration(),
                           clock_before + 2 * maximum_idle_period_duration(),
                           clock_before + 3 * maximum_idle_period_duration()));

  g_max_idle_task_reposts = 5;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingUpdateClockIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count,
                     test_task_runner_, idle_task_runtime, &actual_deadlines));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&EndIdlePeriodIdleTask,
                                base::Unretained(idle_helper_.get())));

  // Ensure that reposting tasks stop after EndIdlePeriod is called.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(4, run_count);
}

TEST_F(IdleHelperTestWithIdlePeriodObserver,
       TestLongIdlePeriodWhenNotCanEnterLongIdlePeriod) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta half_delay = base::TimeDelta::FromMilliseconds(500);
  base::TimeTicks delay_over = test_task_runner_->NowTicks() + delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  ON_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
      .WillByDefault(
          Invoke([delay, delay_over](
                     base::TimeTicks now,
                     base::TimeDelta* next_long_idle_period_delay_out) {
            if (now >= delay_over)
              return true;
            *next_long_idle_period_delay_out = delay;
            return false;
          }));

  EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _)).Times(2);
  EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(AnyNumber());

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  // Make sure Idle tasks don't run until the delay has occurred.
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  test_task_runner_->AdvanceMockTickClock(half_delay);
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // Delay is finished, idle task should run.
  test_task_runner_->AdvanceMockTickClock(half_delay);
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperTest,
       TestLongIdlePeriodDoesNotImmediatelyRestartIfMaxDeadline) {
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  base::TimeTicks clock_before(test_task_runner_->NowTicks());
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));

  // The second idle period should happen immediately after the first the
  // they have max deadlines.
  g_max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingUpdateClockIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count,
                     test_task_runner_, idle_task_runtime, &actual_deadlines));

  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(2, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(clock_before + maximum_idle_period_duration(),
                           clock_before + 2 * maximum_idle_period_duration()));
}

TEST_F(IdleHelperTest, TestLongIdlePeriodRestartWaitsIfNotMaxDeadline) {
  base::TimeTicks actual_deadline;
  int run_count = 0;

  base::TimeDelta pending_task_delay(base::TimeDelta::FromMilliseconds(20));
  base::TimeDelta idle_task_duration(base::TimeDelta::FromMilliseconds(10));
  base::TimeTicks expected_deadline(
      test_task_runner_->NowTicks() + pending_task_delay +
      maximum_idle_period_duration() + retry_enable_long_idle_period_delay());

  // Post delayed task to ensure idle period doesn't have a max deadline.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        pending_task_delay);

  g_max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&RepostingIdleTestTask,
                                base::RetainedRef(idle_task_runner_),
                                &run_count, &actual_deadline));
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);
  test_task_runner_->AdvanceMockTickClock(idle_task_duration);

  // Next idle period shouldn't happen until the pending task has been run.
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Once the pending task is run the new idle period should start.
  test_task_runner_->AdvanceMockTickClock(pending_task_delay -
                                          idle_task_duration);

  // Since the idle period tried to start before the pending task ran we have to
  // wait for the idle helper to retry starting the long idle period.
  test_task_runner_->AdvanceMockTickClock(
      retry_enable_long_idle_period_delay());
  test_task_runner_->RunUntilIdle();

  EXPECT_EQ(2, run_count);
  EXPECT_EQ(expected_deadline, actual_deadline);
}

TEST_F(IdleHelperTest, TestLongIdlePeriodPaused) {
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  // If there are no idle tasks posted we should start in the paused state.
  idle_helper_->EnableLongIdlePeriod();
  CheckIdlePeriodStateIs("in_long_idle_period_paused");
  // There shouldn't be any delayed tasks posted by the idle helper when paused.
  base::sequence_manager::LazyNow lazy_now_1(
      test_task_runner_->GetMockTickClock());
  EXPECT_FALSE(
      scheduler_helper_->real_time_domain()->DelayTillNextTask(&lazy_now_1));

  // Posting a task should transition us to the an active state.
  g_max_idle_task_reposts = 2;
  base::TimeTicks clock_before(test_task_runner_->NowTicks());
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingUpdateClockIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count,
                     test_task_runner_, idle_task_runtime, &actual_deadlines));
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(2, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(clock_before + maximum_idle_period_duration(),
                           clock_before + 2 * maximum_idle_period_duration()));

  // Once all task have been run we should go back to the paused state.
  CheckIdlePeriodStateIs("in_long_idle_period_paused");
  base::sequence_manager::LazyNow lazy_now_2(
      test_task_runner_->GetMockTickClock());
  EXPECT_FALSE(
      scheduler_helper_->real_time_domain()->DelayTillNextTask(&lazy_now_2));

  idle_helper_->EndIdlePeriod();
  CheckIdlePeriodStateIs("not_in_idle_period");
}

TEST_F(IdleHelperTest, TestLongIdlePeriodWhenShutdown) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  idle_helper_->Shutdown();

  // We shouldn't be able to enter a long idle period when shutdown
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  CheckIdlePeriodStateIs("not_in_idle_period");
  EXPECT_EQ(0, run_count);
}

void TestCanExceedIdleDeadlineIfRequiredTask(IdleHelperForTest* idle_helper,
                                             bool* can_exceed_idle_deadline_out,
                                             int* run_count,
                                             base::TimeTicks deadline) {
  *can_exceed_idle_deadline_out =
      idle_helper->CanExceedIdleDeadlineIfRequired();
  (*run_count)++;
}

TEST_F(IdleHelperTest, CanExceedIdleDeadlineIfRequired) {
  int run_count = 0;
  bool can_exceed_idle_deadline = false;

  // Should return false if not in an idle period.
  EXPECT_FALSE(idle_helper_->CanExceedIdleDeadlineIfRequired());

  // Should return false for short idle periods.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask,
                                idle_helper_.get(), &can_exceed_idle_deadline,
                                &run_count));
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::kInShortIdlePeriod,
      test_task_runner_->NowTicks(),
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Should return false for a long idle period which is shortened due to a
  // pending delayed task.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask,
                                idle_helper_.get(), &can_exceed_idle_deadline,
                                &run_count));
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  test_task_runner_->AdvanceMockTickClock(maximum_idle_period_duration());
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask,
                                idle_helper_.get(), &can_exceed_idle_deadline,
                                &run_count));
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_TRUE(can_exceed_idle_deadline);
}

class IdleHelperWithQuiescencePeriodTest : public BaseIdleHelperTest {
 public:
  enum {
    kQuiescenceDelayMs = 100,
    kLongIdlePeriodMs = 50,
  };

  IdleHelperWithQuiescencePeriodTest()
      : BaseIdleHelperTest(
            nullptr,
            base::TimeDelta::FromMilliseconds(kQuiescenceDelayMs)) {}

  ~IdleHelperWithQuiescencePeriodTest() override = default;

  void SetUp() override {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*idle_helper_, IsNotQuiescent()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(_)).Times(AnyNumber());
  }

  void MakeNonQuiescent() {
    // Run an arbitrary task so we're deemed to be not quiescent.
    default_task_runner_->PostTask(FROM_HERE, base::BindOnce(NullTask));
    test_task_runner_->RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleHelperWithQuiescencePeriodTest);
};

class IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver
    : public IdleHelperWithQuiescencePeriodTest {
 public:
  IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver()
      : IdleHelperWithQuiescencePeriodTest() {}

  ~IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver() override =
      default;

  void SetUp() override {
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(_)).Times(AnyNumber());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver);
};

TEST_F(IdleHelperWithQuiescencePeriodTest,
       LongIdlePeriodStartsImmediatelyIfQuiescent) {
  base::TimeTicks actual_deadline;
  int run_count = 0;
  g_max_idle_task_reposts = 1;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&RepostingIdleTestTask,
                                base::RetainedRef(idle_task_runner_),
                                &run_count, &actual_deadline));

  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();

  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver,
       LongIdlePeriodDoesNotStartsImmediatelyIfBusy) {
  MakeNonQuiescent();
  EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(0);
  EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(0);
  EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _)).Times(0);
  EXPECT_CALL(*idle_helper_, IsNotQuiescent()).Times(AtLeast(1));

  base::TimeTicks actual_deadline;
  int run_count = 0;
  g_max_idle_task_reposts = 1;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&RepostingIdleTestTask,
                                base::RetainedRef(idle_task_runner_),
                                &run_count, &actual_deadline));

  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();

  EXPECT_EQ(0, run_count);

  scheduler_helper_->Shutdown();
}

TEST_F(IdleHelperWithQuiescencePeriodTest,
       LongIdlePeriodStartsAfterQuiescence) {
  MakeNonQuiescent();

  // Run a repeating task so we're deemed to be busy for the next 400ms.
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RepeatingTask,
                                base::Unretained(default_task_runner_.get()),
                                10, base::TimeDelta::FromMilliseconds(40)));

  int run_count = 0;
  // In this scenario EnableLongIdlePeriod deems us not to be quiescent 5x in
  // a row.
  base::TimeTicks expected_deadline =
      test_task_runner_->NowTicks() +
      base::TimeDelta::FromMilliseconds(5 * kQuiescenceDelayMs +
                                        kLongIdlePeriodMs);
  base::TimeTicks deadline_in_task;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperWithQuiescencePeriodTest,
       QuescienceCheckedForAfterLongIdlePeriodEnds) {
  idle_task_runner_->PostIdleTask(FROM_HERE, base::BindOnce(&NullIdleTask));
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();

  // Post a normal task to make the scheduler non-quiescent.
  default_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NullTask));
  test_task_runner_->RunUntilIdle();

  // Post an idle task. The idle task won't run initially because the system is
  // not judged to be quiescent, but should be run after the quiescence delay.
  int run_count = 0;
  base::TimeTicks deadline_in_task;
  base::TimeTicks expected_deadline =
      test_task_runner_->NowTicks() +
      base::TimeDelta::FromMilliseconds(kQuiescenceDelayMs + kLongIdlePeriodMs);
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperTest, NoShortIdlePeriodWhenDeadlineTooClose) {
  int run_count = 0;
  base::TimeTicks deadline_in_task;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  base::TimeDelta half_a_ms(base::TimeDelta::FromMicroseconds(50));
  base::TimeTicks less_than_min_deadline(test_task_runner_->NowTicks() +
                                         minimum_idle_period_duration() -
                                         half_a_ms);
  base::TimeTicks more_than_min_deadline(test_task_runner_->NowTicks() +
                                         minimum_idle_period_duration() +
                                         half_a_ms);

  idle_helper_->StartIdlePeriod(IdleHelper::IdlePeriodState::kInShortIdlePeriod,
                                test_task_runner_->NowTicks(),
                                less_than_min_deadline);
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->StartIdlePeriod(IdleHelper::IdlePeriodState::kInShortIdlePeriod,
                                test_task_runner_->NowTicks(),
                                more_than_min_deadline);
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperTest, NoLongIdlePeriodWhenDeadlineTooClose) {
  int run_count = 0;
  base::TimeTicks deadline_in_task;

  base::TimeDelta half_a_ms(base::TimeDelta::FromMicroseconds(50));
  base::TimeDelta less_than_min_deadline_duration(
      minimum_idle_period_duration() - half_a_ms);
  base::TimeDelta more_than_min_deadline_duration(
      minimum_idle_period_duration() + half_a_ms);

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        less_than_min_deadline_duration);

  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->EndIdlePeriod();
  test_task_runner_->AdvanceMockTickClock(maximum_idle_period_duration());
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        more_than_min_deadline_duration);
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperWithQuiescencePeriodTest,
       PendingEnableLongIdlePeriodNotRunAfterShutdown) {
  MakeNonQuiescent();

  bool shutdown_task_run = false;
  int run_count = 0;
  base::TimeTicks deadline_in_task;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&ShutdownIdleTask, base::Unretained(idle_helper_.get()),
                     &shutdown_task_run));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  // Delayed call to IdleHelper::EnableLongIdlePeriod enables idle tasks.
  idle_helper_->EnableLongIdlePeriod();
  test_task_runner_->AdvanceMockTickClock(maximum_idle_period_duration() * 2.0);
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(shutdown_task_run);
  EXPECT_EQ(0, run_count);

  // Shutdown immediately after idle period started should prevent the idle
  // task from running.
  idle_helper_->Shutdown();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);
}

TEST_F(IdleHelperTest, TestPostDelayedIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  // Posting a delayed idle task should not post anything on the underlying
  // task queue until the delay is up.
  idle_task_runner_->PostDelayedIdleTask(
      FROM_HERE, base::TimeDelta::FromMilliseconds(200),
      base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  EXPECT_EQ(0u, idle_queue()->GetNumberOfPendingTasks());

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(100));

  // It shouldn't run until the delay is over even though we went idle.
  idle_helper_->StartIdlePeriod(IdleHelper::IdlePeriodState::kInShortIdlePeriod,
                                test_task_runner_->NowTicks(),
                                expected_deadline);
  EXPECT_EQ(0u, idle_queue()->GetNumberOfPendingTasks());
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(100));
  idle_helper_->StartIdlePeriod(IdleHelper::IdlePeriodState::kInShortIdlePeriod,
                                test_task_runner_->NowTicks(),
                                expected_deadline);
  EXPECT_EQ(1u, idle_queue()->GetNumberOfPendingTasks());
  test_task_runner_->RunUntilIdle();

  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

// Tests that the OnPendingTasksChanged callback is called once when the idle
// queue becomes non-empty and again when it becomes empty.
TEST_F(IdleHelperTest, OnPendingTasksChanged) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  {
    testing::InSequence dummy;
    // This will be called once. I.e when the one and only task is posted.
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(true)).Times(1);
    // This will be called once. I.e when the one and only task completes.
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(false)).Times(1);
  }

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->StartIdlePeriod(IdleHelper::IdlePeriodState::kInShortIdlePeriod,
                                test_task_runner_->NowTicks(),
                                expected_deadline);
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

// Tests that the OnPendingTasksChanged callback is still only called once
// with false despite there being two idle tasks posted.
TEST_F(IdleHelperTest, OnPendingTasksChanged_TwoTasksAtTheSameTime) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      test_task_runner_->NowTicks() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  {
    testing::InSequence dummy;
    // This will be called 3 times. I.e when T1 and T2 are posted and when T1
    // completes.
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(true)).Times(3);
    // This will be called once. I.e when T2 completes.
    EXPECT_CALL(*idle_helper_, OnPendingTasksChanged(false)).Times(1);
  }

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->StartIdlePeriod(IdleHelper::IdlePeriodState::kInShortIdlePeriod,
                                test_task_runner_->NowTicks(),
                                expected_deadline);
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

}  // namespace idle_helper_unittest
}  // namespace scheduler
}  // namespace blink

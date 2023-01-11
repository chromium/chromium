// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

template <typename T>
void RunFunctor(T functor) {
  functor();
}

template <typename T>
base::OnceClosure GetClosure(T functor) {
  return base::BindOnce(&RunFunctor<T>, functor);
}

class SchedulerTest : public testing::Test {
 public:
  SchedulerTest()
      : sync_point_manager_(new SyncPointManager),
        scheduler_(new Scheduler(sync_point_manager_.get(), GpuPreferences())) {
  }

 protected:

  SyncPointManager* sync_point_manager() const {
    return sync_point_manager_.get();
  }

  Scheduler* scheduler() const { return scheduler_.get(); }

  void RunAllPendingTasks() {
    SequenceId sequence_id =
        scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
    scheduler()->ScheduleTask(Scheduler::Task(
        sequence_id, run_loop_.QuitClosure(), std::vector<SyncToken>()));
    run_loop_.Run();
    scheduler()->DestroySequence(sequence_id);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;

 private:
  std::unique_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<Scheduler> scheduler_;
};

TEST_F(SchedulerTest, ScheduledTasksRunInOrder) {
  SequenceId sequence_id =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);

  static int count = 0;
  int ran1 = 0;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id,
                                            GetClosure([&] { ran1 = ++count; }),
                                            std::vector<SyncToken>()));

  int ran2 = 0;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id,
                                            GetClosure([&] { ran2 = ++count; }),
                                            std::vector<SyncToken>()));

  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, run_loop_.QuitClosure(), std::vector<SyncToken>()));
  run_loop_.Run();

  EXPECT_EQ(ran1, 1);
  EXPECT_EQ(ran2, 2);

  scheduler()->DestroySequence(sequence_id);
}

TEST_F(SchedulerTest, ScheduledTasksRunAfterReporting) {
  SequenceId sequence_id =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);

  bool ran = false;
  bool reported = false;
  scheduler()->ScheduleTask(
      Scheduler::Task(sequence_id, GetClosure([&] {
                        EXPECT_TRUE(reported);
                        ran = true;
                      }),
                      std::vector<SyncToken>(),
                      base::BindOnce(
                          [&](bool& ran, bool& reported, base::TimeTicks t) {
                            EXPECT_FALSE(ran);
                            reported = true;
                          },
                          std::ref(ran), std::ref(reported))));
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, run_loop_.QuitClosure(), std::vector<SyncToken>()));
  run_loop_.Run();

  EXPECT_TRUE(ran);
  scheduler()->DestroySequence(sequence_id);
}

TEST_F(SchedulerTest, ContinuedTasksRunFirst) {
  SequenceId sequence_id =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);

  static int count = 0;
  int ran1 = 0;
  int continued1 = 0;
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, GetClosure([&] {
        scheduler()->ContinueTask(sequence_id,
                                  GetClosure([&] { continued1 = ++count; }));
        ran1 = ++count;
      }),
      std::vector<SyncToken>()));

  int ran2 = 0;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id,
                                            GetClosure([&] { ran2 = ++count; }),
                                            std::vector<SyncToken>()));

  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id, run_loop_.QuitClosure(), std::vector<SyncToken>()));
  run_loop_.Run();

  EXPECT_EQ(ran1, 1);
  EXPECT_EQ(continued1, 2);
  EXPECT_EQ(ran2, 3);

  scheduler()->DestroySequence(sequence_id);
}

class SchedulerTaskRunOrderTest : public SchedulerTest {
 public:
  SchedulerTaskRunOrderTest() = default;
  ~SchedulerTaskRunOrderTest() override {
    while (!sequence_info_.empty()) {
      DestroySequence(sequence_info_.begin()->first);
    }
  }

 protected:
  void CreateSequence(int sequence_key, SchedulingPriority priority) {
    SequenceId sequence_id = scheduler()->CreateSequenceForTesting(priority);
    CommandBufferId command_buffer_id =
        CommandBufferId::FromUnsafeValue(sequence_key);
    scoped_refptr<SyncPointClientState> release_state =
        sync_point_manager()->CreateSyncPointClientState(
            kNamespaceId, command_buffer_id, sequence_id);

    sequence_info_.emplace(std::make_pair(
        sequence_key,
        SequenceInfo(sequence_id, command_buffer_id, release_state)));
  }

  void CreateExternalSequence(int sequence_key) {
    auto order_data = sync_point_manager()->CreateSyncPointOrderData();
    auto command_buffer_id = CommandBufferId::FromUnsafeValue(sequence_key);
    auto release_state = sync_point_manager()->CreateSyncPointClientState(
        kNamespaceId, command_buffer_id, order_data->sequence_id());

    sequence_info_.emplace(std::make_pair(
        sequence_key,
        SequenceInfo(std::move(order_data), command_buffer_id, release_state)));
  }

  void DestroySequence(int sequence_key) {
    auto info_it = sequence_info_.find(sequence_key);
    ASSERT_TRUE(info_it != sequence_info_.end());

    info_it->second.release_state->Destroy();
    if (info_it->second.order_data)
      info_it->second.order_data->Destroy();
    else
      scheduler()->DestroySequence(info_it->second.sequence_id);

    sequence_info_.erase(info_it);
  }

  void CreateSyncToken(int sequence_key, int release_sync) {
    auto info_it = sequence_info_.find(sequence_key);
    ASSERT_TRUE(info_it != sequence_info_.end());

    uint64_t release = release_sync + 1;
    sync_tokens_.emplace(std::make_pair(
        release_sync,
        SyncToken(kNamespaceId, info_it->second.command_buffer_id, release)));
  }

  static void RunExternalTask(base::OnceClosure task,
                              scoped_refptr<SyncPointOrderData> order_data,
                              uint32_t order_num) {
    order_data->BeginProcessingOrderNumber(order_num);
    std::move(task).Run();
    order_data->FinishProcessingOrderNumber(order_num);
  }

  base::OnceClosure GetTaskClosure(int sequence_key, int release_sync) {
    const int task_id = num_tasks_scheduled_++;

    uint64_t release = 0;
    if (release_sync >= 0) {
      CreateSyncToken(sequence_key, release_sync);
      release = release_sync + 1;
    }

    auto info_it = sequence_info_.find(sequence_key);
    DCHECK(info_it != sequence_info_.end());

    auto closure = GetClosure([this, task_id, sequence_key, release] {
      if (release) {
        auto info_it = sequence_info_.find(sequence_key);
        ASSERT_TRUE(info_it != sequence_info_.end());
        info_it->second.release_state->ReleaseFenceSync(release);
      }
      this->tasks_executed_.push_back(task_id);
    });

    // Simulate external sequence, when tasks are run outside of this
    // gpu::Scheduler
    if (info_it->second.external()) {
      auto order_data = info_it->second.order_data;
      uint32_t order_num = order_data->GenerateUnprocessedOrderNumber();

      return base::BindOnce(RunExternalTask, std::move(closure), order_data,
                            order_num);
    } else {
      return closure;
    }
  }

  void ScheduleTask(int sequence_key, int wait_sync, int release_sync) {
    auto closure = GetTaskClosure(sequence_key, release_sync);

    auto info_it = sequence_info_.find(sequence_key);
    ASSERT_TRUE(info_it != sequence_info_.end());

    DCHECK(!info_it->second.external());

    std::vector<SyncToken> wait;
    if (wait_sync >= 0) {
      wait.push_back(sync_tokens_[wait_sync]);
    }

    scheduler()->ScheduleTask(
        Scheduler::Task(info_it->second.sequence_id, std::move(closure), wait));
  }

  const std::vector<int>& tasks_executed() { return tasks_executed_; }

  base::SingleThreadTaskRunner* GetTaskRunnerFromSequence(int sequence_key) {
    auto info_it = sequence_info_.find(sequence_key);
    if (info_it == sequence_info_.end())
      return nullptr;

    return scheduler()->GetTaskRunnerForTesting(info_it->second.sequence_id);
  }

 private:
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;

  int num_tasks_scheduled_ = 0;

  struct SequenceInfo {
    SequenceInfo(SequenceId sequence_id,
                 CommandBufferId command_buffer_id,
                 scoped_refptr<SyncPointClientState> release_state)
        : sequence_id(sequence_id),
          command_buffer_id(command_buffer_id),
          release_state(release_state) {}

    SequenceInfo(scoped_refptr<SyncPointOrderData> order_data,
                 CommandBufferId command_buffer_id,
                 scoped_refptr<SyncPointClientState> release_state)
        : sequence_id(order_data->sequence_id()),
          command_buffer_id(command_buffer_id),
          order_data(order_data),
          release_state(release_state) {}

    bool external() const { return !!order_data; }

    SequenceId sequence_id;
    CommandBufferId command_buffer_id;
    // |order_data| is only set for external sequences.
    scoped_refptr<SyncPointOrderData> order_data;
    scoped_refptr<SyncPointClientState> release_state;
  };

  std::map<int, const SequenceInfo> sequence_info_;
  std::map<int, const SyncToken> sync_tokens_;

  std::vector<int> tasks_executed_;
};

TEST_F(SchedulerTaskRunOrderTest, SequencesRunInPriorityOrder) {
  CreateSequence(0, SchedulingPriority::kLow);
  CreateSequence(1, SchedulingPriority::kNormal);
  CreateSequence(2, SchedulingPriority::kHigh);

  ScheduleTask(0, -1, -1);  // task 0: seq 0, no wait, no release
  ScheduleTask(1, -1, -1);  // task 1: seq 1, no wait, no release
  ScheduleTask(2, -1, -1);  // task 2: seq 2, no wait, no release

  RunAllPendingTasks();

  const int expected_task_order[] = {2, 1, 0};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, SequencesOfSamePriorityRunInOrder) {
  CreateSequence(0, SchedulingPriority::kNormal);
  CreateSequence(1, SchedulingPriority::kNormal);
  CreateSequence(2, SchedulingPriority::kNormal);
  CreateSequence(3, SchedulingPriority::kNormal);

  ScheduleTask(0, -1, -1);  // task 0: seq 0, no wait, no release
  ScheduleTask(1, -1, -1);  // task 1: seq 1, no wait, no release
  ScheduleTask(2, -1, -1);  // task 2: seq 2, no wait, no release
  ScheduleTask(3, -1, -1);  // task 3: seq 2, no wait, no release

  RunAllPendingTasks();

  const int expected_task_order[] = {0, 1, 2, 3};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, SequenceWaitsForFence) {
  CreateSequence(0, SchedulingPriority::kHigh);
  CreateSequence(1, SchedulingPriority::kNormal);

  ScheduleTask(1, -1, 0);  // task 0: seq 1, no wait, release 0
  ScheduleTask(0, 0, -1);  // task 1: seq 0, wait 0, no release

  RunAllPendingTasks();

  const int expected_task_order[] = {0, 1};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, SequenceWaitsForFenceExternal) {
  CreateSequence(0, SchedulingPriority::kHigh);
  CreateExternalSequence(1);

  // Create task 0 on seq 1 that will release 0, but don't post it.
  auto external_task = GetTaskClosure(1, 0);

  ScheduleTask(0, 0, -1);  // task 1: seq 0, wait 0, no release

  // task runner for all the sequences created here from same thread is same.
  // only sequences created on different threads have different task runner.
  GetTaskRunnerFromSequence(0)->PostTask(FROM_HERE, std::move(external_task));

  RunAllPendingTasks();

  const int expected_task_order[] = {0, 1};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, SequenceDoesNotWaitForInvalidFence) {
  CreateSequence(0, SchedulingPriority::kNormal);
  CreateSequence(1, SchedulingPriority::kNormal);

  CreateSyncToken(1, 0);  // declare sync_token 0 on seq 1

  ScheduleTask(0, 0, -1);  // task 0: seq 0, wait 0, no release
  ScheduleTask(1, -1, 0);  // task 1: seq 1, no wait, release 0

  RunAllPendingTasks();

  // Task 0 does not wait on unrelease sync token 0.
  const int expected_task_order[] = {0, 1};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, ReleaseSequenceIsPrioritized) {
  CreateSequence(0, SchedulingPriority::kNormal);
  CreateSequence(1, SchedulingPriority::kLow);
  CreateSequence(2, SchedulingPriority::kHigh);

  ScheduleTask(0, -1, -1);  // task 0: seq 0, no wait, no release
  ScheduleTask(1, -1, 0);   // task 1: seq 1, no wait, release 0
  ScheduleTask(2, 0, -1);   // task 2: seq 2, wait 0, no release

  RunAllPendingTasks();

  const int expected_task_order[] = {1, 2, 0};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, ReleaseSequenceHasPriorityOfWaiter) {
  CreateSequence(0, SchedulingPriority::kLow);
  CreateSequence(1, SchedulingPriority::kNormal);
  CreateSequence(2, SchedulingPriority::kHigh);

  ScheduleTask(0, -1, 0);   // task 0: seq 0, no wait, release 0
  ScheduleTask(1, 0, -1);   // task 1: seq 1, wait 0, no release
  ScheduleTask(2, -1, -1);  // task 2: seq 2, no wait, no release

  RunAllPendingTasks();

  const int expected_task_order[] = {2, 0, 1};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, ReleaseSequenceRevertsToDefaultPriority) {
  CreateSequence(0, SchedulingPriority::kNormal);
  CreateSequence(1, SchedulingPriority::kLow);
  CreateSequence(2, SchedulingPriority::kHigh);

  ScheduleTask(0, -1, -1);  // task 0: seq 0, no wait, no release
  ScheduleTask(1, -1, 0);   // task 1: seq 1, no wait, release 0
  ScheduleTask(2, 0, -1);   // task 2: seq 2, wait 0, no release

  DestroySequence(2);

  RunAllPendingTasks();

  const int expected_task_order[] = {0, 1};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, ReleaseSequenceCircularRelease) {
  CreateSequence(0, SchedulingPriority::kLow);
  CreateSequence(1, SchedulingPriority::kNormal);
  CreateSequence(2, SchedulingPriority::kHigh);

  ScheduleTask(0, -1, -1);  // task 0: seq 0, no wait, no release
  ScheduleTask(1, -1, -1);  // task 1: seq 1, no wait, no release
  ScheduleTask(2, -1, -1);  // task 2: seq 2, no wait, no release

  ScheduleTask(0, -1, 0);   // task 3: seq 0, no wait, release 0
  ScheduleTask(0, -1, -1);  // task 4: seq 0, no wait, no release

  ScheduleTask(1, 0, 1);    // task 5: seq 1, wait 0, release 1
  ScheduleTask(1, -1, -1);  // task 6: seq 1, no wait, no release

  ScheduleTask(2, 1, 2);    // task 7: seq 2, wait 1, release 2
  ScheduleTask(2, -1, -1);  // task 8: seq 2, no wait, no release

  ScheduleTask(0, 2, 3);   // task 9: seq 0, wait 2, releases 3
  ScheduleTask(1, 3, 4);   // task 10: seq 1, wait 3, releases 4
  ScheduleTask(2, 4, -1);  // task 11: seq 2, wait 4, no release

  ScheduleTask(0, -1, -1);  // task 12: seq 0, no wait, no release
  ScheduleTask(1, -1, -1);  // task 13: seq 1, no wait, no release
  ScheduleTask(2, -1, -1);  // task 14: seq 2, no wait, no release

  RunAllPendingTasks();

  const int expected_task_order[] = {0, 1, 2,  3,  4,  5,  6, 7,
                                     8, 9, 10, 11, 14, 13, 12};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTaskRunOrderTest, WaitOnSelfShouldNotBlockSequence) {
  CreateSequence(0, SchedulingPriority::kHigh);
  CreateSyncToken(0, 0);  // declare sync_token 0 on seq 1

  // Dummy order number to avoid the wait_order_num <= processed_order_num + 1
  // check in SyncPointOrderData::ValidateReleaseOrderNum.
  sync_point_manager()->GenerateOrderNumber();

  ScheduleTask(0, 0, -1);  // task 0: seq 0, wait 0, no release

  RunAllPendingTasks();

  const int expected_task_order[] = {0};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_F(SchedulerTest, ReleaseSequenceShouldYield) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id1);

  uint64_t release = 1;
  static int count = 0;
  int ran1 = 0;
  scheduler()->ScheduleTask(
      Scheduler::Task(sequence_id1, GetClosure([&] {
                        EXPECT_FALSE(scheduler()->ShouldYield(sequence_id1));
                        release_state->ReleaseFenceSync(release);
                        EXPECT_TRUE(scheduler()->ShouldYield(sequence_id1));
                        ran1 = ++count;
                      }),
                      std::vector<SyncToken>()));

  int ran2 = 0;
  SyncToken sync_token(namespace_id, command_buffer_id, release);
  SequenceId sequence_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id2, GetClosure([&] { ran2 = ++count; }), {sync_token}));

  RunAllPendingTasks();

  EXPECT_EQ(ran1, 1);
  EXPECT_EQ(ran2, 2);
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  release_state->Destroy();
  scheduler()->DestroySequence(sequence_id1);
  scheduler()->DestroySequence(sequence_id2);
}

TEST_F(SchedulerTest, ReentrantEnableSequenceShouldNotDeadlock) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id1 = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state1 =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id1, sequence_id1);

  SequenceId sequence_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  CommandBufferId command_buffer_id2 = CommandBufferId::FromUnsafeValue(2);
  scoped_refptr<SyncPointClientState> release_state2 =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id2, sequence_id2);

  uint64_t release = 1;
  SyncToken sync_token(namespace_id, command_buffer_id2, release);

  static int count = 0;
  int ran1, ran2 = 0;

  // Schedule task on sequence 2 first so that the sync token wait isn't a nop.
  // BeginProcessingOrderNumber for this task will run the EnableSequence
  // callback. This should not deadlock.
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2,
                                            GetClosure([&] { ran2 = ++count; }),
                                            std::vector<SyncToken>()));

  // This will run first because of the higher priority and no scheduling sync
  // token dependencies.
  scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id1, GetClosure([&] {
        ran1 = ++count;
        release_state1->Wait(
            sync_token,
            base::BindOnce(&Scheduler::EnableSequence,
                           base::Unretained(scheduler()), sequence_id1));
        scheduler()->DisableSequence(sequence_id1);
      }),
      std::vector<SyncToken>()));

  RunAllPendingTasks();

  EXPECT_EQ(ran1, 1);
  EXPECT_EQ(ran2, 2);
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(sync_token));

  release_state1->Destroy();
  release_state2->Destroy();

  scheduler()->DestroySequence(sequence_id1);
  scheduler()->DestroySequence(sequence_id2);
}

TEST_F(SchedulerTest, ClientWaitIsPrioritized) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  SequenceId sequence_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  SequenceId sequence_id3 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);

  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);

  static int count = 0;
  int ran1 = 0, ran2 = 0, ran3 = 0;

  scheduler()->ScheduleTask(Scheduler::Task(sequence_id1,
                                            GetClosure([&] { ran1 = ++count; }),
                                            std::vector<SyncToken>()));

  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2,
                                            GetClosure([&] { ran2 = ++count; }),
                                            std::vector<SyncToken>()));

  scheduler()->ScheduleTask(Scheduler::Task(sequence_id3,
                                            GetClosure([&] { ran3 = ++count; }),
                                            std::vector<SyncToken>()));

  scheduler()->RaisePriorityForClientWait(sequence_id2, command_buffer_id);

  RunAllPendingTasks();

  EXPECT_EQ(ran2, 1);
  EXPECT_EQ(ran3, 2);
  EXPECT_EQ(ran1, 3);

  ran1 = ran2 = ran3 = 0;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id1,
                                            GetClosure([&] { ran1 = ++count; }),
                                            std::vector<SyncToken>()));
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2,
                                            GetClosure([&] { ran2 = ++count; }),
                                            std::vector<SyncToken>()));
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id3,
                                            GetClosure([&] { ran3 = ++count; }),
                                            std::vector<SyncToken>()));

  scheduler()->ResetPriorityForClientWait(sequence_id2, command_buffer_id);

  // Note that we are not using RunAllPendingTasks() here because more than one
  // Run() is not allowed on the same Runloop. Hence creating a new runloop to
  // schedule the task.
  base::RunLoop run_loop_temp;
  SequenceId sequence_id_run_loop =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id_run_loop,
                                            run_loop_temp.QuitClosure(),
                                            std::vector<SyncToken>()));
  run_loop_temp.Run();

  EXPECT_EQ(ran3, 4);
  EXPECT_EQ(ran1, 5);
  EXPECT_EQ(ran2, 6);

  scheduler()->DestroySequence(sequence_id1);
  scheduler()->DestroySequence(sequence_id2);
  scheduler()->DestroySequence(sequence_id3);
  scheduler()->DestroySequence(sequence_id_run_loop);
}

TEST_F(SchedulerTest, StreamPriorities) {
  SequenceId seq_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  SequenceId seq_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  SequenceId seq_id3 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);

  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id1 = CommandBufferId::FromUnsafeValue(1);
  CommandBufferId command_buffer_id2 = CommandBufferId::FromUnsafeValue(2);

  base::AutoLock auto_lock(scheduler()->lock_);

  Scheduler::Sequence* seq1 = scheduler()->GetSequence(seq_id1);
  Scheduler::Sequence* seq2 = scheduler()->GetSequence(seq_id2);
  Scheduler::Sequence* seq3 = scheduler()->GetSequence(seq_id3);

  // Initial default priorities.
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  SyncToken sync_token1(namespace_id, command_buffer_id1, 1);
  SyncToken sync_token2(namespace_id, command_buffer_id2, 1);

  // Wait priorities propagate.
  seq2->AddWaitFence(sync_token1, 1, seq_id1);
  EXPECT_EQ(SchedulingPriority::kNormal, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  seq2->AddWaitFence(sync_token1, 1, seq_id1);
  EXPECT_EQ(SchedulingPriority::kNormal, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  seq3->AddWaitFence(sync_token2, 2, seq_id2);
  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  // Release priority propagate.
  seq2->RemoveWaitFence(sync_token1, 1, seq_id1);
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  seq2->RemoveWaitFence(sync_token1, 1, seq_id1);
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  seq3->RemoveWaitFence(sync_token2, 2, seq_id2);
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  {
    base::AutoUnlock auto_unlock(scheduler()->lock_);
    scheduler()->DestroySequence(seq_id1);
    scheduler()->DestroySequence(seq_id2);
    scheduler()->DestroySequence(seq_id3);
  }
}

TEST_F(SchedulerTest, StreamDestroyRemovesPriorities) {
  SequenceId seq_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  SequenceId seq_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  SequenceId seq_id3 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);

  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id1 = CommandBufferId::FromUnsafeValue(1);
  CommandBufferId command_buffer_id2 = CommandBufferId::FromUnsafeValue(2);

  base::AutoLock auto_lock(scheduler()->lock_);

  Scheduler::Sequence* seq1 = scheduler()->GetSequence(seq_id1);
  Scheduler::Sequence* seq2 = scheduler()->GetSequence(seq_id2);
  Scheduler::Sequence* seq3 = scheduler()->GetSequence(seq_id3);

  SyncToken sync_token1(namespace_id, command_buffer_id1, 1);
  SyncToken sync_token2(namespace_id, command_buffer_id2, 1);

  // Wait priorities propagate.
  seq2->AddWaitFence(sync_token1, 1, seq_id1);
  seq2->AddWaitFence(sync_token1, 1, seq_id1);
  seq3->AddWaitFence(sync_token2, 2, seq_id2);

  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  // Deleting waiting sequences removes priorities.
  {
    base::AutoUnlock auto_unlock(scheduler()->lock_);
    scheduler()->DestroySequence(seq_id3);
  }

  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());

  {
    base::AutoUnlock auto_unlock(scheduler()->lock_);
    scheduler()->DestroySequence(seq_id2);
  }

  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());

  {
    base::AutoUnlock auto_unlock(scheduler()->lock_);
    scheduler()->DestroySequence(seq_id1);
  }
}

// crbug.com/781585#5: Test RemoveWait/AddWait/RemoveWait sequence.
TEST_F(SchedulerTest, StreamPriorityChangeWhileReleasing) {
  SequenceId seq_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  SequenceId seq_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  SequenceId seq_id3 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);

  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id1 = CommandBufferId::FromUnsafeValue(1);
  CommandBufferId command_buffer_id2 = CommandBufferId::FromUnsafeValue(2);

  base::AutoLock auto_lock(scheduler()->lock_);

  Scheduler::Sequence* seq1 = scheduler()->GetSequence(seq_id1);
  Scheduler::Sequence* seq2 = scheduler()->GetSequence(seq_id2);
  Scheduler::Sequence* seq3 = scheduler()->GetSequence(seq_id3);

  SyncToken sync_token1(namespace_id, command_buffer_id1, 1);
  SyncToken sync_token2(namespace_id, command_buffer_id2, 2);

  // Wait on same fence multiple times.
  seq2->AddWaitFence(sync_token1, 1, seq_id1);
  seq2->AddWaitFence(sync_token1, 1, seq_id1);

  EXPECT_EQ(SchedulingPriority::kNormal, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  // All matching wait fences are removed together.
  seq2->RemoveWaitFence(sync_token1, 1, seq_id1);
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  // Add wait fence with higher priority.  This replicates a possible race.
  seq3->AddWaitFence(sync_token2, 2, seq_id2);
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  // This should be a No-op.
  seq2->RemoveWaitFence(sync_token1, 1, seq_id1);
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  seq3->RemoveWaitFence(sync_token2, 2, seq_id2);
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  {
    base::AutoUnlock auto_unlock(scheduler()->lock_);
    scheduler()->DestroySequence(seq_id1);
    scheduler()->DestroySequence(seq_id2);
    scheduler()->DestroySequence(seq_id3);
  }
}

TEST_F(SchedulerTest, CircularPriorities) {
  SequenceId seq_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);
  SequenceId seq_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  SequenceId seq_id3 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);

  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id2 = CommandBufferId::FromUnsafeValue(2);
  CommandBufferId command_buffer_id3 = CommandBufferId::FromUnsafeValue(3);

  base::AutoLock auto_lock(scheduler()->lock_);

  Scheduler::Sequence* seq1 = scheduler()->GetSequence(seq_id1);
  Scheduler::Sequence* seq2 = scheduler()->GetSequence(seq_id2);
  Scheduler::Sequence* seq3 = scheduler()->GetSequence(seq_id3);

  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kLow, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq3->current_priority());

  SyncToken sync_token_seq2_1(namespace_id, command_buffer_id2, 1);
  SyncToken sync_token_seq2_2(namespace_id, command_buffer_id2, 2);
  SyncToken sync_token_seq2_3(namespace_id, command_buffer_id2, 3);
  SyncToken sync_token_seq3_1(namespace_id, command_buffer_id3, 1);

  seq3->AddWaitFence(sync_token_seq2_1, 1, seq_id2);
  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq3->current_priority());

  seq1->AddWaitFence(sync_token_seq2_2, 2, seq_id2);
  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq3->current_priority());

  seq3->AddWaitFence(sync_token_seq2_3, 3, seq_id2);
  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq3->current_priority());

  seq2->AddWaitFence(sync_token_seq3_1, 4, seq_id3);
  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq3->current_priority());

  seq3->RemoveWaitFence(sync_token_seq2_1, 1, seq_id2);
  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq3->current_priority());

  seq1->RemoveWaitFence(sync_token_seq2_2, 2, seq_id2);
  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq3->current_priority());

  seq3->RemoveWaitFence(sync_token_seq2_3, 3, seq_id2);
  EXPECT_EQ(SchedulingPriority::kHigh, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kLow, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq3->current_priority());

  {
    base::AutoUnlock auto_unlock(scheduler()->lock_);
    scheduler()->DestroySequence(seq_id1);
    scheduler()->DestroySequence(seq_id2);
    scheduler()->DestroySequence(seq_id3);
  }
}

}  // namespace gpu

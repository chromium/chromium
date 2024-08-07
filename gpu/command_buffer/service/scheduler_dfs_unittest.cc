// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied from scheduler_unittest.cc, with modifications that test the new
// expected sequence order, and the new public methods added to SchedulerDfs.
// Most of the unit tests do not use SchedulerDfs directly on purpose, but go
// through the Scheduler, which forwards the calls to SchedulerDfs. This tests
// the way SchedulerDfs is actually used.

#include "gpu/command_buffer/service/scheduler_dfs.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
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

class SchedulerDfsTest : public testing::WithParamInterface<bool>,
                         public testing::Test {
 public:
  SchedulerDfsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kUseGpuSchedulerDfs,
                                features::kSyncPointGraphValidation},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kUseGpuSchedulerDfs},
          /*disabled_features=*/{features::kSyncPointGraphValidation});
    }
    // Create the scheduler after setting up the feature flags.
    sync_point_manager_ = std::make_unique<SyncPointManager>();
    scheduler_ = std::make_unique<Scheduler>(sync_point_manager_.get());
    CHECK_EQ(GetParam(), sync_point_manager_->graph_validation_enabled());
  }

 protected:
  SyncPointManager* sync_point_manager() const {
    return sync_point_manager_.get();
  }

  Scheduler* scheduler() const { return scheduler_.get(); }

  bool graph_validation_enabled() const {
    return sync_point_manager_->graph_validation_enabled();
  }

  void RunAllPendingTasks() {
    base::RunLoop run_loop;
    SequenceId sequence_id =
        scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
    scheduler()->ScheduleTask(Scheduler::Task(
        sequence_id, run_loop.QuitClosure(), std::vector<SyncToken>()));
    run_loop.Run();
    scheduler()->DestroySequence(sequence_id);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<Scheduler> scheduler_;
};

TEST_P(SchedulerDfsTest, ScheduledTasksRunInOrder) {
  SequenceId sequence_id =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);

  int count = 0;
  int ran1 = 0;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id,
                                            GetClosure([&] { ran1 = ++count; }),
                                            std::vector<SyncToken>()));

  int ran2 = 0;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id,
                                            GetClosure([&] { ran2 = ++count; }),
                                            std::vector<SyncToken>()));

  base::RunLoop run_loop;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id, run_loop.QuitClosure(),
                                            std::vector<SyncToken>()));
  run_loop.Run();

  EXPECT_EQ(ran1, 1);
  EXPECT_EQ(ran2, 2);

  scheduler()->DestroySequence(sequence_id);
}

TEST_P(SchedulerDfsTest, ScheduledTasksRunAfterReporting) {
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
  base::RunLoop run_loop;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id, run_loop.QuitClosure(),
                                            std::vector<SyncToken>()));
  run_loop.Run();

  EXPECT_TRUE(ran);
  scheduler()->DestroySequence(sequence_id);
}

TEST_P(SchedulerDfsTest, ContinuedTasksRunFirst) {
  SequenceId sequence_id =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);

  int count = 0;
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

  base::RunLoop run_loop;
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id, run_loop.QuitClosure(),
                                            std::vector<SyncToken>()));
  run_loop.Run();

  EXPECT_EQ(ran1, 1);
  EXPECT_EQ(continued1, 2);
  EXPECT_EQ(ran2, 3);

  scheduler()->DestroySequence(sequence_id);
}

class SchedulerDfsTaskRunOrderTest : public SchedulerDfsTest {
 public:
  SchedulerDfsTaskRunOrderTest() = default;
  ~SchedulerDfsTaskRunOrderTest() override {
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

    sequence_info_.emplace(
        sequence_key,
        SequenceInfo(sequence_id, command_buffer_id, release_state));
  }

  void CreateExternalSequence(int sequence_key) {
    auto order_data = sync_point_manager()->CreateSyncPointOrderData();
    auto command_buffer_id = CommandBufferId::FromUnsafeValue(sequence_key);
    auto release_state = sync_point_manager()->CreateSyncPointClientState(
        kNamespaceId, command_buffer_id, order_data->sequence_id());

    sequence_info_.emplace(
        sequence_key,
        SequenceInfo(std::move(order_data), command_buffer_id, release_state));
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
    sync_tokens_.emplace(
        release_sync,
        SyncToken(kNamespaceId, info_it->second.command_buffer_id, release));
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
    CHECK(info_it != sequence_info_.end());

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
    ScheduleTask(sequence_key, std::vector<int>{wait_sync}, release_sync);
  }

  void ScheduleTask(int sequence_key,
                    const std::vector<int>& wait_syncs,
                    int release_sync) {
    auto closure = GetTaskClosure(sequence_key, release_sync);

    auto info_it = sequence_info_.find(sequence_key);
    ASSERT_TRUE(info_it != sequence_info_.end());

    DCHECK(!info_it->second.external());

    std::vector<SyncToken> waits;
    for (int wait_sync : wait_syncs) {
      if (wait_sync >= 0) {
        waits.push_back(sync_tokens_[wait_sync]);
      }
    }

    SyncToken release;
    if (release_sync >= 0) {
      release = sync_tokens_[release_sync];
    }

    scheduler()->ScheduleTask(Scheduler::Task(
        info_it->second.sequence_id, std::move(closure), waits, release));
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

TEST_P(SchedulerDfsTaskRunOrderTest, SequencesRunInPriorityOrder) {
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

TEST_P(SchedulerDfsTaskRunOrderTest, SequencesOfSamePriorityRunInOrder) {
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

TEST_P(SchedulerDfsTaskRunOrderTest, SequenceWaitsForFence) {
  CreateSequence(0, SchedulingPriority::kHigh);
  CreateSequence(1, SchedulingPriority::kNormal);

  ScheduleTask(1, -1, 0);  // task 0: seq 1, no wait, release 0
  ScheduleTask(0, 0, -1);  // task 1: seq 0, wait 0, no release

  RunAllPendingTasks();

  const int expected_task_order[] = {0, 1};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_P(SchedulerDfsTaskRunOrderTest, SequenceWaitsForFenceExternal) {
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

TEST_P(SchedulerDfsTaskRunOrderTest, WaitOrderNumSmallerThanReleaseOrderNum) {
  CreateSequence(0, SchedulingPriority::kNormal);
  CreateSequence(1, SchedulingPriority::kNormal);

  CreateSyncToken(1, 0);  // declare sync_token 0 on seq 1

  ScheduleTask(0, 0, -1);  // task 0: seq 0, wait 0, no release
  ScheduleTask(1, -1, 0);  // task 1: seq 1, no wait, release 0

  RunAllPendingTasks();

  std::vector<int> expected_task_order;

  if (!graph_validation_enabled()) {
    // In this mode, the wait order number must be larger than the corresponding
    // release number. The wait of task 0 is considered invalid.
    // Task 0 does not wait on unrelease sync token 0.
    expected_task_order = {0, 1};
  } else {
    // In this mode, there is no requirement that the wait order number is
    // larger than the corresponding release number, so task 0 waits on task 1
    // to release the sync token.
    expected_task_order = {1, 0};
  }
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

// Tests that Scheduler::RebuildSchedulingQueueIfNeeded inserts all non-running
// sequences into the queue - even if a sequence is completely blocked.
TEST_P(SchedulerDfsTaskRunOrderTest, SchedulingQueueContainsBlockedSequences) {
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

TEST_P(SchedulerDfsTaskRunOrderTest, ReleaseSequenceHasPriorityOfWaiter) {
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

TEST_P(SchedulerDfsTaskRunOrderTest, ReleaseSequenceRevertsToDefaultPriority) {
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

TEST_P(SchedulerDfsTaskRunOrderTest, ReleaseSequenceCircularRelease) {
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

  // Below is the job graph implied by the above code. The scheduler traverses
  // the graph using DFS. At each node, it visits the highest descendent whose
  // predecessors have all been visited. The traversal for a path stops if there
  // are no such descendents. It then continues from the first ancestor that has
  // a valid descendent.
  /*
    ┌────────────────┐
    │task 2          │
    └┬──────────────┬┘
    ┌▽─────────┐   │
    │task 1     │   │
    └┬─────────┬┘   │
    ┌▽────┐   │    │
    │task 0│   │    │
    └┬─────┘   │    │
    ┌▽───────┐│    │
    │task 3   ││    │
    └┬───────┬┘│    │
    ┌▽────┐┌▽▽──┐│
    │task 4││task 5││
    └┬─────┘└┬───┬─┘│
     │┌─────▽─┐┌▽─▽─┐
     ││task 6  ││task 7│
     │└┬───────┘└┬─┬───┘
    ┌│─┘         │ │
    │└┐    ┌─────┘ │
    │┌▽──▽┐┌────▽┐
    ││task 9││task 8│
    │└────┬┬┘└─────┬┘
    │     │└───┐   │
    └─────│───┐│   └──┐
    ┌────▽─┐┌▽▽───┐│
    │task 12││task 10││
    └───────┘└┬──┬───┘│
    ┌────────▽┐┌▽──▽─┐
    │task 13   ││task 11│
    └──────────┘└┬──────┘
    ┌───────────▽┐
    │task 14      │
    └─────────────┘
*/

  const int expected_task_order[] = {2, 1, 0,  3,  5,  7,  8, 6,
                                     4, 9, 10, 11, 14, 13, 12};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_P(SchedulerDfsTaskRunOrderTest, WaitOnSelfShouldNotBlockSequence) {
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

TEST_P(SchedulerDfsTest, ShouldNotYieldWhenNoTasksToRun) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id1);
  SyncToken sync_token(namespace_id, command_buffer_id, 1);

  scheduler()->ScheduleTask(
      Scheduler::Task(sequence_id1, GetClosure([&] {
                        EXPECT_FALSE(scheduler()->ShouldYield(sequence_id1));
                        release_state->ReleaseFenceSync(1);
                      }),
                      std::vector<SyncToken>()));

  // Schedule a task on another sequence that depends on the above task.
  // ShouldYield should return false because the sequence below isn't runnable
  // (until ReleaseFenceSync is called).
  SequenceId sequence_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  scheduler()->ScheduleTask(
      Scheduler::Task(sequence_id2, GetClosure([] {}), {sync_token}));
  RunAllPendingTasks();

  release_state->Destroy();
  scheduler()->DestroySequence(sequence_id1);
  scheduler()->DestroySequence(sequence_id2);
}

TEST_P(SchedulerDfsTest, ReleaseSequenceShouldYield) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id = CommandBufferId::FromUnsafeValue(1);
  scoped_refptr<SyncPointClientState> release_state =
      sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id1);

  uint64_t release = 1;
  int count = 0;
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

// Tests a situation where a sequence's WaitFence has an order number less than
// the sequence's first order number, because the sequence is currently running,
// and called ShouldYield before release the WaitFence.
TEST_P(SchedulerDfsTest, ShouldYieldIsValidWhenSequenceReleaseIsPending) {
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

  SyncToken sync_token1(namespace_id, command_buffer_id1, 1);
  SyncToken sync_token2(namespace_id, command_buffer_id2, 2);

  // Job 1.1 doesn't depend on anything.
  scheduler()->ScheduleTask(
      Scheduler::Task(sequence_id1, GetClosure([&] {
                        EXPECT_FALSE(scheduler()->ShouldYield(sequence_id1));
                        release_state1->ReleaseFenceSync(1);
                      }),
                      {}));

  // Job 2.1 depends on Job 1.1.
  scheduler()->ScheduleTask(Scheduler::Task(sequence_id2, GetClosure([&] {
                                              release_state2->ReleaseFenceSync(
                                                  sync_token2.release_count());
                                            }),
                                            {sync_token1}));

  // Job 1.2 depends on Job 2.1.
  scheduler()->ScheduleTask(
      Scheduler::Task(sequence_id1, GetClosure([&] {}), {sync_token2}));

  RunAllPendingTasks();

  release_state1->Destroy();
  release_state2->Destroy();

  scheduler()->DestroySequence(sequence_id1);
  scheduler()->DestroySequence(sequence_id2);
}

TEST_P(SchedulerDfsTest, ReentrantEnableSequenceShouldNotDeadlock) {
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

  int count = 0;
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

TEST_P(SchedulerDfsTest, CanSetSequencePriority) {
  SequenceId sequence_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  SequenceId sequence_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  SequenceId sequence_id3 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);

  int count = 0;
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

  scheduler()->GetSchedulerDfsForTesting()->SetSequencePriority(
      sequence_id2, SchedulingPriority::kHigh);

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

  scheduler()->GetSchedulerDfsForTesting()->SetSequencePriority(
      sequence_id2,
      scheduler()->GetSchedulerDfsForTesting()->GetSequenceDefaultPriority(
          sequence_id2));

  RunAllPendingTasks();

  EXPECT_EQ(ran3, 4);
  EXPECT_EQ(ran1, 5);
  EXPECT_EQ(ran2, 6);

  scheduler()->DestroySequence(sequence_id1);
  scheduler()->DestroySequence(sequence_id2);
  scheduler()->DestroySequence(sequence_id3);
}

TEST_P(SchedulerDfsTest, StreamPriorities) {
  SequenceId seq_id1 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kLow);
  SequenceId seq_id2 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kNormal);
  SequenceId seq_id3 =
      scheduler()->CreateSequenceForTesting(SchedulingPriority::kHigh);

  CommandBufferNamespace namespace_id = CommandBufferNamespace::GPU_IO;
  CommandBufferId command_buffer_id1 = CommandBufferId::FromUnsafeValue(1);
  CommandBufferId command_buffer_id2 = CommandBufferId::FromUnsafeValue(2);

  base::AutoLock auto_lock(scheduler()->GetSchedulerDfsForTesting()->lock());

  SchedulerDfs::Sequence* seq1 =
      scheduler()->GetSchedulerDfsForTesting()->GetSequence(seq_id1);
  SchedulerDfs::Sequence* seq2 =
      scheduler()->GetSchedulerDfsForTesting()->GetSequence(seq_id2);
  SchedulerDfs::Sequence* seq3 =
      scheduler()->GetSchedulerDfsForTesting()->GetSequence(seq_id3);

  // Initial priorities.
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  SyncToken sync_token1(namespace_id, command_buffer_id1, 1);
  SyncToken sync_token2(namespace_id, command_buffer_id2, 1);

  // Make sure that waiting for fences does not change sequence priorities.
  seq2->AddTask(/*closure=*/{}, {sync_token1}, /*release=*/{},
                /*report_callback=*/{});
  seq3->AddTask(/*closure=*/{}, {sync_token2}, /*release=*/{},
                /*report_callback=*/{});
  EXPECT_EQ(SchedulingPriority::kLow, seq1->current_priority());
  EXPECT_EQ(SchedulingPriority::kNormal, seq2->current_priority());
  EXPECT_EQ(SchedulingPriority::kHigh, seq3->current_priority());

  {
    base::AutoUnlock auto_unlock(
        scheduler()->GetSchedulerDfsForTesting()->lock());
    scheduler()->DestroySequence(seq_id1);
    scheduler()->DestroySequence(seq_id2);
    scheduler()->DestroySequence(seq_id3);
  }
}

// Tests SchedulerDfs behavior when graph validation of sync points is enabled.
// The tests verify that the integration with TaskGraph works properly. More
// comprehensive testing of validation behavior is done in
// task_graph_unittest.cc.
class SchedulerDfsGraphValidationTest : public SchedulerDfsTaskRunOrderTest {
 public:
  SchedulerDfsGraphValidationTest() = default;
  ~SchedulerDfsGraphValidationTest() override = default;

 protected:
  void SetUp() override {
    SchedulerDfsTaskRunOrderTest::SetUp();
    CHECK(graph_validation_enabled());
  }
};

TEST_P(SchedulerDfsGraphValidationTest, ValidationWaitWithoutRelease) {
  // Two tasks on the same sequence wait for unreleased fences.
  CreateSequence(0, SchedulingPriority::kNormal);
  CreateSequence(1, SchedulingPriority::kNormal);
  CreateSequence(2, SchedulingPriority::kNormal);

  CreateSyncToken(1, 0);  // declare sync_token 0 on seq 1
  CreateSyncToken(1, 1);  // declare sync_token 1 on seq 1

  CreateSyncToken(2, 2);  // declare sync_token 2 on seq 2
  CreateSyncToken(2, 3);  // declare sync_token 3 on seq 2

  ScheduleTask(0, {0, 3}, -1);  // task 0: seq 0, wait {0,3}, no release

  RunAllPendingTasks();
  EXPECT_TRUE(tasks_executed().empty());

  // Submit a task close to the time when the validation timer will be fired.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay -
                                  TaskGraph::kMinValidationDelay +
                                  base::Seconds(1));
  ScheduleTask(0, {1, 2}, -1);  // task 1: seq 0, wait {1,2}, no release

  // Cause the validation timer to fire.
  task_environment_.FastForwardBy(TaskGraph::kMinValidationDelay);
  RunAllPendingTasks();

  // Only task 0 is supposed to be executed.
  // Task 1 has sync_token 1 that is not satisfied. And it is too new to be
  // validated.
  std::vector<int> expected_task_order = {0};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));

  // The validation timer should be fired again and resolve the invalid wait
  // of task 1.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay +
                                  base::Seconds(1));
  RunAllPendingTasks();

  expected_task_order = {0, 1};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

TEST_P(SchedulerDfsGraphValidationTest, ValidationCircularWaits) {
  // Task 0 waits for task 1; while task 1 waits for task 2:
  //
  //   seq 0           seq 1
  // |        |     |        |
  // |(task 0)|---->|(task 1)|
  // |        |    /|        |
  // |(task 2)|<--/ |        |
  // |        |     |        |

  CreateSequence(0, SchedulingPriority::kNormal);
  CreateSequence(1, SchedulingPriority::kNormal);

  CreateSyncToken(1, 0);  // declare sync_token 0 on seq 1
  CreateSyncToken(0, 1);  // declare sync_token 1 on seq 0

  ScheduleTask(0, 0, -1);  // task 0: seq 0, wait 0, no release

  // Submit task 1 on sequence 1 later. Validation on sequence 0 will be
  // triggered first.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay -
                                  base::Seconds(1));

  ScheduleTask(1, 1, 0);   // task 1: seq 1, wait 1, release 0
  ScheduleTask(0, -1, 1);  // task 2: seq 0, no wait, release 1

  RunAllPendingTasks();
  EXPECT_TRUE(tasks_executed().empty());

  // Trigger validation on sequence 0.
  task_environment_.FastForwardBy(base::Seconds(2));
  RunAllPendingTasks();

  std::vector<int> expected_task_order{1, 0, 2};
  EXPECT_THAT(tasks_executed(), testing::ElementsAreArray(expected_task_order));
}

INSTANTIATE_TEST_SUITE_P(All, SchedulerDfsTest, testing::Values(false, true));

INSTANTIATE_TEST_SUITE_P(All,
                         SchedulerDfsTaskRunOrderTest,
                         testing::Values(false, true));

// Only test the case of IsSyncPointGraphValidationEnabled() being true.
INSTANTIATE_TEST_SUITE_P(All,
                         SchedulerDfsGraphValidationTest,
                         testing::Values(true));

}  // namespace gpu

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/task_graph.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class TaskGraphTest : public testing::Test {
 protected:
  TaskGraphTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSyncPointGraphValidation);
    // Initialize after the feature flag has set.
    sync_point_manager_ = std::make_unique<SyncPointManager>();
    task_graph_ = std::make_unique<TaskGraph>(sync_point_manager_.get());

    CHECK(task_graph_->graph_validation_enabled());
  }

  ~TaskGraphTest() override {
    for (auto& info : sequence_info_) {
      task_graph_->DestroySequence(info.second.sequence_id);
    }
  }

  void CreateSequence(int sequence_key) {
    CommandBufferId command_buffer_id =
        CommandBufferId::FromUnsafeValue(sequence_key);
    SequenceId sequence_id = task_graph_->CreateSequence(
        base::DoNothing(), base::SingleThreadTaskRunner::GetCurrentDefault(),
        kNamespaceId, command_buffer_id);

    sequence_info_.emplace(sequence_key,
                           SequenceInfo(sequence_id, command_buffer_id));
  }

  void CreateSyncToken(int sequence_key, int release_sync) {
    auto info_it = sequence_info_.find(sequence_key);
    ASSERT_TRUE(info_it != sequence_info_.end());

    uint64_t release = release_sync + 1;
    sync_tokens_.emplace(
        release_sync,
        SyncToken(kNamespaceId, info_it->second.command_buffer_id, release));
  }

  TaskCallback GetTaskCallback() {
    const int task_id = num_tasks_added_++;

    return base::BindLambdaForTesting(
        [this, task_id](FenceSyncReleaseDelegate* release_delegate) {
          if (release_delegate) {
            release_delegate->Release();
          }
          tasks_executed_.push_back(task_id);
        });
  }

  void AddTask(int sequence_key, int wait_sync, int release_sync) {
    AddTask(sequence_key, std::vector<int>{wait_sync}, release_sync);
  }

  void AddTask(int sequence_key,
               const std::vector<int>& wait_syncs,
               int release_sync) {
    auto info_it = sequence_info_.find(sequence_key);
    ASSERT_TRUE(info_it != sequence_info_.end());

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

    base::AutoLock auto_lock(task_graph_->lock());

    task_graph_->GetSequence(info_it->second.sequence_id)
        ->AddTask(GetTaskCallback(), std::move(waits), release,
                  /*report_callback=*/{});
  }

  void RunAllPendingTasks() {
    size_t previous_tasks_executed;
    base::AutoLock auto_lock(task_graph_->lock());
    do {
      previous_tasks_executed = tasks_executed_.size();
      for (auto& info : sequence_info_) {
        TaskGraph::Sequence* sequence =
            task_graph_->GetSequence(info.second.sequence_id);

        while (sequence->IsFrontTaskUnblocked()) {
          base::OnceClosure task_closure;
          uint32_t order_num = sequence->BeginTask(&task_closure);
          SyncToken release = sequence->current_task_release();

          {
            base::AutoUnlock auto_unlock(task_graph_->lock());
            sequence->order_data()->BeginProcessingOrderNumber(order_num);
            std::move(task_closure).Run();

            if (release.HasData()) {
              task_graph_->sync_point_manager()->EnsureFenceSyncReleased(
                  release, ReleaseCause::kTaskCompletionRelease);
            }
            sequence->order_data()->FinishProcessingOrderNumber(order_num);
          }
          sequence->FinishTask();
        }
      }
    } while (previous_tasks_executed != tasks_executed_.size());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  std::vector<int> tasks_executed_;

  std::unique_ptr<SyncPointManager> sync_point_manager_;

 private:
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;

  int num_tasks_added_ = 0;

  struct SequenceInfo {
    SequenceInfo(SequenceId sequence_id, CommandBufferId command_buffer_id)
        : sequence_id(sequence_id), command_buffer_id(command_buffer_id) {}

    SequenceId sequence_id;
    CommandBufferId command_buffer_id;
  };

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TaskGraph> task_graph_;

  std::map<int, const SequenceInfo> sequence_info_;
  std::map<int, const SyncToken> sync_tokens_;
};

TEST_F(TaskGraphTest, ValidationWaitWithoutRelease) {
  // Two tasks on the same sequence wait for unreleased fences.
  CreateSequence(0);
  CreateSequence(1);
  CreateSequence(2);

  CreateSyncToken(1, 0);  // declare sync_token 0 on seq 1
  CreateSyncToken(1, 1);  // declare sync_token 1 on seq 1

  CreateSyncToken(2, 2);  // declare sync_token 2 on seq 2
  CreateSyncToken(2, 3);  // declare sync_token 3 on seq 2

  AddTask(0, {0, 3}, -1);  // task 0: seq 0, wait {0,3}, no release

  // Submit a task close to the time when the validation timer will be fired.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay -
                                  TaskGraph::kMinValidationDelay +
                                  base::Seconds(1));
  AddTask(0, {1, 2}, -1);  // task 1: seq 0, wait {1,2}, no release

  // Cause the validation timer to fire.
  task_environment_.FastForwardBy(TaskGraph::kMinValidationDelay);
  RunAllPendingTasks();

  // Only task 0 is supposed to be executed.
  // Task 1 has unsatisfied waits, but it is too new to be validated.
  std::vector<int> expected_task_order = {0};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));

  // The validation timer should be fired again and resolve the invalid waits
  // of task 1.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay +
                                  base::Seconds(1));
  RunAllPendingTasks();

  expected_task_order = {0, 1};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));
}

TEST_F(TaskGraphTest, ValidationWaitWithoutRelease2) {
  // Task 0 waits for task 1 on a different sequence. Task 1 waits for an
  // unreleased fence.

  CreateSequence(0);
  CreateSequence(1);
  CreateSequence(2);

  CreateSyncToken(1, 0);  // declare sync_token 0 on seq 1
  CreateSyncToken(2, 1);  // declare sync_token 1 on seq 2

  AddTask(0, 0, -1);  // task 0: seq 0, wait 0, no release

  // Submit task 1 on sequence 1 later. Validation on sequence 0 will be
  // triggered first.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay -
                                  base::Seconds(1));

  // Sync_token 1 that task 1 waits on is not released by anyone.
  AddTask(1, 1, 0);  // task 1: seq 1, wait 1, release 0

  RunAllPendingTasks();
  EXPECT_TRUE(tasks_executed_.empty());

  // Trigger validation on sequence 0.
  task_environment_.FastForwardBy(base::Seconds(2));
  RunAllPendingTasks();

  std::vector<int> expected_task_order{1, 0};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));
}

TEST_F(TaskGraphTest, ValidationCircularWaits) {
  // Task 0 and task 1 wait for each other.
  //
  //   seq 0           seq 1
  // |        |     |        |
  // |(task 0)|<--->|(task 1)|
  // |        |     |        |

  CreateSequence(0);
  CreateSequence(1);

  CreateSyncToken(1, 2);  // declare sync_token 2 on seq 1
  CreateSyncToken(0, 3);  // declare sync_token 3 on seq 0

  AddTask(0, 2, 3);  // task 0: seq 0, wait 2, release 3

  // Submit task 1 on sequence 1 later. Validation on sequence 0 will be
  // triggered first.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay -
                                  base::Seconds(1));

  AddTask(1, 3, 2);  // task 1: seq 1, wait 3, release 2

  RunAllPendingTasks();
  EXPECT_TRUE(tasks_executed_.empty());

  // Trigger validation on sequence 0.
  task_environment_.FastForwardBy(base::Seconds(2));
  RunAllPendingTasks();

  std::vector<int> expected_task_order{1, 0};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));
}

TEST_F(TaskGraphTest, ValidationCircularWaits2) {
  // Task 0 waits for task 1; while task 1 waits for task 2:
  //
  //   seq 0           seq 1
  // |        |     |        |
  // |(task 0)|---->|(task 1)|
  // |        |    /|        |
  // |(task 2)|<--/ |        |
  // |        |     |        |

  CreateSequence(0);
  CreateSequence(1);

  CreateSyncToken(1, 2);  // declare sync_token 2 on seq 1
  CreateSyncToken(0, 3);  // declare sync_token 3 on seq 0

  AddTask(0, 2, -1);  // task 0: seq 0, wait 2, no release

  // Submit task 1 on sequence 1 later. Validation on sequence 0 will be
  // triggered first.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay -
                                  base::Seconds(1));

  AddTask(1, 3, 2);   // task 1: seq 1, wait 3, release 2
  AddTask(0, -1, 3);  // task 2: seq 0, no wait, release 3

  RunAllPendingTasks();
  EXPECT_TRUE(tasks_executed_.empty());

  // Trigger validation on sequence 0.
  task_environment_.FastForwardBy(base::Seconds(2));
  RunAllPendingTasks();

  std::vector<int> expected_task_order{1, 0, 2};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));
}

TEST_F(TaskGraphTest, ValidationCircularWaits3) {
  // Task 0 waits for task 1 on the same sequence.
  //
  //   seq 0
  // |        |
  // |(task 0)|---|
  // |        |   |
  // |(task 1)|<--|
  // |        |

  CreateSequence(0);

  CreateSyncToken(0, 0);  // declare sync_token 0 on seq 0

  AddTask(0, 0, -1);  // task 0: seq 0, wait 0, no release
  AddTask(0, -1, 0);  // task 1: seq 0, no wait, release 0

  // No need to trigger the validation. Because SyncPointManager::Wait() will
  // refuse to add a wait for the same sequence, task 0 is not blocked with
  // circular dependency.
  RunAllPendingTasks();
  std::vector<int> expected_task_order{0, 1};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));
}

TEST_F(TaskGraphTest, ValidationCircularWaits4) {
  // A more complex graph with multiple cycles.
  //
  //   seq 0           seq 1          seq 2
  // |        |     |        |     |        |
  // |        |     |(task 3)|     |        |
  // |        |     |        |     |        |
  // |(task 0)|--2->|(task 4)|--1->|(task 7)|----|
  // |        |     |        |     |        |    |
  // |(task 1)|--5->|(task 5)|--4->|(task 8)|-|  |
  // |        |     |        |                |  |
  // |(task 2)|<-3--+--------+----------------|  |
  // |        |     |        |                   |
  //                |(task 6)|<-0----------------|
  //                |        |

  sync_point_manager_->set_suppress_fatal_log_for_testing();

  CreateSequence(0);
  CreateSequence(1);
  CreateSequence(2);

  CreateSyncToken(0, 3);  // declare sync_token 3 on seq 0

  CreateSyncToken(1, 0);  // declare sync_token 0 on seq 1
  CreateSyncToken(1, 2);  // declare sync_token 2 on seq 1
  CreateSyncToken(1, 5);  // declare sync_token 5 on seq 1

  CreateSyncToken(2, 1);  // declare sync_token 1 on seq 2
  CreateSyncToken(2, 4);  // declare sync_token 4 on seq 2

  AddTask(0, 2, -1);  // task 0: seq 0, wait 2, no release

  // Submit task 1 on sequence 0 later, so that the first validation only covers
  // task 0.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay -
                                  base::Seconds(2));

  AddTask(0, 5, -1);  // task 1: seq 0, wait 5, no release
  AddTask(0, -1, 3);  // task 2: seq 0, no wait, release 3

  AddTask(1, -1, -1);  // task 3: seq 1, no wait, no release
  AddTask(1, 1, 2);    // task 4: seq 1, wait 1, release 2
  AddTask(1, 4, 5);    // task 5: seq 1, wait 4, release 5
  AddTask(1, -1, 0);   // task 6: seq 1, no wait, release 0

  // Submit tasks on sequence 2 later. The next validation will happen on
  // sequence 1.
  task_environment_.FastForwardBy(base::Seconds(1));
  AddTask(2, 0, 1);  // task 7: seq 2, wait 0, release 1
  AddTask(2, 3, 4);  // task 8: seq 2, wait 3, release 4

  RunAllPendingTasks();
  std::vector<int> expected_task_order{3};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));

  // Trigger validation on sequence 0 for task 0.
  // It should forcefully release sync_token 0 to break circular dependencies.
  task_environment_.FastForwardBy(base::Seconds(2));
  RunAllPendingTasks();

  expected_task_order = {3, 7, 4, 0};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));

  // Trigger another validation on sequence 1 for task 5 and 6.
  // It should forcefully release sync_token 5 to break circular dependencies.
  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay);
  RunAllPendingTasks();

  expected_task_order = {3, 7, 4, 0, 1, 2, 8, 5, 6};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));
}

TEST_F(TaskGraphTest, ValidationPartiallyValidated) {
  // Test validation when part of the task graph has been validated in previous
  // validation rounds.
  //
  //   seq 0           seq 1
  // |        |     |        |
  // |(task 0)|--4->|(task 1)|
  // |        |     |        |
  // |(task 3)|<-5--|(task 2)|
  // |        |     |        |
  //
  // Task 0 and task 1 are validated by the first validation round triggered
  // on seq 0; while task 2 and task 3 are validated by the second validation
  // round triggered on seq 1.

  CreateSequence(0);
  CreateSequence(1);

  CreateSyncToken(1, 4);  // declare sync_token 4 on seq 1
  CreateSyncToken(0, 5);  // declare sync_token 5 on seq 0

  AddTask(0, 4, -1);  // task 0: seq 0, wait 4, no release

  // Submit task 1 on sequence 1 later, so that the first validation is
  // triggered on seq 0.
  task_environment_.FastForwardBy(base::Seconds(1));

  AddTask(1, -1, 4);  // task 1: seq 1, no wait, release 4

  task_environment_.FastForwardBy(base::Seconds(1));

  AddTask(1, 5, -1);  // task 2: seq 1, wait 5, no release

  task_environment_.FastForwardBy(TaskGraph::kMaxValidationDelay -
                                  base::Seconds(3));

  AddTask(0, -1, 5);  // task 3: seq 0, no wait, release 5

  // Trigger first validation round on seq 0.
  task_environment_.FastForwardBy(base::Milliseconds(1500));

  // Trigger second validation round on seq 1.
  task_environment_.FastForwardBy(base::Seconds(1));

  RunAllPendingTasks();
  std::vector<int> expected_task_order{1, 0, 3, 2};
  EXPECT_THAT(tasks_executed_, testing::ElementsAreArray(expected_task_order));
}

}  // namespace gpu

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

using base::sequence_manager::TaskQueue;
using QueueType = blink::scheduler::MainThreadTaskQueue::QueueType;
using QueueTraits = blink::scheduler::MainThreadTaskQueue::QueueTraits;

namespace blink {
namespace scheduler {

class FrameTaskQueueControllerTest : public testing::Test,
                                     public FrameTaskQueueController::Delegate {
 public:
  FrameTaskQueueControllerTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        task_queue_created_count_(0) {}
  FrameTaskQueueControllerTest(const FrameTaskQueueControllerTest&) = delete;
  FrameTaskQueueControllerTest& operator=(const FrameTaskQueueControllerTest&) =
      delete;

  ~FrameTaskQueueControllerTest() override = default;

  void SetUp() override {
    auto settings = base::sequence_manager::SequenceManager::Settings::Builder()
                        .SetPrioritySettings(CreatePrioritySettings())
                        .Build();
    scheduler_ = std::make_unique<MainThreadSchedulerImpl>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMockTickClock(), std::move(settings)));
    agent_group_scheduler_ = scheduler_->CreateAgentGroupScheduler();
    page_scheduler_ = agent_group_scheduler_->CreatePageScheduler(nullptr);
    frame_scheduler_ = page_scheduler_->CreateFrameScheduler(
        nullptr, /*is_in_embedded_frame_tree=*/false,
        FrameScheduler::FrameType::kSubframe);
    frame_task_queue_controller_ = std::make_unique<FrameTaskQueueController>(
        scheduler_.get(),
        static_cast<FrameSchedulerImpl*>(frame_scheduler_.get()), this);
  }

  void TearDown() override {
    frame_task_queue_controller_.reset();
    frame_scheduler_.reset();
    page_scheduler_.reset();
    agent_group_scheduler_ = nullptr;
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  // FrameTaskQueueController::Delegate implementation.
  void OnTaskQueueCreated(MainThreadTaskQueue* task_queue,
                          TaskQueue::QueueEnabledVoter* voter) override {
    ++task_queue_created_count_;
  }

 protected:
  scoped_refptr<MainThreadTaskQueue> LoadingTaskQueue() const {
    return frame_task_queue_controller_->GetTaskQueue(QueueTraits()
        .SetCanBePaused(true)
        .SetCanBeFrozen(true)
        .SetCanBeDeferred(true)
        .SetPrioritisationType(
            QueueTraits::PrioritisationType::kLoading));
  }

  scoped_refptr<MainThreadTaskQueue> LoadingControlTaskQueue() const {
    return frame_task_queue_controller_->GetTaskQueue(QueueTraits()
        .SetCanBePaused(true)
        .SetCanBeFrozen(true)
        .SetCanBeDeferred(true)
        .SetPrioritisationType(
            QueueTraits::PrioritisationType::kLoadingControl));
  }

  scoped_refptr<MainThreadTaskQueue> ThrottleableTaskQueue() const {
    return frame_task_queue_controller_->GetTaskQueue(
        QueueTraits()
            .SetCanBeThrottled(true)
            .SetCanBeFrozen(true)
            .SetCanBeDeferred(true)
            .SetCanBePaused(true)
            .SetCanRunWhenVirtualTimePaused(false));
  }

  scoped_refptr<MainThreadTaskQueue> GetTaskQueue(
      QueueTraits queue_traits) const {
    return frame_task_queue_controller_->GetTaskQueue(queue_traits);
  }

  size_t task_queue_created_count() const { return task_queue_created_count_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  Persistent<AgentGroupScheduler> agent_group_scheduler_;
  std::unique_ptr<PageScheduler> page_scheduler_;
  std::unique_ptr<FrameScheduler> frame_scheduler_;
  std::unique_ptr<FrameTaskQueueController> frame_task_queue_controller_;

 private:
  size_t task_queue_created_count_;
};

TEST_F(FrameTaskQueueControllerTest, CreateAllTaskQueues) {
  enum class QueueCheckResult { kDidNotSeeQueue, kDidSeeQueue };

  WTF::HashMap<scoped_refptr<MainThreadTaskQueue>, QueueCheckResult>
      all_task_queues;

  scoped_refptr<MainThreadTaskQueue> task_queue = LoadingTaskQueue();
  EXPECT_FALSE(all_task_queues.Contains(task_queue));
  all_task_queues.insert(task_queue.get(), QueueCheckResult::kDidNotSeeQueue);
  EXPECT_EQ(all_task_queues.size(), task_queue_created_count());

  task_queue = LoadingControlTaskQueue();
  EXPECT_FALSE(all_task_queues.Contains(task_queue));
  all_task_queues.insert(task_queue.get(), QueueCheckResult::kDidNotSeeQueue);
  EXPECT_EQ(all_task_queues.size(), task_queue_created_count());

  // Create the 4 default task queues used by FrameSchedulerImpl.
  task_queue = GetTaskQueue(QueueTraits()
                                .SetCanBeThrottled(true)
                                .SetCanBeDeferred(true)
                                .SetCanBeFrozen(true)
                                .SetCanBePaused(true)
                                .SetCanRunWhenVirtualTimePaused(false));
  EXPECT_FALSE(all_task_queues.Contains(task_queue));
  all_task_queues.insert(task_queue.get(), QueueCheckResult::kDidNotSeeQueue);
  EXPECT_EQ(all_task_queues.size(), task_queue_created_count());

  task_queue = GetTaskQueue(QueueTraits()
                                .SetCanBeDeferred(true)
                                .SetCanBePaused(true)
                                .SetCanRunWhenVirtualTimePaused(false));
  EXPECT_FALSE(all_task_queues.Contains(task_queue));
  all_task_queues.insert(task_queue.get(), QueueCheckResult::kDidNotSeeQueue);
  EXPECT_EQ(all_task_queues.size(), task_queue_created_count());

  task_queue = GetTaskQueue(
      QueueTraits().SetCanBePaused(true).SetCanRunWhenVirtualTimePaused(false));
  EXPECT_FALSE(all_task_queues.Contains(task_queue));
  all_task_queues.insert(task_queue.get(), QueueCheckResult::kDidNotSeeQueue);
  EXPECT_EQ(all_task_queues.size(), task_queue_created_count());

  task_queue =
      GetTaskQueue(QueueTraits().SetCanRunWhenVirtualTimePaused(false));
  EXPECT_FALSE(all_task_queues.Contains(task_queue));
  all_task_queues.insert(task_queue.get(), QueueCheckResult::kDidNotSeeQueue);
  EXPECT_EQ(all_task_queues.size(), task_queue_created_count());

  // Verify that we get all of the queues that we added, and only those queues.
  EXPECT_EQ(all_task_queues.size(),
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  for (const auto& task_queue_and_voter :
       frame_task_queue_controller_->GetAllTaskQueuesAndVoters()) {
    auto [task_queue_ptr, voter] = task_queue_and_voter;

    EXPECT_NE(task_queue_ptr, nullptr);
    EXPECT_TRUE(base::Contains(all_task_queues, task_queue_ptr));
    // Make sure we don't get the same queue twice.
    auto it = all_task_queues.find(task_queue_ptr);
    EXPECT_FALSE(it == all_task_queues.end());
    EXPECT_EQ(it->value, QueueCheckResult::kDidNotSeeQueue);
    all_task_queues.Set(task_queue_ptr, QueueCheckResult::kDidSeeQueue);
    EXPECT_NE(voter, nullptr);
  }
}

TEST_F(FrameTaskQueueControllerTest,
       NonWebSchedulingTaskQueueWebSchedulingPriorityNullopt) {
  scoped_refptr<MainThreadTaskQueue> task_queue =
      frame_task_queue_controller_->GetTaskQueue(
          MainThreadTaskQueue::QueueTraits());
  EXPECT_EQ(std::nullopt, task_queue->GetWebSchedulingPriority());
}

TEST_F(FrameTaskQueueControllerTest, AddWebSchedulingTaskQueues) {
  scoped_refptr<MainThreadTaskQueue> task_queue =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          QueueTraits(), WebSchedulingQueueType::kTaskQueue,
          WebSchedulingPriority::kUserBlockingPriority);
  EXPECT_EQ(1u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  EXPECT_EQ(WebSchedulingPriority::kUserBlockingPriority,
            task_queue->GetWebSchedulingPriority().value());

  task_queue = frame_task_queue_controller_->NewWebSchedulingTaskQueue(
      QueueTraits(), WebSchedulingQueueType::kTaskQueue,
      WebSchedulingPriority::kUserVisiblePriority);
  EXPECT_EQ(2u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  EXPECT_EQ(WebSchedulingPriority::kUserVisiblePriority,
            task_queue->GetWebSchedulingPriority().value());

  task_queue = frame_task_queue_controller_->NewWebSchedulingTaskQueue(
      QueueTraits(), WebSchedulingQueueType::kTaskQueue,
      WebSchedulingPriority::kBackgroundPriority);
  EXPECT_EQ(3u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  EXPECT_EQ(WebSchedulingPriority::kBackgroundPriority,
            task_queue->GetWebSchedulingPriority().value());
}

TEST_F(FrameTaskQueueControllerTest, RemoveWebSchedulingTaskQueues) {
  scoped_refptr<MainThreadTaskQueue> task_queue =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          QueueTraits(), WebSchedulingQueueType::kTaskQueue,
          WebSchedulingPriority::kUserBlockingPriority);
  EXPECT_EQ(1u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  EXPECT_EQ(WebSchedulingPriority::kUserBlockingPriority,
            task_queue->GetWebSchedulingPriority().value());

  scoped_refptr<MainThreadTaskQueue> task_queue2 =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          QueueTraits(), WebSchedulingQueueType::kTaskQueue,
          WebSchedulingPriority::kUserVisiblePriority);
  EXPECT_EQ(2u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  EXPECT_EQ(WebSchedulingPriority::kUserVisiblePriority,
            task_queue2->GetWebSchedulingPriority().value());

  frame_task_queue_controller_->RemoveWebSchedulingTaskQueue(task_queue.get());
  EXPECT_EQ(1u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  frame_task_queue_controller_->RemoveWebSchedulingTaskQueue(task_queue2.get());
  EXPECT_EQ(0u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
}

TEST_F(FrameTaskQueueControllerTest,
       AddMultipleSamePriorityWebSchedulingTaskQueues) {
  scoped_refptr<MainThreadTaskQueue> task_queue1 =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          QueueTraits(), WebSchedulingQueueType::kTaskQueue,
          WebSchedulingPriority::kUserBlockingPriority);
  EXPECT_EQ(1u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  EXPECT_EQ(WebSchedulingPriority::kUserBlockingPriority,
            task_queue1->GetWebSchedulingPriority().value());

  scoped_refptr<MainThreadTaskQueue> task_queue2 =
      frame_task_queue_controller_->NewWebSchedulingTaskQueue(
          QueueTraits(), WebSchedulingQueueType::kTaskQueue,
          WebSchedulingPriority::kUserBlockingPriority);
  EXPECT_EQ(2u,
            frame_task_queue_controller_->GetAllTaskQueuesAndVoters().size());
  EXPECT_EQ(WebSchedulingPriority::kUserBlockingPriority,
            task_queue2->GetWebSchedulingPriority().value());

  EXPECT_NE(task_queue1.get(), task_queue2.get());
}

TEST_F(FrameTaskQueueControllerTest, QueueTypeFromQueueTraits) {
  scoped_refptr<MainThreadTaskQueue> task_queue = LoadingTaskQueue();
  EXPECT_EQ(task_queue->queue_type(),
            MainThreadTaskQueue::QueueType::kFrameLoading);

  task_queue = LoadingControlTaskQueue();
  EXPECT_EQ(task_queue->queue_type(),
            MainThreadTaskQueue::QueueType::kFrameLoadingControl);

  task_queue = ThrottleableTaskQueue();
  EXPECT_EQ(task_queue->queue_type(),
            MainThreadTaskQueue::QueueType::kFrameThrottleable);
}

class TaskQueueCreationFromQueueTraitsTest :
    public FrameTaskQueueControllerTest,
    public testing::WithParamInterface<QueueTraits::PrioritisationType> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    TaskQueueCreationFromQueueTraitsTest,
    ::testing::Values(
        QueueTraits::PrioritisationType::kInternalScriptContinuation,
        QueueTraits::PrioritisationType::kBestEffort,
        QueueTraits::PrioritisationType::kRegular,
        QueueTraits::PrioritisationType::kLoading,
        QueueTraits::PrioritisationType::kLoadingControl,
        QueueTraits::PrioritisationType::kFindInPage,
        QueueTraits::PrioritisationType::kExperimentalDatabase,
        QueueTraits::PrioritisationType::kJavaScriptTimer,
        QueueTraits::PrioritisationType::kHighPriorityLocalFrame,
        QueueTraits::PrioritisationType::kInput));

TEST_P(TaskQueueCreationFromQueueTraitsTest,
        AddAndRetrieveAllTaskQueues) {
  // Create queues for all combination of queue traits for all combinations of
  // the 6 QueueTraits bits with different PrioritisationTypes.
  WTF::HashSet<scoped_refptr<MainThreadTaskQueue>> all_task_queues;
  constexpr size_t kTotalUniqueQueueTraits = 1 << 6;
  for (size_t i = 0; i < kTotalUniqueQueueTraits; i++) {
    QueueTraits::PrioritisationType prioritisation_type = GetParam();
    MainThreadTaskQueue::QueueTraits queue_traits =
        QueueTraits()
            .SetCanBeThrottled(!!(i & 1 << 0))
            .SetCanBeDeferred(!!(i & 1 << 1))
            .SetCanBeFrozen(!!(i & 1 << 2))
            .SetCanBePaused(!!(i & 1 << 3))
            .SetCanRunInBackground(!!(i & 1 << 4))
            .SetCanRunWhenVirtualTimePaused(!!(i & 1 << 5))
            .SetPrioritisationType(prioritisation_type);
    scoped_refptr<MainThreadTaskQueue> task_queue =
        frame_task_queue_controller_->GetTaskQueue(queue_traits);
    EXPECT_FALSE(all_task_queues.Contains(task_queue));
    all_task_queues.insert(task_queue);
    EXPECT_EQ(task_queue->GetQueueTraits(), queue_traits);
    EXPECT_EQ(task_queue->GetQueueTraits().prioritisation_type,
              prioritisation_type);
  }
  // Make sure we get the same queues back, with matching QueueTraits.
  EXPECT_EQ(all_task_queues.size(), kTotalUniqueQueueTraits);
  for (const auto& task_queue : all_task_queues) {
    scoped_refptr<MainThreadTaskQueue> returned_task_queue =
        frame_task_queue_controller_->GetTaskQueue(
            task_queue->GetQueueTraits());
    EXPECT_EQ(task_queue->GetQueueTraits(),
              returned_task_queue->GetQueueTraits());
    EXPECT_TRUE(task_queue == returned_task_queue);
  }
}

}  // namespace scheduler
}  // namespace blink

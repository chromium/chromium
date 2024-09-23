// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/cpu_time_budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/test/web_scheduling_test_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using testing::ElementsAre;
using testing::ElementsAreArray;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace worker_scheduler_unittest {

namespace {

void AppendToVectorTestTask(Vector<String>* vector, String value) {
  vector->push_back(value);
}

void RunChainedTask(scoped_refptr<NonMainThreadTaskQueue> task_queue,
                    int count,
                    base::TimeDelta duration,
                    scoped_refptr<base::TestMockTimeTaskRunner> environment,
                    Vector<base::TimeTicks>* tasks) {
  tasks->push_back(environment->GetMockTickClock()->NowTicks());

  environment->AdvanceMockTickClock(duration);

  if (count == 1)
    return;

  // Add a delay of 50ms to ensure that wake-up based throttling does not affect
  // us.
  task_queue->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, task_queue, count - 1, duration,
                     environment, base::Unretained(tasks)),
      base::Milliseconds(50));
}

void IncrementCounter(int* counter) {
  ++*counter;
}

class TestObject {
 public:
  explicit TestObject(int* counter) : counter_(counter) {}

  ~TestObject() { ++(*counter_); }

 private:
  raw_ptr<int> counter_;
};

}  // namespace

class WorkerThreadSchedulerForTest : public WorkerThreadScheduler {
 public:
  // |manager| and |proxy| must remain valid for the entire lifetime of this
  // object.
  WorkerThreadSchedulerForTest(ThreadType thread_type,
                               base::sequence_manager::SequenceManager* manager,
                               WorkerSchedulerProxy* proxy)
      : WorkerThreadScheduler(thread_type, manager, proxy) {}

  const HashSet<WorkerSchedulerImpl*>& worker_schedulers() {
    return GetWorkerSchedulersForTesting();
  }

  using WorkerThreadScheduler::CreateBudgetPools;
  using WorkerThreadScheduler::SetCPUTimeBudgetPoolForTesting;
};

class WorkerSchedulerForTest : public WorkerSchedulerImpl {
 public:
  explicit WorkerSchedulerForTest(
      WorkerThreadSchedulerForTest* thread_scheduler)
      : WorkerSchedulerImpl(thread_scheduler, nullptr) {}

  using WorkerSchedulerImpl::ThrottleableTaskQueue;
  using WorkerSchedulerImpl::UnpausableTaskQueue;
};

class WorkerSchedulerImplTest : public testing::Test {
 public:
  WorkerSchedulerImplTest()
      : mock_task_runner_(new base::TestMockTimeTaskRunner()),
        sequence_manager_(
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr,
                mock_task_runner_,
                mock_task_runner_->GetMockTickClock(),
                base::sequence_manager::SequenceManager::Settings::Builder()
                    .SetPrioritySettings(CreatePrioritySettings())
                    .Build())),
        scheduler_(new WorkerThreadSchedulerForTest(ThreadType::kTestThread,
                                                    sequence_manager_.get(),
                                                    nullptr /* proxy */)) {
    mock_task_runner_->AdvanceMockTickClock(base::Microseconds(5000));
    start_time_ = mock_task_runner_->NowTicks();
  }

  WorkerSchedulerImplTest(const WorkerSchedulerImplTest&) = delete;
  WorkerSchedulerImplTest& operator=(const WorkerSchedulerImplTest&) = delete;
  ~WorkerSchedulerImplTest() override = default;

  void SetUp() override {
    scheduler_->Init();
    scheduler_->AttachToCurrentThread();
    worker_scheduler_ =
        std::make_unique<WorkerSchedulerForTest>(scheduler_.get());
  }

  void TearDown() override {
    if (worker_scheduler_) {
      worker_scheduler_->Dispose();
      worker_scheduler_.reset();
    }
  }

  const base::TickClock* GetClock() {
    return mock_task_runner_->GetMockTickClock();
  }

  void RunUntilIdle() { mock_task_runner_->FastForwardUntilNoTasksRemain(); }

  // Helper for posting a task.
  void PostTestTask(Vector<String>* run_order,
                    const String& task_descriptor,
                    TaskType task_type) {
    PostTestTask(run_order, task_descriptor,
                 *worker_scheduler_->GetTaskRunner(task_type).get());
  }

  void PostTestTask(Vector<String>* run_order,
                    const String& task_descriptor,
                    base::SingleThreadTaskRunner& task_runner) {
    task_runner.PostTask(
        FROM_HERE, WTF::BindOnce(&AppendToVectorTestTask,
                                 WTF::Unretained(run_order), task_descriptor));
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  std::unique_ptr<base::sequence_manager::SequenceManagerForTest>
      sequence_manager_;
  std::unique_ptr<WorkerThreadSchedulerForTest> scheduler_;
  std::unique_ptr<WorkerSchedulerForTest> worker_scheduler_;
  base::TimeTicks start_time_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WorkerSchedulerImplTest, TestPostTasks) {
  Vector<String> run_order;
  PostTestTask(&run_order, "T1", TaskType::kInternalTest);
  PostTestTask(&run_order, "T2", TaskType::kInternalTest);
  RunUntilIdle();
  PostTestTask(&run_order, "T3", TaskType::kInternalTest);
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2", "T3"));

  // GetTaskRunner() is only supposed to be called by the WorkerThread, and only
  // during initialization. Simulate this by using a cached task runner after
  // disposal.
  scoped_refptr<base::SingleThreadTaskRunner> test_task_runner =
      worker_scheduler_->GetTaskRunner(TaskType::kInternalTest);
  // Tasks should not run after the scheduler is disposed of.
  worker_scheduler_->Dispose();
  run_order.clear();
  PostTestTask(&run_order, "T4", *test_task_runner.get());
  PostTestTask(&run_order, "T5", *test_task_runner.get());
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  worker_scheduler_.reset();
}

TEST_F(WorkerSchedulerImplTest, RegisterWorkerSchedulers) {
  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::ElementsAre(worker_scheduler_.get()));

  std::unique_ptr<WorkerSchedulerForTest> worker_scheduler2 =
      std::make_unique<WorkerSchedulerForTest>(scheduler_.get());

  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::UnorderedElementsAre(worker_scheduler_.get(),
                                            worker_scheduler2.get()));

  worker_scheduler_->Dispose();
  worker_scheduler_.reset();

  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::ElementsAre(worker_scheduler2.get()));

  worker_scheduler2->Dispose();

  EXPECT_THAT(scheduler_->worker_schedulers(), testing::ElementsAre());
}

TEST_F(WorkerSchedulerImplTest, ThrottleWorkerScheduler) {
  scheduler_->CreateBudgetPools();

  EXPECT_FALSE(worker_scheduler_->ThrottleableTaskQueue()->IsThrottled());

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);
  EXPECT_TRUE(worker_scheduler_->ThrottleableTaskQueue()->IsThrottled());

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);
  EXPECT_TRUE(worker_scheduler_->ThrottleableTaskQueue()->IsThrottled());

  // Ensure that two calls with kThrottled do not mess with throttling
  // refcount.
  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kNotThrottled);
  EXPECT_FALSE(worker_scheduler_->ThrottleableTaskQueue()->IsThrottled());
}

TEST_F(WorkerSchedulerImplTest, ThrottleWorkerScheduler_CreateThrottled) {
  scheduler_->CreateBudgetPools();

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);

  std::unique_ptr<WorkerSchedulerForTest> worker_scheduler2 =
      std::make_unique<WorkerSchedulerForTest>(scheduler_.get());

  // Ensure that newly created scheduler is throttled.
  EXPECT_TRUE(worker_scheduler2->ThrottleableTaskQueue()->IsThrottled());

  worker_scheduler2->Dispose();
}

TEST_F(WorkerSchedulerImplTest, ThrottleWorkerScheduler_RunThrottledTasks) {
  scheduler_->CreateBudgetPools();
  scheduler_->SetCPUTimeBudgetPoolForTesting(nullptr);

  // Create a new |worker_scheduler| to ensure that it's properly initialised.
  worker_scheduler_->Dispose();
  worker_scheduler_ =
      std::make_unique<WorkerSchedulerForTest>(scheduler_.get());

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);

  Vector<base::TimeTicks> tasks;

  worker_scheduler_->ThrottleableTaskQueue()
      ->GetTaskRunnerWithDefaultTaskType()
      ->PostTask(FROM_HERE,
                 base::BindOnce(&RunChainedTask,
                                worker_scheduler_->ThrottleableTaskQueue(), 5,
                                base::TimeDelta(), mock_task_runner_,
                                base::Unretained(&tasks)));

  RunUntilIdle();

  EXPECT_THAT(tasks, ElementsAre(base::TimeTicks() + base::Seconds(1),
                                 base::TimeTicks() + base::Seconds(2),
                                 base::TimeTicks() + base::Seconds(3),
                                 base::TimeTicks() + base::Seconds(4),
                                 base::TimeTicks() + base::Seconds(5)));
}

TEST_F(WorkerSchedulerImplTest,
       ThrottleWorkerScheduler_RunThrottledTasks_CPUBudget) {
  scheduler_->CreateBudgetPools();

  scheduler_->cpu_time_budget_pool()->SetTimeBudgetRecoveryRate(
      GetClock()->NowTicks(), 0.01);

  // Create a new |worker_scheduler| to ensure that it's properly initialised.
  worker_scheduler_->Dispose();
  worker_scheduler_ =
      std::make_unique<WorkerSchedulerForTest>(scheduler_.get());

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);

  Vector<base::TimeTicks> tasks;

  worker_scheduler_->ThrottleableTaskQueue()
      ->GetTaskRunnerWithDefaultTaskType()
      ->PostTask(FROM_HERE,
                 base::BindOnce(&RunChainedTask,
                                worker_scheduler_->ThrottleableTaskQueue(), 5,
                                base::Milliseconds(100), mock_task_runner_,
                                base::Unretained(&tasks)));

  RunUntilIdle();

  EXPECT_THAT(tasks, ElementsAre(base::TimeTicks() + base::Seconds(1),
                                 start_time_ + base::Seconds(10),
                                 start_time_ + base::Seconds(20),
                                 start_time_ + base::Seconds(30),
                                 start_time_ + base::Seconds(40)));
}

TEST_F(WorkerSchedulerImplTest, PausableTasks) {
  Vector<String> run_order;
  auto pause_handle = worker_scheduler_->Pause();
  // Tests interlacing pausable, throttable and unpausable tasks and
  // ensures that the pausable & throttable tasks don't run when paused.
  // Throttable
  PostTestTask(&run_order, "T1", TaskType::kJavascriptTimerDelayedLowNesting);
  // Pausable
  PostTestTask(&run_order, "T2", TaskType::kNetworking);
  // Unpausable
  PostTestTask(&run_order, "T3", TaskType::kInternalTest);
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T3"));
  pause_handle.reset();
  RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("T3", "T1", "T2"));
}

TEST_F(WorkerSchedulerImplTest, NestedPauseHandlesTasks) {
  Vector<String> run_order;
  auto pause_handle = worker_scheduler_->Pause();
  {
    auto pause_handle2 = worker_scheduler_->Pause();
    PostTestTask(&run_order, "T1", TaskType::kJavascriptTimerDelayedLowNesting);
    PostTestTask(&run_order, "T2", TaskType::kNetworking);
  }
  RunUntilIdle();
  EXPECT_EQ(0u, run_order.size());
  pause_handle.reset();
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2"));
}

class WorkerSchedulerDelegateForTesting : public WorkerScheduler::Delegate {
 public:
  MOCK_METHOD(void, UpdateBackForwardCacheDisablingFeatures, (BlockingDetails));
};

MATCHER(BlockingDetailsHasCCNS, "Compares two blocking details.") {
  bool vector_empty =
      arg.non_sticky_features_and_js_locations->details_list.empty();
  bool vector_has_ccns =
      arg.sticky_features_and_js_locations->details_list.Contains(
          FeatureAndJSLocationBlockingBFCache(
              SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore,
              nullptr)) &&
      arg.sticky_features_and_js_locations->details_list.Contains(
          FeatureAndJSLocationBlockingBFCache(
              SchedulingPolicy::Feature::kMainResourceHasCacheControlNoCache,
              nullptr));
  return vector_empty && vector_has_ccns;
}

// Confirms that the feature usage in a dedicated worker is uploaded to
// somewhere (the browser side in the actual implementation) via a delegate.
TEST_F(WorkerSchedulerImplTest, FeatureUpload) {
  auto delegate = std::make_unique<
      testing::StrictMock<WorkerSchedulerDelegateForTesting>>();
  worker_scheduler_->InitializeOnWorkerThread(delegate.get());

  // As the tracked features are uplodaed after the current task is done by
  // ExecuteAfterCurrentTask, register features in a different task, and wait
  // for the task execution.
  worker_scheduler_->GetTaskRunner(TaskType::kJavascriptTimerImmediate)
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     [](WorkerSchedulerImpl* worker_scheduler,
                        testing::StrictMock<WorkerSchedulerDelegateForTesting>*
                            delegate) {
                       worker_scheduler->RegisterStickyFeature(
                           SchedulingPolicy::Feature::
                               kMainResourceHasCacheControlNoStore,
                           {SchedulingPolicy::DisableBackForwardCache()});
                       worker_scheduler->RegisterStickyFeature(
                           SchedulingPolicy::Feature::
                               kMainResourceHasCacheControlNoCache,
                           {SchedulingPolicy::DisableBackForwardCache()});
                       testing::Mock::VerifyAndClearExpectations(delegate);
                       EXPECT_CALL(*delegate,
                                   UpdateBackForwardCacheDisablingFeatures(
                                       BlockingDetailsHasCCNS()));
                     },
                     worker_scheduler_.get(), delegate.get()));

  RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(delegate.get());
}

class NonMainThreadWebSchedulingTaskQueueTest
    : public WorkerSchedulerImplTest,
      public WebSchedulingTestHelper::Delegate {
 public:
  void SetUp() override {
    WorkerSchedulerImplTest::SetUp();
    web_scheduling_test_helper_ =
        std::make_unique<WebSchedulingTestHelper>(*this);
  }

  void TearDown() override {
    WorkerSchedulerImplTest::TearDown();
    web_scheduling_test_helper_.reset();
  }

  FrameOrWorkerScheduler& GetFrameOrWorkerScheduler() override {
    return *worker_scheduler_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType task_type) override {
    return worker_scheduler_->GetTaskRunner(task_type);
  }

 protected:
  using TestTaskSpecEntry = WebSchedulingTestHelper::TestTaskSpecEntry;
  using WebSchedulingParams = WebSchedulingTestHelper::WebSchedulingParams;

  std::unique_ptr<WebSchedulingTestHelper> web_scheduling_test_helper_;
};

TEST_F(NonMainThreadWebSchedulingTaskQueueTest, TasksRunInPriorityOrder) {
  Vector<String> run_order;

  Vector<TestTaskSpecEntry> test_spec = {
      {.descriptor = "BG1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "BG2",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "UV1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UV2",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UB1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})},
      {.descriptor = "UB2",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})}};
  web_scheduling_test_helper_->PostTestTasks(&run_order, test_spec);

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("UB1", "UB2", "UV1", "UV2", "BG1", "BG2"));
}

TEST_F(NonMainThreadWebSchedulingTaskQueueTest, DynamicTaskPriorityOrder) {
  Vector<String> run_order;
  Vector<TestTaskSpecEntry> test_spec = {
      {.descriptor = "BG1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "BG2",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "UV1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UV2",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UB1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})},
      {.descriptor = "UB2",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})}};
  web_scheduling_test_helper_->PostTestTasks(&run_order, test_spec);

  web_scheduling_test_helper_
      ->GetWebSchedulingTaskQueue(WebSchedulingQueueType::kTaskQueue,
                                  WebSchedulingPriority::kUserBlockingPriority)
      ->SetPriority(WebSchedulingPriority::kBackgroundPriority);

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("UV1", "UV2", "BG1", "BG2", "UB1", "UB2"));
}

TEST_F(NonMainThreadWebSchedulingTaskQueueTest, TasksAndContinuations) {
  Vector<String> run_order;
  Vector<TestTaskSpecEntry> test_spec = {
      {.descriptor = "BG",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "BG-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "UV",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UV-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UB",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})},
      {.descriptor = "UB-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})}};
  web_scheduling_test_helper_->PostTestTasks(&run_order, test_spec);

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("UB-C", "UB", "UV-C", "UV", "BG-C", "BG"));
}

TEST_F(NonMainThreadWebSchedulingTaskQueueTest, DynamicPriorityContinuations) {
  Vector<String> run_order;
  Vector<TestTaskSpecEntry> test_spec = {
      {.descriptor = "BG-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "UV-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UB-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})}};
  web_scheduling_test_helper_->PostTestTasks(&run_order, test_spec);

  web_scheduling_test_helper_
      ->GetWebSchedulingTaskQueue(WebSchedulingQueueType::kContinuationQueue,
                                  WebSchedulingPriority::kUserBlockingPriority)
      ->SetPriority(WebSchedulingPriority::kBackgroundPriority);

  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("UV-C", "BG-C", "UB-C"));
}

TEST_F(NonMainThreadWebSchedulingTaskQueueTest,
       WebScheduingAndNonWebScheduingTasks) {
  Vector<String> run_order;
  Vector<TestTaskSpecEntry> test_spec = {
      {.descriptor = "PostMessage", .type_info = TaskType::kPostedMessage},
      {.descriptor = "BG",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "BG-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "UV",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UV-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UB",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})},
      {.descriptor = "UB-C",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kContinuationQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})},
      {.descriptor = "Timer",
       .type_info = TaskType::kJavascriptTimerImmediate}};
  web_scheduling_test_helper_->PostTestTasks(&run_order, test_spec);

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("UB-C", "UB", "UV-C", "PostMessage", "UV",
                                   "Timer", "BG-C", "BG"));
}

enum class DeleterTaskRunnerEnabled { kEnabled, kDisabled };

TEST_F(WorkerSchedulerImplTest, DeleteSoonAfterDispose) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      worker_scheduler_->GetTaskRunner(TaskType::kInternalTest);
  int counter = 0;

  // Deleting before shutdown should always work.
  std::unique_ptr<TestObject> test_object1 =
      std::make_unique<TestObject>(&counter);
  task_runner->DeleteSoon(FROM_HERE, std::move(test_object1));
  EXPECT_EQ(counter, 0);
  RunUntilIdle();
  EXPECT_EQ(counter, 1);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  worker_scheduler_->Dispose();
  worker_scheduler_.reset();

  // No more tasks should run after worker scheduler disposal.
  EXPECT_EQ(counter, 1);
  RunUntilIdle();
  EXPECT_EQ(counter, 1);

  std::unique_ptr<TestObject> test_object2 =
      std::make_unique<TestObject>(&counter);
  task_runner->DeleteSoon(FROM_HERE, std::move(test_object2));
  EXPECT_EQ(counter, 1);
  RunUntilIdle();
  EXPECT_EQ(counter, 2);
}

}  // namespace worker_scheduler_unittest
}  // namespace scheduler
}  // namespace blink

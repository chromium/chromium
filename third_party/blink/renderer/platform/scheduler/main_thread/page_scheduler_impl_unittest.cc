// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using base::sequence_manager::FakeTask;
using base::sequence_manager::FakeTaskTiming;
using base::sequence_manager::TaskQueue;
using testing::ElementsAre;
using VirtualTimePolicy = blink::VirtualTimeController::VirtualTimePolicy;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace page_scheduler_impl_unittest {

namespace {

constexpr base::TimeDelta kEpsilon = base::Milliseconds(1);

void IncrementCounter(int* counter) {
  ++*counter;
}

// This is a wrapper around MainThreadSchedulerImpl::CreatePageScheduler, that
// returns the PageScheduler as a PageSchedulerImpl.
std::unique_ptr<PageSchedulerImpl> CreatePageScheduler(
    PageScheduler::Delegate* page_scheduler_delegate,
    MainThreadSchedulerImpl* scheduler,
    AgentGroupScheduler& agent_group_scheduler) {
  std::unique_ptr<PageScheduler> page_scheduler =
      agent_group_scheduler.CreatePageScheduler(page_scheduler_delegate);
  std::unique_ptr<PageSchedulerImpl> page_scheduler_impl(
      static_cast<PageSchedulerImpl*>(page_scheduler.release()));
  return page_scheduler_impl;
}

// This is a wrapper around PageSchedulerImpl::CreateFrameScheduler, that
// returns the FrameScheduler as a FrameSchedulerImpl.
std::unique_ptr<FrameSchedulerImpl> CreateFrameScheduler(
    PageSchedulerImpl* page_scheduler,
    FrameScheduler::Delegate* delegate,
    bool is_in_embedded_frame_tree,
    FrameScheduler::FrameType frame_type) {
  auto frame_scheduler = page_scheduler->CreateFrameScheduler(
      delegate, is_in_embedded_frame_tree, frame_type);
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_impl(
      static_cast<FrameSchedulerImpl*>(frame_scheduler.release()));
  return frame_scheduler_impl;
}
}  // namespace

using base::Bucket;
using testing::UnorderedElementsAreArray;

class MockPageSchedulerDelegate : public PageScheduler::Delegate {
 public:
  MockPageSchedulerDelegate() {}

 private:
  bool RequestBeginMainFrameNotExpected(bool) override { return false; }
  void OnSetPageFrozen(bool is_frozen) override {}
  bool IsOrdinary() const override { return true; }
};

class PageSchedulerImplTest : public testing::Test {
 public:
  PageSchedulerImplTest() {
    feature_list_.InitAndEnableFeature(blink::features::kStopInBackground);
  }

  PageSchedulerImplTest(std::vector<base::test::FeatureRef> enabled_features,
                        std::vector<base::test::FeatureRef> disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~PageSchedulerImplTest() override = default;

 protected:
  void SetUp() override {
    test_task_runner_ = base::WrapRefCounted(new base::TestMockTimeTaskRunner(
        base::TestMockTimeTaskRunner::Type::kBoundToThread));
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(base::Milliseconds(5));
    scheduler_ = std::make_unique<MainThreadSchedulerImpl>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, test_task_runner_->GetMockTickClock(),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetPrioritySettings(CreatePrioritySettings())
                .Build()));
    agent_group_scheduler_ = scheduler_->CreateAgentGroupScheduler();
    page_scheduler_delegate_ = std::make_unique<MockPageSchedulerDelegate>();
    page_scheduler_ =
        CreatePageScheduler(page_scheduler_delegate_.get(), scheduler_.get(),
                            *agent_group_scheduler_);
    frame_scheduler_ =
        CreateFrameScheduler(page_scheduler_.get(), nullptr,
                             /*is_in_embedded_frame_tree=*/false,
                             FrameScheduler::FrameType::kSubframe);
  }

  void TearDown() override {
    frame_scheduler_.reset();
    page_scheduler_.reset();
    agent_group_scheduler_ = nullptr;
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  void FastForwardTo(base::TimeTicks time) {
    base::TimeTicks now = test_task_runner_->GetMockTickClock()->NowTicks();
    CHECK_LE(now, time);
    test_task_runner_->FastForwardBy(time - now);
  }

  static scoped_refptr<MainThreadTaskQueue> ThrottleableTaskQueueForScheduler(
      FrameSchedulerImpl* scheduler) {
    auto* frame_task_queue_controller =
        scheduler->FrameTaskQueueControllerForTest();
    auto queue_traits = FrameSchedulerImpl::ThrottleableTaskQueueTraits();
    return frame_task_queue_controller->GetTaskQueue(queue_traits);
  }

  base::TimeDelta delay_for_background_tab_freezing() const {
    return page_scheduler_->delay_for_background_tab_freezing_;
  }

  PageScheduler::Delegate* delegate() { return page_scheduler_->delegate_; }

  static base::TimeDelta recent_audio_delay() {
    return PageSchedulerImpl::kRecentAudioDelay;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ThrottleableTaskRunner() {
    return ThrottleableTaskQueue()->CreateTaskRunner(TaskType::kInternalTest);
  }

  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() {
    return LoadingTaskQueue()->CreateTaskRunner(TaskType::kInternalTest);
  }

  scoped_refptr<MainThreadTaskQueue> GetTaskQueue(
      MainThreadTaskQueue::QueueTraits queue_traits) {
    return frame_scheduler_->FrameTaskQueueControllerForTest()
        ->GetTaskQueue(queue_traits);
  }

  scoped_refptr<MainThreadTaskQueue> ThrottleableTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::ThrottleableTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> LoadingTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::LoadingTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> DeferrableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::DeferrableTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> PausableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::PausableTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> UnpausableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::UnpausableTaskQueueTraits());
  }

  // Verifies that freezing the PageScheduler prevents tasks from running. Then
  // set the page as visible or unfreezes it while still hidden (depending on
  // the argument), and verifies that tasks can run.
  void TestFreeze(bool make_page_visible) {
    int counter = 0;
    LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    DeferrableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    PausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

    page_scheduler_->SetPageVisible(false);
    EXPECT_EQ(false, page_scheduler_->IsFrozen());

    // In a backgrounded active page, all queues should run.
    test_task_runner_->FastForwardUntilNoTasksRemain();
    EXPECT_EQ(5, counter);

    LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    DeferrableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    PausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    counter = 0;

    page_scheduler_->SetPageFrozen(true);
    EXPECT_EQ(true, page_scheduler_->IsFrozen());

    // In a backgrounded frozen page, only Unpausable queue should run.
    test_task_runner_->FastForwardUntilNoTasksRemain();
    EXPECT_EQ(1, counter);

    // Make the page visible or unfreeze it while hidden.
    if (make_page_visible)
      page_scheduler_->SetPageVisible(true);
    else
      page_scheduler_->SetPageFrozen(false);
    EXPECT_EQ(false, page_scheduler_->IsFrozen());

    // Once the page is unfrozen, the rest of the queues should run.
    test_task_runner_->FastForwardUntilNoTasksRemain();
    EXPECT_EQ(5, counter);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  Persistent<AgentGroupScheduler> agent_group_scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
  std::unique_ptr<MockPageSchedulerDelegate> page_scheduler_delegate_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PageSchedulerImplTest, TestDestructionOfFrameSchedulersBefore) {
  std::unique_ptr<blink::FrameScheduler> frame1(
      page_scheduler_->CreateFrameScheduler(
          nullptr, /*is_in_embedded_frame_tree=*/false,
          FrameScheduler::FrameType::kSubframe));
  std::unique_ptr<blink::FrameScheduler> frame2(
      page_scheduler_->CreateFrameScheduler(
          nullptr, /*is_in_embedded_frame_tree=*/false,
          FrameScheduler::FrameType::kSubframe));
}

TEST_F(PageSchedulerImplTest, TestDestructionOfFrameSchedulersAfter) {
  std::unique_ptr<blink::FrameScheduler> frame1(
      page_scheduler_->CreateFrameScheduler(
          nullptr, /*is_in_embedded_frame_tree=*/false,
          FrameScheduler::FrameType::kSubframe));
  std::unique_ptr<blink::FrameScheduler> frame2(
      page_scheduler_->CreateFrameScheduler(
          nullptr, /*is_in_embedded_frame_tree=*/false,
          FrameScheduler::FrameType::kSubframe));
  page_scheduler_.reset();
}

namespace {

void RunRepeatingTask(scoped_refptr<base::SingleThreadTaskRunner>,
                      int* run_count,
                      base::TimeDelta delay);

base::OnceClosure MakeRepeatingTask(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int* run_count,
    base::TimeDelta delay) {
  return base::BindOnce(&RunRepeatingTask, std::move(task_runner),
                        base::Unretained(run_count), delay);
}

void RunRepeatingTask(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      int* run_count,
                      base::TimeDelta delay) {
  // Limit the number of repetitions.
  // Test cases can make expectations against this number.
  if (++*run_count == 2000)
    return;
  task_runner->PostDelayedTask(
      FROM_HERE, MakeRepeatingTask(task_runner, run_count, delay), delay);
}

}  // namespace

TEST_F(PageSchedulerImplTest, RepeatingTimer_PageInForeground) {
  page_scheduler_->SetPageVisible(true);

  int run_count = 0;
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          &run_count, base::Milliseconds(1)),
      base::Milliseconds(1));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(PageSchedulerImplTest, RepeatingTimer_PageInBackgroundThenForeground) {
  page_scheduler_->SetPageVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          &run_count, base::Milliseconds(20)),
      base::Milliseconds(20));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, run_count);

  // Make sure there's no delay in throttling being removed for pages that have
  // become visible.
  page_scheduler_->SetPageVisible(true);

  run_count = 0;
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(50, run_count);
}

TEST_F(PageSchedulerImplTest, RepeatingLoadingTask_PageInBackground) {
  page_scheduler_->SetPageVisible(false);

  int run_count = 0;
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
                        &run_count, base::Milliseconds(1)),
      base::Milliseconds(1));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1000, run_count);  // Loading tasks should not be throttled
}

TEST_F(PageSchedulerImplTest, RepeatingTimers_OneBackgroundOneForeground) {
  std::unique_ptr<PageSchedulerImpl> page_scheduler2 =
      CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler2 =
      CreateFrameScheduler(page_scheduler2.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);

  page_scheduler_->SetPageVisible(true);
  page_scheduler2->SetPageVisible(false);

  int run_count1 = 0;
  int run_count2 = 0;
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          &run_count1, base::Milliseconds(20)),
      base::Milliseconds(20));
  ThrottleableTaskQueueForScheduler(frame_scheduler2.get())
      ->GetTaskRunnerWithDefaultTaskType()
      ->PostDelayedTask(
          FROM_HERE,
          MakeRepeatingTask(
              ThrottleableTaskQueueForScheduler(frame_scheduler2.get())
                  ->GetTaskRunnerWithDefaultTaskType(),
              &run_count2, base::Milliseconds(20)),
          base::Milliseconds(20));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(50, run_count1);
  EXPECT_EQ(1, run_count2);
}

TEST_F(PageSchedulerImplTest, IsLoadingTest) {
  // 1st Page is loaded.
  EXPECT_FALSE(page_scheduler_->IsLoading());

  std::unique_ptr<PageSchedulerImpl> page_scheduler2 =
      CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler2 =
      CreateFrameScheduler(page_scheduler2.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kMainFrame);

  // 1st Page is loaded. 2nd page is loading.
  EXPECT_FALSE(page_scheduler_->IsLoading());
  EXPECT_TRUE(page_scheduler2->IsLoading());

  // 2nd page finishes loading.
  frame_scheduler2->OnFirstContentfulPaintInMainFrame();
  frame_scheduler2->OnFirstMeaningfulPaint(
      base::TimeTicks::Now() -
      GetLoadingPhaseBufferTimeAfterFirstMeaningfulPaint());

  // Both pages are loaded.
  EXPECT_FALSE(page_scheduler_->IsLoading());
  EXPECT_FALSE(page_scheduler2->IsLoading());
}

namespace {

void RunVirtualTimeRecorderTask(const base::TickClock* clock,
                                MainThreadSchedulerImpl* scheduler,
                                Vector<base::TimeTicks>* out_real_times,
                                Vector<base::TimeTicks>* out_virtual_times) {
  out_real_times->push_back(clock->NowTicks());
  out_virtual_times->push_back(scheduler->NowTicks());
}

base::OnceClosure MakeVirtualTimeRecorderTask(
    const base::TickClock* clock,
    MainThreadSchedulerImpl* scheduler,
    Vector<base::TimeTicks>* out_real_times,
    Vector<base::TimeTicks>* out_virtual_times) {
  return base::BindOnce(&RunVirtualTimeRecorderTask, base::Unretained(clock),
                        base::Unretained(scheduler),
                        base::Unretained(out_real_times),
                        base::Unretained(out_virtual_times));
}
}  // namespace

TEST_F(PageSchedulerImplTest, VirtualTime_TimerFastForwarding) {
  Vector<base::TimeTicks> real_times;
  Vector<base::TimeTicks> virtual_times;

  page_scheduler_->GetVirtualTimeController()->EnableVirtualTime(base::Time());

  base::TimeTicks initial_real_time = scheduler_->NowTicks();
  base::TimeTicks initial_virtual_time = scheduler_->NowTicks();

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(2));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(20));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(virtual_times,
              ElementsAre(initial_virtual_time + base::Milliseconds(2),
                          initial_virtual_time + base::Milliseconds(20),
                          initial_virtual_time + base::Milliseconds(200)));
}

TEST_F(PageSchedulerImplTest, VirtualTime_LoadingTaskFastForwarding) {
  Vector<base::TimeTicks> real_times;
  Vector<base::TimeTicks> virtual_times;

  page_scheduler_->GetVirtualTimeController()->EnableVirtualTime(base::Time());

  base::TimeTicks initial_real_time = scheduler_->NowTicks();
  base::TimeTicks initial_virtual_time = scheduler_->NowTicks();

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(2));

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(20));

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(virtual_times,
              ElementsAre(initial_virtual_time + base::Milliseconds(2),
                          initial_virtual_time + base::Milliseconds(20),
                          initial_virtual_time + base::Milliseconds(200)));
}

TEST_F(PageSchedulerImplTest,
       RepeatingTimer_PageInBackground_MeansNothingForVirtualTime) {
  page_scheduler_->GetVirtualTimeController()->EnableVirtualTime(base::Time());
  page_scheduler_->SetPageVisible(false);
  scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(1);
  base::TimeTicks initial_real_time = scheduler_->NowTicks();

  int run_count = 0;
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          &run_count, base::Milliseconds(1)),
      base::Milliseconds(1));

  test_task_runner_->RunUntilIdle();
  // Virtual time means page visibility is ignored.
  // 2000 is the |run_count| limit, we expect to reach it.
  EXPECT_EQ(2000, run_count);

  // The global tick clock has not moved, yet we ran a large number of "delayed"
  // tasks despite calling setPageVisible(false).
  EXPECT_EQ(initial_real_time, test_task_runner_->NowTicks());
}

// Check that enabling virtual time while the page is backgrounded prevents a
// page from being frozen if it wasn't already.
TEST_F(PageSchedulerImplTest, PageBackgrounded_EnableVirtualTime) {
  page_scheduler_->SetPageVisible(false);
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  page_scheduler_->GetVirtualTimeController()->EnableVirtualTime(base::Time());
  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Page should not be frozen after a delay since virtual time
  // was enabled.
  EXPECT_FALSE(page_scheduler_->IsFrozen());
}

// Check that enabling virtual time while a backgrounded page is frozen
// unfreezes it.
TEST_F(PageSchedulerImplTest, PageFrozen_EnableVirtualTime) {
  page_scheduler_->SetPageVisible(false);
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(page_scheduler_->IsFrozen());

  page_scheduler_->GetVirtualTimeController()->EnableVirtualTime(base::Time());

  // Page should not be frozen since virtual time was enabled.
  EXPECT_FALSE(page_scheduler_->IsFrozen());
}

namespace {

void RunOrderTask(int index, Vector<int>* out_run_order) {
  out_run_order->push_back(index);
}

void DelayedRunOrderTask(
    int index,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Vector<int>* out_run_order) {
  out_run_order->push_back(index);
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&RunOrderTask, index + 1,
                                       base::Unretained(out_run_order)));
}
}  // namespace

TEST_F(PageSchedulerImplTest, VirtualTime_NotAllowedToAdvance) {
  Vector<int> run_order;

  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kPause);

  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunOrderTask, 0, base::Unretained(&run_order)));

  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &DelayedRunOrderTask, 1,
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          base::Unretained(&run_order)),
      base::Milliseconds(2));

  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &DelayedRunOrderTask, 3,
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          base::Unretained(&run_order)),
      base::Milliseconds(4));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // No timer tasks are allowed to run.
  EXPECT_THAT(run_order, ElementsAre());
}

TEST_F(PageSchedulerImplTest, VirtualTime_AllowedToAdvance) {
  Vector<int> run_order;

  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunOrderTask, 0, base::Unretained(&run_order)));

  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &DelayedRunOrderTask, 1,
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          base::Unretained(&run_order)),
      base::Milliseconds(2));

  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &DelayedRunOrderTask, 3,
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          base::Unretained(&run_order)),
      base::Milliseconds(4));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_order, ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(PageSchedulerImplTest, RepeatingTimer_PageInBackground) {
  ScopedTimerThrottlingForBackgroundTabsForTest timer_throttling_enabler(false);
  page_scheduler_->SetPageVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
          &run_count, base::Milliseconds(1)),
      base::Milliseconds(1));

  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(PageSchedulerImplTest, VirtualTimeSettings_NewFrameScheduler) {
  Vector<int> run_order;

  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kPause);

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);

  ThrottleableTaskQueueForScheduler(frame_scheduler.get())
      ->GetTaskRunnerWithDefaultTaskType()
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&RunOrderTask, 1, base::Unretained(&run_order)),
          base::Milliseconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(run_order.empty());

  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_order, ElementsAre(1));
}

namespace {

template <typename T>
base::OnceClosure MakeDeletionTask(T* obj) {
  return base::BindOnce([](T* obj) { delete obj; }, base::Unretained(obj));
}

}  // namespace

TEST_F(PageSchedulerImplTest, DeleteFrameSchedulers_InTask) {
  for (int i = 0; i < 10; i++) {
    FrameSchedulerImpl* frame_scheduler =
        CreateFrameScheduler(page_scheduler_.get(), nullptr,
                             /*is_in_embedded_frame_tree=*/false,
                             FrameScheduler::FrameType::kSubframe)
            .release();
    ThrottleableTaskQueueForScheduler(frame_scheduler)
        ->GetTaskRunnerWithDefaultTaskType()
        ->PostDelayedTask(FROM_HERE, MakeDeletionTask(frame_scheduler),
                          base::Milliseconds(1));
  }
  test_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(PageSchedulerImplTest, DeletePageScheduler_InTask) {
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, MakeDeletionTask(page_scheduler_.release()));
  test_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(PageSchedulerImplTest, DeleteThrottledQueue_InTask) {
  page_scheduler_->SetPageVisible(false);

  FrameSchedulerImpl* frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe)
          .release();
  scoped_refptr<MainThreadTaskQueue> timer_task_queue =
      ThrottleableTaskQueueForScheduler(frame_scheduler);

  int run_count = 0;
  timer_task_queue->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(timer_task_queue->GetTaskRunnerWithDefaultTaskType(),
                        &run_count, base::Milliseconds(100)),
      base::Milliseconds(100));

  // Note this will run at time t = 10s since we start at time t = 5000us.
  // However, we still should run all tasks after frame scheduler deletion.
  timer_task_queue->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE, MakeDeletionTask(frame_scheduler), base::Milliseconds(9990));

  test_task_runner_->FastForwardBy(base::Seconds(20));
  EXPECT_EQ(110, run_count);
}

TEST_F(PageSchedulerImplTest, VirtualTimePauseCount_DETERMINISTIC_LOADING) {
  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kDeterministicLoading);
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->IncrementVirtualTimePauseCount();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->IncrementVirtualTimePauseCount();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->DecrementVirtualTimePauseCount();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->DecrementVirtualTimePauseCount();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->IncrementVirtualTimePauseCount();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->DecrementVirtualTimePauseCount();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(PageSchedulerImplTest,
       WebScopedVirtualTimePauser_DETERMINISTIC_LOADING) {
  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kDeterministicLoading);

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);

  {
    WebScopedVirtualTimePauser virtual_time_pauser =
        frame_scheduler->CreateWebScopedVirtualTimePauser(
            "test",
            WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
    EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

    virtual_time_pauser.PauseVirtualTime();
    EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

    virtual_time_pauser.UnpauseVirtualTime();
    EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

    virtual_time_pauser.PauseVirtualTime();
    EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());
  }

  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
}

namespace {

void RecordVirtualTime(MainThreadSchedulerImpl* scheduler,
                       base::TimeTicks* out) {
  *out = scheduler->NowTicks();
}

void PauseAndUnpauseVirtualTime(MainThreadSchedulerImpl* scheduler,
                                FrameSchedulerImpl* frame_scheduler,
                                base::TimeTicks* paused,
                                base::TimeTicks* unpaused) {
  *paused = scheduler->NowTicks();

  {
    WebScopedVirtualTimePauser virtual_time_pauser =
        frame_scheduler->CreateWebScopedVirtualTimePauser(
            "test",
            WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
    virtual_time_pauser.PauseVirtualTime();
  }

  *unpaused = scheduler->NowTicks();
}

}  // namespace

TEST_F(PageSchedulerImplTest,
       WebScopedVirtualTimePauserWithInterleavedTasks_DETERMINISTIC_LOADING) {
  // Make task queue manager ask the virtual time domain for the next task delay
  // after each task.
  scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(1);

  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kDeterministicLoading);

  base::TimeTicks initial_virtual_time = scheduler_->NowTicks();

  base::TimeTicks time_paused;
  base::TimeTicks time_unpaused;
  base::TimeTicks time_second_task;

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);

  // Pauses and unpauses virtual time, thereby advancing virtual time by an
  // additional 10ms due to WebScopedVirtualTimePauser's delay.
  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &PauseAndUnpauseVirtualTime, base::Unretained(scheduler_.get()),
          base::Unretained(frame_scheduler.get()),
          base::Unretained(&time_paused), base::Unretained(&time_unpaused)),
      base::Milliseconds(3));

  // Will run after the first task has advanced virtual time past 5ms.
  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RecordVirtualTime, base::Unretained(scheduler_.get()),
                     base::Unretained(&time_second_task)),
      base::Milliseconds(5));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(time_paused, initial_virtual_time + base::Milliseconds(3));
  EXPECT_EQ(time_unpaused, initial_virtual_time + base::Milliseconds(13));
  EXPECT_EQ(time_second_task, initial_virtual_time + base::Milliseconds(13));
}

TEST_F(PageSchedulerImplTest,
       MultipleWebScopedVirtualTimePausers_DETERMINISTIC_LOADING) {
  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kDeterministicLoading);

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);

  WebScopedVirtualTimePauser virtual_time_pauser1 =
      frame_scheduler->CreateWebScopedVirtualTimePauser(
          "test", WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
  WebScopedVirtualTimePauser virtual_time_pauser2 =
      frame_scheduler->CreateWebScopedVirtualTimePauser(
          "test", WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);

  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  virtual_time_pauser1.PauseVirtualTime();
  virtual_time_pauser2.PauseVirtualTime();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  virtual_time_pauser2.UnpauseVirtualTime();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  virtual_time_pauser1.UnpauseVirtualTime();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
}

TEST_F(PageSchedulerImplTest, NestedMessageLoop_DETERMINISTIC_LOADING) {
  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kDeterministicLoading);
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  FakeTask fake_task;
  fake_task.set_enqueue_order(
      base::sequence_manager::EnqueueOrder::FromIntForTesting(42));
  const base::TimeTicks start = scheduler_->NowTicks();
  scheduler_->OnTaskStarted(nullptr, fake_task,
                            FakeTaskTiming(start, base::TimeTicks()));
  scheduler_->GetSchedulerHelperForTesting()->OnBeginNestedRunLoop();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->GetSchedulerHelperForTesting()->OnExitNestedRunLoop();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
  FakeTaskTiming task_timing(start, scheduler_->NowTicks());
  scheduler_->OnTaskCompleted(nullptr, fake_task, &task_timing, nullptr);
}

TEST_F(PageSchedulerImplTest, PauseTimersWhileVirtualTimeIsPaused) {
  Vector<int> run_order;

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);
  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kPause);

  ThrottleableTaskQueueForScheduler(frame_scheduler.get())
      ->GetTaskRunnerWithDefaultTaskType()
      ->PostTask(FROM_HERE, base::BindOnce(&RunOrderTask, 1,
                                           base::Unretained(&run_order)));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(run_order.empty());

  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(PageSchedulerImplTest, VirtualTimeBudgetExhaustedCallback) {
  Vector<base::TimeTicks> real_times;
  Vector<base::TimeTicks> virtual_times;

  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());

  base::TimeTicks initial_real_time = scheduler_->NowTicks();
  base::TimeTicks initial_virtual_time = scheduler_->NowTicks();

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(1));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(2));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(5));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::Milliseconds(7));

  vtc->GrantVirtualTimeBudget(
      base::Milliseconds(5),
      base::BindOnce(&VirtualTimeController::SetVirtualTimePolicy,
                     base::Unretained(vtc), VirtualTimePolicy::kPause));
  test_task_runner_->FastForwardUntilNoTasksRemain();

  // The timer that is scheduled for the exact point in time when virtual time
  // expires will not run.
  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(virtual_times,
              ElementsAre(initial_virtual_time + base::Milliseconds(1),
                          initial_virtual_time + base::Milliseconds(2),
                          initial_virtual_time + base::Milliseconds(5)));
}

namespace {
void RepostingTask(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   int max_count,
                   int* count) {
  if (++(*count) >= max_count)
    return;

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&RepostingTask, task_runner, max_count,
                                       base::Unretained(count)));
}

void DelayedTask(int* count_in, int* count_out) {
  *count_out = *count_in;
}

}  // namespace

TEST_F(PageSchedulerImplTest, MaxVirtualTimeTaskStarvationCountOneHundred) {
  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();

  vtc->EnableVirtualTime(base::Time());
  vtc->SetMaxVirtualTimeTaskStarvationCount(100);
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
                1000, &count);
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, base::Unretained(&count),
                     base::Unretained(&delayed_task_run_at_count)),
      base::Milliseconds(10));

  vtc->GrantVirtualTimeBudget(
      base::Milliseconds(1000),
      base::BindOnce(&VirtualTimeController::SetVirtualTimePolicy,
                     base::Unretained(vtc), VirtualTimePolicy::kPause));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Two delayed tasks with a run of 100 tasks, plus initial call.
  EXPECT_EQ(201, count);
  EXPECT_EQ(102, delayed_task_run_at_count);
}

TEST_F(PageSchedulerImplTest,
       MaxVirtualTimeTaskStarvationCountOneHundredNestedMessageLoop) {
  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetMaxVirtualTimeTaskStarvationCount(100);
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  FakeTask fake_task;
  fake_task.set_enqueue_order(
      base::sequence_manager::EnqueueOrder::FromIntForTesting(42));
  const base::TimeTicks start = scheduler_->NowTicks();
  scheduler_->OnTaskStarted(nullptr, fake_task,
                            FakeTaskTiming(start, base::TimeTicks()));
  scheduler_->GetSchedulerHelperForTesting()->OnBeginNestedRunLoop();

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
                1000, &count);
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, base::Unretained(&count),
                     base::Unretained(&delayed_task_run_at_count)),
      base::Milliseconds(10));

  vtc->GrantVirtualTimeBudget(
      base::Milliseconds(1000),
      base::BindOnce(&VirtualTimeController::SetVirtualTimePolicy,
                     base::Unretained(vtc), VirtualTimePolicy::kPause));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(1000, count);
  EXPECT_EQ(1000, delayed_task_run_at_count);
}

TEST_F(PageSchedulerImplTest, MaxVirtualTimeTaskStarvationCountZero) {
  VirtualTimeController* vtc = page_scheduler_->GetVirtualTimeController();
  vtc->EnableVirtualTime(base::Time());
  vtc->SetMaxVirtualTimeTaskStarvationCount(0);
  vtc->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType(),
                1000, &count);
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, base::Unretained(&count),
                     base::Unretained(&delayed_task_run_at_count)),
      base::Milliseconds(10));

  vtc->GrantVirtualTimeBudget(
      base::Milliseconds(1000),
      base::BindOnce(&VirtualTimeController::SetVirtualTimePolicy,
                     base::Unretained(vtc), VirtualTimePolicy::kPause));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(1000, count);
  // If the initial count had been higher, the delayed task could have been
  // arbitrarily delayed.
  EXPECT_EQ(1000, delayed_task_run_at_count);
}

namespace {

void ExpensiveTestTask(scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
                       Vector<base::TimeTicks>* run_times) {
  run_times->push_back(task_runner->GetMockTickClock()->NowTicks());
  task_runner->AdvanceMockTickClock(base::Milliseconds(250));
}

void InitializeTrialParams() {
  base::FieldTrialParams params = {{"cpu_budget", "0.01"},
                                   {"max_budget", "0.0"},
                                   {"initial_budget", "0.0"},
                                   {"max_delay", "0.0"}};
  const char kParamName[] = "ExpensiveBackgroundTimerThrottling";
  const char kGroupName[] = "Enabled";
  EXPECT_TRUE(base::AssociateFieldTrialParams(kParamName, kGroupName, params));
  EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(kParamName, kGroupName));

  base::FieldTrialParams actual_params;
  base::GetFieldTrialParams(kParamName, &actual_params);
  EXPECT_EQ(actual_params, params);
}

}  // namespace

TEST_F(PageSchedulerImplTest, BackgroundTimerThrottling) {
  InitializeTrialParams();
  page_scheduler_ =
      CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
  EXPECT_FALSE(page_scheduler_->IsCPUTimeThrottled());
  base::TimeTicks start_time = test_task_runner_->NowTicks();

  Vector<base::TimeTicks> run_times;
  frame_scheduler_ = CreateFrameScheduler(page_scheduler_.get(), nullptr,
                                          /*is_in_embedded_frame_tree=*/false,
                                          FrameScheduler::FrameType::kSubframe);
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(page_scheduler_->IsCPUTimeThrottled());

  FastForwardTo(base::TimeTicks() + base::Milliseconds(2500));

  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
      base::Milliseconds(1));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
      base::Milliseconds(1));

  FastForwardTo(base::TimeTicks() + base::Milliseconds(3500));

  // Check that these tasks are aligned, but are not subject to budget-based
  // throttling.
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::Milliseconds(2501),
                          base::TimeTicks() + base::Milliseconds(2751)));
  run_times.clear();

  page_scheduler_->SetPageVisible(false);
  EXPECT_FALSE(page_scheduler_->IsCPUTimeThrottled());

  // Ensure that the page is fully throttled.
  FastForwardTo(base::TimeTicks() + base::Seconds(15));
  EXPECT_TRUE(page_scheduler_->IsCPUTimeThrottled());

  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
      base::Microseconds(1));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
      base::Microseconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Check that tasks are aligned and throttled.
  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() + base::Seconds(16),
                                     start_time + base::Seconds(25)));

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
}

TEST_F(PageSchedulerImplTest, OpenWebSocketExemptsFromBudgetThrottling) {
  InitializeTrialParams();
  std::unique_ptr<PageSchedulerImpl> page_scheduler =
      CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
  base::TimeTicks start_time = test_task_runner_->NowTicks();

  Vector<base::TimeTicks> run_times;

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler1 =
      CreateFrameScheduler(page_scheduler.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler2 =
      CreateFrameScheduler(page_scheduler.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);

  page_scheduler->SetPageVisible(false);

  // Wait for 20s to avoid initial throttling delay.
  FastForwardTo(base::TimeTicks() + base::Milliseconds(20500));

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(frame_scheduler1.get())
        ->GetTaskRunnerWithDefaultTaskType()
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
            base::Milliseconds(1));
  }

  FastForwardTo(base::TimeTicks() + base::Milliseconds(55500));

  // Check that tasks are throttled.
  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() + base::Seconds(21),
                                     start_time + base::Seconds(25),
                                     start_time + base::Seconds(50)));
  run_times.clear();

  FrameScheduler::SchedulingAffectingFeatureHandle websocket_feature =
      frame_scheduler1->RegisterFeature(
          SchedulingPolicy::Feature::kWebSocket,
          {SchedulingPolicy::DisableAggressiveThrottling()});

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(frame_scheduler1.get())
        ->GetTaskRunnerWithDefaultTaskType()
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
            base::Milliseconds(1));
  }

  FastForwardTo(base::TimeTicks() + base::Milliseconds(58500));

  // Check that the timer task queue from the first frame is aligned,
  // but not throttled.
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::Milliseconds(56000),
                          base::TimeTicks() + base::Milliseconds(56250),
                          base::TimeTicks() + base::Milliseconds(56500)));
  run_times.clear();

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(frame_scheduler2.get())
        ->GetTaskRunnerWithDefaultTaskType()
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
            base::Milliseconds(1));
  }

  FastForwardTo(base::TimeTicks() + base::Milliseconds(59500));

  // Check that the second frame scheduler becomes unthrottled.
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::Milliseconds(59000),
                          base::TimeTicks() + base::Milliseconds(59250),
                          base::TimeTicks() + base::Milliseconds(59500)));
  run_times.clear();

  websocket_feature.reset();

  // Wait for 10s to enable throttling back.
  FastForwardTo(base::TimeTicks() + base::Milliseconds(70500));

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(frame_scheduler1.get())
        ->GetTaskRunnerWithDefaultTaskType()
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
            base::Milliseconds(1));
  }

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // WebSocket is closed, budget-based throttling now applies.
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::Milliseconds(84500),
                          base::TimeTicks() + base::Milliseconds(109500),
                          base::TimeTicks() + base::Milliseconds(134500)));

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
}

// Verify that freezing a page prevents tasks in its task queues from running.
// Then, verify that making the page visible unfreezes it and allows tasks in
// its task queues to run.
TEST_F(PageSchedulerImplTest, PageFreezeAndSetVisible) {
  TestFreeze(true);
}

// Same as before, but unfreeze the page explicitly instead of making it
// visible.
TEST_F(PageSchedulerImplTest, PageFreezeAndUnfreeze) {
  TestFreeze(false);
}

TEST_F(PageSchedulerImplTest, AudioState) {
  page_scheduler_->AudioStateChanged(true);
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  page_scheduler_->AudioStateChanged(false);
  // We are audible for a certain period after raw signal disappearing.
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  test_task_runner_->FastForwardBy(recent_audio_delay() / 2);

  page_scheduler_->AudioStateChanged(false);
  // We are still audible. A new call to AudioStateChanged shouldn't change
  // anything.
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  test_task_runner_->FastForwardBy(recent_audio_delay() / 2);

  // Audio is finally silent.
  EXPECT_FALSE(page_scheduler_->IsAudioPlaying());
}

TEST_F(PageSchedulerImplTest, PageSchedulerDestroyedWhileAudioChangePending) {
  page_scheduler_->AudioStateChanged(true);
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());
  page_scheduler_->AudioStateChanged(false);

  page_scheduler_.reset();

  test_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(PageSchedulerImplTest, AudiblePagesAreNotThrottled) {
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(ThrottleableTaskQueue()->IsThrottled());

  // No throttling when the page is audible.
  page_scheduler_->AudioStateChanged(true);
  EXPECT_FALSE(ThrottleableTaskQueue()->IsThrottled());

  // No throttling for some time after audio signal disappears.
  page_scheduler_->AudioStateChanged(false);
  EXPECT_FALSE(ThrottleableTaskQueue()->IsThrottled());

  // Eventually throttling is reenabled again.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(ThrottleableTaskQueue()->IsThrottled());
}

// Regression test for crbug.com/1431695. Test freezing and state changes work
// correctly if the OnAudioSilent timer fires after the page is frozen.
TEST_F(PageSchedulerImplTest, FreezingRecentlyAudiblePage) {
  page_scheduler_->AudioStateChanged(true);
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  page_scheduler_->AudioStateChanged(false);
  // The page is audible for a certain period after raw signal disappearing.
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  page_scheduler_->SetPageVisible(false);
  // Freeze the page from the external entrypoint. This should transition the
  // page to silent and frozen.
  page_scheduler_->SetPageFrozen(true);
  EXPECT_FALSE(page_scheduler_->IsAudioPlaying());
  EXPECT_TRUE(page_scheduler_->IsFrozen());

  // Fast-forwarding past the recent audio delay should not affect the state.
  test_task_runner_->FastForwardBy(recent_audio_delay() +
                                   base::Milliseconds(10));
  EXPECT_FALSE(page_scheduler_->IsAudioPlaying());
  EXPECT_TRUE(page_scheduler_->IsFrozen());
}

// Regression test for crbug.com/1431695. Test freezing and state changes work
// correctly if the AudioStateChanged notification occurs after the page is
// frozen.
TEST_F(PageSchedulerImplTest, FreezingAudiblePage) {
  page_scheduler_->AudioStateChanged(true);
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);
  EXPECT_TRUE(page_scheduler_->IsFrozen());

  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());
  page_scheduler_->AudioStateChanged(false);
  // The page should become silent immediately.
  EXPECT_FALSE(page_scheduler_->IsAudioPlaying());
  // And the page should still be frozen.
  EXPECT_TRUE(page_scheduler_->IsFrozen());

  // Fast-forwarding past the recent audio delay should not affect the state.
  test_task_runner_->FastForwardBy(recent_audio_delay() +
                                   base::Milliseconds(10));
  EXPECT_FALSE(page_scheduler_->IsAudioPlaying());
  EXPECT_TRUE(page_scheduler_->IsFrozen());
}

TEST_F(PageSchedulerImplTest, BudgetBasedThrottlingForPageScheduler) {
  page_scheduler_->SetPageVisible(false);
}

TEST_F(PageSchedulerImplTest, TestPageBackgroundedTimerSuspension) {
  int counter = 0;
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  // The background signal will not immediately suspend the timer queue.
  page_scheduler_->SetPageVisible(false);
  test_task_runner_->FastForwardBy(base::Milliseconds(1100));
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  EXPECT_EQ(2, counter);

  counter = 0;
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  EXPECT_EQ(1, counter);

  // Advance the time until after the scheduled timer queue suspension.
  counter = 0;
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() +
                                   base::Milliseconds(10));
  EXPECT_TRUE(page_scheduler_->IsFrozen());
  EXPECT_EQ(0, counter);

  // Timer tasks should be paused until the page becomes visible.
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  test_task_runner_->FastForwardBy(base::Seconds(10));
  EXPECT_EQ(1, counter);

  counter = 0;
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(2, counter);

  // Subsequent timer tasks should fire as usual.
  counter = 0;
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, counter);
}

TEST_F(PageSchedulerImplTest, PageFrozenOnlyWhileAudioSilent) {
  page_scheduler_->AudioStateChanged(true);
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  page_scheduler_->AudioStateChanged(false);
  // We are audible for a certain period after raw signal disappearing. The page
  // should not be eligible to freeze until after this delay.
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  test_task_runner_->FastForwardBy(recent_audio_delay());
  // Audio is finally silent. The page should be eligible for freezing.
  EXPECT_FALSE(page_scheduler_->IsAudioPlaying());
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() -
                                   kEpsilon);
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  test_task_runner_->FastForwardBy(kEpsilon);
  EXPECT_TRUE(page_scheduler_->IsFrozen());

  // Page should unfreeze if audio starts playing.
  page_scheduler_->AudioStateChanged(true);
  EXPECT_FALSE(page_scheduler_->IsFrozen());
}

TEST_F(PageSchedulerImplTest, PageFrozenOnlyWhileNotVisible) {
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  // Page should freeze after delay.
  page_scheduler_->SetPageVisible(false);
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() -
                                   kEpsilon);
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  test_task_runner_->FastForwardBy(kEpsilon);
  EXPECT_TRUE(page_scheduler_->IsFrozen());

  // Page should unfreeze when it becomes visible.
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  // If the page becomes visible before the freezing delay expires, it should
  // not freeze after the delay elapses.
  page_scheduler_->SetPageVisible(false);
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() -
                                   kEpsilon);
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  page_scheduler_->SetPageVisible(true);
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() +
                                   kEpsilon);
  EXPECT_FALSE(page_scheduler_->IsFrozen());
}

}  // namespace page_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
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
using VirtualTimePolicy = blink::PageScheduler::VirtualTimePolicy;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace page_scheduler_impl_unittest {

namespace {
void IncrementCounter(int* counter) {
  ++*counter;
}
}  // namespace

using base::Bucket;
using testing::UnorderedElementsAreArray;

class MockPageSchedulerDelegate : public PageScheduler::Delegate {
 public:
  MockPageSchedulerDelegate() : idle_(false) {}

  void SetLocalMainFrameNetworkIsAlmostIdle(bool idle) { idle_ = idle; }
  bool LocalMainFrameNetworkIsAlmostIdle() const override { return idle_; }

 private:
  void ReportIntervention(const WTF::String&) override {}
  bool RequestBeginMainFrameNotExpected(bool) override { return false; }
  void SetLifecycleState(PageLifecycleState) override {}
  bool IsOrdinary() const override { return true; }

  bool idle_;
};

class PageSchedulerImplTest : public testing::Test {
 public:
  PageSchedulerImplTest() {
    feature_list_.InitAndEnableFeature(blink::features::kStopInBackground);
  }

  PageSchedulerImplTest(std::vector<base::Feature> enabled_features,
                        std::vector<base::Feature> disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~PageSchedulerImplTest() override = default;

 protected:
  void SetUp() override {
    test_task_runner_ = base::WrapRefCounted(new base::TestMockTimeTaskRunner(
        base::TestMockTimeTaskRunner::Type::kBoundToThread));
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMilliseconds(5));
    scheduler_.reset(new MainThreadSchedulerImpl(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, test_task_runner_->GetMockTickClock()),
        base::nullopt));
    page_scheduler_delegate_.reset(new MockPageSchedulerDelegate());
    page_scheduler_.reset(new PageSchedulerImpl(page_scheduler_delegate_.get(),
                                                scheduler_.get()));
    frame_scheduler_ =
        FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                   FrameScheduler::FrameType::kSubframe);
  }

  void TearDown() override {
    frame_scheduler_.reset();
    page_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  void FastForwardTo(base::TimeTicks time) {
    base::TimeTicks now = test_task_runner_->GetMockTickClock()->NowTicks();
    CHECK_LE(now, time);
    test_task_runner_->FastForwardBy(time - now);
  }

  static scoped_refptr<TaskQueue> ThrottleableTaskQueueForScheduler(
      FrameSchedulerImpl* scheduler) {
    auto* frame_task_queue_controller =
        scheduler->FrameTaskQueueControllerForTest();
    auto queue_traits = FrameSchedulerImpl::ThrottleableTaskQueueTraits();
    return frame_task_queue_controller->GetTaskQueue(queue_traits);
  }

  base::TimeDelta delay_for_background_tab_freezing() const {
    return page_scheduler_->delay_for_background_tab_freezing_;
  }

  base::TimeDelta delay_for_background_and_network_idle_tab_freezing() const {
    return page_scheduler_->delay_for_background_and_network_idle_tab_freezing_;
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

  bool ShouldFreezePage() { return page_scheduler_->ShouldFreezePage(); }

  // Verifies that freezing the PageScheduler prevents tasks from running. Then
  // set the page as visible or unfreezes it while still hidden (depending on
  // the argument), and verifies that tasks can run.
  void TestFreeze(bool make_page_visible) {
    int counter = 0;
    LoadingTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    ThrottleableTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    DeferrableTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    PausableTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    UnpausableTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

    page_scheduler_->SetPageVisible(false);
    EXPECT_EQ(false, page_scheduler_->IsFrozen());

    // In a backgrounded active page, all queues should run.
    test_task_runner_->FastForwardUntilNoTasksRemain();
    EXPECT_EQ(5, counter);

    LoadingTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    ThrottleableTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    DeferrableTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    PausableTaskQueue()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
    UnpausableTaskQueue()->task_runner()->PostTask(
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

  bool NetworkIsAlmostIdle() const {
    return page_scheduler_delegate_->LocalMainFrameNetworkIsAlmostIdle();
  }

  void NotifyLocalMainFrameNetworkIsAlmostIdle() {
    EXPECT_FALSE(page_scheduler_delegate_->LocalMainFrameNetworkIsAlmostIdle());
    page_scheduler_delegate_->SetLocalMainFrameNetworkIsAlmostIdle(true);
    page_scheduler_->OnLocalMainFrameNetworkAlmostIdle();
  }

  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
  std::unique_ptr<MockPageSchedulerDelegate> page_scheduler_delegate_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PageSchedulerImplStopNonTimersInBackgroundEnabledTest
    : public PageSchedulerImplTest {
 public:
  PageSchedulerImplStopNonTimersInBackgroundEnabledTest()
      : PageSchedulerImplTest({blink::features::kStopInBackground,
                               blink::features::kStopNonTimersInBackground},
                              {}) {}

  ~PageSchedulerImplStopNonTimersInBackgroundEnabledTest() override = default;
};

TEST_F(PageSchedulerImplTest, TestDestructionOfFrameSchedulersBefore) {
  std::unique_ptr<blink::FrameScheduler> frame1(
      page_scheduler_->CreateFrameScheduler(
          nullptr, nullptr, FrameScheduler::FrameType::kSubframe));
  std::unique_ptr<blink::FrameScheduler> frame2(
      page_scheduler_->CreateFrameScheduler(
          nullptr, nullptr, FrameScheduler::FrameType::kSubframe));
}

TEST_F(PageSchedulerImplTest, TestDestructionOfFrameSchedulersAfter) {
  std::unique_ptr<blink::FrameScheduler> frame1(
      page_scheduler_->CreateFrameScheduler(
          nullptr, nullptr, FrameScheduler::FrameType::kSubframe));
  std::unique_ptr<blink::FrameScheduler> frame2(
      page_scheduler_->CreateFrameScheduler(
          nullptr, nullptr, FrameScheduler::FrameType::kSubframe));
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
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(ThrottleableTaskQueue()->task_runner(), &run_count,
                        base::TimeDelta::FromMilliseconds(1)),
      base::TimeDelta::FromMilliseconds(1));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(PageSchedulerImplTest, RepeatingTimer_PageInBackgroundThenForeground) {
  page_scheduler_->SetPageVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(ThrottleableTaskQueue()->task_runner(), &run_count,
                        base::TimeDelta::FromMilliseconds(20)),
      base::TimeDelta::FromMilliseconds(20));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);

  // Make sure there's no delay in throttling being removed for pages that have
  // become visible.
  page_scheduler_->SetPageVisible(true);

  run_count = 0;
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(50, run_count);
}

TEST_F(PageSchedulerImplTest, RepeatingLoadingTask_PageInBackground) {
  page_scheduler_->SetPageVisible(false);

  int run_count = 0;
  LoadingTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(LoadingTaskQueue()->task_runner(), &run_count,
                        base::TimeDelta::FromMilliseconds(1)),
      base::TimeDelta::FromMilliseconds(1));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);  // Loading tasks should not be throttled
}

TEST_F(PageSchedulerImplTest, RepeatingTimers_OneBackgroundOneForeground) {
  std::unique_ptr<PageSchedulerImpl> page_scheduler2(
      new PageSchedulerImpl(nullptr, scheduler_.get()));
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler2 =
      FrameSchedulerImpl::Create(page_scheduler2.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);

  page_scheduler_->SetPageVisible(true);
  page_scheduler2->SetPageVisible(false);

  int run_count1 = 0;
  int run_count2 = 0;
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(ThrottleableTaskQueue()->task_runner(), &run_count1,
                        base::TimeDelta::FromMilliseconds(20)),
      base::TimeDelta::FromMilliseconds(20));
  ThrottleableTaskQueueForScheduler(frame_scheduler2.get())
      ->task_runner()
      ->PostDelayedTask(
          FROM_HERE,
          MakeRepeatingTask(
              ThrottleableTaskQueueForScheduler(frame_scheduler2.get())
                  ->task_runner(),
              &run_count2, base::TimeDelta::FromMilliseconds(20)),
          base::TimeDelta::FromMilliseconds(20));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(50, run_count1);
  EXPECT_EQ(1, run_count2);
}

namespace {

void RunVirtualTimeRecorderTask(const base::TickClock* clock,
                                MainThreadSchedulerImpl* scheduler,
                                Vector<base::TimeTicks>* out_real_times,
                                Vector<base::TimeTicks>* out_virtual_times) {
  out_real_times->push_back(clock->NowTicks());
  out_virtual_times->push_back(scheduler->GetVirtualTimeDomain()->Now());
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

  page_scheduler_->EnableVirtualTime();

  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  base::TimeTicks initial_virtual_time =
      scheduler_->GetVirtualTimeDomain()->Now();

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(2));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(20));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(
      virtual_times,
      ElementsAre(
          initial_virtual_time + base::TimeDelta::FromMilliseconds(2),
          initial_virtual_time + base::TimeDelta::FromMilliseconds(20),
          initial_virtual_time + base::TimeDelta::FromMilliseconds(200)));
}

TEST_F(PageSchedulerImplTest, VirtualTime_LoadingTaskFastForwarding) {
  Vector<base::TimeTicks> real_times;
  Vector<base::TimeTicks> virtual_times;

  page_scheduler_->EnableVirtualTime();

  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  base::TimeTicks initial_virtual_time =
      scheduler_->GetVirtualTimeDomain()->Now();

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(2));

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(20));

  LoadingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(200));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(
      virtual_times,
      ElementsAre(
          initial_virtual_time + base::TimeDelta::FromMilliseconds(2),
          initial_virtual_time + base::TimeDelta::FromMilliseconds(20),
          initial_virtual_time + base::TimeDelta::FromMilliseconds(200)));
}

TEST_F(PageSchedulerImplTest,
       RepeatingTimer_PageInBackground_MeansNothingForVirtualTime) {
  page_scheduler_->EnableVirtualTime();
  page_scheduler_->SetPageVisible(false);
  scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(1);
  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();

  int run_count = 0;
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(ThrottleableTaskQueue()->task_runner(), &run_count,
                        base::TimeDelta::FromMilliseconds(1)),
      base::TimeDelta::FromMilliseconds(1));

  test_task_runner_->RunUntilIdle();
  // Virtual time means page visibility is ignored.
  // 2000 is the |run_count| limit, we expect to reach it.
  EXPECT_EQ(2000, run_count);

  // The global tick clock has not moved, yet we ran a large number of "delayed"
  // tasks despite calling setPageVisible(false).
  EXPECT_EQ(initial_real_time, scheduler_->tick_clock()->NowTicks());
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

  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
  page_scheduler_->EnableVirtualTime();

  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunOrderTask, 0, base::Unretained(&run_order)));

  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DelayedRunOrderTask, 1,
                     ThrottleableTaskQueue()->task_runner(),
                     base::Unretained(&run_order)),
      base::TimeDelta::FromMilliseconds(2));

  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DelayedRunOrderTask, 3,
                     ThrottleableTaskQueue()->task_runner(),
                     base::Unretained(&run_order)),
      base::TimeDelta::FromMilliseconds(4));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // No timer tasks are allowed to run.
  EXPECT_THAT(run_order, ElementsAre());
}

TEST_F(PageSchedulerImplTest, VirtualTime_AllowedToAdvance) {
  Vector<int> run_order;

  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
  page_scheduler_->EnableVirtualTime();

  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunOrderTask, 0, base::Unretained(&run_order)));

  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DelayedRunOrderTask, 1,
                     ThrottleableTaskQueue()->task_runner(),
                     base::Unretained(&run_order)),
      base::TimeDelta::FromMilliseconds(2));

  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DelayedRunOrderTask, 3,
                     ThrottleableTaskQueue()->task_runner(),
                     base::Unretained(&run_order)),
      base::TimeDelta::FromMilliseconds(4));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_order, ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(PageSchedulerImplTest, RepeatingTimer_PageInBackground) {
  ScopedTimerThrottlingForBackgroundTabsForTest timer_throttling_enabler(false);
  page_scheduler_->SetPageVisible(false);

  int run_count = 0;
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(ThrottleableTaskQueue()->task_runner(), &run_count,
                        base::TimeDelta::FromMilliseconds(1)),
      base::TimeDelta::FromMilliseconds(1));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(PageSchedulerImplTest, VirtualTimeSettings_NewFrameScheduler) {
  Vector<int> run_order;

  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
  page_scheduler_->EnableVirtualTime();

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);

  ThrottleableTaskQueueForScheduler(frame_scheduler.get())
      ->task_runner()
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&RunOrderTask, 1, base::Unretained(&run_order)),
          base::TimeDelta::FromMilliseconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(run_order.IsEmpty());

  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
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
        FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                   FrameScheduler::FrameType::kSubframe)
            .release();
    ThrottleableTaskQueueForScheduler(frame_scheduler)
        ->task_runner()
        ->PostDelayedTask(FROM_HERE, MakeDeletionTask(frame_scheduler),
                          base::TimeDelta::FromMilliseconds(1));
  }
  test_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(PageSchedulerImplTest, DeletePageScheduler_InTask) {
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, MakeDeletionTask(page_scheduler_.release()));
  test_task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(PageSchedulerImplTest, DeleteThrottledQueue_InTask) {
  page_scheduler_->SetPageVisible(false);

  FrameSchedulerImpl* frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe)
          .release();
  scoped_refptr<TaskQueue> timer_task_queue =
      ThrottleableTaskQueueForScheduler(frame_scheduler);

  int run_count = 0;
  timer_task_queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      MakeRepeatingTask(timer_task_queue->task_runner(), &run_count,
                        base::TimeDelta::FromMilliseconds(100)),
      base::TimeDelta::FromMilliseconds(100));

  // Note this will run at time t = 10s since we start at time t = 5000us.
  // However, we still should run all tasks after frame scheduler deletion.
  timer_task_queue->task_runner()->PostDelayedTask(
      FROM_HERE, MakeDeletionTask(frame_scheduler),
      base::TimeDelta::FromMilliseconds(9990));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(20));
  EXPECT_EQ(110, run_count);
}

TEST_F(PageSchedulerImplTest, VirtualTimePauseCount_DETERMINISTIC_LOADING) {
  page_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);
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
  page_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
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
  *out = scheduler->GetVirtualTimeDomain()->Now();
}

void PauseAndUnpauseVirtualTime(MainThreadSchedulerImpl* scheduler,
                                FrameSchedulerImpl* frame_scheduler,
                                base::TimeTicks* paused,
                                base::TimeTicks* unpaused) {
  *paused = scheduler->GetVirtualTimeDomain()->Now();

  {
    WebScopedVirtualTimePauser virtual_time_pauser =
        frame_scheduler->CreateWebScopedVirtualTimePauser(
            "test",
            WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
    virtual_time_pauser.PauseVirtualTime();
  }

  *unpaused = scheduler->GetVirtualTimeDomain()->Now();
}

}  // namespace

TEST_F(PageSchedulerImplTest,
       WebScopedVirtualTimePauserWithInterleavedTasks_DETERMINISTIC_LOADING) {
  // Make task queue manager ask the virtual time domain for the next task delay
  // after each task.
  scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(1);

  page_scheduler_->EnableVirtualTime();
  page_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);

  base::TimeTicks initial_virtual_time =
      scheduler_->GetVirtualTimeDomain()->Now();

  base::TimeTicks time_paused;
  base::TimeTicks time_unpaused;
  base::TimeTicks time_second_task;

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);

  // Pauses and unpauses virtual time, thereby advancing virtual time by an
  // additional 10ms due to WebScopedVirtualTimePauser's delay.
  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &PauseAndUnpauseVirtualTime, base::Unretained(scheduler_.get()),
          base::Unretained(frame_scheduler.get()),
          base::Unretained(&time_paused), base::Unretained(&time_unpaused)),
      base::TimeDelta::FromMilliseconds(3));

  // Will run after the first task has advanced virtual time past 5ms.
  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RecordVirtualTime, base::Unretained(scheduler_.get()),
                     base::Unretained(&time_second_task)),
      base::TimeDelta::FromMilliseconds(5));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(time_paused,
            initial_virtual_time + base::TimeDelta::FromMilliseconds(3));
  EXPECT_EQ(time_unpaused,
            initial_virtual_time + base::TimeDelta::FromMilliseconds(13));
  EXPECT_EQ(time_second_task,
            initial_virtual_time + base::TimeDelta::FromMilliseconds(13));
}

TEST_F(PageSchedulerImplTest,
       MultipleWebScopedVirtualTimePausers_DETERMINISTIC_LOADING) {
  page_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
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
  page_scheduler_->SetVirtualTimePolicy(
      VirtualTimePolicy::kDeterministicLoading);
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());

  FakeTask fake_task;
  fake_task.set_enqueue_order(
      base::sequence_manager::EnqueueOrder::FromIntForTesting(42));
  const base::TimeTicks start = scheduler_->real_time_domain()->Now();
  scheduler_->OnTaskStarted(nullptr, fake_task,
                            FakeTaskTiming(start, base::TimeTicks()));
  scheduler_->OnBeginNestedRunLoop();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  scheduler_->OnExitNestedRunLoop();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
  FakeTaskTiming task_timing(start, scheduler_->real_time_domain()->Now());
  scheduler_->OnTaskCompleted(nullptr, fake_task, &task_timing, nullptr);
}

TEST_F(PageSchedulerImplTest, PauseTimersWhileVirtualTimeIsPaused) {
  Vector<int> run_order;

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);
  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
  page_scheduler_->EnableVirtualTime();

  ThrottleableTaskQueueForScheduler(frame_scheduler.get())
      ->task_runner()
      ->PostTask(FROM_HERE, base::BindOnce(&RunOrderTask, 1,
                                           base::Unretained(&run_order)));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(run_order.IsEmpty());

  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(PageSchedulerImplTest, VirtualTimeBudgetExhaustedCallback) {
  Vector<base::TimeTicks> real_times;
  Vector<base::TimeTicks> virtual_times;

  page_scheduler_->EnableVirtualTime();

  base::TimeTicks initial_real_time = scheduler_->tick_clock()->NowTicks();
  base::TimeTicks initial_virtual_time =
      scheduler_->GetVirtualTimeDomain()->Now();

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(1));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(2));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(5));

  ThrottleableTaskRunner()->PostDelayedTask(
      FROM_HERE,
      MakeVirtualTimeRecorderTask(test_task_runner_->GetMockTickClock(),
                                  scheduler_.get(), &real_times,
                                  &virtual_times),
      base::TimeDelta::FromMilliseconds(7));

  page_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(5),
      base::BindOnce(
          [](PageScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
          },
          base::Unretained(page_scheduler_.get())));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // The timer that is scheduled for the exact point in time when virtual time
  // expires will not run.
  EXPECT_THAT(real_times, ElementsAre(initial_real_time, initial_real_time,
                                      initial_real_time));
  EXPECT_THAT(
      virtual_times,
      ElementsAre(initial_virtual_time + base::TimeDelta::FromMilliseconds(1),
                  initial_virtual_time + base::TimeDelta::FromMilliseconds(2),
                  initial_virtual_time + base::TimeDelta::FromMilliseconds(5)));
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
  page_scheduler_->EnableVirtualTime();
  page_scheduler_->SetMaxVirtualTimeTaskStarvationCount(100);
  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue()->task_runner(), 1000, &count);
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, base::Unretained(&count),
                     base::Unretained(&delayed_task_run_at_count)),
      base::TimeDelta::FromMilliseconds(10));

  page_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(1000),
      base::BindOnce(
          [](PageScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
          },
          base::Unretained(page_scheduler_.get())));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Two delayed tasks with a run of 100 tasks, plus initial call.
  EXPECT_EQ(201, count);
  EXPECT_EQ(102, delayed_task_run_at_count);
}

TEST_F(PageSchedulerImplTest,
       MaxVirtualTimeTaskStarvationCountOneHundredNestedMessageLoop) {
  page_scheduler_->EnableVirtualTime();
  page_scheduler_->SetMaxVirtualTimeTaskStarvationCount(100);
  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  FakeTask fake_task;
  fake_task.set_enqueue_order(
      base::sequence_manager::EnqueueOrder::FromIntForTesting(42));
  const base::TimeTicks start = scheduler_->real_time_domain()->Now();
  scheduler_->OnTaskStarted(nullptr, fake_task,
                            FakeTaskTiming(start, base::TimeTicks()));
  scheduler_->OnBeginNestedRunLoop();

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue()->task_runner(), 1000, &count);
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, base::Unretained(&count),
                     base::Unretained(&delayed_task_run_at_count)),
      base::TimeDelta::FromMilliseconds(10));

  page_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(1000),
      base::BindOnce(
          [](PageScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
          },
          base::Unretained(page_scheduler_.get())));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(1000, count);
  EXPECT_EQ(1000, delayed_task_run_at_count);
}

TEST_F(PageSchedulerImplTest, MaxVirtualTimeTaskStarvationCountZero) {
  page_scheduler_->EnableVirtualTime();
  page_scheduler_->SetMaxVirtualTimeTaskStarvationCount(0);
  page_scheduler_->SetVirtualTimePolicy(VirtualTimePolicy::kAdvance);

  int count = 0;
  int delayed_task_run_at_count = 0;
  RepostingTask(ThrottleableTaskQueue()->task_runner(), 1000, &count);
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(DelayedTask, base::Unretained(&count),
                     base::Unretained(&delayed_task_run_at_count)),
      base::TimeDelta::FromMilliseconds(10));

  page_scheduler_->GrantVirtualTimeBudget(
      base::TimeDelta::FromMilliseconds(1000),
      base::BindOnce(
          [](PageScheduler* scheduler) {
            scheduler->SetVirtualTimePolicy(VirtualTimePolicy::kPause);
          },
          base::Unretained(page_scheduler_.get())));

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
  task_runner->AdvanceMockTickClock(base::TimeDelta::FromMilliseconds(250));
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
  ScopedExpensiveBackgroundTimerThrottlingForTest
      budget_background_throttling_enabler(true);

  InitializeTrialParams();
  page_scheduler_.reset(new PageSchedulerImpl(nullptr, scheduler_.get()));
  EXPECT_FALSE(page_scheduler_->IsThrottled());

  Vector<base::TimeTicks> run_times;
  frame_scheduler_ =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(page_scheduler_->IsThrottled());

  FastForwardTo(base::TimeTicks() + base::TimeDelta::FromMilliseconds(2500));

  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
      base::TimeDelta::FromMilliseconds(1));
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
      base::TimeDelta::FromMilliseconds(1));

  FastForwardTo(base::TimeTicks() + base::TimeDelta::FromMilliseconds(3500));

  // Check that these tasks are aligned, but are not subject to budget-based
  // throttling.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(2501),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(2751)));
  run_times.clear();

  page_scheduler_->SetPageVisible(false);
  EXPECT_FALSE(page_scheduler_->IsThrottled());

  // Ensure that the page is fully throttled.
  FastForwardTo(base::TimeTicks() + base::TimeDelta::FromSeconds(15));
  EXPECT_TRUE(page_scheduler_->IsThrottled());

  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
      base::TimeDelta::FromMicroseconds(1));
  ThrottleableTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
      base::TimeDelta::FromMicroseconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Check that tasks are aligned and throttled.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(16),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(26)));

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
}

TEST_F(PageSchedulerImplTest, OpenWebSocketExemptsFromBudgetThrottling) {
  ScopedExpensiveBackgroundTimerThrottlingForTest
      budget_background_throttling_enabler(true);

  InitializeTrialParams();
  std::unique_ptr<PageSchedulerImpl> page_scheduler(
      new PageSchedulerImpl(nullptr, scheduler_.get()));

  Vector<base::TimeTicks> run_times;

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler1 =
      FrameSchedulerImpl::Create(page_scheduler.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler2 =
      FrameSchedulerImpl::Create(page_scheduler.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);

  page_scheduler->SetPageVisible(false);

  // Wait for 20s to avoid initial throttling delay.
  FastForwardTo(base::TimeTicks() + base::TimeDelta::FromMilliseconds(20500));

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(frame_scheduler1.get())
        ->task_runner()
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
            base::TimeDelta::FromMilliseconds(1));
  }

  FastForwardTo(base::TimeTicks() + base::TimeDelta::FromMilliseconds(55500));

  // Check that tasks are throttled.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(21),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(26),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(51)));
  run_times.clear();

  FrameScheduler::SchedulingAffectingFeatureHandle websocket_feature =
      frame_scheduler1->RegisterFeature(
          SchedulingPolicy::Feature::kWebSocket,
          {SchedulingPolicy::DisableAggressiveThrottling()});

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(frame_scheduler1.get())
        ->task_runner()
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
            base::TimeDelta::FromMilliseconds(1));
  }

  FastForwardTo(base::TimeTicks() + base::TimeDelta::FromMilliseconds(58500));

  // Check that the timer task queue from the first frame is aligned,
  // but not throttled.
  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(56000),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(56250),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(56500)));
  run_times.clear();

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(frame_scheduler2.get())
        ->task_runner()
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
            base::TimeDelta::FromMilliseconds(1));
  }

  FastForwardTo(base::TimeTicks() + base::TimeDelta::FromMilliseconds(59500));

  // Check that the second frame scheduler becomes unthrottled.
  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(59000),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(59250),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(59500)));
  run_times.clear();

  websocket_feature.reset();

  // Wait for 10s to enable throttling back.
  FastForwardTo(base::TimeTicks() + base::TimeDelta::FromMilliseconds(70500));

  for (size_t i = 0; i < 3; ++i) {
    ThrottleableTaskQueueForScheduler(frame_scheduler1.get())
        ->task_runner()
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ExpensiveTestTask, test_task_runner_, &run_times),
            base::TimeDelta::FromMilliseconds(1));
  }

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // WebSocket is closed, budget-based throttling now applies.
  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(84),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(109),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(134)));

  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
}

// Verify that freezing a page prevents tasks in its task queues from running.
// Then, verify that making the page visible unfreezes it and allows tasks in
// its task queues to run.
TEST_F(PageSchedulerImplStopNonTimersInBackgroundEnabledTest,
       PageFreezeAndSetVisible) {
  TestFreeze(true);
}

// Same as before, but unfreeze the page explicitly instead of making it
// visible.
TEST_F(PageSchedulerImplStopNonTimersInBackgroundEnabledTest,
       PageFreezeAndUnfreeze) {
  TestFreeze(false);
}

TEST_F(PageSchedulerImplTest, AudioState) {
  page_scheduler_->AudioStateChanged(true);
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  page_scheduler_->AudioStateChanged(false);
  // We are audible for a certain period after raw signal disappearing.
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(3));

  page_scheduler_->AudioStateChanged(false);
  // We are still audible. A new call to AudioStateChanged shouldn't change
  // anything.
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());

  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(3));

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
  EXPECT_TRUE(scheduler_->task_queue_throttler()->IsThrottled(
      ThrottleableTaskQueue().get()));

  // No throttling when the page is audible.
  page_scheduler_->AudioStateChanged(true);
  EXPECT_FALSE(scheduler_->task_queue_throttler()->IsThrottled(
      ThrottleableTaskQueue().get()));

  // No throttling for some time after audio signal disappears.
  page_scheduler_->AudioStateChanged(false);
  EXPECT_FALSE(scheduler_->task_queue_throttler()->IsThrottled(
      ThrottleableTaskQueue().get()));

  // Eventually throttling is reenabled again.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(scheduler_->task_queue_throttler()->IsThrottled(
      ThrottleableTaskQueue().get()));
}

TEST_F(PageSchedulerImplTest, BudgetBasedThrottlingForPageScheduler) {
  page_scheduler_->SetPageVisible(false);
}

TEST_F(PageSchedulerImplTest, KeepActiveSetForNewPages) {
  scheduler_->SetSchedulerKeepActive(true);

  std::unique_ptr<PageSchedulerImpl> page_scheduler2 =
      std::make_unique<PageSchedulerImpl>(nullptr, scheduler_.get());

  EXPECT_TRUE(page_scheduler_->KeepActive());
  EXPECT_TRUE(page_scheduler2->KeepActive());
}

TEST_F(PageSchedulerImplTest, TestPageBackgroundedTimerSuspension) {
  int counter = 0;
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  // The background signal will not immediately suspend the timer queue.
  page_scheduler_->SetPageVisible(false);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(1100));
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  EXPECT_EQ(2, counter);

  counter = 0;
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  EXPECT_EQ(1, counter);

  // Advance the time until after the scheduled timer queue suspension.
  counter = 0;
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() +
                                   base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(page_scheduler_->IsFrozen());
  EXPECT_EQ(0, counter);

  // Timer tasks should be paused until the page becomes visible.
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(1, counter);

  counter = 0;
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(2, counter);

  // Subsequent timer tasks should fire as usual.
  counter = 0;
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, counter);
}

TEST_F(PageSchedulerImplTest, PageFrozenOnlyWhileAudioSilent) {
  page_scheduler_->AudioStateChanged(true);
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());
  EXPECT_FALSE(ShouldFreezePage());
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  page_scheduler_->AudioStateChanged(false);
  // We are audible for a certain period after raw signal disappearing. The page
  // should not be eligible to freeze until after this delay.
  EXPECT_TRUE(page_scheduler_->IsAudioPlaying());
  EXPECT_FALSE(ShouldFreezePage());

  test_task_runner_->FastForwardBy(recent_audio_delay() +
                                   base::TimeDelta::FromMilliseconds(100));
  // Audio is finally silent. The page should be eligible for freezing.
  EXPECT_FALSE(page_scheduler_->IsAudioPlaying());
  EXPECT_TRUE(ShouldFreezePage());
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() +
                                   base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(page_scheduler_->IsFrozen());

  // Page should unfreeze if audio starts playing.
  page_scheduler_->AudioStateChanged(true);
  EXPECT_FALSE(page_scheduler_->IsFrozen());
}

TEST_F(PageSchedulerImplTest, PageFrozenOnlyWhileNotVisible) {
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(ShouldFreezePage());
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  // Page should freeze after delay.
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(ShouldFreezePage());
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() +
                                   base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(page_scheduler_->IsFrozen());

  // Page should unfreeze when it becomes visible.
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  // If the page becomes visible before the freezing delay expires, it should
  // not freeze after the delay elapses.
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(ShouldFreezePage());
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() -
                                   base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  page_scheduler_->SetPageVisible(true);
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() +
                                   base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(page_scheduler_->IsFrozen());
}

class PageSchedulerImplFreezeBackgroundTabOnNetworkIdleEnabledTest
    : public PageSchedulerImplTest {
 public:
  PageSchedulerImplFreezeBackgroundTabOnNetworkIdleEnabledTest()
      : PageSchedulerImplTest(
            {blink::features::kStopInBackground,
             blink::features::kFreezeBackgroundTabOnNetworkIdle},
            {}) {}
};

TEST_F(PageSchedulerImplFreezeBackgroundTabOnNetworkIdleEnabledTest,
       PageFrozenOnlyOnLocalMainFrameNetworkIdle) {
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(ShouldFreezePage());
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  EXPECT_FALSE(NetworkIsAlmostIdle());

  // After network is idle, page should freeze after delay for quick
  // background tab freezing.
  NotifyLocalMainFrameNetworkIsAlmostIdle();
  EXPECT_TRUE(NetworkIsAlmostIdle());
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  page_scheduler_->SetPageVisible(false);
  test_task_runner_->FastForwardBy(
      delay_for_background_and_network_idle_tab_freezing() +
      base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(page_scheduler_->IsFrozen());
}

TEST_F(PageSchedulerImplFreezeBackgroundTabOnNetworkIdleEnabledTest,
       PageFrozenOnlyOnLocalMainFrameNetworkAlmostIdleNoRegress) {
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(ShouldFreezePage());
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  EXPECT_FALSE(NetworkIsAlmostIdle());

  // Page should freeze after delay for background tab freezing.
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(ShouldFreezePage());
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() +
                                   base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(page_scheduler_->IsFrozen());
}

TEST_F(PageSchedulerImplFreezeBackgroundTabOnNetworkIdleEnabledTest,
       PageFrozenWhenNetworkIdleAfterQuickFreezingDelay) {
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(ShouldFreezePage());
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  EXPECT_FALSE(NetworkIsAlmostIdle());

  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(ShouldFreezePage());
  test_task_runner_->FastForwardBy(
      delay_for_background_and_network_idle_tab_freezing() +
      base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  NotifyLocalMainFrameNetworkIsAlmostIdle();
  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(page_scheduler_->IsFrozen());
}

TEST_F(PageSchedulerImplFreezeBackgroundTabOnNetworkIdleEnabledTest,
       PageNotFrozenWhenVisibleBeforeNetworkIdle) {
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(ShouldFreezePage());
  EXPECT_FALSE(page_scheduler_->IsFrozen());
  EXPECT_FALSE(NetworkIsAlmostIdle());

  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(ShouldFreezePage());
  test_task_runner_->FastForwardBy(
      delay_for_background_and_network_idle_tab_freezing() +
      base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(page_scheduler_->IsFrozen());

  // Page should not freeze after delay for background tab freezing, because
  // the page is visible.
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(ShouldFreezePage());
  test_task_runner_->FastForwardBy(delay_for_background_tab_freezing());
  EXPECT_FALSE(page_scheduler_->IsFrozen());
}

class PageSchedulerImplPageTransitionTest : public PageSchedulerImplTest {
 public:
  typedef PageSchedulerImpl::PageLifecycleStateTransition Transition;

  PageSchedulerImplPageTransitionTest() {
    for (int i = 0; i <= static_cast<int>(Transition::kMaxValue); i++)
      transition_counts_.push_back(0);
  }

  ~PageSchedulerImplPageTransitionTest() override = default;

  void IncrementPageTransition(Transition transition) {
    transition_counts_[static_cast<int>(transition)] += 1;
  }

  Vector<Bucket> GetExpectedBuckets() {
    Vector<Bucket> buckets;
    for (int i = 0; i <= static_cast<int>(Transition::kMaxValue); i++) {
      if (transition_counts_[i] > 0)
        buckets.push_back(Bucket(i, transition_counts_[i]));
    }
    return buckets;
  }

  void WaitForFreezingDelay() {
    test_task_runner_->FastForwardBy(delay_for_background_tab_freezing() +
                                     base::TimeDelta::FromMilliseconds(100));
  }

  void DisableAudioAndWaitForSilent() {
    page_scheduler_->AudioStateChanged(false);
    test_task_runner_->FastForwardBy(recent_audio_delay() +
                                     base::TimeDelta::FromMilliseconds(100));
  }

 protected:
  Vector<int> transition_counts_;
};

TEST_F(PageSchedulerImplPageTransitionTest,
       PageLifecycleStateTransitionMetric) {
  typedef PageSchedulerImpl::PageLifecycleStateTransition Transition;

  base::HistogramTester histogram_tester_;

  if (kDefaultPageVisibility == PageVisibilityState::kHidden) {
    page_scheduler_->SetPageVisible(true);
    IncrementPageTransition(Transition::kHiddenBackgroundedToActive);
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
                UnorderedElementsAreArray(GetExpectedBuckets()));
  }

  // Visible w/o audio -> hidden/backgrounded -> frozen.
  page_scheduler_->SetPageVisible(false);
  IncrementPageTransition(Transition::kActiveToHiddenBackgrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  WaitForFreezingDelay();
  IncrementPageTransition(Transition::kHiddenBackgroundedToFrozen);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));

  // Visible w/ audio -> hidden/not backgrouneded -> hidden/backgrounded ->
  // frozen.
  page_scheduler_->SetPageVisible(true);
  IncrementPageTransition(Transition::kFrozenToActive);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  page_scheduler_->AudioStateChanged(true);
  // No transition when audio state changes in the foreground.
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  page_scheduler_->SetPageVisible(false);
  IncrementPageTransition(Transition::kActiveToHiddenForegrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  DisableAudioAndWaitForSilent();
  IncrementPageTransition(Transition::kHiddenForegroundedToHiddenBackgrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  WaitForFreezingDelay();
  IncrementPageTransition(Transition::kHiddenBackgroundedToFrozen);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));

  // When freezing from outside the renderer, it's possible to have transitions
  // to frozen from hidden/foregrounded and hidden/backgrounded.
  //
  // Visible w/o audio -> hidden/backgrounded -> frozen from outside the
  // renderer.
  page_scheduler_->SetPageVisible(true);
  IncrementPageTransition(Transition::kFrozenToActive);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  page_scheduler_->SetPageVisible(false);
  IncrementPageTransition(Transition::kActiveToHiddenBackgrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  page_scheduler_->SetPageFrozen(true);
  IncrementPageTransition(Transition::kHiddenBackgroundedToFrozen);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  // Unfreezing from outside the renderer should return to hidden/backgrounded.
  page_scheduler_->SetPageFrozen(false);
  IncrementPageTransition(Transition::kFrozenToHiddenBackgrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));

  // Hidden/backgrounded -> hidden/not backgrouneded -> frozen from outside the
  // renderer.
  page_scheduler_->AudioStateChanged(true);
  IncrementPageTransition(Transition::kHiddenBackgroundedToHiddenForegrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  page_scheduler_->SetPageFrozen(true);
  IncrementPageTransition(Transition::kHiddenForegroundedToFrozen);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  // Unfreezing from outside the renderer should return to hidden/foregrounded.
  page_scheduler_->SetPageFrozen(false);
  IncrementPageTransition(Transition::kFrozenToHiddenForegrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));

  // Visible -> hidden* -> hidden* -> visible.
  page_scheduler_->SetPageVisible(true);
  IncrementPageTransition(Transition::kHiddenForegroundedToActive);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  page_scheduler_->SetPageVisible(false);
  IncrementPageTransition(Transition::kActiveToHiddenForegrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  DisableAudioAndWaitForSilent();
  IncrementPageTransition(Transition::kHiddenForegroundedToHiddenBackgrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  page_scheduler_->AudioStateChanged(true);
  IncrementPageTransition(Transition::kHiddenBackgroundedToHiddenForegrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  DisableAudioAndWaitForSilent();
  IncrementPageTransition(Transition::kHiddenForegroundedToHiddenBackgrounded);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
  page_scheduler_->SetPageVisible(true);
  IncrementPageTransition(Transition::kHiddenBackgroundedToActive);
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  PageSchedulerImpl::kHistogramPageLifecycleStateTransition),
              UnorderedElementsAreArray(GetExpectedBuckets()));
}

}  // namespace page_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink

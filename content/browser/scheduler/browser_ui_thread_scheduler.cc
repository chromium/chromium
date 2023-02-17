// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/task/task_features.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/scheduler/browser_task_priority.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

namespace {

content::BrowserUIThreadScheduler* g_browser_ui_thread_scheduler = nullptr;

bool IsActiveScrollState(
    content::BrowserUIThreadScheduler::ScrollState scroll_state) {
  return scroll_state ==
             content::BrowserUIThreadScheduler::ScrollState::kFlingActive ||
         scroll_state == content::BrowserUIThreadScheduler::ScrollState::
                             kGestureScrollActive;
}

}  // namespace
namespace content {

namespace features {
// When the "BrowserPrioritizeNativeWork" feature is enabled, the main thread
// will process native messages between each batch of application tasks for some
// duration after an input event. The duration is controlled by the
// "prioritize_for_next_ms" feature param. Special case: If
// "prioritize_for_next_ms" is TimeDelta::Max(), native messages will be
// processed between each batch of application tasks, independently from input
// events.
//
// The goal is to reduce jank by processing subsequent input events sooner after
// a first input event is received. Checking for native messages more frequently
// incurs some overhead, but allows the browser to handle input more
// consistently.
BASE_FEATURE(kBrowserPrioritizeNativeWork,
             "BrowserPrioritizeNativeWork",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<base::TimeDelta>
    kBrowserPrioritizeNativeWorkAfterInputForNMsParam{
        &kBrowserPrioritizeNativeWork, "prioritize_for_next_ms",
        base::TimeDelta::Max()};

// Feature to defer tasks on the UI thread to prioritise input.
BASE_FEATURE(kBrowserDeferUIThreadTasks,
             "BrowserDeferUIThreadTasks",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kDeferNormalOrLessPriorityTasks{
    &kBrowserDeferUIThreadTasks, "defer_normal_or_less_priority_tasks", false};

constexpr base::FeatureParam<bool> kDeferKnownLongRunningTasks{
    &kBrowserDeferUIThreadTasks, "defer_known_long_running_tasks", false};
}  // namespace features

BrowserUIThreadScheduler::UserInputActiveHandle::UserInputActiveHandle(
    BrowserUIThreadScheduler* scheduler)
    : scheduler_(scheduler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(scheduler_);
  DCHECK_GE(scheduler_->user_input_active_handle_count, 0);

  ++scheduler_->user_input_active_handle_count;
  if (scheduler_->user_input_active_handle_count == 1) {
    TRACE_EVENT("input", "RenderWidgetHostImpl::UserInputStarted");
    scheduler_->DidStartUserInput();
  }
}

BrowserUIThreadScheduler::UserInputActiveHandle::UserInputActiveHandle(
    UserInputActiveHandle&& other) {
  MoveFrom(&other);
}

BrowserUIThreadScheduler::UserInputActiveHandle&
BrowserUIThreadScheduler::UserInputActiveHandle::operator=(
    UserInputActiveHandle&& other) {
  MoveFrom(&other);
  return *this;
}

void BrowserUIThreadScheduler::UserInputActiveHandle::MoveFrom(
    UserInputActiveHandle* other) {
  scheduler_ = other->scheduler_;
  // Prevent the other's deconstructor from decrementing
  // |user_input_active_handle_counter|.
  other->scheduler_ = nullptr;
}

BrowserUIThreadScheduler::UserInputActiveHandle::~UserInputActiveHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!scheduler_) {
    return;
  }
  DCHECK_GE(scheduler_->user_input_active_handle_count, 1);

  --scheduler_->user_input_active_handle_count;
  if (scheduler_->user_input_active_handle_count == 0) {
    scheduler_->DidEndUserInput();
  }
}

BrowserUIThreadScheduler::~BrowserUIThreadScheduler() = default;

// static
std::unique_ptr<BrowserUIThreadScheduler>
BrowserUIThreadScheduler::CreateForTesting(
    base::sequence_manager::SequenceManager* sequence_manager) {
  return base::WrapUnique(new BrowserUIThreadScheduler(sequence_manager));
}
BrowserUIThreadScheduler* BrowserUIThreadScheduler::Get() {
  DCHECK(g_browser_ui_thread_scheduler);
  return g_browser_ui_thread_scheduler;
}
BrowserUIThreadScheduler::BrowserUIThreadScheduler()
    : owned_sequence_manager_(
          base::sequence_manager::CreateUnboundSequenceManager(
              base::sequence_manager::SequenceManager::Settings::Builder()
                  .SetMessagePumpType(base::MessagePumpType::UI)
                  .SetPrioritySettings(
                      internal::CreateBrowserTaskPrioritySettings())
                  .Build())),
      task_queues_(BrowserThread::UI, owned_sequence_manager_.get()),
      queue_data_(task_queues_.GetQueueData()),
      handle_(task_queues_.GetHandle()) {
  CommonSequenceManagerSetup(owned_sequence_manager_.get());
  owned_sequence_manager_->SetDefaultTaskRunner(
      handle_->GetDefaultTaskRunner());

  owned_sequence_manager_->BindToMessagePump(
      base::MessagePump::Create(base::MessagePumpType::UI));
  g_browser_ui_thread_scheduler = this;
}

BrowserUIThreadScheduler::BrowserUIThreadScheduler(
    base::sequence_manager::SequenceManager* sequence_manager)
    : task_queues_(BrowserThread::UI, sequence_manager),
      queue_data_(task_queues_.GetQueueData()),
      handle_(task_queues_.GetHandle()) {
  CommonSequenceManagerSetup(sequence_manager);
  g_browser_ui_thread_scheduler = this;
}

void BrowserUIThreadScheduler::CommonSequenceManagerSetup(
    base::sequence_manager::SequenceManager* sequence_manager) {
  DCHECK_EQ(static_cast<size_t>(sequence_manager->GetPriorityCount()),
            static_cast<size_t>(internal::BrowserTaskPriority::kPriorityCount));
  sequence_manager->EnableCrashKeys("ui_scheduler_async_stack");
}

BrowserUIThreadScheduler::UserInputActiveHandle
BrowserUIThreadScheduler::OnUserInputStart() {
  return BrowserUIThreadScheduler::UserInputActiveHandle(this);
}

void BrowserUIThreadScheduler::DidStartUserInput() {
  // Avoiding crashes in tests that doesn't mock sequence manager.
  if (!owned_sequence_manager_)
    return;
  if (browser_prioritize_native_work_ &&
      !browser_prioritize_native_work_after_input_end_ms_.is_inf()) {
    owned_sequence_manager_->PrioritizeYieldingToNative(base::TimeTicks::Max());
  }

  if (browser_enable_periodic_yielding_native_ &&
      scroll_state_ != kFlingActive) {
    TRACE_EVENT("input",
                "RenderWidgetHostImpl::DidStartUserInputExperimentEnabled");
    owned_sequence_manager_->EnablePeriodicYieldingToNative(
        yield_to_native_for_normal_input_after_ms_);
  }
}

void BrowserUIThreadScheduler::OnScrollStateUpdate(ScrollState scroll_state) {
  if (browser_enable_deferring_ui_thread_tasks_) {
    UpdatePolicyOnScrollStateUpdate(scroll_state_, scroll_state);
  }
  scroll_state_ = scroll_state;
  // Avoiding crashes in tests that doesn't mock sequence manager.
  if (!owned_sequence_manager_ || !browser_enable_periodic_yielding_native_)
    return;
  switch (scroll_state) {
    case kGestureScrollActive:
      owned_sequence_manager_->EnablePeriodicYieldingToNative(
          yield_to_native_for_normal_input_after_ms_);
      break;
    case kFlingActive:
      owned_sequence_manager_->EnablePeriodicYieldingToNative(
          yield_to_native_for_fling_input_after_ms_);
      break;
    case kNone:
      // if count is > 0 it means touch moves  in flight, leave the frequent
      // alternation enabled for now.
      if (user_input_active_handle_count == 0)
        owned_sequence_manager_->EnablePeriodicYieldingToNative(
            yield_to_native_for_default_after_ms_);
      break;
  }
}

void BrowserUIThreadScheduler::UpdatePolicyOnScrollStateUpdate(
    ScrollState old_state,
    ScrollState new_state) {
  TRACE_EVENT("input",
              "BrowserUIThreadScheduler::UpdatePolicyOnScrollStateUpdate",
              "enabled", IsActiveScrollState(new_state));
  bool state_change =
      IsActiveScrollState(old_state) != IsActiveScrollState(new_state);
  if (!state_change) {
    return;
  }

  current_policy_.should_defer_task_queues_ = IsActiveScrollState(new_state);
  UpdateTaskQueueStates();
}

void BrowserUIThreadScheduler::UpdateTaskQueueStates() {
  for (unsigned int i = 0; i < task_queues_.kNumQueueTypes; i++) {
    QueueType type = static_cast<QueueType>(i);
    GetBrowserTaskRunnerVoter(type).SetVoteToEnable(
        current_policy_.IsQueueEnabled(type));
  }
}

bool BrowserUIThreadScheduler::Policy::IsQueueEnabled(
    BrowserTaskQueues::QueueType task_queue) const {
  if (!should_defer_task_queues_) {
    return true;
  }

  switch (task_queue) {
    case BrowserTaskQueues::QueueType::kDeferrableUserBlocking:
      return !(should_defer_task_queues_ && defer_known_long_running_tasks_);
    case BrowserTaskQueues::QueueType::kBestEffort:
    case BrowserTaskQueues::QueueType::kUserVisible:
    case BrowserTaskQueues::QueueType::kUserBlocking:
      return !(should_defer_task_queues_ &&
               defer_normal_or_lower_priority_tasks_);
    // TODO(b/261554018): defer the default task queue after pumping the
    // priority of scoped blocking calls from renderer to UI main to
    // avoid deadlocks on deferring default queue.
    case BrowserTaskQueues::QueueType::kDefault:
    case BrowserTaskQueues::QueueType::kUserInput:
    case BrowserTaskQueues::QueueType::kNavigationNetworkResponse:
    case BrowserTaskQueues::QueueType::kServiceWorkerStorageControlResponse:
      return true;
  }
  NOTREACHED();
}

void BrowserUIThreadScheduler::DidEndUserInput() {
  // Avoiding crashes in tests that doesn't mock sequence manager.
  if (!owned_sequence_manager_)
    return;
  if (browser_prioritize_native_work_ &&
      !browser_prioritize_native_work_after_input_end_ms_.is_inf()) {
    owned_sequence_manager_->PrioritizeYieldingToNative(
        base::TimeTicks::Now() +
        browser_prioritize_native_work_after_input_end_ms_);
  }

  // This disables the alternating behaviour if there are no scrolls ongoing.
  if (browser_enable_periodic_yielding_native_ && scroll_state_ == kNone) {
    owned_sequence_manager_->EnablePeriodicYieldingToNative(
        yield_to_native_for_default_after_ms_);
  }
  return;
}

void BrowserUIThreadScheduler::PostFeatureListSetup() {
  if (base::FeatureList::IsEnabled(features::kBrowserPrioritizeNativeWork)) {
    EnableBrowserPrioritizesNativeWork();
  }

  if (base::FeatureList::IsEnabled(base::kBrowserPeriodicYieldingToNative)) {
    EnableAlternatingScheduler();
  }

  if (base::FeatureList::IsEnabled(features::kBrowserDeferUIThreadTasks)) {
    EnableDeferringBrowserUIThreadTasks();
  }
}

void BrowserUIThreadScheduler::EnableBrowserPrioritizesNativeWork() {
  browser_prioritize_native_work_after_input_end_ms_ =
      features::kBrowserPrioritizeNativeWorkAfterInputForNMsParam.Get();
  // Rather than just enable immediately we post a task at default priority.
  // This ensures most start up work should be finished before we start using
  // this policy.
  //
  // TODO(nuskos): Switch this to use ThreadControllerObserver after start up
  // notification once available on android.
  handle_->GetDefaultTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserUIThreadScheduler* scheduler) {
            scheduler->browser_prioritize_native_work_ = true;
            if (scheduler->browser_prioritize_native_work_after_input_end_ms_
                    .is_inf()) {
              // We will always prioritize yielding to native if the
              // experiment is enabled but the delay after input is infinity.
              // So enable it now.
              scheduler->owned_sequence_manager_->PrioritizeYieldingToNative(
                  base::TimeTicks::Max());
            }
          },
          base::Unretained(this)));
}

void BrowserUIThreadScheduler::EnableAlternatingScheduler() {
  // Initialize feature parameters. This can't be done in the constructor
  // because the FeatureList hasn't been initialized when the
  // BrowserUIThreadScheduler is created.
  yield_to_native_for_normal_input_after_ms_ =
      base::kBrowserPeriodicYieldingToNativeNormalInputAfterMsParam.Get();
  yield_to_native_for_fling_input_after_ms_ =
      base::kBrowserPeriodicYieldingToNativeFlingInputAfterMsParam.Get();
  yield_to_native_for_default_after_ms_ =
      base::kBrowserPeriodicYieldingToNativeNoInputAfterMsParam.Get();

  // Rather than just enable immediately we post a task at default priority
  // This ensures most start up work should be finished before we start using
  // this policy.
  //
  // TODO(nuskos): Switch this to use ThreadControllerObserver after start up
  // notification once available on android.
  handle_->GetDefaultTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](BrowserUIThreadScheduler* scheduler) {
            scheduler->browser_enable_periodic_yielding_native_ = true;
            if (scheduler->scroll_state_ == ScrollState::kNone) {
              scheduler->owned_sequence_manager_
                  ->EnablePeriodicYieldingToNative(
                      scheduler->yield_to_native_for_default_after_ms_);
            } else {
              scheduler->OnScrollStateUpdate(scheduler->scroll_state_);
            }
          },
          base::Unretained(this)));
}

void BrowserUIThreadScheduler::EnableDeferringBrowserUIThreadTasks() {
  // Initialize feature parameters. This can't be done in the constructor
  // because the FeatureList hasn't been initialized when the
  // BrowserUIThreadScheduler is created.
  current_policy_.defer_normal_or_lower_priority_tasks_ =
      features::kDeferNormalOrLessPriorityTasks.Get();
  current_policy_.defer_known_long_running_tasks_ =
      features::kDeferKnownLongRunningTasks.Get();
  browser_enable_deferring_ui_thread_tasks_ = true;
}
}  // namespace content

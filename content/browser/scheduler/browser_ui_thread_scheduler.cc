// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

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
constexpr base::Feature kBrowserPrioritizeNativeWork{
    "BrowserPrioritizeNativeWork", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<base::TimeDelta>
    kBrowserPrioritizeNativeWorkAfterInputForNMsParam{
        &kBrowserPrioritizeNativeWork, "prioritize_for_next_ms",
        base::TimeDelta::Max()};
}  // namespace features

BrowserUIThreadScheduler::UserInputActiveHandle::UserInputActiveHandle(
    BrowserUIThreadScheduler* scheduler)
    : scheduler_(scheduler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(scheduler_);
  DCHECK_GE(scheduler_->user_input_active_handle_count, 0);

  ++scheduler_->user_input_active_handle_count;
  if (scheduler_->user_input_active_handle_count == 1) {
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

BrowserUIThreadScheduler::BrowserUIThreadScheduler()
    : owned_sequence_manager_(
          base::sequence_manager::CreateUnboundSequenceManager(
              base::sequence_manager::SequenceManager::Settings::Builder()
                  .SetMessagePumpType(base::MessagePumpType::UI)
                  .Build())),
      task_queues_(BrowserThread::UI, owned_sequence_manager_.get()),
      handle_(task_queues_.GetHandle()) {
  CommonSequenceManagerSetup(owned_sequence_manager_.get());
  owned_sequence_manager_->SetDefaultTaskRunner(
      handle_->GetDefaultTaskRunner());

  owned_sequence_manager_->BindToMessagePump(
      base::MessagePump::Create(base::MessagePumpType::UI));
}

BrowserUIThreadScheduler::BrowserUIThreadScheduler(
    base::sequence_manager::SequenceManager* sequence_manager)
    : task_queues_(BrowserThread::UI, sequence_manager),
      handle_(task_queues_.GetHandle()) {
  CommonSequenceManagerSetup(sequence_manager);
}

void BrowserUIThreadScheduler::CommonSequenceManagerSetup(
    base::sequence_manager::SequenceManager* sequence_manager) {
  sequence_manager->EnableCrashKeys("ui_scheduler_async_stack");
}

BrowserUIThreadScheduler::UserInputActiveHandle
BrowserUIThreadScheduler::OnUserInputStart() {
  return BrowserUIThreadScheduler::UserInputActiveHandle(this);
}

void BrowserUIThreadScheduler::DidStartUserInput() {
  if (!browser_prioritize_native_work_ ||
      browser_prioritize_native_work_after_input_end_ms_.is_inf()) {
    return;
  }
  owned_sequence_manager_->PrioritizeYieldingToNative(base::TimeTicks::Max());
}

void BrowserUIThreadScheduler::DidEndUserInput() {
  if (!browser_prioritize_native_work_ ||
      browser_prioritize_native_work_after_input_end_ms_.is_inf()) {
    return;
  }
  owned_sequence_manager_->PrioritizeYieldingToNative(
      base::TimeTicks::Now() +
      browser_prioritize_native_work_after_input_end_ms_);
}

void BrowserUIThreadScheduler::PostFeatureListSetup() {
  if (!base::FeatureList::IsEnabled(features::kBrowserPrioritizeNativeWork)) {
    return;
  }
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

}  // namespace content

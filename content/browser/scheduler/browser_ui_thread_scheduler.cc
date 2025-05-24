// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
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
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/scheduler/browser_task_priority.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

namespace {

content::BrowserUIThreadScheduler* g_browser_ui_thread_scheduler = nullptr;

}  // namespace
namespace content {

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
                  .SetCanRunTasksByBatches(true)
                  .SetPrioritySettings(
                      internal::CreateBrowserTaskPrioritySettings())
                  .SetShouldSampleCPUTime(true)
                  .Build())),
      task_queues_(BrowserThread::UI, owned_sequence_manager_.get()),
      handle_(task_queues_.GetHandle()) {
  task_queues_.SetOnTaskCompletedHandler(base::BindRepeating(
      &BrowserUIThreadScheduler::OnTaskCompleted, base::Unretained(this)));
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

void BrowserUIThreadScheduler::OnTaskCompleted(
    const base::sequence_manager::Task& task,
    base::sequence_manager::TaskQueue::TaskTiming* task_timing,
    base::LazyNow* lazy_now) {
  // Note: Thread time is already subsampled in sequence manager by a factor of
  // |kTaskSamplingRateForRecordingCPUTime|. So browser main can piggy back on
  // that subsampling to record histograms without fear of oversampling.
  task_timing->RecordTaskEnd(lazy_now);
  task_timing->RecordUmaOnCpuMetrics("BrowserScheduler.UIThread");
}

}  // namespace content

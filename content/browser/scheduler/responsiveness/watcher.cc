// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/watcher.h"

#include "base/bind.h"
#include "base/pending_task.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/scheduler/responsiveness/calculator.h"
#include "content/browser/scheduler/responsiveness/message_loop_observer.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace responsiveness {

Watcher::Metadata::Metadata(const void* identifier) : identifier(identifier) {}


std::unique_ptr<Calculator> Watcher::CreateCalculator() {
  return std::make_unique<Calculator>();
}

std::unique_ptr<MetricSource> Watcher::CreateMetricSource() {
  return std::make_unique<MetricSource>(this);
}

void Watcher::WillRunTaskOnUIThread(const base::PendingTask* task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  WillRunTask(task, &currently_running_metadata_ui_);
}

void Watcher::DidRunTaskOnUIThread(const base::PendingTask* task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // It's safe to use base::Unretained since the callback will be synchronously
  // invoked.
  TaskOrEventFinishedCallback callback =
      base::BindOnce(&Calculator::TaskOrEventFinishedOnUIThread,
                     base::Unretained(calculator_.get()));

  DidRunTask(task, &currently_running_metadata_ui_,
             &mismatched_task_identifiers_ui_, std::move(callback));
}

void Watcher::WillRunTaskOnIOThread(const base::PendingTask* task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  WillRunTask(task, &currently_running_metadata_io_);
}

void Watcher::DidRunTaskOnIOThread(const base::PendingTask* task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // It's safe to use base::Unretained since the callback will be synchronously
  // invoked.
  TaskOrEventFinishedCallback callback =
      base::BindOnce(&Calculator::TaskOrEventFinishedOnIOThread,
                     base::Unretained(calculator_io_));
  DidRunTask(task, &currently_running_metadata_io_,
             &mismatched_task_identifiers_io_, std::move(callback));
}

void Watcher::WillRunTask(const base::PendingTask* task,
                          std::vector<Metadata>* currently_running_metadata) {
  // Reentrancy should be rare.
  if (UNLIKELY(!currently_running_metadata->empty())) {
    currently_running_metadata->back().caused_reentrancy = true;
  }

  currently_running_metadata->emplace_back(task);

  // For delayed tasks, record the time right before the task is run.
  if (!task->delayed_run_time.is_null()) {
    currently_running_metadata->back().execution_start_time =
        base::TimeTicks::Now();
  }
}

void Watcher::DidRunTask(const base::PendingTask* task,
                         std::vector<Metadata>* currently_running_metadata,
                         int* mismatched_task_identifiers,
                         TaskOrEventFinishedCallback callback) {
  // Calls to DidRunTask should always be paired with WillRunTask. The only time
  // the identifier should differ is when Watcher is first constructed. The
  // TaskRunner Observers may be added while a task is being run, which means
  // that there was no corresponding WillRunTask.
  if (UNLIKELY(currently_running_metadata->empty() ||
               (task != currently_running_metadata->back().identifier))) {
    *mismatched_task_identifiers += 1;
    // Mismatches can happen (e.g: on ozone/wayland when Paste button is pressed
    // in context menus, among others). Simply ignore the mismatches for now.
    // See https://crbug.com/929813 for the details of why the mismatch
    // happens.
#if !defined(OS_CHROMEOS) && defined(OS_LINUX) && defined(USE_OZONE)
    return currently_running_metadata_ui_.clear();
#endif
    DCHECK_LE(*mismatched_task_identifiers, 1);
    return;
  }

  bool caused_reentrancy = currently_running_metadata->back().caused_reentrancy;
  base::TimeTicks execution_start_time =
      currently_running_metadata->back().execution_start_time;
  currently_running_metadata->pop_back();

  // Ignore tasks that caused reentrancy, since their execution latency will
  // be very large, but Chrome was still responsive.
  if (UNLIKELY(caused_reentrancy))
    return;

  // For delayed tasks, measure the duration of the task itself, rather than the
  // duration from schedule time to finish time.
  base::TimeTicks schedule_time;
  if (execution_start_time.is_null()) {
    // Tasks which were posted before the MessageLoopObserver was created will
    // not have a queue_time, and should be ignored. This doesn't affect delayed
    // tasks.
    if (UNLIKELY(task->queue_time.is_null()))
      return;

    schedule_time = task->queue_time;
  } else {
    schedule_time = execution_start_time;
  }

  std::move(callback).Run(schedule_time, base::TimeTicks::Now());
}

void Watcher::WillRunEventOnUIThread(const void* opaque_identifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Reentrancy should be rare.
  if (UNLIKELY(!currently_running_metadata_ui_.empty())) {
    currently_running_metadata_ui_.back().caused_reentrancy = true;
  }

  currently_running_metadata_ui_.emplace_back(opaque_identifier);
  currently_running_metadata_ui_.back().execution_start_time =
      base::TimeTicks::Now();
}

void Watcher::DidRunEventOnUIThread(const void* opaque_identifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Calls to DidRunEventOnUIThread should always be paired with
  // WillRunEventOnUIThread. The only time the identifier should differ is when
  // Watcher is first constructed. The TaskRunner Observers may be added while a
  // task is being run, which means that there was no corresponding WillRunTask.
  if (UNLIKELY(currently_running_metadata_ui_.empty() ||
               (opaque_identifier !=
                currently_running_metadata_ui_.back().identifier))) {
    mismatched_event_identifiers_ui_ += 1;
    // See comment in DidRunTask() for why |currently_running_metadata_ui_| may
    // be reset.
#if !defined(OS_CHROMEOS) && defined(OS_LINUX) && defined(USE_OZONE)
    return currently_running_metadata_ui_.clear();
#endif
    DCHECK_LE(mismatched_event_identifiers_ui_, 1);
    return;
  }

  bool caused_reentrancy =
      currently_running_metadata_ui_.back().caused_reentrancy;
  base::TimeTicks execution_start_time =
      currently_running_metadata_ui_.back().execution_start_time;
  currently_running_metadata_ui_.pop_back();

  // Ignore events that caused reentrancy, since their execution latency will
  // be very large, but Chrome was still responsive.
  if (UNLIKELY(caused_reentrancy))
    return;

  calculator_->TaskOrEventFinishedOnUIThread(execution_start_time,
                                             base::TimeTicks::Now());
}

Watcher::Watcher() = default;
Watcher::~Watcher() = default;

void Watcher::SetUp() {
  // Set up |calculator_| before |metric_source_| because SetUpOnIOThread()
  // uses |calculator_|.
  calculator_ = CreateCalculator();
  currently_running_metadata_ui_.reserve(5);

  metric_source_ = CreateMetricSource();
  metric_source_->SetUp();
}

void Watcher::Destroy() {
  // This holds a ref to |this| until the destroy flow completes.
  base::ScopedClosureRunner on_destroy_complete(base::BindOnce(
      &Watcher::FinishDestroyMetricSource, base::RetainedRef(this)));

  metric_source_->Destroy(std::move(on_destroy_complete));
}

void Watcher::SetUpOnIOThread() {
  currently_running_metadata_io_.reserve(5);
  DCHECK(calculator_.get());
  calculator_io_ = calculator_.get();
}

void Watcher::TearDownOnUIThread() {}

void Watcher::FinishDestroyMetricSource() {
  metric_source_ = nullptr;
}

void Watcher::TearDownOnIOThread() {
  calculator_io_ = nullptr;
}

}  // namespace responsiveness
}  // namespace content

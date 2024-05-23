// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/watcher.h"

#include <variant>

#include "base/cpu_reduction_experiment.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/pending_task.h"
#include "base/power_monitor/power_monitor.h"
#include "build/build_config.h"
#include "content/browser/scheduler/responsiveness/calculator.h"
#include "content/browser/scheduler/responsiveness/message_loop_observer.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {
namespace responsiveness {

Watcher::Metadata::Metadata(const void* identifier,
                            bool was_blocked_or_low_priority,
                            base::TimeTicks execution_start_time)
    : identifier(identifier),
      was_blocked_or_low_priority(was_blocked_or_low_priority),
      execution_start_time(execution_start_time) {}

std::unique_ptr<Calculator> Watcher::CreateCalculator() {
  return std::make_unique<Calculator>(
      GetContentClient()->browser()->CreateResponsivenessCalculatorDelegate());
}

std::unique_ptr<MetricSource> Watcher::CreateMetricSource() {
  return std::make_unique<MetricSource>(this);
}

void Watcher::WillRunTaskOnUIThread(const base::PendingTask* task,
                                    bool was_blocked_or_low_priority) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  WillRunTask(task, was_blocked_or_low_priority,
              &currently_running_metadata_ui_);
}

void Watcher::DidRunTaskOnUIThread(const base::PendingTask* task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::IsRunningCpuReductionExperiment()) {
    // Capturing `this` is safe because the callback is invoked synchronously by
    // `DidRunTask()`.
    auto lambda = [this](base::TimeTicks queue_time,
                         base::TimeTicks execution_start_time,
                         base::TimeTicks execution_finish_time) {
      calculator_->TaskOrEventFinishedOnUIThread(
          queue_time, execution_start_time, execution_finish_time);
    };
    DidRunTask(task, &currently_running_metadata_ui_,
               &mismatched_task_identifiers_ui_, lambda);
  } else {
    // Unretained() is safe because the callback is invoked synchronously by
    // `DidRunTask()`.
    auto callback = base::BindOnce(&Calculator::TaskOrEventFinishedOnUIThread,
                                   base::Unretained(calculator_.get()));
    DidRunTask(task, &currently_running_metadata_ui_,
               &mismatched_task_identifiers_ui_, std::move(callback));
  }
}

void Watcher::WillRunTaskOnIOThread(const base::PendingTask* task,
                                    bool was_blocked_or_low_priority) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  WillRunTask(task, was_blocked_or_low_priority,
              &currently_running_metadata_io_);
}

void Watcher::DidRunTaskOnIOThread(const base::PendingTask* task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (base::IsRunningCpuReductionExperiment()) {
    // Capturing `this` is safe because the callback is invoked synchronously by
    // `DidRunTask()`.
    auto lambda = [this](base::TimeTicks queue_time,
                         base::TimeTicks execution_start_time,
                         base::TimeTicks execution_finish_time) {
      calculator_io_->TaskOrEventFinishedOnIOThread(
          queue_time, execution_start_time, execution_finish_time);
    };
    DidRunTask(task, &currently_running_metadata_io_,
               &mismatched_task_identifiers_io_, lambda);
  } else {
    // Unretained() is safe because the callback is invoked synchronously by
    // `DidRunTask()` below.
    auto callback = base::BindOnce(&Calculator::TaskOrEventFinishedOnIOThread,
                                   base::Unretained(calculator_io_));
    DidRunTask(task, &currently_running_metadata_io_,
               &mismatched_task_identifiers_io_, std::move(callback));
  }
}

void Watcher::WillRunTask(const base::PendingTask* task,
                          bool was_blocked_or_low_priority,
                          std::vector<Metadata>* currently_running_metadata) {
  // Reentrancy should be rare.
  if (!currently_running_metadata->empty()) [[unlikely]] {
    currently_running_metadata->back().caused_reentrancy = true;
  }

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  currently_running_metadata->emplace_back(task, was_blocked_or_low_priority,
                                           execution_start_time);
}

void Watcher::DidRunTask(const base::PendingTask* task,
                         std::vector<Metadata>* currently_running_metadata,
                         int* mismatched_task_identifiers,
                         TaskOrEventFinishedCallback callback) {
  // Calls to DidRunTask should always be paired with WillRunTask. The only time
  // the identifier should differ is when Watcher is first constructed. The
  // TaskRunner Observers may be added while a task is being run, which means
  // that there was no corresponding WillRunTask.
  if (currently_running_metadata->empty() ||
      (task != currently_running_metadata->back().identifier)) [[unlikely]] {
    *mismatched_task_identifiers += 1;
    // Mismatches can happen, so just ignore them for now. See
    // https://crbug.com/929813 and https://crbug.com/931874 for details.
    return currently_running_metadata->clear();
  }

  const Metadata metadata = currently_running_metadata->back();
  currently_running_metadata->pop_back();

  // Ignore tasks that caused reentrancy, since their execution latency will
  // be very large, but Chrome was still responsive.
  if (metadata.caused_reentrancy) [[unlikely]] {
    return;
  }

  // Immediate tasks which were posted before the MessageLoopObserver was
  // created will not have a queue_time nor a delayed run time, and should be
  // ignored.
  if (task->queue_time.is_null() && task->delayed_run_time.is_null())
      [[unlikely]] {
    return;
  }

  // For delayed tasks and tasks that were blocked or low priority, pretend that
  // the queuing duration is zero. It is normal to have long queueing time for
  // these tasks, so it shouldn't be used to measure jank.
  const bool is_delayed_task = !task->delayed_run_time.is_null();
  const base::TimeTicks queue_time =
      is_delayed_task || metadata.was_blocked_or_low_priority
          ? metadata.execution_start_time
          : task->queue_time;
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();

  DCHECK(!queue_time.is_null());
  DCHECK(!metadata.execution_start_time.is_null());
  DCHECK(!execution_finish_time.is_null());
  DCHECK_LE(queue_time, metadata.execution_start_time);
  DCHECK_LE(metadata.execution_start_time, execution_finish_time);

  absl::visit(
      base::Overloaded{
          [&](base::FunctionRef<TaskOrEventFinishedSignature>& function_ref) {
            function_ref(queue_time, metadata.execution_start_time,
                         execution_finish_time);
          },
          [&](base::OnceCallback<TaskOrEventFinishedSignature>& base_callback) {
            std::move(base_callback)
                .Run(queue_time, metadata.execution_start_time,
                     execution_finish_time);
          }},
      callback);
}

void Watcher::WillRunEventOnUIThread(const void* opaque_identifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Reentrancy should be rare.
  if (!currently_running_metadata_ui_.empty()) [[unlikely]] {
    currently_running_metadata_ui_.back().caused_reentrancy = true;
  }

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  currently_running_metadata_ui_.emplace_back(
      opaque_identifier, /* was_blocked_or_low_priority= */ false,
      execution_start_time);
}

void Watcher::DidRunEventOnUIThread(const void* opaque_identifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Calls to DidRunEventOnUIThread should always be paired with
  // WillRunEventOnUIThread. The only time the identifier should differ is when
  // Watcher is first constructed. The TaskRunner Observers may be added while a
  // task is being run, which means that there was no corresponding WillRunTask.
  if (currently_running_metadata_ui_.empty() ||
      (opaque_identifier != currently_running_metadata_ui_.back().identifier))
      [[unlikely]] {
    mismatched_event_identifiers_ui_ += 1;
    // See comment in DidRunTask() for why |currently_running_metadata_ui_| may
    // be reset.
    return currently_running_metadata_ui_.clear();
  }

  const bool caused_reentrancy =
      currently_running_metadata_ui_.back().caused_reentrancy;
  const base::TimeTicks execution_start_time =
      currently_running_metadata_ui_.back().execution_start_time;
  currently_running_metadata_ui_.pop_back();

  // Ignore events that caused reentrancy, since their execution latency will
  // be very large, but Chrome was still responsive.
  if (caused_reentrancy) [[unlikely]] {
    return;
  }

  const base::TimeTicks queue_time = execution_start_time;
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();
  calculator_->TaskOrEventFinishedOnUIThread(queue_time, execution_start_time,
                                             execution_finish_time);
}

Watcher::Watcher() = default;
Watcher::~Watcher() = default;

void Watcher::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Set up |calculator_| before |metric_source_| because SetUpOnIOThread()
  // uses |calculator_|.
  calculator_ = CreateCalculator();
  currently_running_metadata_ui_.reserve(5);

  metric_source_ = CreateMetricSource();
  metric_source_->SetUp();
}

void Watcher::Destroy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This holds a ref to |this| until the destroy flow completes.
  base::ScopedClosureRunner on_destroy_complete(base::BindOnce(
      &Watcher::FinishDestroyMetricSource, base::RetainedRef(this)));

  metric_source_->Destroy(std::move(on_destroy_complete));
}

void Watcher::OnFirstIdle() {
  calculator_->OnFirstIdle();
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

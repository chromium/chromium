// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/watcher.h"

#include "base/pending_task.h"
#include "base/task/post_task.h"
#include "content/browser/scheduler/responsiveness/calculator.h"
#include "content/browser/scheduler/responsiveness/message_loop_observer.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace responsiveness {

Watcher::Metadata::Metadata(const void* identifier) : identifier(identifier) {}

Watcher::Watcher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void Watcher::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Destroy() has the corresponding call to Release().
  // We need this additional reference to make sure the object stays alive
  // through hops to the IO thread, which are necessary both during construction
  // and destruction.
  AddRef();

  calculator_ = CreateCalculator();
  native_event_observer_ui_ = CreateNativeEventObserver();
  currently_running_metadata_ui_.reserve(5);

  RegisterMessageLoopObserverUI();

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&Watcher::SetUpOnIOThread, base::Unretained(this),
                     calculator_.get()));
}

void Watcher::Destroy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(!destroy_was_called_);
  destroy_was_called_ = true;

  message_loop_observer_ui_.reset();
  native_event_observer_ui_.reset();

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&Watcher::TearDownOnIOThread, base::Unretained(this)));
}

std::unique_ptr<Calculator> Watcher::CreateCalculator() {
  return std::make_unique<Calculator>();
}

std::unique_ptr<NativeEventObserver> Watcher::CreateNativeEventObserver() {
  NativeEventObserver::WillRunEventCallback will_run_callback =
      base::BindRepeating(&Watcher::WillRunEventOnUIThread,
                          base::Unretained(this));
  NativeEventObserver::DidRunEventCallback did_run_callback =
      base::BindRepeating(&Watcher::DidRunEventOnUIThread,
                          base::Unretained(this));
  return std::make_unique<NativeEventObserver>(std::move(will_run_callback),
                                               std::move(did_run_callback));
}

Watcher::~Watcher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(destroy_was_called_);
}

void Watcher::RegisterMessageLoopObserverUI() {
  // We must use base::Unretained(this) to prevent ownership cycle.
  MessageLoopObserver::TaskCallback will_run_callback = base::BindRepeating(
      &Watcher::WillRunTaskOnUIThread, base::Unretained(this));
  MessageLoopObserver::TaskCallback did_run_callback = base::BindRepeating(
      &Watcher::DidRunTaskOnUIThread, base::Unretained(this));
  message_loop_observer_ui_.reset(new MessageLoopObserver(
      std::move(will_run_callback), std::move(did_run_callback)));
}

void Watcher::RegisterMessageLoopObserverIO() {
  // We must use base::Unretained(this) to prevent ownership cycle.
  MessageLoopObserver::TaskCallback will_run_callback = base::BindRepeating(
      &Watcher::WillRunTaskOnIOThread, base::Unretained(this));
  MessageLoopObserver::TaskCallback did_run_callback = base::BindRepeating(
      &Watcher::DidRunTaskOnIOThread, base::Unretained(this));
  message_loop_observer_io_.reset(new MessageLoopObserver(
      std::move(will_run_callback), std::move(did_run_callback)));
}

void Watcher::SetUpOnIOThread(Calculator* calculator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  currently_running_metadata_io_.reserve(5);
  RegisterMessageLoopObserverIO();
  calculator_io_ = calculator;
}

void Watcher::TearDownOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  message_loop_observer_io_.reset();

  calculator_io_ = nullptr;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Watcher::TearDownOnUIThread, base::Unretained(this)));
}

void Watcher::TearDownOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Corresponding call to AddRef() is in the constructor.
  Release();
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
    if (UNLIKELY(!task->queue_time))
      return;

    schedule_time = task->queue_time.value();
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

}  // namespace responsiveness
}  // namespace content

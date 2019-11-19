// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/jank_monitor.h"

#include "base/compiler_specific.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ui_base_features.h"

namespace content {
namespace responsiveness {

// Interval of the monitor performing jankiness checks against the watched
// threads.
static constexpr int64_t kMonitorCheckIntervalMs = 500;
// A task running for longer than |kJankThresholdMs| is considered janky.
static constexpr int64_t kJankThresholdMs = 1000;
// The threshold (10 sec) for shutting down the monitor timer, in microseconds.
static constexpr int64_t kInactivityThresholdUs =
    10 * base::TimeTicks::kMicrosecondsPerSecond;

JankMonitor::Observer::~Observer() = default;

JankMonitor::JankMonitor()
    : timer_running_(false),
      janky_task_id_(nullptr),
      last_activity_time_us_(0) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(monitor_sequence_checker_);
}

JankMonitor::~JankMonitor() = default;

void JankMonitor::AddObserver(Observer* observer) {
  base::AutoLock auto_lock(observers_lock_);
  observers_.AddObserver(observer);
}

void JankMonitor::RemoveObserver(Observer* observer) {
  base::AutoLock auto_lock(observers_lock_);
  observers_.RemoveObserver(observer);
}

void JankMonitor::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Dependencies in SetUp() and Destroy():
  // * Target thread --(may schedule the timer on)--> Monitor thread.
  // * Monitor thread --(read/write)--> ThreadExecutionState data members.
  // * Target thread --(write)--> ThreadExecutionState data members.

  // ThreadExecutionState data members are created first.
  ui_thread_exec_state_ = std::make_unique<ThreadExecutionState>();
  io_thread_exec_state_ = std::make_unique<ThreadExecutionState>();

  // Then the monitor thread.
  monitor_task_runner_ = base::CreateSequencedTaskRunner({base::ThreadPool()});

  // Finally set up the MetricSource.
  metric_source_ = CreateMetricSource();
  metric_source_->SetUp();
}

void JankMonitor::Destroy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Destroy() tears down the object in reverse order of SetUp(): destroy the
  // MetricSource first to silence the WillRun/DidRun callbacks. Then the
  // monitor timer is stopped. Finally |ui_thread_exec_state_| and
  // |io_thread_exec_state_| can be safely destroyed.

  // This holds a reference to |this| until |metric_source_| finishes async
  // shutdown.
  base::ScopedClosureRunner finish_destroy_metric_source(base::BindOnce(
      &JankMonitor::FinishDestroyMetricSource, base::RetainedRef(this)));
  metric_source_->Destroy(std::move(finish_destroy_metric_source));
}

void JankMonitor::FinishDestroyMetricSource() {
  // Destruction of MetricSource takes place on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  metric_source_ = nullptr;

  // We won't receive any RullRun* or DidRun* callbacks. Now shut down the
  // monitor thread.
  monitor_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JankMonitor::DestroyOnMonitorThread,
                                base::RetainedRef(this)));
}

void JankMonitor::SetUpOnIOThread() {}

void JankMonitor::TearDownOnUIThread() {
  // Don't destroy |ui_thread_exec_state_| yet because it might be used if the
  // monitor timer runs.
}

void JankMonitor::TearDownOnIOThread() {
  // Don't destroy |io_thread_exec_state_| yet because it might be used if the
  // monitor timer fires.
}

void JankMonitor::WillRunTaskOnUIThread(const base::PendingTask* task) {
  DCHECK(ui_thread_exec_state_);
  WillRunTaskOrEvent(ui_thread_exec_state_.get(), task);
}

void JankMonitor::DidRunTaskOnUIThread(const base::PendingTask* task) {
  DCHECK(ui_thread_exec_state_);
  DidRunTaskOrEvent(ui_thread_exec_state_.get(), task);
}

void JankMonitor::WillRunTaskOnIOThread(const base::PendingTask* task) {
  DCHECK(io_thread_exec_state_);
  WillRunTaskOrEvent(io_thread_exec_state_.get(), task);
}

void JankMonitor::DidRunTaskOnIOThread(const base::PendingTask* task) {
  DCHECK(io_thread_exec_state_);
  DidRunTaskOrEvent(io_thread_exec_state_.get(), task);
}

void JankMonitor::WillRunEventOnUIThread(const void* opaque_identifier) {
  DCHECK(ui_thread_exec_state_);
  WillRunTaskOrEvent(ui_thread_exec_state_.get(), opaque_identifier);
}

void JankMonitor::DidRunEventOnUIThread(const void* opaque_identifier) {
  DCHECK(ui_thread_exec_state_);
  DidRunTaskOrEvent(ui_thread_exec_state_.get(), opaque_identifier);
}

void JankMonitor::WillRunTaskOrEvent(ThreadExecutionState* thread_exec_state,
                                     const void* opaque_identifier) {
  thread_exec_state->WillRunTaskOrEvent(opaque_identifier);
  if (!timer_running_) {
    monitor_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&JankMonitor::StartTimerIfNecessary,
                                  base::RetainedRef(this)));
  }
}

void JankMonitor::DidRunTaskOrEvent(ThreadExecutionState* thread_exec_state,
                                    const void* opaque_identifier) {
  thread_exec_state->DidRunTaskOrEvent(opaque_identifier);
  NotifyJankStopIfNecessary(opaque_identifier);

  // This might lead to concurrent writes to |last_activity_time_us_|. Either
  // write is fine, and we don't require it to be monotonically increasing.
  last_activity_time_us_ =
      (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
}

void JankMonitor::StartTimerIfNecessary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);
  DCHECK_EQ(timer_.IsRunning(), timer_running_);

  // Already running. Maybe both UI and IO threads saw the timer stopped, and
  // one attempt has already succeeded.
  if (timer_.IsRunning())
    return;

  static base::TimeDelta monitor_check_interval =
      base::TimeDelta::FromMilliseconds(kMonitorCheckIntervalMs);
  // RepeatingClosure bound to the timer doesn't hold a ref to |this| because
  // the ref will only be released on timer destruction.
  timer_.Start(FROM_HERE, monitor_check_interval,
               base::BindRepeating(&JankMonitor::OnCheckJankiness,
                                   base::Unretained(this)));
  timer_running_ = true;
}

void JankMonitor::StopTimerIfIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);
  DCHECK(timer_.IsRunning());

  auto now_us = (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
  if (now_us - last_activity_time_us_ < kInactivityThresholdUs)
    return;

  timer_.Stop();
  timer_running_ = false;
}

std::unique_ptr<MetricSource> JankMonitor::CreateMetricSource () {
  return std::make_unique<MetricSource>(this);
}

void JankMonitor::DestroyOnMonitorThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);

  timer_.AbandonAndStop();
  timer_running_ = false;

  // This is the last step of shutdown: no tasks will be observed on either IO
  // or IO thread, and the monitor timer is stopped. It's safe to destroy
  // |ui_thread_exec_state_| and |io_thread_exec_state_| now.
  ui_thread_exec_state_ = nullptr;
  io_thread_exec_state_ = nullptr;
}

bool JankMonitor::timer_running() const {
  return timer_running_;
}

void JankMonitor::OnCheckJankiness() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);

  if (janky_task_id_) {
    return;
  }

  auto task_id = ui_thread_exec_state_->CheckJankiness();
  if (task_id.has_value()) {
    OnJankStarted(*task_id);
    return;
  }

  DCHECK(!janky_task_id_);
  // Jankiness is checked in the order of UI, IO thread.
  task_id = io_thread_exec_state_->CheckJankiness();
  if (task_id.has_value()) {
    OnJankStarted(*task_id);
    return;
  }

  DCHECK(!janky_task_id_);
  StopTimerIfIdle();
}

void JankMonitor::OnJankStarted(const void* opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);

  janky_task_id_ = opaque_identifier;

  base::AutoLock auto_lock(observers_lock_);
  for (Observer& observer : observers_)
    observer.OnJankStarted();
}

void JankMonitor::OnJankStopped(const void* opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);
  DCHECK_NE(opaque_identifier, nullptr);
  if (janky_task_id_ != opaque_identifier)
    return;

  janky_task_id_ = nullptr;

  base::AutoLock auto_lock(observers_lock_);
  for (Observer& observer : observers_)
    observer.OnJankStopped();
}

void JankMonitor::NotifyJankStopIfNecessary(const void* opaque_identifier) {
  if (LIKELY(!janky_task_id_ || janky_task_id_ != opaque_identifier)) {
    // Most tasks are unlikely to be janky.
    return;
  }

  monitor_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JankMonitor::OnJankStopped,
                                base::RetainedRef(this), opaque_identifier));
}

JankMonitor::ThreadExecutionState::TaskMetadata::~TaskMetadata() = default;

JankMonitor::ThreadExecutionState::ThreadExecutionState() {
  // Constructor is always on the UI thread. Detach |target_sequence_checker_|
  // to make it work on IO thread.
  DETACH_FROM_SEQUENCE(target_sequence_checker_);
  DETACH_FROM_SEQUENCE(monitor_sequence_checker_);
}

JankMonitor::ThreadExecutionState::~ThreadExecutionState() = default;

base::Optional<const void*>
JankMonitor::ThreadExecutionState::CheckJankiness() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);

  base::TimeTicks now = base::TimeTicks::Now();
  static base::TimeDelta jank_threshold =
      base::TimeDelta::FromMilliseconds(kJankThresholdMs);

  base::AutoLock lock(lock_);
  if (LIKELY(task_execution_metadata_.empty() ||
             (now - task_execution_metadata_.front().execution_start_time) <
                 jank_threshold)) {
    // Most tasks are unlikely to be janky.
    return base::nullopt;
  }

  // Mark that the target thread is janky and notify the monitor thread.
  return task_execution_metadata_.front().identifier;
}

void JankMonitor::ThreadExecutionState::WillRunTaskOrEvent(
    const void* opaque_identifier) {
  AssertOnTargetThread();

  base::TimeTicks now = base::TimeTicks::Now();

  base::AutoLock lock(lock_);
  task_execution_metadata_.emplace_back(now, opaque_identifier);
}

void JankMonitor::ThreadExecutionState::DidRunTaskOrEvent(
    const void* opaque_identifier) {
  AssertOnTargetThread();

  base::AutoLock lock(lock_);
  if (UNLIKELY(task_execution_metadata_.empty()) ||
      opaque_identifier != task_execution_metadata_.back().identifier) {
    // Mismatches can happen (e.g: on ozone/wayland when Paste button is pressed
    // in context menus, among others). Simply ignore the mismatches for now.
    // See https://crbug.com/929813 for the details of why the mismatch
    // happens.
#if !defined(OS_CHROMEOS) && defined(OS_LINUX) && defined(USE_OZONE)
    task_execution_metadata_.clear();
#endif
    return;
  }

  task_execution_metadata_.pop_back();
}

void JankMonitor::ThreadExecutionState::AssertOnTargetThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(target_sequence_checker_);
}

}  // namespace responsiveness.
}  // namespace content.

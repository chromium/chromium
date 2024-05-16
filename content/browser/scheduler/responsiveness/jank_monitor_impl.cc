// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/jank_monitor_impl.h"

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ui_base_features.h"

namespace content {

JankMonitor::~JankMonitor() = default;

JankMonitor::Observer::~Observer() = default;

// static
scoped_refptr<JankMonitor> JankMonitor::Create() {
  return base::MakeRefCounted<responsiveness::JankMonitorImpl>();
}

namespace responsiveness {

// Interval of the monitor performing jankiness checks against the watched
// threads.
static constexpr int64_t kMonitorCheckIntervalMs = 500;
// A task running for longer than |kJankThresholdMs| is considered janky.
static constexpr int64_t kJankThresholdMs = 1000;
// The threshold (10 sec) for shutting down the monitor timer, in microseconds.
static constexpr int64_t kInactivityThresholdUs =
    10 * base::TimeTicks::kMicrosecondsPerSecond;

JankMonitorImpl::JankMonitorImpl()
    : timer_(std::make_unique<base::RepeatingTimer>()),
      timer_running_(false),
      janky_task_id_(nullptr),
      last_activity_time_us_(0) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(monitor_sequence_checker_);
}

JankMonitorImpl::~JankMonitorImpl() = default;

void JankMonitorImpl::AddObserver(content::JankMonitor::Observer* observer) {
  base::AutoLock auto_lock(observers_lock_);
  observers_.AddObserver(observer);
}

void JankMonitorImpl::RemoveObserver(content::JankMonitor::Observer* observer) {
  base::AutoLock auto_lock(observers_lock_);
  observers_.RemoveObserver(observer);
}

void JankMonitorImpl::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Dependencies in SetUp() and Destroy():
  // * Target thread --(may schedule the timer on)--> Monitor thread.
  // * Monitor thread --(read/write)--> ThreadExecutionState data members.
  // * Target thread --(write)--> ThreadExecutionState data members.

  // ThreadExecutionState data members are created first.
  ui_thread_exec_state_ = std::make_unique<ThreadExecutionState>();
  io_thread_exec_state_ = std::make_unique<ThreadExecutionState>();

  // Then the monitor thread.
  monitor_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});

  // Finally set up the MetricSource.
  metric_source_ = CreateMetricSource();
  metric_source_->SetUp();
}

void JankMonitorImpl::Destroy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Destroy shuts down the monitor timer and the metric source in parallel.
  // |timer_| is shut down and destroyed on the monitor thread. |metric_source_|
  // is destroyed in calling its Destroy() method. The shared data members,
  // |ui_thread_exec_state_| and |io_thread_exec_state_| is destroyed in the
  // JankMonitor dtor, which can happen on either the monitor or the UI thread.

  monitor_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&JankMonitorImpl::DestroyOnMonitorThread,
                                base::RetainedRef(this)));

  base::ScopedClosureRunner finish_destroy_metric_source(base::BindOnce(
      &JankMonitorImpl::FinishDestroyMetricSource, base::RetainedRef(this)));
  metric_source_->Destroy(std::move(finish_destroy_metric_source));
}

void JankMonitorImpl::FinishDestroyMetricSource() {
  // Destruction of MetricSource takes place on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  metric_source_ = nullptr;
}

void JankMonitorImpl::SetUpOnIOThread() {}

void JankMonitorImpl::TearDownOnUIThread() {
  // Don't destroy |ui_thread_exec_state_| yet because it might be used if the
  // monitor timer runs.
}

void JankMonitorImpl::TearDownOnIOThread() {
  // Don't destroy |io_thread_exec_state_| yet because it might be used if the
  // monitor timer fires.
}

void JankMonitorImpl::WillRunTaskOnUIThread(
    const base::PendingTask* task,
    bool /* was_blocked_or_low_priority */) {
  DCHECK(ui_thread_exec_state_);
  WillRunTaskOrEvent(ui_thread_exec_state_.get(), task);
}

void JankMonitorImpl::DidRunTaskOnUIThread(const base::PendingTask* task) {
  DCHECK(ui_thread_exec_state_);
  DidRunTaskOrEvent(ui_thread_exec_state_.get(), task);
}

void JankMonitorImpl::WillRunTaskOnIOThread(
    const base::PendingTask* task,
    bool /* was_blocked_or_low_priority */) {
  DCHECK(io_thread_exec_state_);
  WillRunTaskOrEvent(io_thread_exec_state_.get(), task);
}

void JankMonitorImpl::DidRunTaskOnIOThread(const base::PendingTask* task) {
  DCHECK(io_thread_exec_state_);
  DidRunTaskOrEvent(io_thread_exec_state_.get(), task);
}

void JankMonitorImpl::WillRunEventOnUIThread(const void* opaque_identifier) {
  DCHECK(ui_thread_exec_state_);
  WillRunTaskOrEvent(ui_thread_exec_state_.get(), opaque_identifier);
}

void JankMonitorImpl::DidRunEventOnUIThread(const void* opaque_identifier) {
  DCHECK(ui_thread_exec_state_);
  DidRunTaskOrEvent(ui_thread_exec_state_.get(), opaque_identifier);
}

void JankMonitorImpl::WillRunTaskOrEvent(
    ThreadExecutionState* thread_exec_state,
    const void* opaque_identifier) {
  thread_exec_state->WillRunTaskOrEvent(opaque_identifier);
  if (!timer_running_) {
    monitor_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&JankMonitorImpl::StartTimerIfNecessary,
                                  base::RetainedRef(this)));
  }
}

void JankMonitorImpl::DidRunTaskOrEvent(ThreadExecutionState* thread_exec_state,
                                        const void* opaque_identifier) {
  thread_exec_state->DidRunTaskOrEvent(opaque_identifier);
  NotifyJankStopIfNecessary(opaque_identifier);

  // This might lead to concurrent writes to |last_activity_time_us_|. Either
  // write is fine, and we don't require it to be monotonically increasing.
  last_activity_time_us_ =
      (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
}

void JankMonitorImpl::StartTimerIfNecessary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);

  // |timer_| is already destroyed. This function is posted from UI or IO thread
  // after Destroy() is called. Just do nothing.
  if (!timer_)
    return;

  DCHECK_EQ(timer_->IsRunning(), timer_running_);

  // Already running. Maybe both UI and IO threads saw the timer stopped, and
  // one attempt has already succeeded.
  if (timer_->IsRunning())
    return;

  static base::TimeDelta monitor_check_interval =
      base::Milliseconds(kMonitorCheckIntervalMs);
  // RepeatingClosure bound to the timer doesn't hold a ref to |this| because
  // the ref will only be released on timer destruction.
  timer_->Start(FROM_HERE, monitor_check_interval,
                base::BindRepeating(&JankMonitorImpl::OnCheckJankiness,
                                    base::Unretained(this)));
  timer_running_ = true;
}

void JankMonitorImpl::StopTimerIfIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);
  DCHECK(timer_->IsRunning());

  auto now_us = (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
  if (now_us - last_activity_time_us_ < kInactivityThresholdUs)
    return;

  timer_->Stop();
  timer_running_ = false;
}

std::unique_ptr<MetricSource> JankMonitorImpl::CreateMetricSource() {
  return std::make_unique<MetricSource>(this);
}

void JankMonitorImpl::DestroyOnMonitorThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);
  DCHECK(timer_);

  timer_->AbandonAndStop();
  timer_ = nullptr;
  timer_running_ = false;
}

bool JankMonitorImpl::timer_running() const {
  return timer_running_;
}

void JankMonitorImpl::OnCheckJankiness() {
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

void JankMonitorImpl::OnJankStarted(const void* opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);

  janky_task_id_ = opaque_identifier;

  base::AutoLock auto_lock(observers_lock_);
  for (content::JankMonitor::Observer& observer : observers_)
    observer.OnJankStarted();
}

void JankMonitorImpl::OnJankStopped(
    MayBeDangling<const void> opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);
  DCHECK_NE(opaque_identifier, nullptr);
  if (janky_task_id_ != opaque_identifier)
    return;

  janky_task_id_ = nullptr;

  base::AutoLock auto_lock(observers_lock_);
  for (content::JankMonitor::Observer& observer : observers_)
    observer.OnJankStopped();
}

void JankMonitorImpl::NotifyJankStopIfNecessary(const void* opaque_identifier) {
  if (!janky_task_id_ || janky_task_id_ != opaque_identifier) [[likely]] {
    // Most tasks are unlikely to be janky.
    return;
  }

  monitor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JankMonitorImpl::OnJankStopped, base::RetainedRef(this),
                     // It is relatively safe to have `UnsafeDangling` here
                     // because the ptr is only used as an identifier, and since
                     // the events should be coming in order, it is unlikely
                     // that we encounter issue with memory being reused.
                     base::UnsafeDangling(opaque_identifier)));
}

JankMonitorImpl::ThreadExecutionState::TaskMetadata::~TaskMetadata() = default;

JankMonitorImpl::ThreadExecutionState::ThreadExecutionState() {
  // Constructor is always on the UI thread. Detach |target_sequence_checker_|
  // to make it work on IO thread.
  DETACH_FROM_SEQUENCE(target_sequence_checker_);
  DETACH_FROM_SEQUENCE(monitor_sequence_checker_);
}

JankMonitorImpl::ThreadExecutionState::~ThreadExecutionState() = default;

std::optional<const void*>
JankMonitorImpl::ThreadExecutionState::CheckJankiness() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_sequence_checker_);

  base::TimeTicks now = base::TimeTicks::Now();
  static base::TimeDelta jank_threshold = base::Milliseconds(kJankThresholdMs);

  base::AutoLock lock(lock_);
  if (task_execution_metadata_.empty() ||
      (now - task_execution_metadata_.back().execution_start_time) <
          jank_threshold) [[likely]] {
    // Most tasks are unlikely to be janky.
    return std::nullopt;
  }

  // Mark that the target thread is janky and notify the monitor thread.
  return task_execution_metadata_.back().identifier;
}

void JankMonitorImpl::ThreadExecutionState::WillRunTaskOrEvent(
    const void* opaque_identifier) {
  AssertOnTargetThread();

  base::TimeTicks now = base::TimeTicks::Now();

  base::AutoLock lock(lock_);
  task_execution_metadata_.emplace_back(now, opaque_identifier);
}

void JankMonitorImpl::ThreadExecutionState::DidRunTaskOrEvent(
    const void* opaque_identifier) {
  AssertOnTargetThread();

  base::AutoLock lock(lock_);
  if (task_execution_metadata_.empty() ||
      opaque_identifier != task_execution_metadata_.back().identifier)
      [[unlikely]] {
    // Mismatches can happen (e.g: on ozone/wayland when Paste button is pressed
    // in context menus, among others). Simply ignore the mismatches for now.
    // See https://crbug.com/929813 for the details of why the mismatch
    // happens.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    BUILDFLAG(IS_OZONE)
    task_execution_metadata_.clear();
#endif
    return;
  }

  task_execution_metadata_.pop_back();
}

void JankMonitorImpl::ThreadExecutionState::AssertOnTargetThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(target_sequence_checker_);
}

}  // namespace responsiveness.
}  // namespace content.

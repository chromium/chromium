// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_JANK_MONITOR_IMPL_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_JANK_MONITOR_IMPL_H_

#include <atomic>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/scheduler/responsiveness/metric_source.h"
#include "content/common/content_export.h"
#include "content/public/browser/jank_monitor.h"

namespace content {
namespace responsiveness {

class CONTENT_EXPORT JankMonitorImpl : public content::JankMonitor,
                                       public MetricSource::Delegate {
 public:
  JankMonitorImpl();

  // JankMonitor implementation:
  void AddObserver(content::JankMonitor::Observer* observer) override;
  void RemoveObserver(content::JankMonitor::Observer* observer) override;
  void SetUp() override;
  void Destroy() override;

 protected:
  ~JankMonitorImpl() override;

  // MetricSource::Delegate implementation.
  void SetUpOnIOThread() override;
  void TearDownOnUIThread() override;
  void TearDownOnIOThread() override;

  void WillRunTaskOnUIThread(const base::PendingTask* task,
                             bool was_blocked_or_low_priority) override;
  void DidRunTaskOnUIThread(const base::PendingTask* task) override;

  void WillRunTaskOnIOThread(const base::PendingTask* task,
                             bool was_blocked_or_low_priority) override;
  void DidRunTaskOnIOThread(const base::PendingTask* task) override;

  void WillRunEventOnUIThread(const void* opaque_identifier) override;
  void DidRunEventOnUIThread(const void* opaque_identifier) override;

  // Exposed for tests
  virtual void DestroyOnMonitorThread();
  virtual void FinishDestroyMetricSource();
  virtual std::unique_ptr<MetricSource> CreateMetricSource();
  virtual void OnCheckJankiness();  // Timer callback.
  bool timer_running() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(JankinessJankMonitorTest, SetUpDestroy);

  class ThreadExecutionState final {
   public:
    ThreadExecutionState();
    ~ThreadExecutionState();

    void WillRunTaskOrEvent(const void* opaque_identifier);
    void DidRunTaskOrEvent(const void* opaque_identifier);

    // Checks the jankiness of the target thread. Returns the opaque identifier
    // of the janky task or std::nullopt if the current task is not janky.
    std::optional<const void*> CheckJankiness();
    void AssertOnTargetThread();

   private:
    // Synchronizes the access between the target thread and the monitor thread.
    base::Lock lock_;

    struct TaskMetadata {
      TaskMetadata(base::TimeTicks execution_start_time, const void* identifier)
          : execution_start_time(execution_start_time),
            identifier(identifier) {}
      ~TaskMetadata();

      base::TimeTicks execution_start_time;
      // RAW_PTR_EXCLUSION: Performance reasons: based on analysis of sampling
      // profiler data (JankMonitorImpl::WillRunTaskOrEvent ->
      // JankMonitorImpl::ThreadExecutionState::WillRunTaskOrEvent -> emplaces
      // TaskMetadata in a vector).
      RAW_PTR_EXCLUSION const void* identifier;
    };
    std::vector<TaskMetadata> task_execution_metadata_;

    // Checks some methods are called on the monitor thread.
    SEQUENCE_CHECKER(monitor_sequence_checker_);
    // Checks some methods are called on the target thread.
    SEQUENCE_CHECKER(target_sequence_checker_);
  };

  void AddObserverOnMonitorThread(Observer* observer);
  void RemoveObserverOnMonitorThread(Observer* observer);

  void WillRunTaskOrEvent(ThreadExecutionState* thread_exec_state,
                          const void* opaque_identifier);
  void DidRunTaskOrEvent(ThreadExecutionState* thread_exec_state,
                         const void* opaque_identifier);

  // Called in WillRunTaskOrEvent() to start the timer to monitor janks if
  // the timer is not running.
  void StartTimerIfNecessary();
  // Stops the timer on inactivity for longer than a threshold.
  void StopTimerIfIdle();

  // Sends out notifications.
  void OnJankStarted(const void* opaque_identifier);
  void OnJankStopped(MayBeDangling<const void> opaque_identifier);

  // Call in DidRunTaskOrEvent() to for notification of jank stops.
  void NotifyJankStopIfNecessary(const void* opaque_identifier);

  // The source that emits responsiveness events.
  std::unique_ptr<content::responsiveness::MetricSource> metric_source_;

  std::unique_ptr<ThreadExecutionState> ui_thread_exec_state_;
  std::unique_ptr<ThreadExecutionState> io_thread_exec_state_;

  // The timer that runs on the monitor sequence to perform periodic check of
  // janks.
  std::unique_ptr<base::RepeatingTimer> timer_;

  // |timer_running_| is equivalent to timer_.IsRunning() except that it is
  // thread-safe. It is checked in WillRunTaskOrEvent() (from UI or IO thread)
  // to start the timer if necessary. Always updated on the monitor thread to
  // mirror the result of timer_.IsRunning().
  std::atomic_bool timer_running_;

  // The opaque identifier of the janky task. Updated on the monitor thread when
  // a janky task is detected. Checked when a task finishes running on UI or IO
  // thread to notify observers (from the monitor thread) that the jank has
  // stopped.
  std::atomic<const void*> janky_task_id_;

  // The timestamp of last activity on either UI or IO thread. Checked on the
  // monitor thread for stopping the timer on inactivity. Updated on UI or IO
  // thread in DidRunTaskOrEvent().
  std::atomic<int64_t> last_activity_time_us_;

  // Use a dedicated sequence for watching jankiness.
  scoped_refptr<base::SequencedTaskRunner> monitor_task_runner_;

  // The lock synchronizes access the |observers| from AddObserver(),
  // RemoveObserver(), OnJankStarted() and OnJankStopped().
  base::Lock observers_lock_;
  base::ObserverList<Observer, /* check_empty = */ true>::Unchecked observers_;

  // Checks some methods are called on the monitor thread.
  SEQUENCE_CHECKER(monitor_sequence_checker_);
};

}  // namespace responsiveness.
}  // namespace content.

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_JANK_MONITOR_IMPL_H_

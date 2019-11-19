// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_JANK_MONITOR_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_JANK_MONITOR_H_

#include <atomic>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/scheduler/responsiveness/metric_source.h"

namespace content {
namespace responsiveness {
// This class monitors the responsiveness of the browser to notify the presence
// of janks to its observers. A jank is defined as a task or native event
// running for longer than a threshold on the UI or IO thread. An observer of
// this class is notified through the Observer interface on jank starts/stops so
// the observer can take actions (e.g. gather system-wide profile to capture the
// jank) *before* the janky task finishes execution. Notifications are sent on a
// dedicated sequence internal to this class so the observer needs to be careful
// with threading. For example, access to browser-related objects requires
// posting a task to the UI thread.
//
// Internally, a timer (bound to the monitor sequence) is used to perform
// periodic checks to decide the presence of janks. When a jank is detected, the
// monitor notifies its observers that a jank has started (through the
// Observer::OnJankStarted() method). The start of a jank is imprecise w.r.t.
// the jank threshold. When a janky task has finished execution, the monitor
// notifies the observers ASAP (through the Observer::OnJankStopped() method).
//
// Usage example:
//
// class Profiler : public Observer {
//  public:
//   void OnJankStarted() override; // Start the profiler.
//   void OnJankStopped() override; // Stop the profiler.
// }
// Profiler* profiler = ...;
//
// scoped_refptr<JankMonitor> monitor = base::MakeRefCounted<JankMonitor>();
// monitor->SetUp();
// monitor->AddObserver(profiler);
//
// (Then start receiving notifications in Profiler::OnJankStarted() and
// Profiler::OnJankStopped()).
class CONTENT_EXPORT JankMonitor
    : public base::RefCountedThreadSafe<JankMonitor>,
      public content::responsiveness::MetricSource::Delegate {
 public:
  // Interface for observing janky tasks from the monitor. Note that the
  // callbacks are called *off* the UI thread. Post a task to the UI thread is
  // necessary if you need to access browser-related objects.
  class CONTENT_EXPORT Observer {
   public:
    virtual ~Observer();

    virtual void OnJankStarted() = 0;
    virtual void OnJankStopped() = 0;
  };

  JankMonitor();

  void SetUp();
  void Destroy();

  // AddObserver() and RemoveObserver() can be called on any sequence, but the
  // notifications only take place on the monitor sequence. Note: do *not* call
  // AddObserver() or RemoveObserver() synchronously in the observer callbacks,
  // or undefined behavior will result.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  friend class base::RefCountedThreadSafe<JankMonitor>;
  ~JankMonitor() override;

  // MetricSource::Delegate implementation.
  void SetUpOnIOThread() override;
  void TearDownOnUIThread() override;
  void TearDownOnIOThread() override;

  void WillRunTaskOnUIThread(const base::PendingTask* task) override;
  void DidRunTaskOnUIThread(const base::PendingTask* task) override;

  void WillRunTaskOnIOThread(const base::PendingTask* task) override;
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
    // of the janky task or base::nullopt if the current task is not janky.
    base::Optional<const void*> CheckJankiness();
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
      const void* identifier;
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
  void OnJankStopped(const void* opaque_identifier);

  // Call in DidRunTaskOrEvent() to for notification of jank stops.
  void NotifyJankStopIfNecessary(const void* opaque_identifier);

  // The source that emits responsiveness events.
  std::unique_ptr<content::responsiveness::MetricSource> metric_source_;

  std::unique_ptr<ThreadExecutionState> ui_thread_exec_state_;
  std::unique_ptr<ThreadExecutionState> io_thread_exec_state_;

  // The timer that runs on the monitor sequence to perform periodic check of
  // janks.
  base::RepeatingTimer timer_;

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

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_JANK_MONITOR_H_

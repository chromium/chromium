// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_

#include <vector>

#include "base/power_monitor/power_observer.h"
#include "content/browser/scheduler/responsiveness/metric_source.h"

namespace content {
namespace responsiveness {

class Calculator;

class CONTENT_EXPORT Watcher : public base::RefCounted<Watcher>,
                               public MetricSource::Delegate,
                               public base::PowerObserver {
 public:
  Watcher();
  void SetUp();
  void Destroy();

 protected:
  friend class base::RefCounted<Watcher>;

  // Exposed for tests.
  virtual std::unique_ptr<Calculator> CreateCalculator();
  virtual std::unique_ptr<MetricSource> CreateMetricSource();

  ~Watcher() override;

  // Delegate interface implementation.
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

  // base::PowerObserver interface implementation. The PowerObserver
  // notifications are asynchronously callbacks on their registration sequence
  // and may be delayed if there is a long queue of pending tasks to be
  // executed.
  void OnSuspend() override;
  void OnResume() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, TaskForwarding);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, TaskNesting);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, NativeEvents);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, BlockedOrLowPriorityTask);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, DelayedTask);

  // Metadata for currently running tasks and events is needed to track whether
  // or not they caused reentrancy.
  struct Metadata {
    explicit Metadata(const void* identifier,
                      bool was_blocked_or_low_priority,
                      base::TimeTicks execution_start_time);

    // An opaque identifier for the task or event.
    const void* const identifier;

    // Whether the task was at some point in a queue that was blocked or low
    // priority.
    const bool was_blocked_or_low_priority;

    // The time at which the task or event started running.
    const base::TimeTicks execution_start_time;

    // Whether the task or event has caused reentrancy.
    bool caused_reentrancy = false;
  };

  // This is called when |metric_source_| finishes destruction.
  void FinishDestroyMetricSource();

  // Common implementations for the thread-specific methods.
  void WillRunTask(const base::PendingTask* task,
                   bool was_blocked_or_low_priority,
                   std::vector<Metadata>* currently_running_metadata);

  // |callback| will either be synchronously invoked, or else never invoked.
  using TaskOrEventFinishedCallback = base::OnceCallback<
      void(base::TimeTicks, base::TimeTicks, base::TimeTicks)>;
  void DidRunTask(const base::PendingTask* task,
                  std::vector<Metadata>* currently_running_metadata,
                  int* mismatched_task_identifiers,
                  TaskOrEventFinishedCallback callback);

  // The source that emits responsiveness events.
  std::unique_ptr<MetricSource> metric_source_;

  // The following members are all affine to the UI thread.
  std::unique_ptr<Calculator> calculator_;

  // Metadata for currently running tasks and events on the UI thread.
  std::vector<Metadata> currently_running_metadata_ui_;

  // Task identifiers should only be mismatched once, since the Watcher may
  // register itself during a Task execution, and thus doesn't capture the
  // initial WillRunTask() callback.
  int mismatched_task_identifiers_ui_ = 0;

  // Event identifiers should be mismatched at most once, since the Watcher may
  // register itself during an event execution, and thus doesn't capture the
  // initial WillRunEventOnUIThread callback.
  int mismatched_event_identifiers_ui_ = 0;

  // The following members are all affine to the IO thread.
  std::vector<Metadata> currently_running_metadata_io_;
  int mismatched_task_identifiers_io_ = 0;

  // The implementation of this class guarantees that |calculator_io_| will be
  // non-nullptr and point to a valid object any time it is used on the IO
  // thread. To ensure this, the first task that this class posts onto the IO
  // thread sets |calculator_io_|. On destruction, this class first tears down
  // all consumers of |calculator_io_|, and then clears the member and destroys
  // Calculator.
  Calculator* calculator_io_ = nullptr;
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_

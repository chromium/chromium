// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace base {
struct PendingTask;
}  // namespace base

namespace content {
namespace responsiveness {

class Calculator;
class MessageLoopObserver;
class NativeEventObserver;

// This class watches events and tasks processed on the UI and IO threads of the
// browser process. It forwards stats on execution latency to Calculator, which
// emits UMA metrics.
//
// This class must only be constructed/destroyed on the UI thread. It has some
// private members that are affine to the IO thread. It takes care of deleting
// them appropriately.
//
// TODO(erikchen): Once the browser scheduler is implemented, this entire class
// should become simpler, as either BrowserUIThreadScheduler or
// BrowserIOThreadScheduler should implement much of the same functionality.
class CONTENT_EXPORT Watcher : public base::RefCounted<Watcher> {
 public:
  Watcher();

  // Must be called immediately after the constructor. This cannot be called
  // from the constructor because subclasses [for tests] need to be able to
  // override functions.
  void SetUp();

  // Destruction requires a thread-hop to the IO thread, so cannot be completed
  // synchronously. Owners of this class should call this method, and then
  // release their reference.
  void Destroy();

 protected:
  // Exposed for tests.
  virtual std::unique_ptr<Calculator> CreateCalculator();
  virtual std::unique_ptr<NativeEventObserver> CreateNativeEventObserver();
  virtual ~Watcher();
  virtual void RegisterMessageLoopObserverUI();
  virtual void RegisterMessageLoopObserverIO();

 private:
  friend class base::RefCounted<Watcher>;
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, TaskForwarding);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, TaskNesting);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, NativeEvents);

  // Metadata for currently running tasks and events is needed to track whether
  // or not they caused reentrancy.
  struct Metadata {
    explicit Metadata(const void* identifier);

    // An opaque identifier for the task or event.
    const void* identifier = nullptr;

    // Whether the task or event has caused reentrancy.
    bool caused_reentrancy = false;

    // For delayed tasks, the time at which the event is scheduled to run
    // is only loosely coupled to the time that the task actually runs. The
    // difference between these is not interesting for computing responsiveness.
    // Instead of measuring the duration between |queue_time| and |finish_time|,
    // we measure the duration of execution itself.
    //
    // We have evidence on Windows, macOS and Linux that the timestamp on native
    // events is not reliable. For native events, we also measure execution
    // duration instead of queue time + execution duration. See
    // https://crbug.com/859155#c39.
    base::TimeTicks execution_start_time;
  };

  void SetUpOnIOThread(Calculator*);
  void TearDownOnIOThread();
  void TearDownOnUIThread();

  // These methods are called by the MessageLoopObserver of the UI thread to
  // allow Watcher to collect metadata about the tasks being run.
  void WillRunTaskOnUIThread(const base::PendingTask* task);
  void DidRunTaskOnUIThread(const base::PendingTask* task);

  // These methods are called by the MessageLoopObserver of the IO thread to
  // allow Watcher to collect metadata about the tasks being run.
  void WillRunTaskOnIOThread(const base::PendingTask* task);
  void DidRunTaskOnIOThread(const base::PendingTask* task);

  // Common implementations for the thread-specific methods.
  void WillRunTask(const base::PendingTask* task,
                   std::vector<Metadata>* currently_running_metadata);

  // |callback| will either be synchronously invoked, or else never invoked.
  using TaskOrEventFinishedCallback =
      base::OnceCallback<void(base::TimeTicks, base::TimeTicks)>;
  void DidRunTask(const base::PendingTask* task,
                  std::vector<Metadata>* currently_running_metadata,
                  int* mismatched_task_identifiers,
                  TaskOrEventFinishedCallback callback);

  // These methods are called by the NativeEventObserver of the UI thread to
  // allow Watcher to collect metadata about the events being run.
  void WillRunEventOnUIThread(const void* opaque_identifier);
  void DidRunEventOnUIThread(const void* opaque_identifier);

  // The following members are all affine to the UI thread.
  std::unique_ptr<Calculator> calculator_;
  std::unique_ptr<MessageLoopObserver> message_loop_observer_ui_;
  std::unique_ptr<NativeEventObserver> native_event_observer_ui_;

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
  std::unique_ptr<MessageLoopObserver> message_loop_observer_io_;

  // The implementation of this class guarantees that |calculator_io_| will be
  // non-nullptr and point to a valid object any time it is used on the IO
  // thread. To ensure this, the first task that this class posts onto the IO
  // thread sets |calculator_io_|. On destruction, this class first tears down
  // all consumers of |calculator_io_|, and then clears the member and destroys
  // Calculator.
  Calculator* calculator_io_ = nullptr;

  bool destroy_was_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(Watcher);
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_METRIC_SOURCE_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_METRIC_SOURCE_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"

namespace base {
struct PendingTask;
namespace responsiveness {
class MessageLoopObserver;
}  // namespace responsiveness
}  // namespace base

namespace content {
namespace responsiveness {

class BrowserUINativeEventObserver;

// Observes tasks and native events processed on the browser UI and IO threads
// and notifies its Delegate via WillRun* and DidRun* callbacks. (Note: browser
// responsiveness metrics use base::responsiveness::Watcher instead.)
//
// Must only be constructed and destroyed on the UI thread; Destroy() hops to
// the IO thread to tear down IO-thread state before invoking the completion
// closure on the UI thread.
class CONTENT_EXPORT MetricSource {
 public:
  class CONTENT_EXPORT Delegate {
   public:
    virtual ~Delegate();

    // Interface for SetUp/TearDown on UI/IO threads in the implementer.
    virtual void SetUpOnIOThread() = 0;
    virtual void TearDownOnUIThread() = 0;
    virtual void TearDownOnIOThread() = 0;

    // These methods are called by the MessageLoopObserver of the UI thread to
    // allow Delegate to collect metadata about the tasks being run.
    virtual void WillRunTaskOnUIThread(const base::PendingTask* task,
                                       bool was_blocked_or_low_priority) = 0;
    virtual void DidRunTaskOnUIThread(const base::PendingTask* task) = 0;

    // These methods are called by the MessageLoopObserver of the IO thread to
    // allow Delegate to collect metadata about the tasks being run.
    virtual void WillRunTaskOnIOThread(const base::PendingTask* task,
                                       bool was_blocked_or_low_priority) = 0;
    virtual void DidRunTaskOnIOThread(const base::PendingTask* task) = 0;

    // These methods are called by the NativeEventObserver of the UI thread to
    // allow Delegate to collect metadata about the events being run.
    virtual void WillRunEventOnUIThread(uintptr_t opaque_identifier) = 0;
    virtual void DidRunEventOnUIThread(uintptr_t opaque_identifier) = 0;
  };

  explicit MetricSource(Delegate* delegate);

  MetricSource(const MetricSource&) = delete;
  MetricSource& operator=(const MetricSource&) = delete;

  virtual ~MetricSource();

  // Must be called immediately after the constructor. This cannot be called
  // from the constructor because subclasses [for tests] need to be able to
  // override functions.
  void SetUp();

  // Destruction requires a thread-hop to the IO thread so cannot be completed
  // synchronously. Owners of this class calling Destroy() should pass one
  // base::ScopedClosureRunner for final cleanup when this class finishes the
  // destruction on the UI thread. It's safe to delete this instance in
  // |on_finish_destroy|.
  void Destroy(base::ScopedClosureRunner on_finish_destroy);

 protected:
  virtual std::unique_ptr<BrowserUINativeEventObserver>
  CreateNativeEventObserver();
  virtual void RegisterMessageLoopObserverUI();
  virtual void RegisterMessageLoopObserverIO();

 private:
  void SetUpOnIOThread();
  void TearDownOnIOThread(base::ScopedClosureRunner on_finish_destroy);
  void TearDownOnUIThread(base::ScopedClosureRunner on_finish_destroy);

  raw_ptr<Delegate> delegate_;

  // The following members are all affine to the UI thread.
  std::unique_ptr<base::responsiveness::MessageLoopObserver>
      message_loop_observer_ui_;
  std::unique_ptr<BrowserUINativeEventObserver> native_event_observer_ui_;

  // The following members are all affine to the IO thread.
  std::unique_ptr<base::responsiveness::MessageLoopObserver>
      message_loop_observer_io_;

  bool destroy_was_called_ = false;
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_METRIC_SOURCE_H_

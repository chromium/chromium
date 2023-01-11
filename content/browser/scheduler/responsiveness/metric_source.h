// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_METRIC_SOURCE_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_METRIC_SOURCE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"

namespace base {
struct PendingTask;
}  // namespace base

namespace content {
namespace responsiveness {

class MessageLoopObserver;
class NativeEventObserver;

// This class represents the source of browser responsiveness metrics.
// This class watches events and tasks processed on the UI and IO threads of the
// browser process. It notifies the registered Delegate implementer about
// event and task execution through the WillRun* and DidRun* methods.
//
// This class must only be constructed/destroyed on the UI thread. It has some
// private members that are affine to the IO thread. It takes care of deleting
// them appropriately.
//
// TODO(erikchen): Once the browser scheduler is implemented, this entire class
// should become simpler, as either BrowserUIThreadScheduler or
// BrowserIOThreadScheduler should implement much of the same functionality.
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
    virtual void WillRunEventOnUIThread(const void* opaque_identifier) = 0;
    virtual void DidRunEventOnUIThread(const void* opaque_identifier) = 0;
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
  virtual std::unique_ptr<NativeEventObserver> CreateNativeEventObserver();
  virtual void RegisterMessageLoopObserverUI();
  virtual void RegisterMessageLoopObserverIO();

 private:
  void SetUpOnIOThread();
  void TearDownOnIOThread(base::ScopedClosureRunner on_finish_destroy);
  void TearDownOnUIThread(base::ScopedClosureRunner on_finish_destroy);

  raw_ptr<Delegate> delegate_;

  // The following members are all affine to the UI thread.
  std::unique_ptr<MessageLoopObserver> message_loop_observer_ui_;
  std::unique_ptr<NativeEventObserver> native_event_observer_ui_;

  // The following members are all affine to the IO thread.
  std::unique_ptr<MessageLoopObserver> message_loop_observer_io_;

  bool destroy_was_called_ = false;
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_METRIC_SOURCE_H_

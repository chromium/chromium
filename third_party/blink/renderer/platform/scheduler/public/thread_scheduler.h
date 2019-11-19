// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_SCHEDULER_H_

#include <memory>
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/pending_user_input_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace v8 {
class Isolate;
}

namespace base {
class TaskObserver;
}

namespace blink {
namespace scheduler {
class NonMainThreadSchedulerImpl;
}

class RAILModeObserver;

// This class is used to submit tasks and pass other information from Blink to
// the platform's scheduler.
// TODO(skyostil): Replace this class with WebMainThreadScheduler.
class PLATFORM_EXPORT ThreadScheduler {
 public:
  using RendererPauseHandle =
      scheduler::WebThreadScheduler::RendererPauseHandle;

  // Return the current thread's ThreadScheduler.
  static ThreadScheduler* Current();

  virtual ~ThreadScheduler() = default;

  // Called to prevent any more pending tasks from running. Must be called on
  // the associated WebThread.
  virtual void Shutdown() = 0;

  // Returns true if there is high priority work pending on the associated
  // WebThread and the caller should yield to let the scheduler service that
  // work.  Must be called on the associated WebThread.
  virtual bool ShouldYieldForHighPriorityWork() = 0;

  // Returns true if a currently running idle task could exceed its deadline
  // without impacting user experience too much. This should only be used if
  // there is a task which cannot be pre-empted and is likely to take longer
  // than the largest expected idle task deadline. It should NOT be polled to
  // check whether more work can be performed on the current idle task after
  // its deadline has expired - post a new idle task for the continuation of
  // the work in this case.
  // Must be called from the associated WebThread.
  virtual bool CanExceedIdleDeadlineIfRequired() const = 0;

  // Schedule an idle task to run the associated WebThread. For non-critical
  // tasks which may be reordered relative to other task types and may be
  // starved for an arbitrarily long time if no idle time is available.
  // Takes ownership of |IdleTask|. Can be called from any thread.
  virtual void PostIdleTask(const base::Location&, Thread::IdleTask) = 0;

  // As above, except that the task is guaranteed to not run before |delay|.
  // Takes ownership of |IdleTask|. Can be called from any thread.
  virtual void PostDelayedIdleTask(const base::Location&,
                                   base::TimeDelta delay,
                                   Thread::IdleTask) = 0;

  // Like postIdleTask but guarantees that the posted task will not run
  // nested within an already-running task. Posting an idle task as
  // non-nestable may not affect when the task gets run, or it could
  // make it run later than it normally would, but it won't make it
  // run earlier than it normally would.
  virtual void PostNonNestableIdleTask(const base::Location&,
                                       Thread::IdleTask) = 0;

  virtual void AddRAILModeObserver(RAILModeObserver* observer) = 0;

  virtual void RemoveRAILModeObserver(RAILModeObserver const* observer) = 0;

  // Returns a task runner for kV8 tasks. Can be called from any thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() = 0;

  // Returns a task runner for compositor tasks. This is intended only to be
  // used by specific animation and rendering related tasks (e.g. animated GIFS)
  // and should not generally be used.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  CompositorTaskRunner() = 0;

  // Returns a task runner for handling IPC messages.
  virtual scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() = 0;

  // Returns a default task runner. This is basically same as the default task
  // runner, but is explicitly allowed to run JavaScript. We plan to forbid V8
  // execution on per-thread task runners (crbug.com/913912). If you need to
  // replace a default task runner usages that executes JavaScript but it is
  // hard to replace with an appropriate (per-context) task runner, use this as
  // a temporal step.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  DeprecatedDefaultTaskRunner() = 0;

  // Creates a new PageScheduler for a given Page. Must be called from the
  // associated WebThread.
  virtual std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) = 0;

  // Pauses the scheduler. See WebThreadScheduler::PauseRenderer for
  // details. May only be called from the main thread.
  virtual std::unique_ptr<RendererPauseHandle> PauseScheduler()
      WARN_UNUSED_RESULT = 0;

  // Returns the current time recognized by the scheduler, which may perhaps
  // be based on a real or virtual time domain. Used by Timer.
  virtual base::TimeTicks MonotonicallyIncreasingVirtualTime() = 0;

  // Adds or removes a task observer from the scheduler. The observer will be
  // notified before and after every executed task. These functions can only be
  // called on the thread this scheduler was created on.
  virtual void AddTaskObserver(base::TaskObserver* task_observer) = 0;
  virtual void RemoveTaskObserver(base::TaskObserver* task_observer) = 0;

  virtual scheduler::PendingUserInputInfo GetPendingUserInputInfo() const {
    return scheduler::PendingUserInputInfo();
  }

  // Indicates that a BeginMainFrame task has been scheduled to run on the main
  // thread. Note that this is inherently racy, as it will be affected by code
  // running on the compositor thread.
  virtual bool IsBeginMainFrameScheduled() const { return false; }

  // Associates |isolate| to the scheduler.
  virtual void SetV8Isolate(v8::Isolate* isolate) = 0;

  virtual void OnSafepointEntered() {}
  virtual void OnSafepointExited() {}

  // Test helpers.

  // Return a reference to an underlying main thread WebThreadScheduler object.
  // Can be null if there is no underlying main thread WebThreadScheduler
  // (e.g. worker threads).
  virtual scheduler::WebThreadScheduler* GetWebMainThreadSchedulerForTest() {
    return nullptr;
  }

  virtual scheduler::NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_THREAD_SCHEDULER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_DEBUG_COMMAND_QUEUE_H_
#define SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_DEBUG_COMMAND_QUEUE_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"

namespace ax {

// DebugCommandQueue helps coordinate command transfer between Session (lives on
// V8 thread) and IOSession (lives on mojo / main thread), as well as blocking
// execution of V8 thread when paused in debugger. It's owned by the
// V8Environment (but may extend its own lifetime a bit to keep callbacks
// safe).
class DebugCommandQueue : public base::RefCountedThreadSafe<DebugCommandQueue> {
 public:
  // Will always be created on V8 thread since it is created by V8Environment.
  DebugCommandQueue();
  DebugCommandQueue(const DebugCommandQueue&) = delete;
  DebugCommandQueue& operator=(const DebugCommandQueue&) = delete;

  // Requests exit from PauseForDebuggerAndRunCommands().
  //
  // Can be called from any thread.
  void QuitPauseForDebugger();

  // Adds `task` to queue of tasks to be executed on v8 thread, either within
  // PauseForDebuggerAndRunCommands()  or the regular event loop.
  //
  // Can be called from any thread.
  //
  // Note: `task` should probably be bound to a WeakPtr bound on V8 thread,
  // since with a cross-thread QueueTaskForV8Thread it would be hard for origin
  // to reason about lifetime of V8-thread objects.
  void QueueTaskForV8Thread(base::OnceClosure task);

 private:
  friend class base::RefCountedThreadSafe<DebugCommandQueue>;

  ~DebugCommandQueue();

  void RunQueue();
  void RunQueueWithLockHeld() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;

  base::Lock lock_;
  base::ConditionVariable wake_up_ GUARDED_BY(lock_);
  base::queue<base::OnceClosure> queue_ GUARDED_BY(lock_);
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_DEBUG_COMMAND_QUEUE_H_

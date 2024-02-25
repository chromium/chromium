// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_DEBUG_COMMAND_QUEUE_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_DEBUG_COMMAND_QUEUE_H_

#include <set>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"

namespace auction_worklet {

// DebugCommandQueue helps coordinate command transfer between Session (lives on
// V8 thread) and IOSession (lives on mojo thread), as well as blocking
// execution of V8 thread when paused in debugger. It's jointly owned by the
// AuctionV8Helper and IOSession, and may extend its own lifetime a bit to keep
// callbacks safe.
class CONTENT_EXPORT DebugCommandQueue
    : public base::RefCountedThreadSafe<DebugCommandQueue> {
 public:
  // May be created and destroyed on any thread.
  explicit DebugCommandQueue(
      scoped_refptr<base::SequencedTaskRunner> v8_runner);
  DebugCommandQueue(const DebugCommandQueue&) = delete;
  DebugCommandQueue& operator=(const DebugCommandQueue&) = delete;

  // Blocks the current thread until QuitPauseForDebugger() is called, executing
  // only things added via Post().
  //
  // If AbortPauses(context_group_id) has been called, exits immediately.
  //
  // `abort_helper` should be a closure that, when called on the v8 thread, will
  // eventually lead to QuitPauseForDebugger being called.
  //
  // Called on v8 thread only.
  void PauseForDebuggerAndRunCommands(
      int context_group_id,
      base::OnceClosure abort_helper = base::OnceClosure());

  // If the v8 thread is within PauseForDebuggerAndRunCommands() the
  // `abort_helper` passed to the method will be queued for execution.
  //
  // Otherwise, marks `context_group_id` as requiring
  // PauseForDebuggerAndRunCommands to exit immediately.
  //
  // Can be called from any thread.
  void AbortPauses(int context_group_id);

  // Notes that the meaning of `context_group_id` has changed, and so any
  // previous calls to AbortPauses() for given value should no longer apply.
  //
  // Can be called from any thread.
  void RecycleContextGroupId(int context_group_id);

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

  void PostRunQueue() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RunQueue();
  void RunQueueWithLockHeld() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;

  base::Lock lock_;
  base::ConditionVariable wake_up_ GUARDED_BY(lock_);
  base::queue<base::OnceClosure> queue_ GUARDED_BY(lock_);
  base::OnceClosure pause_abort_helper_ GUARDED_BY(lock_);

  bool v8_thread_paused_ GUARDED_BY(lock_) = false;
  int paused_context_group_id_ GUARDED_BY(lock_);
  std::set<int> aborted_context_group_ids_ GUARDED_BY(lock_);
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_DEBUG_COMMAND_QUEUE_H_

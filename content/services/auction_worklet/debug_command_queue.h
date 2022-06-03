// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_DEBUG_COMMAND_QUEUE_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_DEBUG_COMMAND_QUEUE_H_

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"

namespace auction_worklet {

// DebugCommandQueue helps coordinate command transfer between Session (lives on
// V8 thread) and IOSession (lives on mojo thread), as well as blocking
// execution of V8 thread when paused in debugger. It's owned by the
// AuctionV8Helper
class DebugCommandQueue {
 public:
  // Must be created and destroyed on the v8 thread.
  DebugCommandQueue();
  DebugCommandQueue(const DebugCommandQueue&) = delete;
  DebugCommandQueue& operator=(const DebugCommandQueue&) = delete;
  ~DebugCommandQueue();

  // Blocks the current thread until QuitPauseForDebugger() is called, executing
  // only things added via Post().
  //
  // Called on v8 thread only.
  void PauseForDebuggerAndRunCommands();

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
  void PostRunQueue() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RunQueue();
  void RunQueueWithLockHeld() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;

  base::Lock lock_;
  base::ConditionVariable wake_up_ GUARDED_BY(lock_);
  base::queue<base::OnceClosure> queue_ GUARDED_BY(lock_);
  bool v8_thread_paused_ GUARDED_BY(lock_) = false;

  base::RepeatingClosure run_queue_closure_;
  base::WeakPtrFactory<DebugCommandQueue> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_DEBUG_COMMAND_QUEUE_H_

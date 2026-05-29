// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_RACE_CONDITION_SCHEDULER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_RACE_CONDITION_SCHEDULER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/tests/testable_multiplex_router.h"

namespace mojo::test {

// Orchestrates deterministic race conditions between MultiplexRouter
// operations and destruction.
//
// Uses three threads to reproduce the race:
//   - Test thread: drives the test and waits on synchronization events.
//   - Router thread: the sequence the router is bound to. Runs pipe I/O
//     callbacks (OnWatcherHandleReady) and processes posted Release() calls.
//   - Racing thread: releases the last scoped_refptr off-sequence,
//     optionally after calling a cross-sequence method like RaiseError().
//
// Synchronization flow (ScheduleDeathAndWait):
//   1. Test installs a WaitableEvent in the router's destructor via
//      AddDestructionWait, pausing destruction mid-execution.
//   2. Test posts ResetRouter() to the racing thread, which releases the
//      last scoped_refptr. The overridden Release() posts the ref-count
//      decrement back to the router thread.
//   3. Racing thread signals reset_event_waiter_, unblocking the test.
//   4. Test runs assertions (e.g., verifies error handler ran on the
//      router thread before the posted Release).
//   5. Test calls SignalDeath(), unblocking the destructor on the router
//      thread.
//
// The ScheduleNotifyPeerClosureAndDeath and ScheduleRaiseErrorAndDeath
// variants add a cross-sequence router call before releasing the last ref,
// verifying that the posted Release serializes correctly with cross-sequence
// work.
class RaceConditionScheduler {
 public:
  RaceConditionScheduler();
  ~RaceConditionScheduler();

  // Posts an OnWatcherHandleReady call to the router thread, simulating a pipe
  // error event from the SimpleWatcher. Runs `callback` just before invoking
  // the Connector's handler, allowing the test to observe the sequencing.
  void ScheduleOnWatcherHandleReady(base::OnceClosure callback,
                                    MojoResult result);

  // Releases the last router ref from the racing thread, triggering
  // destruction. The destructor is paused via destruction_pause_waiter_ until
  // SignalDeath() is called. `callback` runs when the destructor is unblocked.
  // Blocks until the racing thread has started the release.
  void ScheduleDeathAndWait(base::OnceClosure callback);

  // Calls NotifyLocalEndpointOfPeerClosure from the racing thread (off-sequence
  // from the router), then releases the last ref to trigger destruction. This
  // reproduces the race where a cross-sequence call and destruction overlap.
  void ScheduleNotifyPeerClosureAndDeath(base::OnceClosure error_callback,
                                         base::OnceClosure death_callback,
                                         InterfaceId id);

  // Same as above but calls RaiseError instead of
  // NotifyLocalEndpointOfPeerClosure.
  void ScheduleRaiseErrorAndDeath(base::OnceClosure error_callback,
                                  base::OnceClosure death_callback);

  // Unblocks the router's destructor (which is paused on
  // destruction_pause_waiter_).
  void SignalDeath();

  bool RouterHasOneRef() const { return router_ && router_->HasOneRef(); }

  bool IsRouterDead() const { return !router_; }

 private:
  void CallOnWatcherHandleReady(base::OnceClosure callback, MojoResult result);
  void ResetRouter();

  // The sequence the router is bound to. Handles pipe I/O and destruction.
  std::unique_ptr<base::Thread> router_thread_;
  // A separate thread that releases the last ref, simulating off-sequence
  // destruction (the scenario that causes use-after-free without the fix).
  std::unique_ptr<base::Thread> racing_thread_;

  scoped_refptr<TestableMultiplexRouter> router_;
  // Used to invoke Connector::OnWatcherHandleReady after the router may have
  // been destroyed. The WeakPtr safely detects if the Connector is gone.
  base::WeakPtr<Connector> connector_weak_ptr_;

  // Signaled by the racing thread after it has released (or is about to
  // release) the router ref. The test thread waits on this to know that
  // destruction has been initiated.
  base::WaitableEvent reset_event_waiter_;
  // Blocks the router's destructor (via TestableMultiplexRouter) until the
  // test is ready to let destruction complete. This creates the window where
  // the router is "dying but not yet dead," which is the race scenario.
  base::WaitableEvent destruction_pause_waiter_;
};

}  // namespace mojo::test

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_RACE_CONDITION_SCHEDULER_H_

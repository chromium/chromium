// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_WATCHER_DISPATCHER_H_
#define MOJO_CORE_WATCHER_DISPATCHER_H_

#include <stdint.h>

#include <set>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/handle_signals_state.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/c/system/trap.h"

namespace mojo {
namespace core {

class Watch;

// The dispatcher type which backs watcher handles.
class WatcherDispatcher : public Dispatcher {
 public:
  // Constructs a new WatcherDispatcher which invokes |handler| when a
  // registered watch observes some relevant state change.
  explicit WatcherDispatcher(MojoTrapEventHandler handler);

  // Methods used by watched dispatchers to notify watchers of events.
  void NotifyHandleState(Dispatcher* dispatcher,
                         const HandleSignalsState& state);
  void NotifyHandleClosed(Dispatcher* dispatcher);

  // Method used by RequestContext (indirectly, via Watch) to complete
  // notification operations from a safe stack frame to avoid reentrancy.
  void InvokeWatchCallback(uintptr_t context,
                           MojoResult result,
                           const HandleSignalsState& state,
                           MojoTrapEventFlags flags);

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult WatchDispatcher(scoped_refptr<Dispatcher> dispatcher,
                             MojoHandleSignals signals,
                             MojoTriggerCondition condition,
                             uintptr_t context) override;
  MojoResult CancelWatch(uintptr_t context) override;
  MojoResult Arm(uint32_t* num_blocking_events,
                 MojoTrapEvent* blocking_events) override;

 private:
  friend class Watch;

  using WatchSet = std::set<const Watch*>;

  ~WatcherDispatcher() override;

  const MojoTrapEventHandler handler_;

  // Guards access to the fields below.
  //
  // NOTE: This may be acquired while holding another dispatcher's lock, as
  // watched dispatchers call into WatcherDispatcher methods which lock this
  // when issuing state change notifications. WatcherDispatcher must therefore
  // take caution to NEVER acquire other dispatcher locks while this is held.
  base::Lock lock_;

  bool armed_ = false;
  bool closed_ = false;

  // A mapping from context to Watch.
  base::flat_map<uintptr_t, scoped_refptr<Watch>> watches_;

  // A mapping from watched dispatcher to Watch.
  base::flat_map<Dispatcher*, scoped_refptr<Watch>> watched_handles_;

  // The set of all Watch instances which are currently ready to signal. This is
  // used for efficient arming behavior, as it allows for O(1) discovery of
  // whether or not arming can succeed and quick determination of who's
  // responsible if it can't.
  WatchSet ready_watches_;

  // Tracks the last Watch whose state was returned by Arm(). This is used to
  // ensure consistent round-robin behavior in the event that multiple Watches
  // remain ready over the span of several Arm() attempts.
  //
  // NOTE: This pointer is only used to index |ready_watches_| and may point to
  // an invalid object. It must therefore never be dereferenced.
  const Watch* last_watch_to_block_arming_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WatcherDispatcher);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_WATCHER_DISPATCHER_H_

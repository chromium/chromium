// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_WATCH_H_
#define MOJO_CORE_WATCH_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/core/atomic_flag.h"
#include "mojo/core/handle_signals_state.h"
#include "mojo/public/c/system/trap.h"

namespace mojo {
namespace core {

class Dispatcher;
class WatcherDispatcher;

// Encapsulates the state associated with a single watch context within a
// watcher.
//
// Every Watch has its own cancellation state, and is captured by RequestContext
// notification finalizers to avoid redundant context resolution during
// finalizer execution.
class Watch : public base::RefCountedThreadSafe<Watch> {
 public:
  // Constructs a Watch which represents a watch within |watcher| associated
  // with |context|, watching |dispatcher| for |signals|.
  Watch(const scoped_refptr<WatcherDispatcher>& watcher,
        const scoped_refptr<Dispatcher>& dispatcher,
        uintptr_t context,
        MojoHandleSignals signals,
        MojoTriggerCondition condition);

  Watch(const Watch&) = delete;
  Watch& operator=(const Watch&) = delete;

  // Notifies the Watch of a potential state change.
  //
  // If |allowed_to_call_callback| is true, this may add a notification
  // finalizer to the current RequestContext to invoke the watcher's callback
  // with this watch's context. See return values below.
  //
  // This is called directly by WatcherDispatcher whenever the Watch's observed
  // dispatcher notifies the WatcherDispatcher of a state change.
  //
  // Returns |true| if the Watch entered or remains in a ready state as a result
  // of the state change. If |allowed_to_call_callback| was true in this case,
  // the Watch will have also attached a notification finalizer to the current
  // RequestContext.
  //
  // Returns |false| if the
  bool NotifyState(const HandleSignalsState& state,
                   bool allowed_to_call_callback);

  // Notifies the watch of cancellation ASAP. This will always be the last
  // notification sent for the watch.
  void Cancel();

  // Finalizer method for RequestContexts. This method is invoked once for every
  // notification finalizer added to a RequestContext by this object. This calls
  // down into the WatcherDispatcher to do the actual notification call.
  void InvokeCallback(MojoResult result,
                      const HandleSignalsState& state,
                      MojoTrapEventFlags flags);

  const scoped_refptr<Dispatcher>& dispatcher() const { return dispatcher_; }
  uintptr_t context() const { return context_; }

  MojoResult last_known_result() const {
    AssertWatcherLockAcquired();
    return last_known_result_;
  }

  MojoHandleSignalsState last_known_signals_state() const {
    AssertWatcherLockAcquired();
    return last_known_signals_state_;
  }

  bool ready() const {
    AssertWatcherLockAcquired();
    return last_known_result_ == MOJO_RESULT_OK ||
           last_known_result_ == MOJO_RESULT_FAILED_PRECONDITION;
  }

 private:
  friend class base::RefCountedThreadSafe<Watch>;

  ~Watch();

#if DCHECK_IS_ON()
  void AssertWatcherLockAcquired() const;
#else
  void AssertWatcherLockAcquired() const {}
#endif

  const scoped_refptr<WatcherDispatcher> watcher_;
  const scoped_refptr<Dispatcher> dispatcher_;
  const uintptr_t context_;
  const MojoHandleSignals signals_;
  const MojoTriggerCondition condition_;

  // The result code with which this Watch would notify if currently armed,
  // based on the last known signaling state of |dispatcher_|. Guarded by the
  // owning WatcherDispatcher's lock.
  MojoResult last_known_result_ = MOJO_RESULT_UNKNOWN;

  // The last known signaling state of |dispatcher_|. Guarded by the owning
  // WatcherDispatcher's lock.
  MojoHandleSignalsState last_known_signals_state_ = {0, 0};

  // Guards |is_cancelled_| below and mutually excludes individual watch
  // notification executions for this same watch context.
  //
  // Note that this should only be acquired from a RequestContext finalizer to
  // ensure that no other internal locks are already held.
  base::Lock notification_lock_;

  // Guarded by |notification_lock_|.
  bool is_cancelled_ = false;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_WATCH_H_

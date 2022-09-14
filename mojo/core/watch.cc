// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/watch.h"

#include "mojo/core/request_context.h"
#include "mojo/core/watcher_dispatcher.h"

namespace mojo {
namespace core {

Watch::Watch(const scoped_refptr<WatcherDispatcher>& watcher,
             const scoped_refptr<Dispatcher>& dispatcher,
             uintptr_t context,
             MojoHandleSignals signals,
             MojoTriggerCondition condition)
    : watcher_(watcher),
      dispatcher_(dispatcher),
      context_(context),
      signals_(signals),
      condition_(condition) {}

bool Watch::NotifyState(const HandleSignalsState& state,
                        bool allowed_to_call_callback) {
  AssertWatcherLockAcquired();

  // NOTE: This method must NEVER call into |dispatcher_| directly, because it
  // may be called while |dispatcher_| holds a lock.
  MojoResult rv = MOJO_RESULT_SHOULD_WAIT;
  RequestContext* const request_context = RequestContext::current();
  const bool notify_success =
      (state.satisfies_any(signals_) &&
       condition_ == MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED) ||
      (!state.satisfies_all(signals_) &&
       condition_ == MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED);
  if (notify_success) {
    rv = MOJO_RESULT_OK;
    if (allowed_to_call_callback && rv != last_known_result_) {
      request_context->AddWatchNotifyFinalizer(this, MOJO_RESULT_OK, state);
    }
  } else if (condition_ == MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED &&
             !state.can_satisfy_any(signals_)) {
    rv = MOJO_RESULT_FAILED_PRECONDITION;
    if (allowed_to_call_callback && rv != last_known_result_) {
      request_context->AddWatchNotifyFinalizer(
          this, MOJO_RESULT_FAILED_PRECONDITION, state);
    }
  }

  last_known_signals_state_ =
      *static_cast<const MojoHandleSignalsState*>(&state);
  last_known_result_ = rv;
  return ready();
}

void Watch::Cancel() {
  RequestContext::current()->AddWatchCancelFinalizer(this);
}

void Watch::InvokeCallback(MojoResult result,
                           const HandleSignalsState& state,
                           MojoTrapEventFlags flags) {
  // We hold the lock through invocation to ensure that only one notification
  // callback runs for this context at any given time.
  base::AutoLock lock(notification_lock_);

  // Ensure that no notifications are dispatched beyond cancellation.
  if (is_cancelled_)
    return;

  if (result == MOJO_RESULT_CANCELLED)
    is_cancelled_ = true;

  // NOTE: This will acquire |watcher_|'s internal lock. It's safe because a
  // thread can only enter InvokeCallback() from within a RequestContext
  // destructor where no dispatcher locks are held.
  watcher_->InvokeWatchCallback(context_, result, state, flags);
}

Watch::~Watch() = default;

#if DCHECK_IS_ON()
void Watch::AssertWatcherLockAcquired() const {
  watcher_->lock_.AssertAcquired();
}
#endif

}  // namespace core
}  // namespace mojo

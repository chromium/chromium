// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/watcher_dispatcher.h"

#include <algorithm>
#include <limits>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "mojo/core/watch.h"

namespace mojo {
namespace core {

WatcherDispatcher::WatcherDispatcher(MojoTrapEventHandler handler)
    : handler_(handler) {}

void WatcherDispatcher::NotifyHandleState(Dispatcher* dispatcher,
                                          const HandleSignalsState& state) {
  base::AutoLock lock(lock_);
  auto it = watched_handles_.find(dispatcher);
  if (it == watched_handles_.end())
    return;

  // Maybe fire a notification to the watch associated with this dispatcher,
  // provided we're armed and it cares about the new state.
  if (it->second->NotifyState(state, armed_)) {
    ready_watches_.insert(it->second.get());

    // If we were armed and got here, we notified the watch. Disarm.
    armed_ = false;
  } else {
    ready_watches_.erase(it->second.get());
  }
}

void WatcherDispatcher::NotifyHandleClosed(Dispatcher* dispatcher) {
  scoped_refptr<Watch> watch;
  {
    base::AutoLock lock(lock_);
    auto it = watched_handles_.find(dispatcher);
    if (it == watched_handles_.end())
      return;

    watch = std::move(it->second);

    // Wipe out all state associated with the closed dispatcher.
    watches_.erase(watch->context());
    ready_watches_.erase(watch.get());
    watched_handles_.erase(it);
  }

  // NOTE: It's important that this is called outside of |lock_| since it
  // acquires internal Watch locks.
  watch->Cancel();
}

// handler_ may be address-taken in a different DSO, and hence incompatible with
// CFI-icall.
NO_SANITIZE("cfi-icall")
void WatcherDispatcher::InvokeWatchCallback(uintptr_t context,
                                            MojoResult result,
                                            const HandleSignalsState& state,
                                            MojoTrapEventFlags flags) {
  MojoTrapEvent event;
  event.struct_size = sizeof(event);
  event.trigger_context = context;
  event.result = result;
  event.signals_state = static_cast<MojoHandleSignalsState>(state);
  event.flags = flags;

  {
    // We avoid holding the lock during dispatch. It's OK for notification
    // callbacks to close this watcher, and it's OK for notifications to race
    // with closure, if for example the watcher is closed from another thread
    // between this test and the invocation of |callback_| below.
    //
    // Because cancellation synchronously blocks all future notifications, and
    // because notifications themselves are mutually exclusive for any given
    // context, we still guarantee that a single MOJO_RESULT_CANCELLED result
    // is the last notification received for any given context.
    //
    // This guarantee is sufficient to make safe, synchronized, per-context
    // state management possible in user code.
    base::AutoLock lock(lock_);
    if (closed_ && result != MOJO_RESULT_CANCELLED)
      return;
  }

  handler_(&event);
}

Dispatcher::Type WatcherDispatcher::GetType() const {
  return Type::WATCHER;
}

MojoResult WatcherDispatcher::Close() {
  // We swap out all the watched handle information onto the stack so we can
  // call into their dispatchers without our own lock held.
  base::flat_map<uintptr_t, scoped_refptr<Watch>> watches;
  {
    base::AutoLock lock(lock_);
    if (closed_)
      return MOJO_RESULT_INVALID_ARGUMENT;
    closed_ = true;
    std::swap(watches, watches_);
    watched_handles_.clear();
  }

  // Remove all refs from our watched dispatchers and fire cancellations.
  for (auto& entry : watches) {
    entry.second->dispatcher()->RemoveWatcherRef(this, entry.first);
    entry.second->Cancel();
  }

  return MOJO_RESULT_OK;
}

MojoResult WatcherDispatcher::WatchDispatcher(
    scoped_refptr<Dispatcher> dispatcher,
    MojoHandleSignals signals,
    MojoTriggerCondition condition,
    uintptr_t context) {
  // NOTE: Because it's critical to avoid acquiring any other dispatcher locks
  // while |lock_| is held, we defer adding oursevles to the dispatcher until
  // after we've updated all our own relevant state and released |lock_|.
  {
    base::AutoLock lock(lock_);
    if (closed_)
      return MOJO_RESULT_INVALID_ARGUMENT;

    if (watches_.count(context) || watched_handles_.count(dispatcher.get()))
      return MOJO_RESULT_ALREADY_EXISTS;

    scoped_refptr<Watch> watch =
        new Watch(this, dispatcher, context, signals, condition);
    watches_.insert({context, watch});
    auto result =
        watched_handles_.insert(std::make_pair(dispatcher.get(), watch));
    DCHECK(result.second);
  }

  MojoResult rv = dispatcher->AddWatcherRef(this, context);
  if (rv != MOJO_RESULT_OK) {
    // Oops. This was not a valid handle to watch. Undo the above work and
    // fail gracefully.
    base::AutoLock lock(lock_);
    watches_.erase(context);
    watched_handles_.erase(dispatcher.get());
    return rv;
  }

  bool remove_now;
  {
    // If we've been closed already, there's a chance our closure raced with
    // the call to AddWatcherRef() above. In that case we want to ensure we've
    // removed our ref from |dispatcher|. Note that this may in turn race
    // with normal removal, but that's fine.
    base::AutoLock lock(lock_);
    remove_now = closed_;
  }
  if (remove_now)
    dispatcher->RemoveWatcherRef(this, context);

  return MOJO_RESULT_OK;
}

MojoResult WatcherDispatcher::CancelWatch(uintptr_t context) {
  // We may remove the last stored ref to the Watch below, so we retain
  // a reference on the stack.
  scoped_refptr<Watch> watch;
  {
    base::AutoLock lock(lock_);
    if (closed_)
      return MOJO_RESULT_INVALID_ARGUMENT;
    auto it = watches_.find(context);
    if (it == watches_.end())
      return MOJO_RESULT_NOT_FOUND;
    watch = it->second;
    watches_.erase(it);
  }

  // Mark the watch as cancelled so no further notifications get through.
  watch->Cancel();

  // We remove the watcher ref for this context before updating any more
  // internal watcher state, ensuring that we don't receiving further
  // notifications for this context.
  watch->dispatcher()->RemoveWatcherRef(this, context);

  {
    base::AutoLock lock(lock_);
    auto handle_it = watched_handles_.find(watch->dispatcher().get());

    // If another thread races to close this watcher handler, |watched_handles_|
    // may have been cleared by the time we reach this section.
    if (handle_it == watched_handles_.end())
      return MOJO_RESULT_OK;

    ready_watches_.erase(handle_it->second.get());
    watched_handles_.erase(handle_it);
  }

  return MOJO_RESULT_OK;
}

MojoResult WatcherDispatcher::Arm(uint32_t* num_blocking_events,
                                  MojoTrapEvent* blocking_events) {
  base::AutoLock lock(lock_);
  if (num_blocking_events && !blocking_events)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (watched_handles_.empty())
    return MOJO_RESULT_NOT_FOUND;

  if (ready_watches_.empty()) {
    // Fast path: No watches are ready to notify, so we're done.
    armed_ = true;
    return MOJO_RESULT_OK;
  }

  if (num_blocking_events) {
    DCHECK_LE(ready_watches_.size(), std::numeric_limits<uint32_t>::max());
    *num_blocking_events = std::min(
        *num_blocking_events, static_cast<uint32_t>(ready_watches_.size()));

    WatchSet::const_iterator next_ready_iter = ready_watches_.begin();
    if (last_watch_to_block_arming_) {
      // Find the next watch to notify in simple round-robin order on the
      // |ready_watches_| map, wrapping around to the beginning if necessary.
      next_ready_iter = ready_watches_.find(
          reinterpret_cast<const Watch*>(last_watch_to_block_arming_));
      if (next_ready_iter != ready_watches_.end())
        ++next_ready_iter;
      if (next_ready_iter == ready_watches_.end())
        next_ready_iter = ready_watches_.begin();
    }

    for (size_t i = 0; i < *num_blocking_events; ++i) {
      const Watch* const watch = *next_ready_iter;
      if (blocking_events[i].struct_size < sizeof(*blocking_events))
        return MOJO_RESULT_INVALID_ARGUMENT;
      blocking_events[i].flags = MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL;
      blocking_events[i].trigger_context = watch->context();
      blocking_events[i].result = watch->last_known_result();
      blocking_events[i].signals_state = watch->last_known_signals_state();

      // Iterate and wrap around.
      last_watch_to_block_arming_ = reinterpret_cast<uintptr_t>(watch);
      ++next_ready_iter;
      if (next_ready_iter == ready_watches_.end())
        next_ready_iter = ready_watches_.begin();
    }
  }

  return MOJO_RESULT_FAILED_PRECONDITION;
}

WatcherDispatcher::~WatcherDispatcher() = default;

}  // namespace core
}  // namespace mojo

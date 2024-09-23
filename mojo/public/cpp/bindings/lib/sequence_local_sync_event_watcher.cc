// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sequence_local_sync_event_watcher.h"

#include <map>
#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "mojo/public/cpp/bindings/sync_event_watcher.h"

namespace mojo {

namespace {

struct WatcherState;

using WatcherStateMap =
    std::map<const SequenceLocalSyncEventWatcher*, scoped_refptr<WatcherState>>;

// Ref-counted watcher state which may outlive the watcher to which it pertains.
// This is necessary to store outside of the SequenceLocalSyncEventWatcher
// itself in order to support nested sync operations where an inner operation
// may destroy the watcher.
struct WatcherState : public base::RefCounted<WatcherState> {
  WatcherState() = default;

  WatcherState(const WatcherState&) = delete;
  WatcherState& operator=(const WatcherState&) = delete;

  bool watcher_was_destroyed = false;

 private:
  friend class base::RefCounted<WatcherState>;

  ~WatcherState() = default;
};

}  // namespace

// Owns the WaitableEvent and SyncEventWatcher shared by all
// SequenceLocalSyncEventWatchers on a single sequence, and coordinates the
// multiplexing of those shared objects to support an arbitrary number of
// SequenceLocalSyncEventWatchers waiting and signaling potentially while
// nested.
class SequenceLocalSyncEventWatcher::SequenceLocalState {
 public:
  SequenceLocalState()
      : event_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED),
        event_watcher_(&event_,
                       base::BindRepeating(&SequenceLocalState::OnEventSignaled,
                                           base::Unretained(this))) {
    // We always allow this event handler to be awoken during any sync event on
    // the sequence. Individual watchers still must opt into having such
    // wake-ups propagated to them.
    event_watcher_.AllowWokenUpBySyncWatchOnSameThread();
  }

  SequenceLocalState(const SequenceLocalState&) = delete;
  SequenceLocalState& operator=(const SequenceLocalState&) = delete;

  ~SequenceLocalState() {}

  // Initializes a SequenceLocalState instance in sequence-local storage if
  // not already initialized. Returns a WeakPtr to the stored state object.
  static base::WeakPtr<SequenceLocalState> GetOrCreate() {
    return GetStorageSlot().GetOrCreateValue().weak_ptr_factory_.GetWeakPtr();
  }

  // Registers a new watcher and returns an iterator into the WatcherStateMap to
  // be used for fast access with other methods.
  WatcherStateMap::iterator RegisterWatcher(
      const SequenceLocalSyncEventWatcher* watcher) {
    auto result = registered_watchers_.emplace(
        watcher, base::MakeRefCounted<WatcherState>());
    DCHECK(result.second);
    return result.first;
  }

  void UnregisterWatcher(WatcherStateMap::iterator iter) {
    if (top_watcher_ == iter->first) {
      // If the watcher being unregistered is currently blocking in a
      // |SyncWatch()| operation, we need to unblock it. Setting this flag does
      // that.
      top_watcher_state_->watcher_was_destroyed = true;
      top_watcher_state_ = nullptr;
      top_watcher_ = nullptr;
    }

    {
      base::AutoLock lock(ready_watchers_lock_);
      ready_watchers_.erase(iter->first);
      if (ready_watchers_.empty())
        event_.Reset();
    }

    registered_watchers_.erase(iter);
    if (registered_watchers_.empty()) {
      // If no more watchers are registered, clear our sequence-local storage.
      // Deletes |this|.
      // Check if the SequenceLocalStorageMap is valid before doing this to
      // avoid races at shutdown when other objects use SequenceLocalStorageSlot
      // and indirectly call to here.
      if (base::internal::SequenceLocalStorageMap::IsSetForCurrentThread())
        GetStorageSlot().reset();
    }
  }

  void SignalForWatcher(const SequenceLocalSyncEventWatcher* watcher) {
    bool must_signal = false;
    {
      base::AutoLock lock(ready_watchers_lock_);
      must_signal = ready_watchers_.empty();
      ready_watchers_.insert(watcher);
    }

    // If we didn't have any ready watchers before, the event may not have
    // been signaled. Signal it to ensure that |OnEventSignaled()| is run.
    if (must_signal)
      event_.Signal();
  }

  void ResetForWatcher(const SequenceLocalSyncEventWatcher* watcher) {
    base::AutoLock lock(ready_watchers_lock_);
    ready_watchers_.erase(watcher);

    // No more watchers are ready, so we can reset the event. The next watcher
    // to call |SignalForWatcher()| will re-signal the event.
    if (ready_watchers_.empty())
      event_.Reset();
  }

  bool SyncWatch(const SequenceLocalSyncEventWatcher* watcher,
                 WatcherState* watcher_state,
                 const bool* should_stop) {
    // |SyncWatch()| calls may nest arbitrarily deep on the same sequence. We
    // preserve the outer watcher state on the stack and restore it once the
    // innermost watch is complete.
    const SequenceLocalSyncEventWatcher* outer_watcher = top_watcher_;
    WatcherState* outer_watcher_state = top_watcher_state_;

    // Keep a ref on the stack so the state stays alive even if the watcher is
    // destroyed.
    scoped_refptr<WatcherState> top_watcher_state(watcher_state);
    top_watcher_state_ = watcher_state;
    top_watcher_ = watcher;

    // In addition to the caller's own stop condition, we need to interrupt the
    // SyncEventWatcher if |watcher| is destroyed while we're waiting.
    const bool* stop_flags[] = {should_stop,
                                &top_watcher_state_->watcher_was_destroyed};

    // |SyncWatch()| may delete |this|.
    auto weak_self = weak_ptr_factory_.GetWeakPtr();
    bool result = event_watcher_.SyncWatch(stop_flags, 2);
    if (!weak_self)
      return false;

    top_watcher_state_ = outer_watcher_state;
    top_watcher_ = outer_watcher;
    return result;
  }

 private:
  // GenericSequenceLocalStorageSlot needs to be specified since
  // SequenceLocalStorageSlot doesn't support forward declared types.
  using StorageSlotType =
      base::GenericSequenceLocalStorageSlot<SequenceLocalState>;
  static StorageSlotType& GetStorageSlot() {
    static StorageSlotType storage;
    return storage;
  }

  void OnEventSignaled();

  // The shared event and watcher used for this sequence.
  base::WaitableEvent event_;
  mojo::SyncEventWatcher event_watcher_;

  // All SequenceLocalSyncEventWatchers on the current sequence have some state
  // registered here.
  WatcherStateMap registered_watchers_;

  // Tracks state of the top-most |SyncWatch()| invocation on the stack.
  raw_ptr<const SequenceLocalSyncEventWatcher> top_watcher_ = nullptr;
  raw_ptr<WatcherState> top_watcher_state_ = nullptr;

  // Set of all SequenceLocalSyncEventWatchers in a signaled state, guarded by
  // a lock for sequence-safe signaling.
  base::Lock ready_watchers_lock_;
  base::flat_set<raw_ptr<const SequenceLocalSyncEventWatcher, CtnExperimental>>
      ready_watchers_;

  base::WeakPtrFactory<SequenceLocalState> weak_ptr_factory_{this};
};

void SequenceLocalSyncEventWatcher::SequenceLocalState::OnEventSignaled() {
  for (;;) {
    base::flat_set<
        raw_ptr<const SequenceLocalSyncEventWatcher, CtnExperimental>>
        ready_watchers;
    {
      base::AutoLock lock(ready_watchers_lock_);
      std::swap(ready_watchers_, ready_watchers);
    }
    if (ready_watchers.empty()) {
      event_.Reset();
      return;
    }

    auto weak_self = weak_ptr_factory_.GetWeakPtr();
    for (const SequenceLocalSyncEventWatcher* watcher : ready_watchers) {
      if (top_watcher_ == watcher || watcher->can_wake_up_during_any_watch_) {
        watcher->callback_.Run();

        // The callback may have deleted |this|.
        if (!weak_self)
          return;
      }
    }
  }
}

// Manages a watcher's reference to the sequence-local state. This hides
// implementation details from the SequenceLocalSyncEventWatcher interface.
class SequenceLocalSyncEventWatcher::Registration {
 public:
  explicit Registration(const SequenceLocalSyncEventWatcher* watcher)
      : weak_shared_state_(SequenceLocalState::GetOrCreate()),
        shared_state_(weak_shared_state_.get()),
        watcher_state_iterator_(shared_state_->RegisterWatcher(watcher)),
        watcher_state_(watcher_state_iterator_->second) {}

  Registration(const Registration&) = delete;
  Registration& operator=(const Registration&) = delete;

  ~Registration() {
    if (weak_shared_state_) {
      // Because |this| may itself be owned by sequence- or thread-local storage
      // (e.g. if an interface binding lives there) we have no guarantee that
      // our SequenceLocalState's storage slot will still be alive during our
      // own destruction; so we have to guard against any access to it. Note
      // that this uncertainty only exists within the destructor and does not
      // apply to other methods on SequenceLocalSyncEventWatcher.
      //
      // May delete |shared_state_|.
      shared_state_->UnregisterWatcher(watcher_state_iterator_);
    }
  }

  SequenceLocalState* shared_state() const { return shared_state_; }
  WatcherState* watcher_state() { return watcher_state_.get(); }

 private:
  const base::WeakPtr<SequenceLocalState> weak_shared_state_;
  const raw_ptr<SequenceLocalState, AcrossTasksDanglingUntriaged> shared_state_;
  WatcherStateMap::iterator watcher_state_iterator_;
  const scoped_refptr<WatcherState> watcher_state_;
};

SequenceLocalSyncEventWatcher::SequenceLocalSyncEventWatcher(
    const base::RepeatingClosure& callback)
    : registration_(std::make_unique<Registration>(this)),
      callback_(callback) {}

SequenceLocalSyncEventWatcher::~SequenceLocalSyncEventWatcher() = default;

void SequenceLocalSyncEventWatcher::SignalEvent() {
  registration_->shared_state()->SignalForWatcher(this);
}

void SequenceLocalSyncEventWatcher::ResetEvent() {
  registration_->shared_state()->ResetForWatcher(this);
}

void SequenceLocalSyncEventWatcher::AllowWokenUpBySyncWatchOnSameSequence() {
  can_wake_up_during_any_watch_ = true;
}

bool SequenceLocalSyncEventWatcher::SyncWatch(const bool* should_stop) {
  // NOTE: |SyncWatch()| may delete |this|.
  return registration_->shared_state()->SyncWatch(
      this, registration_->watcher_state(), should_stop);
}

}  // namespace mojo

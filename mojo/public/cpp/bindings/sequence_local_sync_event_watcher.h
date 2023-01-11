// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SEQUENCE_LOCAL_SYNC_EVENT_WATCHER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SEQUENCE_LOCAL_SYNC_EVENT_WATCHER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace mojo {

// This encapsulates a SyncEventWatcher watching an event shared by all
// |SequenceLocalSyncEventWatcher| on the same sequence. This class is NOT
// sequence-safe in general, but |SignalEvent()| is safe to call from any
// sequence.
//
// Interfaces which support sync messages use a WaitableEvent to block and
// be signaled when messages are available, but having a WaitableEvent for every
// such interface endpoint would cause the number of WaitableEvents to grow
// arbitrarily large.
//
// Some platform constraints may limit the number of WaitableEvents the bindings
// layer can wait upon concurrently, so this type is used to keep the number
// of such events fixed at a small constant value per sequence regardless of the
// number of active interface endpoints supporting sync messages on that
// sequence.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) SequenceLocalSyncEventWatcher {
 public:
  explicit SequenceLocalSyncEventWatcher(
      const base::RepeatingClosure& callback);

  SequenceLocalSyncEventWatcher(const SequenceLocalSyncEventWatcher&) = delete;
  SequenceLocalSyncEventWatcher& operator=(
      const SequenceLocalSyncEventWatcher&) = delete;

  ~SequenceLocalSyncEventWatcher();

  // Signals the shared event on behalf of this specific watcher. Safe to call
  // from any sequence.
  void SignalEvent();

  // Resets the shared event on behalf of this specific watcher.
  void ResetEvent();

  // Allows this watcher to be notified during sync wait operations invoked by
  // other watchers (for example, other SequenceLocalSyncEventWatchers calling
  // |SyncWatch()|) on the same sequence.
  void AllowWokenUpBySyncWatchOnSameSequence();

  // Blocks the calling sequence until the shared event is signaled on behalf of
  // this specific watcher (i.e. until someone calls |SignalEvent()| on |this|).
  // Behaves similarly to SyncEventWatcher and SyncHandleWatcher, returning
  // |true| when |*should_stop| is set to |true|, or |false| if some other
  // (e.g. error) event interrupts the wait.
  bool SyncWatch(const bool* should_stop);

 private:
  class Registration;
  class SequenceLocalState;
  friend class SequenceLocalState;

  const std::unique_ptr<Registration> registration_;
  const base::RepeatingClosure callback_;
  bool can_wake_up_during_any_watch_ = false;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SEQUENCE_LOCAL_SYNC_EVENT_WATCHER_H_

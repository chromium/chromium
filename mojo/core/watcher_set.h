// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_WATCHER_SET_H_
#define MOJO_CORE_WATCHER_SET_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "mojo/core/handle_signals_state.h"
#include "mojo/core/watcher_dispatcher.h"

#ifdef OS_MAC
extern "C" int V8RecordReplayPointerId(void* ptr);
extern "C" bool V8IsRecordingOrReplaying();
#endif

namespace mojo {
namespace core {

struct CompareRecordReplayPointerId {
  template <typename T>
  bool operator()(const T* a, const T* b) const {
#ifdef OS_MAC
    if (V8IsRecordingOrReplaying()) {
      int ida = V8RecordReplayPointerId((void*)a);
      int idb = V8RecordReplayPointerId((void*)b);
      CHECK(ida && idb);
      return ida < idb;
    }
#endif
    return (uintptr_t)a < (uintptr_t)b;
  }
};

// A WatcherSet maintains a set of references to WatcherDispatchers to be
// notified when a handle changes state.
//
// Dispatchers which may be watched by a watcher should own a WatcherSet and
// notify it of all relevant state changes.
class WatcherSet {
 public:
  // |owner| is the Dispatcher who owns this WatcherSet.
  explicit WatcherSet(Dispatcher* owner);
  ~WatcherSet();

  // Notifies all watchers of the handle's current signals state.
  void NotifyState(const HandleSignalsState& state);

  // Notifies all watchers that this handle has been closed.
  void NotifyClosed();

  // Adds a new watcher+context.
  MojoResult Add(const scoped_refptr<WatcherDispatcher>& watcher,
                 uintptr_t context,
                 const HandleSignalsState& current_state);

  // Removes a watcher+context.
  MojoResult Remove(WatcherDispatcher* watcher, uintptr_t context);

 private:
  using ContextSet = std::set<uintptr_t>;

  struct Entry {
    Entry(const scoped_refptr<WatcherDispatcher>& dispatcher);
    Entry(Entry&& other);
    ~Entry();

    Entry& operator=(Entry&& other);

    scoped_refptr<WatcherDispatcher> dispatcher;
    ContextSet contexts;

   private:
    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  Dispatcher* const owner_;
  base::flat_map<WatcherDispatcher*, Entry, CompareRecordReplayPointerId> watchers_;
  base::Optional<HandleSignalsState> last_known_state_;

  DISALLOW_COPY_AND_ASSIGN(WatcherSet);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_WATCHER_SET_H_

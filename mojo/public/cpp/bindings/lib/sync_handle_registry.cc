// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_handle_registry.h"

#include <algorithm>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/c/system/core.h"

namespace mojo {

// static
scoped_refptr<SyncHandleRegistry> SyncHandleRegistry::current() {
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<scoped_refptr<SyncHandleRegistry>>>
      g_current_sync_handle_watcher;

  // SyncMessageFilter can be used on threads without sequence-local storage
  // being available. Those receive a unique, standalone SyncHandleRegistry.
  if (!base::SequencedTaskRunnerHandle::IsSet())
    return new SyncHandleRegistry();

  if (!*g_current_sync_handle_watcher)
    g_current_sync_handle_watcher->emplace(new SyncHandleRegistry());
  return *g_current_sync_handle_watcher->GetValuePointer();
}

bool SyncHandleRegistry::RegisterHandle(const Handle& handle,
                                        MojoHandleSignals handle_signals,
                                        HandleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::Contains(handles_, handle))
    return false;

  MojoResult result = wait_set_.AddHandle(handle, handle_signals);
  if (result != MOJO_RESULT_OK)
    return false;

  handles_[handle] = std::move(callback);
  return true;
}

void SyncHandleRegistry::UnregisterHandle(const Handle& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::Contains(handles_, handle))
    return;

  MojoResult result = wait_set_.RemoveHandle(handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  handles_.erase(handle);
}

void SyncHandleRegistry::RegisterEvent(base::WaitableEvent* event,
                                       base::RepeatingClosure callback) {
  auto it = events_.find(event);
  if (it == events_.end()) {
    auto result = events_.emplace(event, EventCallbackList{});
    it = result.first;
  }

  // The event may already be in the WaitSet, but we don't care. This will be a
  // no-op in that case, which is more efficient than scanning the list of
  // callbacks to see if any are valid.
  wait_set_.AddEvent(event);

  it->second.container().push_back(std::move(callback));
}

void SyncHandleRegistry::UnregisterEvent(base::WaitableEvent* event,
                                         base::RepeatingClosure callback) {
  auto it = events_.find(event);
  if (it == events_.end())
    return;

  bool has_valid_callbacks = false;
  auto& callbacks = it->second.container();
  if (is_dispatching_event_callbacks_) {
    // Not safe to remove any elements from |callbacks| here since an outer
    // stack frame is currently iterating over it in Wait().
    for (auto& cb : callbacks) {
      if (cb == callback)
        cb.Reset();
      else if (cb)
        has_valid_callbacks = true;
    }
    remove_invalid_event_callbacks_after_dispatch_ = true;
  } else {
    callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), callback),
                    callbacks.end());
    if (callbacks.empty())
      events_.erase(it);
    else
      has_valid_callbacks = true;
  }

  if (!has_valid_callbacks) {
    // Regardless of whether or not we're nested within a Wait(), we need to
    // ensure that |event| is removed from the WaitSet before returning if this
    // was the last callback registered for it.
    MojoResult rv = wait_set_.RemoveEvent(event);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
  }
}

bool SyncHandleRegistry::Wait(const bool* should_stop[], size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t num_ready_handles;
  Handle ready_handle;
  MojoResult ready_handle_result;

  scoped_refptr<SyncHandleRegistry> preserver(this);
  while (true) {
    for (size_t i = 0; i < count; ++i) {
      if (*should_stop[i])
        return true;
    }

    // TODO(yzshen): Theoretically it can reduce sync call re-entrancy if we
    // give priority to the handle that is waiting for sync response.
    base::WaitableEvent* ready_event = nullptr;
    num_ready_handles = 1;
    wait_set_.Wait(&ready_event, &num_ready_handles, &ready_handle,
                   &ready_handle_result);
    if (num_ready_handles) {
      DCHECK_EQ(1u, num_ready_handles);
      const auto iter = handles_.find(ready_handle);
      iter->second.Run(ready_handle_result);
    }

    if (ready_event) {
      const auto iter = events_.find(ready_event);
      DCHECK(iter != events_.end());
      bool was_dispatching_event_callbacks = is_dispatching_event_callbacks_;
      is_dispatching_event_callbacks_ = true;

      // NOTE: It's possible for the container to be extended by any of these
      // callbacks if they call RegisterEvent, so we are careful to iterate by
      // index. Also note that conversely, elements cannot be *removed* from the
      // container, by any of these callbacks, so it is safe to assume the size
      // only stays the same or increases, with no elements changing position.
      auto& callbacks = iter->second.container();
      for (size_t i = 0; i < callbacks.size(); ++i) {
        auto& callback = callbacks[i];
        if (callback)
          callback.Run();
      }

      is_dispatching_event_callbacks_ = was_dispatching_event_callbacks;
      if (!was_dispatching_event_callbacks &&
          remove_invalid_event_callbacks_after_dispatch_) {
        // If we've had events unregistered within any callback dispatch, now is
        // a good time to prune them from the map.
        RemoveInvalidEventCallbacks();
        remove_invalid_event_callbacks_after_dispatch_ = false;
      }
    }
  };

  return false;
}

SyncHandleRegistry::SyncHandleRegistry() = default;

SyncHandleRegistry::~SyncHandleRegistry() = default;

void SyncHandleRegistry::RemoveInvalidEventCallbacks() {
  for (auto it = events_.begin(); it != events_.end();) {
    auto& callbacks = it->second.container();
    callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(),
                                   [](const base::RepeatingClosure& callback) {
                                     return !callback;
                                   }),
                    callbacks.end());
    if (callbacks.empty())
      events_.erase(it++);
    else
      ++it;
  }
}

}  // namespace mojo

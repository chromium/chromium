// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/bindings/sync_handle_registry.h"

#include <algorithm>
#include <map>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/not_fatal_until.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/types/pass_key.h"
#include "mojo/public/c/system/core.h"

namespace mojo {

SyncHandleRegistry::Subscription::Subscription(base::OnceClosure remove_closure,
                                               EventCallbackList* callbacks,
                                               EventCallback event_callback)
    : remove_runner_(std::move(remove_closure)),
      subscription_(callbacks->Add(std::move(event_callback))) {}

SyncHandleRegistry::Subscription::Subscription(Subscription&&) = default;

SyncHandleRegistry::Subscription& SyncHandleRegistry::Subscription::operator=(
    Subscription&&) = default;

SyncHandleRegistry::Subscription::~Subscription() = default;

// static
scoped_refptr<SyncHandleRegistry> SyncHandleRegistry::current() {
  static base::SequenceLocalStorageSlot<scoped_refptr<SyncHandleRegistry>>
      g_current_sync_handle_watcher;

  // SyncMessageFilter can be used on threads without sequence-local storage
  // being available. Those receive a unique, standalone SyncHandleRegistry.
  if (!base::SequencedTaskRunner::HasCurrentDefault()) {
    return base::MakeRefCounted<SyncHandleRegistry>(
        base::PassKey<SyncHandleRegistry>());
  }

  if (!g_current_sync_handle_watcher) {
    g_current_sync_handle_watcher.emplace(
        base::MakeRefCounted<SyncHandleRegistry>(
            base::PassKey<SyncHandleRegistry>()));
  }
  return *g_current_sync_handle_watcher.GetValuePointer();
}

SyncHandleRegistry::SyncHandleRegistry(base::PassKey<SyncHandleRegistry>) {}

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

SyncHandleRegistry::EventCallbackSubscription SyncHandleRegistry::RegisterEvent(
    base::WaitableEvent* event,
    EventCallback callback) {
  auto it = events_.find(event);
  if (it == events_.end()) {
    auto result = events_.emplace(event, std::make_unique<EventCallbackList>());
    it = result.first;
  }

  // The event may already be in the WaitSet, but we don't care. This will be a
  // no-op in that case, which is more efficient than scanning the list of
  // callbacks to see if any are valid.
  wait_set_.AddEvent(event);

  // Return an object that will synchronously clear the entry for |event| when
  // its last callback is destroyed.
  const auto remove_closure = [](EventCallbackList* callbacks,
                                 WaitSet* wait_set,
                                 base::WaitableEvent* event) {
    // |callbacks| is guaranteed to be valid here. The callbacks are repeating
    // and are thus only removed by their subscriptions being destroyed, so it's
    // impossible for empty() to be true until the last subscription has been
    // destroyed.  Since Wait() only deletes a callback list once it's empty,
    // and this callback runs synchronously with subscription destruction, it's
    // impossible for |callbacks| to be deleted before this gets to run at the
    // destruction of the last remaining subscription.
    if (callbacks->empty()) {
      // If this was the last callback registered for |event|, ensure that it's
      // removed from the WaitSet before returning. Otherwise a nested Wait()
      // call inside the scope that destroys the subscription will fail.
      const MojoResult rv = wait_set->RemoveEvent(event);
      DCHECK_EQ(MOJO_RESULT_OK, rv);
    }
  };
  return std::make_unique<Subscription>(
      base::BindOnce(remove_closure, it->second.get(), &wait_set_, event),
      it->second.get(), std::move(callback));
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
      CHECK(iter != events_.end(), base::NotFatalUntil::M130);

      {
        base::AutoReset<bool> in_nested_wait(&in_nested_wait_, true);
        iter->second->Notify();
      }

      // Notify() above may have both added and removed event registrations, for
      // any event.  If we're in the outermost frame, prune any empty map
      // entries to avoid unbounded growth.
      if (!in_nested_wait_) {
        std::erase_if(events_,
                      [](const auto& entry) { return entry.second->empty(); });
      }
    }
  }
}

SyncHandleRegistry::~SyncHandleRegistry() = default;

}  // namespace mojo

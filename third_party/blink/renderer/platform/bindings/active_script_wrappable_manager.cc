// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_manager.h"

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

bool ScriptWrappableIsActive(const ActiveScriptWrappableBase& asw) {
  // A wrapper isn't kept alive after its ExecutionContext becomes detached,
  // even if |HasPendingActivity()| returns |true|. This measure avoids
  // memory leaks and has proven not to be too eager wrt garbage collection
  // of objects belonging to discarded browser contexts (
  // https://html.spec.whatwg.org/C/#a-browsing-context-is-discarded )
  //
  // Consequently, an implementation of |HasPendingActivity()| is not
  // required to take the detached state of the associated ExecutionContext
  // into account (i.e., return |false|.) We probe the detached state of the
  // ExecutionContext via |IsContextDestroyed()|.
  if (asw.IsContextDestroyed())
    return false;

  return asw.HasPendingActivity();
}

}  // namespace

void ActiveScriptWrappableManager::RecomputeActiveScriptWrappables(
    RecomputeMode mode) {
  if (mode == RecomputeMode::kOpportunistic && recomputed_cnt_ > 0)
    return;
  ThreadState::NoAllocationScope no_allocations(ThreadState::Current());
  for (auto& pair : active_script_wrappables_) {
    pair.second =
        ScriptWrappableIsActive(*pair.first) ? pair.first.Get() : nullptr;
  }
  recomputed_cnt_++;
}

void ActiveScriptWrappableManager::
    CleanupInactiveAndClearActiveScriptWrappables(
        const LivenessBroker& broker) {
  active_script_wrappables_.erase(
      std::remove_if(
          active_script_wrappables_.begin(), active_script_wrappables_.end(),
          [broker](auto& pair) {
            // If the ASW is not alive, the Member reference must be nullptr.
            DCHECK(broker.IsHeapObjectAlive(pair.first) || !pair.second);
            return !broker.IsHeapObjectAlive(pair.first);
          }),
      active_script_wrappables_.end());
  recomputed_cnt_ = 0;
}

void ActiveScriptWrappableManager::Trace(Visitor* visitor) const {
  visitor->Trace(active_script_wrappables_);
  visitor->RegisterWeakCallbackMethod<
      ActiveScriptWrappableManager,
      &ActiveScriptWrappableManager::
          CleanupInactiveAndClearActiveScriptWrappables>(this);
}

}  // namespace blink

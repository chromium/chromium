// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_transition.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {
NavigationTransition::NavigationTransition(
    ExecutionContext* context,
    V8NavigationType::Enum navigation_type,
    NavigationHistoryEntry* from)
    : navigation_type_(navigation_type),
      from_(from),
      finished_(MakeGarbageCollected<FinishedProperty>(context)) {
  // See comment for the finished promise in navigation_api_method_tracker.cc
  // for the reason why we mark finished promises as handled.
  finished_->MarkAsHandled();
}

ScriptPromise<IDLUndefined> NavigationTransition::finished(
    ScriptState* script_state) {
  return finished_->Promise(script_state->World());
}

void NavigationTransition::ResolveFinishedPromise() {
  finished_->ResolveWithUndefined();
}

void NavigationTransition::RejectFinishedPromise(ScriptValue ex) {
  finished_->Reject(ex);
}

void NavigationTransition::Trace(Visitor* visitor) const {
  visitor->Trace(from_);
  visitor->Trace(finished_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

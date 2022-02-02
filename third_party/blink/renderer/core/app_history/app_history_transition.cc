// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history_transition.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/app_history/app_history_entry.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {
AppHistoryTransition::AppHistoryTransition(ScriptState* script_state,
                                           const String& navigation_type,
                                           AppHistoryEntry* from)
    : navigation_type_(navigation_type),
      from_(from),
      finished_(MakeGarbageCollected<FinishedProperty>(
          ExecutionContext::From(script_state))) {
  // See comment for the finished promise in app_history_api_navigation.cc for
  // the reason why we mark finished promises as handled.
  finished_->MarkAsHandled();
}

ScriptPromise AppHistoryTransition::finished(ScriptState* script_state) {
  return finished_->Promise(script_state->World());
}

void AppHistoryTransition::ResolveFinishedPromise() {
  finished_->ResolveWithUndefined();
}

void AppHistoryTransition::RejectFinishedPromise(ScriptValue ex) {
  finished_->Reject(ex);
}

void AppHistoryTransition::Trace(Visitor* visitor) const {
  visitor->Trace(from_);
  visitor->Trace(finished_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

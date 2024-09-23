// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_api_method_tracker.h"

#include "base/check_op.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_result.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

NavigationApiMethodTracker::NavigationApiMethodTracker(
    ScriptState* script_state,
    NavigationOptions* options,
    const String& key,
    scoped_refptr<SerializedScriptValue> state)
    : serialized_state_(std::move(state)),
      info_(options->getInfoOr(
          ScriptValue(script_state->GetIsolate(),
                      v8::Undefined(script_state->GetIsolate())))),
      key_(key),
      committed_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver<NavigationHistoryEntry>>(
              script_state)),
      finished_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver<NavigationHistoryEntry>>(
              script_state)),
      result_(NavigationResult::Create()) {
  result_->setCommitted(committed_resolver_->Promise());
  result_->setFinished(finished_resolver_->Promise());

  // The web developer doesn't necessarily care about finished promise
  // rejections:
  // * They could be listening to other transition-failure signals, like the
  // navigateerror event, or navigation.transition.finished.
  // * They could be doing synchronous navigations within the same task, in
  // which case the second will always abort the first (causing a rejected
  // finished promise), but they might not care
  // * If the committed promise rejects, finished will also reject in the same
  // way, so any commit failures will already be signaled and saying that you
  // also have to handle the finished promise is frustrating.
  //
  // As such, we mark it as handled to avoid unhandled rejection events.
  finished_resolver_->Promise().MarkAsHandled();
}

void NavigationApiMethodTracker::NotifyAboutTheCommittedToEntry(
    NavigationHistoryEntry* entry,
    WebFrameLoadType type) {
  CHECK_EQ(committed_to_entry_, nullptr);
  committed_to_entry_ = entry;

  if (type != WebFrameLoadType::kBackForward) {
    committed_to_entry_->SetAndSaveState(std::move(serialized_state_));
  }

  committed_resolver_->Resolve(committed_to_entry_);
}

void NavigationApiMethodTracker::ResolveFinishedPromise() {
  finished_resolver_->Resolve(committed_to_entry_);
}

void NavigationApiMethodTracker::RejectFinishedPromise(
    const ScriptValue& value) {
  if (committed_resolver_) {
    // We never hit NotifyAboutTheCommittedToEntry(), so we should reject that
    // too.
    committed_resolver_->Reject(value);
  }

  finished_resolver_->Reject(value);
  serialized_state_.reset();
}

void NavigationApiMethodTracker::CleanupForWillNeverSettle() {
  CHECK_EQ(committed_to_entry_, nullptr);
  committed_resolver_->Detach();
  finished_resolver_->Detach();
  serialized_state_.reset();
}

void NavigationApiMethodTracker::Trace(Visitor* visitor) const {
  visitor->Trace(info_);
  visitor->Trace(committed_to_entry_);
  visitor->Trace(committed_resolver_);
  visitor->Trace(finished_resolver_);
  visitor->Trace(result_);
}

}  // namespace blink

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/app_history/app_history_api_navigation.h"

#include "base/check_op.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_navigation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_app_history_result.h"
#include "third_party/blink/renderer/core/app_history/app_history.h"
#include "third_party/blink/renderer/core/app_history/app_history_entry.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

AppHistoryApiNavigation::AppHistoryApiNavigation(
    ScriptState* script_state,
    AppHistory* app_history,
    AppHistoryNavigationOptions* options,
    const String& key,
    scoped_refptr<SerializedScriptValue> state)
    : serialized_state_(std::move(state)),
      info_(options->getInfoOr(
          ScriptValue(script_state->GetIsolate(),
                      v8::Undefined(script_state->GetIsolate())))),
      key_(key),
      app_history_(app_history),
      committed_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      finished_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      result_(AppHistoryResult::Create()) {
  result_->setCommitted(committed_resolver_->Promise());
  result_->setFinished(finished_resolver_->Promise());
}

void AppHistoryApiNavigation::NotifyAboutTheCommittedToEntry(
    AppHistoryEntry* entry) {
  DCHECK_EQ(committed_to_entry_, nullptr);
  committed_to_entry_ = entry;

  committed_resolver_->Resolve(committed_to_entry_);

  if (did_finish_before_commit_) {
    ResolveFinishedPromise();
  }
}

void AppHistoryApiNavigation::ResolveFinishedPromise() {
  if (!app_history_)
    return;

  if (!committed_to_entry_) {
    did_finish_before_commit_ = true;
    return;
  }

  finished_resolver_->Resolve(committed_to_entry_);

  app_history_->CleanupApiNavigation(*this);
  app_history_ = nullptr;
}

void AppHistoryApiNavigation::RejectFinishedPromise(const ScriptValue& value) {
  if (!app_history_)
    return;

  finished_resolver_->Reject(value);

  if (committed_resolver_) {
    // We never hit NotifyAboutTheCommittedToEntry(), so we should reject that
    // too.
    committed_resolver_->Reject(value);
  }

  serialized_state_.reset();

  app_history_->CleanupApiNavigation(*this);
  app_history_ = nullptr;
}

void AppHistoryApiNavigation::CleanupForCrossDocument() {
  DCHECK_EQ(committed_to_entry_, nullptr);

  committed_resolver_->Detach();
  finished_resolver_->Detach();

  serialized_state_.reset();

  app_history_->CleanupApiNavigation(*this);
  app_history_ = nullptr;
}

void AppHistoryApiNavigation::Trace(Visitor* visitor) const {
  visitor->Trace(info_);
  visitor->Trace(app_history_);
  visitor->Trace(committed_to_entry_);
  visitor->Trace(committed_resolver_);
  visitor->Trace(finished_resolver_);
  visitor->Trace(result_);
}

}  // namespace blink

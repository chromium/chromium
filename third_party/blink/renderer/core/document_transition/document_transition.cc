// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {
namespace {

const int32_t kDefaultDurationMs = 300;

DocumentTransition::Request::Effect ParseEffect(const String& input) {
  using MapType = HashMap<String, DocumentTransition::Request::Effect>;
  DEFINE_STATIC_LOCAL(
      MapType*, lookup_map,
      (new MapType{
          {"cover-down", DocumentTransition::Request::Effect::kCoverDown},
          {"cover-left", DocumentTransition::Request::Effect::kCoverLeft},
          {"cover-right", DocumentTransition::Request::Effect::kCoverRight},
          {"cover-up", DocumentTransition::Request::Effect::kCoverUp},
          {"explode", DocumentTransition::Request::Effect::kExplode},
          {"fade", DocumentTransition::Request::Effect::kFade},
          {"implode", DocumentTransition::Request::Effect::kImplode},
          {"reveal-down", DocumentTransition::Request::Effect::kRevealDown},
          {"reveal-left", DocumentTransition::Request::Effect::kRevealLeft},
          {"reveal-right", DocumentTransition::Request::Effect::kRevealRight},
          {"reveal-up", DocumentTransition::Request::Effect::kRevealUp}}));

  auto it = lookup_map->find(input);
  return it != lookup_map->end() ? it->value
                                 : DocumentTransition::Request::Effect::kNone;
}

}  // namespace

DocumentTransition::DocumentTransition(Document* document)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      document_(document) {}

void DocumentTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(prepare_promise_resolver_);

  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void DocumentTransition::ContextDestroyed() {
  if (prepare_promise_resolver_) {
    prepare_promise_resolver_->Detach();
    prepare_promise_resolver_ = nullptr;
  }
}

bool DocumentTransition::HasPendingActivity() const {
  if (prepare_promise_resolver_)
    return true;
  return false;
}

ScriptPromise DocumentTransition::prepare(
    ScriptState* script_state,
    const DocumentTransitionInit* params) {
  // Reject any previous prepare promises.
  if (state_ == State::kPreparing || state_ == State::kPrepared) {
    if (prepare_promise_resolver_) {
      prepare_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "Aborted due to prepare() call"));
      prepare_promise_resolver_ = nullptr;
    }
    state_ = State::kIdle;
  }

  // Increment the sequence id before any early outs so we will correctly
  // process callbacks from previous requests.
  ++prepare_sequence_id_;

  // If we are not attached to a view, then we can't prepare a transition.
  // Reject the promise. We also reject the promise if we're in any state other
  // than idle.
  if (!document_ || !document_->View() || state_ != State::kIdle) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Invalid state"));
  }

  // We're going to be creating a new transition, initialize the params.
  ParseAndSetTransitionParameters(params);

  prepare_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  state_ = State::kPreparing;
  pending_request_ = Request::CreatePrepare(
      effect_, duration_,
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          &DocumentTransition::NotifyPrepareCommitted,
          WrapCrossThreadWeakPersistent(this), prepare_sequence_id_)));

  NotifyHasChangesToCommit();
  return prepare_promise_resolver_->Promise();
}

void DocumentTransition::start() {
  if (state_ != State::kPrepared)
    return;

  state_ = State::kStarted;
  pending_request_ = Request::CreateStart(ConvertToBaseOnceCallback(
      CrossThreadBindOnce(&DocumentTransition::NotifyStartCommitted,
                          WrapCrossThreadWeakPersistent(this))));
}

void DocumentTransition::NotifyHasChangesToCommit() {
  if (!document_ || !document_->GetPage() || !document_->View())
    return;

  // Schedule a new frame.
  document_->GetPage()->Animator().ScheduleVisualUpdate(document_->GetFrame());

  // Ensure paint artifact compositor does an update, since that's the mechanism
  // we use to pass transition requests to the compositor.
  document_->View()->SetPaintArtifactCompositorNeedsUpdate();
}

void DocumentTransition::NotifyPrepareCommitted(uint32_t sequence_id) {
  // This notification is for a different sequence id.
  if (sequence_id != prepare_sequence_id_)
    return;

  DCHECK(state_ == State::kPreparing);
  DCHECK(prepare_promise_resolver_);

  prepare_promise_resolver_->Resolve();
  prepare_promise_resolver_ = nullptr;
  state_ = State::kPrepared;
}

void DocumentTransition::NotifyStartCommitted() {
  // TODO(vmpstr): This should only be cleared when the animation is actually
  // over which means we need to plumb the callback all the way to viz.
  state_ = State::kIdle;
}

std::unique_ptr<DocumentTransition::Request>
DocumentTransition::TakePendingRequest() {
  return std::move(pending_request_);
}

void DocumentTransition::ParseAndSetTransitionParameters(
    const DocumentTransitionInit* params) {
  duration_ = base::TimeDelta::FromMilliseconds(
      params->hasDuration() ? params->duration() : kDefaultDurationMs);
  effect_ = params->hasRootTransition() ? ParseEffect(params->rootTransition())
                                        : Request::Effect::kNone;
}

}  // namespace blink

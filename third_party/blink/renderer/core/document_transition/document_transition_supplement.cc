// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"

#include "cc/document_transition/document_transition_request.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_callback.h"
#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

namespace blink {

// FinishedResolved implementation.
DocumentTransitionSupplement::FinishedResolved::FinishedResolved(
    DocumentTransitionSupplement* supplement,
    DocumentTransition* transition,
    Document* document)
    : supplement_(supplement), transition_(transition), document_(document) {}

DocumentTransitionSupplement::FinishedResolved::~FinishedResolved() = default;

ScriptValue DocumentTransitionSupplement::FinishedResolved::Call(
    ScriptState* script_state,
    ScriptValue value) {
  if (supplement_)
    supplement_->ResetTransition(transition_);
  return ScriptValue();
}

void DocumentTransitionSupplement::FinishedResolved::Trace(
    Visitor* visitor) const {
  ScriptFunction::Callable::Trace(visitor);
  visitor->Trace(supplement_);
  visitor->Trace(transition_);
  visitor->Trace(document_);
}

// static
const char DocumentTransitionSupplement::kSupplementName[] =
    "DocumentTransition";

// static
DocumentTransitionSupplement* DocumentTransitionSupplement::FromIfExists(
    const Document& document) {
  return Supplement<Document>::From<DocumentTransitionSupplement>(document);
}

// static
DocumentTransitionSupplement* DocumentTransitionSupplement::From(
    Document& document) {
  auto* supplement =
      Supplement<Document>::From<DocumentTransitionSupplement>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<DocumentTransitionSupplement>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return supplement;
}

// static
DocumentTransition* DocumentTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    V8DocumentTransitionCallback* callback,
    ExceptionState& exception_state) {
  auto* supplement = From(document);
  return supplement->StartTransition(script_state, document, callback,
                                     exception_state);
}

DocumentTransition* DocumentTransitionSupplement::StartTransition(
    ScriptState* script_state,
    Document& document,
    V8DocumentTransitionCallback* callback,
    ExceptionState& exception_state) {
  if (transition_)
    transition_->skipTransition();

  transition_ = MakeGarbageCollected<DocumentTransition>(
      &document, script_state, callback, this);

  auto* finished_callable =
      MakeGarbageCollected<FinishedResolved>(this, transition_, &document);
  transition_->finished().Then(
      MakeGarbageCollected<ScriptFunction>(script_state, finished_callable),
      MakeGarbageCollected<ScriptFunction>(script_state, finished_callable));
  return transition_;
}

void DocumentTransitionSupplement::ResetTransition(
    DocumentTransition* transition) {
  // TODO(vmpstr): Do we need to explicitly reset transition state?
  if (transition == transition_)
    transition_ = nullptr;
}

DocumentTransition* DocumentTransitionSupplement::GetActiveTransition() {
  return transition_;
}

DocumentTransitionSupplement::DocumentTransitionSupplement(Document& document)
    : Supplement<Document>(document) {}

void DocumentTransitionSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(transition_);

  Supplement<Document>::Trace(visitor);
}

void DocumentTransitionSupplement::AddPendingRequest(
    std::unique_ptr<DocumentTransitionRequest> request) {
  pending_requests_.push_back(std::move(request));

  auto* document = GetSupplementable();
  if (!document || !document->GetPage() || !document->View())
    return;

  // Schedule a new frame.
  document->View()->ScheduleAnimation();

  // Ensure paint artifact compositor does an update, since that's the mechanism
  // we use to pass transition requests to the compositor.
  document->View()->SetPaintArtifactCompositorNeedsUpdate(
      PaintArtifactCompositorUpdateReason::kDocumentTransitionNotifyChanges);
}

VectorOf<std::unique_ptr<DocumentTransitionRequest>>
DocumentTransitionSupplement::TakePendingRequests() {
  return std::move(pending_requests_);
}

}  // namespace blink

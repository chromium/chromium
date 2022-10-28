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
  // TODO(khushalsagar): Script initiates a transition request during
  // navigation?
  if (transition_ && transition_->IsForNavigationSnapshot())
    return nullptr;

  if (transition_)
    transition_->skipTransition();
  DCHECK(!transition_)
      << "skipTransition() should finish existing |transition_|";

  transition_ = DocumentTransition::CreateFromScript(&document, script_state,
                                                     callback, this);
  return transition_;
}

// static
void DocumentTransitionSupplement::SnapshotDocumentForNavigation(
    Document& document,
    DocumentTransition::ViewTransitionStateCallback callback) {
  auto* supplement = From(document);
  supplement->StartTransition(document, std::move(callback));
}

void DocumentTransitionSupplement::StartTransition(
    Document& document,
    DocumentTransition::ViewTransitionStateCallback callback) {
  if (transition_) {
    DCHECK(!transition_->IsForNavigationSnapshot());
    transition_->skipTransition();
  }
  DCHECK(!transition_)
      << "skipTransition() should finish existing |transition_|";

  transition_ = DocumentTransition::CreateForSnapshotForNavigation(
      &document, std::move(callback), this);
}

// static
void DocumentTransitionSupplement::CreateFromSnapshotForNavigation(
    Document& document,
    ViewTransitionState transition_state) {
  auto* supplement = From(document);
  supplement->StartTransition(document, std::move(transition_state));
}

void DocumentTransitionSupplement::StartTransition(
    Document& document,
    ViewTransitionState transition_state) {
  DCHECK(!transition_) << "Existing transition on new Document";
  transition_ = DocumentTransition::CreateFromSnapshotForNavigation(
      &document, std::move(transition_state), this);
}

void DocumentTransitionSupplement::OnTransitionFinished(
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

DocumentTransitionSupplement::~DocumentTransitionSupplement() = default;

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

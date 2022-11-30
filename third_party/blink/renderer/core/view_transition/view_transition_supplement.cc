// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"

#include "cc/trees/layer_tree_host.h"
#include "cc/view_transition/view_transition_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

namespace blink {
namespace {

bool HasActiveTransitionInAncestorFrame(LocalFrame* frame) {
  auto* parent = frame ? frame->Parent() : nullptr;

  while (parent && parent->IsLocalFrame()) {
    if (To<LocalFrame>(parent)->GetDocument() &&
        ViewTransitionUtils::GetActiveTransition(
            *To<LocalFrame>(parent)->GetDocument())) {
      return true;
    }

    parent = parent->Parent();
  }

  return false;
}

// Skips transitions in all local frames underneath |curr_frame|'s local root
// except |curr_frame| itself.
void SkipTransitionInAllLocalFrames(LocalFrame* curr_frame) {
  auto* root_view = curr_frame ? curr_frame->LocalFrameRoot().View() : nullptr;
  if (!root_view)
    return;

  root_view->ForAllChildLocalFrameViews([curr_frame](LocalFrameView& child) {
    if (child.GetFrame() == *curr_frame)
      return;

    auto* document = child.GetFrame().GetDocument();
    auto* transition = document
                           ? ViewTransitionUtils::GetActiveTransition(*document)
                           : nullptr;
    if (!transition)
      return;

    transition->skipTransition();
    DCHECK(!ViewTransitionUtils::GetActiveTransition(*document));
  });
}

}  // namespace

// static
const char ViewTransitionSupplement::kSupplementName[] = "ViewTransition";

// static
ViewTransitionSupplement* ViewTransitionSupplement::FromIfExists(
    const Document& document) {
  return Supplement<Document>::From<ViewTransitionSupplement>(document);
}

// static
ViewTransitionSupplement* ViewTransitionSupplement::From(Document& document) {
  auto* supplement =
      Supplement<Document>::From<ViewTransitionSupplement>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<ViewTransitionSupplement>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return supplement;
}

// static
ViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    V8ViewTransitionCallback* callback,
    ExceptionState& exception_state) {
  auto* supplement = From(document);
  return supplement->StartTransition(script_state, document, callback,
                                     exception_state);
}

ViewTransition* ViewTransitionSupplement::StartTransition(
    ScriptState* script_state,
    Document& document,
    V8ViewTransitionCallback* callback,
    ExceptionState& exception_state) {
  // Disallow script initiated transitions during a navigation initiated
  // transition.
  if (transition_ && !transition_->IsCreatedViaScriptAPI())
    return nullptr;

  if (transition_)
    transition_->skipTransition();
  DCHECK(!transition_)
      << "skipTransition() should finish existing |transition_|";

  transition_ =
      ViewTransition::CreateFromScript(&document, script_state, callback, this);

  // If there is a transition in a parent frame, give that precedence over a
  // transition in a child frame.
  if (HasActiveTransitionInAncestorFrame(document.GetFrame())) {
    auto skipped_transition = transition_;
    skipped_transition->skipTransition();

    DCHECK(!transition_);
    return skipped_transition;
  }

  // Skip transitions in all frames associated with this widget. We can only
  // have one transition per widget/CC.
  SkipTransitionInAllLocalFrames(document.GetFrame());
  DCHECK(transition_);

  return transition_;
}

// static
void ViewTransitionSupplement::SnapshotDocumentForNavigation(
    Document& document,
    ViewTransition::ViewTransitionStateCallback callback) {
  DCHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  auto* supplement = From(document);
  supplement->StartTransition(document, std::move(callback));
}

void ViewTransitionSupplement::StartTransition(
    Document& document,
    ViewTransition::ViewTransitionStateCallback callback) {
  if (transition_) {
    // We should skip a transition if one exists, regardless of how it was
    // created, since navigation transition takes precedence.
    transition_->skipTransition();
  }
  DCHECK(!transition_)
      << "skipTransition() should finish existing |transition_|";

  transition_ = ViewTransition::CreateForSnapshotForNavigation(
      &document, std::move(callback), this);
}

// static
void ViewTransitionSupplement::CreateFromSnapshotForNavigation(
    Document& document,
    ViewTransitionState transition_state) {
  DCHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  auto* supplement = From(document);
  supplement->StartTransition(document, std::move(transition_state));
}

void ViewTransitionSupplement::StartTransition(
    Document& document,
    ViewTransitionState transition_state) {
  DCHECK(!transition_) << "Existing transition on new Document";
  transition_ = ViewTransition::CreateFromSnapshotForNavigation(
      &document, std::move(transition_state), this);
}

void ViewTransitionSupplement::OnTransitionFinished(
    ViewTransition* transition) {
  // TODO(vmpstr): Do we need to explicitly reset transition state?
  if (transition == transition_)
    transition_ = nullptr;
}

ViewTransition* ViewTransitionSupplement::GetActiveTransition() {
  return transition_;
}

void ViewTransitionSupplement::UpdateViewTransitionNames(
    const Element& element,
    const ComputedStyle* style) {
  if (style && style->ViewTransitionName())
    elements_with_view_transition_name_.insert(&element);
  else
    elements_with_view_transition_name_.erase(&element);
}

ViewTransitionSupplement::ViewTransitionSupplement(Document& document)
    : Supplement<Document>(document) {}

ViewTransitionSupplement::~ViewTransitionSupplement() = default;

void ViewTransitionSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(transition_);
  visitor->Trace(elements_with_view_transition_name_);

  Supplement<Document>::Trace(visitor);
}

void ViewTransitionSupplement::AddPendingRequest(
    std::unique_ptr<ViewTransitionRequest> request) {
  pending_requests_.push_back(std::move(request));

  auto* document = GetSupplementable();
  if (!document || !document->GetPage() || !document->View())
    return;

  // Schedule a new frame.
  document->View()->ScheduleAnimation();

  // Ensure paint artifact compositor does an update, since that's the mechanism
  // we use to pass transition requests to the compositor.
  document->View()->SetPaintArtifactCompositorNeedsUpdate(
      PaintArtifactCompositorUpdateReason::kViewTransitionNotifyChanges);
}

VectorOf<std::unique_ptr<ViewTransitionRequest>>
ViewTransitionSupplement::TakePendingRequests() {
  return std::move(pending_requests_);
}

}  // namespace blink

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"

#include "cc/trees/layer_tree_host.h"
#include "cc/view_transition/view_transition_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {
namespace {

bool HasActiveTransitionInAncestorFrame(LocalFrame* frame) {
  auto* parent = frame ? frame->Parent() : nullptr;

  while (parent && parent->IsLocalFrame()) {
    if (To<LocalFrame>(parent)->GetDocument() &&
        ViewTransitionUtils::GetTransition(
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
    auto* transition =
        document ? ViewTransitionUtils::GetTransition(*document) : nullptr;
    if (!transition)
      return;

    transition->SkipTransition();
    DCHECK(!ViewTransitionUtils::GetTransition(*document));
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
DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    V8ViewTransitionCallback* callback,
    ExceptionState& exception_state) {
  DCHECK(script_state);
  DCHECK(ThreadScheduler::Current());
  auto* supplement = From(document);

  if (callback) {
    auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker();
    // Set the parent task ID if we're not in an extension task (as extensions
    // are not currently supported in TaskAttributionTracker).
    if (tracker && script_state->World().IsMainWorld()) {
      callback->SetParentTask(tracker->RunningTask(script_state));
    }
  }
  return supplement->StartTransition(document, callback, exception_state);
}

DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    ViewTransitionOptions* options,
    ExceptionState& exception_state) {
  CHECK(!options || options->hasUpdate());
  return startViewTransition(script_state, document,
                             options ? options->update() : nullptr,
                             exception_state);
}

DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    ExceptionState& exception_state) {
  return startViewTransition(script_state, document,
                             static_cast<V8ViewTransitionCallback*>(nullptr),
                             exception_state);
}

DOMViewTransition* ViewTransitionSupplement::StartTransition(
    Document& document,
    V8ViewTransitionCallback* callback,
    ExceptionState& exception_state) {
  // Disallow script initiated transitions during a navigation initiated
  // transition.
  if (transition_ && !transition_->IsCreatedViaScriptAPI())
    return nullptr;

  if (transition_)
    transition_->SkipTransition();

  DCHECK(!transition_)
      << "SkipTransition() should finish existing |transition_|";

  // We need to be connected to a view to have a transition. We also need a
  // document element, since that's the originating element for the pseudo tree.
  if (!document.View() || !document.documentElement()) {
    return nullptr;
  }

  transition_ = ViewTransition::CreateFromScript(&document, callback, this);

  // If there is a transition in a parent frame, give that precedence over a
  // transition in a child frame.
  if (HasActiveTransitionInAncestorFrame(document.GetFrame())) {
    auto skipped_transition = transition_;
    skipped_transition->SkipTransition();

    DCHECK(!transition_);
    return skipped_transition->GetScriptDelegate();
  }

  // Skip transitions in all frames associated with this widget. We can only
  // have one transition per widget/CC.
  SkipTransitionInAllLocalFrames(document.GetFrame());
  DCHECK(transition_);

  return transition_->GetScriptDelegate();
}

void ViewTransitionSupplement::SetCrossDocumentOptIn(
    mojom::blink::ViewTransitionSameOriginOptIn cross_document_opt_in) {
  if (cross_document_opt_in_ == cross_document_opt_in) {
    return;
  }

  cross_document_opt_in_ = cross_document_opt_in;

  // If we have a frame, notify the frame host that the opt-in has changed.
  if (auto* document = GetSupplementable(); document->GetFrame()) {
    document->GetFrame()
        ->GetLocalFrameHostRemote()
        .OnViewTransitionOptInChanged(cross_document_opt_in);
  }

  if (cross_document_opt_in_ ==
          mojom::blink::ViewTransitionSameOriginOptIn::kDisabled &&
      transition_ && !transition_->IsCreatedViaScriptAPI()) {
    transition_->SkipTransition();
    DCHECK(!transition_)
        << "SkipTransition() should finish existing |transition_|";
  }
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
    transition_->SkipTransition();
  }
  DCHECK(!transition_)
      << "SkipTransition() should finish existing |transition_|";
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

// static
void ViewTransitionSupplement::AbortTransition(Document& document) {
  auto* supplement = FromIfExists(document);
  if (supplement && supplement->transition_) {
    supplement->transition_->SkipTransition();
    DCHECK(!supplement->transition_);
  }
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
  CHECK(transition);
  CHECK_EQ(transition, transition_);
  // Clear the transition so it can be garbage collected if needed (and to
  // prevent callers of GetTransition thinking there's an ongoing transition).
  transition_ = nullptr;
}

ViewTransition* ViewTransitionSupplement::GetTransition() {
  return transition_.Get();
}

ViewTransitionSupplement::ViewTransitionSupplement(Document& document)
    : Supplement<Document>(document) {}

ViewTransitionSupplement::~ViewTransitionSupplement() = default;

void ViewTransitionSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(transition_);

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
  document->View()->SetPaintArtifactCompositorNeedsUpdate();
}

VectorOf<std::unique_ptr<ViewTransitionRequest>>
ViewTransitionSupplement::TakePendingRequests() {
  return std::move(pending_requests_);
}

void ViewTransitionSupplement::OnMetaTagChanged(
    const AtomicString& content_value) {
  auto cross_document_opt_in =
      EqualIgnoringASCIICase(content_value, "same-origin")
          ? mojom::blink::ViewTransitionSameOriginOptIn::kEnabled
          : mojom::blink::ViewTransitionSameOriginOptIn::kDisabled;

  SetCrossDocumentOptIn(cross_document_opt_in);
}

void ViewTransitionSupplement::OnViewTransitionsStyleUpdated(
    bool cross_document_enabled) {
  CHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  // TODO(https://crbug.com/1463966): Remove meta tag opt-in - ignore the case
  // where both are specified for now.

  SetCrossDocumentOptIn(
      cross_document_enabled
          ? mojom::blink::ViewTransitionSameOriginOptIn::kEnabled
          : mojom::blink::ViewTransitionSameOriginOptIn::kDisabled);
}

void ViewTransitionSupplement::WillInsertBody() {
  if (!transition_ || !transition_->IsForNavigationOnNewDocument()) {
    return;
  }

  CHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());

  auto* document = GetSupplementable();
  CHECK(document);

  // Update active styles will compute the @view-transitions
  // navigation-trigger opt in.
  // TODO(https://crbug.com/1463966): This is probably a bit of a heavy hammer.
  // In the long term, we probably don't want to make this decision at
  // WillInsertBody or, if we do, we could look specifically for
  // @view-transitions rather than all rules.
  document->GetStyleEngine().UpdateActiveStyle();

  // If the opt-in is enabled, then there's nothing to do in this function.
  if (cross_document_opt_in_ ==
      mojom::blink::ViewTransitionSameOriginOptIn::kEnabled) {
    return;
  }

  // Since we don't have an opt-in, skip a navigation transition if it exists.
  transition_->SkipTransition();
  DCHECK(!transition_)
      << "SkipTransition() should finish existing |transition_|";
}

}  // namespace blink

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"

#include "cc/trees/layer_tree_host.h"
#include "cc/view_transition/view_transition_request.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/core/view_transition/page_swap_event.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

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
DOMViewTransition* ViewTransitionSupplement::StartViewTransitionInternal(
    ScriptState* script_state,
    Document& document,
    V8ViewTransitionCallback* callback,
    const std::optional<Vector<String>>& types,
    ExceptionState& exception_state) {
  DCHECK(script_state);
  auto* supplement = From(document);

  if (callback) {
    auto* tracker =
        scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
    // Set the parent task ID if we're not in an extension task (as extensions
    // are not currently supported in TaskAttributionTracker).
    if (tracker && script_state->World().IsMainWorld()) {
      callback->SetParentTask(tracker->RunningTask());
    }
  }
  return supplement->StartTransition(document, callback, types,
                                     exception_state);
}

DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    V8ViewTransitionCallback* callback,
    ExceptionState& exception_state) {
  return StartViewTransitionInternal(script_state, document, callback,
                                     std::nullopt, exception_state);
}

DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    ViewTransitionOptions* options,
    ExceptionState& exception_state) {
  CHECK(!options || (options->hasUpdate() && options->hasTypes()));
  return StartViewTransitionInternal(
      script_state, document, options ? options->update() : nullptr,
      options ? options->types() : std::nullopt, exception_state);
}

DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    ExceptionState& exception_state) {
  return StartViewTransitionInternal(
      script_state, document, static_cast<V8ViewTransitionCallback*>(nullptr),
      std::nullopt, exception_state);
}

DOMViewTransition* ViewTransitionSupplement::StartTransition(
    Document& document,
    V8ViewTransitionCallback* callback,
    const std::optional<Vector<String>>& types,
    ExceptionState& exception_state) {
  // Disallow script initiated transitions during a navigation initiated
  // transition.
  if (transition_ && !transition_->IsCreatedViaScriptAPI()) {
    return ViewTransition::CreateSkipped(&document, callback)
        ->GetScriptDelegate();
  }

  if (transition_) {
    transition_->SkipTransition();
  }

  DCHECK(!transition_)
      << "SkipTransition() should finish existing |transition_|";

  // We need to be connected to a view to have a transition. We also need a
  // document element, since that's the originating element for the pseudo tree.
  if (!document.View() || !document.documentElement()) {
    return nullptr;
  }

  transition_ =
      ViewTransition::CreateFromScript(&document, callback, types, this);

  if (document.hidden()) {
    auto skipped_transition = transition_;
    skipped_transition->SkipTransition(
        ViewTransition::PromiseResponse::kRejectInvalidState);

    DCHECK(!transition_);
    return skipped_transition->GetScriptDelegate();
  }

  // If there is a transition in a parent frame, give that precedence over a
  // transition in a child frame.
  if (!RuntimeEnabledFeatures::ConcurrentViewTransitionsSPAEnabled() &&
      HasActiveTransitionInAncestorFrame(document.GetFrame())) {
    auto skipped_transition = transition_;
    skipped_transition->SkipTransition();

    DCHECK(!transition_);
    return skipped_transition->GetScriptDelegate();
  }

  // Skip transitions in all frames associated with this widget. We can only
  // have one transition per widget/CC.
  if (!RuntimeEnabledFeatures::ConcurrentViewTransitionsSPAEnabled()) {
    SkipTransitionInAllLocalFrames(document.GetFrame());
  }
  DCHECK(transition_);

  return transition_->GetScriptDelegate();
}

void ViewTransitionSupplement::DidChangeVisibilityState() {
  if (GetSupplementable()->hidden() && transition_) {
    transition_->SkipTransition(
        ViewTransition::PromiseResponse::kRejectInvalidState);
  }
  SendOptInStatusToHost();
}

void ViewTransitionSupplement::SendOptInStatusToHost() {
  // If we have a frame, notify the frame host that the opt-in has changed.
  Document* document = GetSupplementable();
  if (!document || !document->GetFrame() || !document->domWindow()) {
    return;
  }

  document->GetFrame()->GetLocalFrameHostRemote().OnViewTransitionOptInChanged(
      (document->domWindow()->HasBeenRevealed() && !document->hidden())
          ? cross_document_opt_in_
          : mojom::blink::ViewTransitionSameOriginOptIn::kDisabled);
}

void ViewTransitionSupplement::SetCrossDocumentOptIn(
    mojom::blink::ViewTransitionSameOriginOptIn cross_document_opt_in) {
  if (cross_document_opt_in_ == cross_document_opt_in) {
    return;
  }

  cross_document_opt_in_ = cross_document_opt_in;
  SendOptInStatusToHost();
}

// static
void ViewTransitionSupplement::SnapshotDocumentForNavigation(
    Document& document,
    const blink::ViewTransitionToken& navigation_id,
    mojom::blink::PageSwapEventParamsPtr params,
    ViewTransition::ViewTransitionStateCallback callback) {
  DCHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  auto* supplement = From(document);
  supplement->StartTransition(document, navigation_id, std::move(params),
                              std::move(callback));
}

void ViewTransitionSupplement::StartTransition(
    Document& document,
    const blink::ViewTransitionToken& navigation_id,
    mojom::blink::PageSwapEventParamsPtr params,
    ViewTransition::ViewTransitionStateCallback callback) {
  // TODO(khushalsagar): Per spec, we should be checking the opt-in at this
  // point. See step 2 in
  // https://drafts.csswg.org/css-view-transitions-2/#setup-outbound-transition.

  if (transition_) {
    // We should skip a transition if one exists, regardless of how it was
    // created, since navigation transition takes precedence.
    transition_->SkipTransition();
  }

  DCHECK(!transition_)
      << "SkipTransition() should finish existing |transition_|";
  transition_ = ViewTransition::CreateForSnapshotForNavigation(
      &document, navigation_id, std::move(callback), cross_document_types_,
      this);

  auto* page_swap_event = MakeGarbageCollected<PageSwapEvent>(
      document, std::move(params), transition_->GetScriptDelegate());
  document.domWindow()->DispatchEvent(*page_swap_event);
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

void ViewTransitionSupplement::OnViewTransitionsStyleUpdated(
    bool cross_document_enabled,
    const Vector<String>& types) {
  CHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  CHECK(RuntimeEnabledFeatures::ViewTransitionTypesEnabled() || types.empty());
  SetCrossDocumentOptIn(
      cross_document_enabled
          ? mojom::blink::ViewTransitionSameOriginOptIn::kEnabled
          : mojom::blink::ViewTransitionSameOriginOptIn::kDisabled);
  cross_document_types_ = types;
}

void ViewTransitionSupplement::WillInsertBody() {
  if (!transition_ || !transition_->IsForNavigationOnNewDocument()) {
    return;
  }

  CHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());

  auto* document = GetSupplementable();
  CHECK(document);

  // Update active styles will compute the @view-transition
  // navigation opt in.
  // TODO(https://crbug.com/1463966): This is probably a bit of a heavy hammer.
  // In the long term, we probably don't want to make this decision at
  // WillInsertBody or, if we do, we could look specifically for
  // @view-transition rather than all rules. Note: the opt-in is checked below
  // from dispatching the pagereveal event during the first update-the-rendering
  // steps.
  document->GetStyleEngine().UpdateActiveStyle();
}

DOMViewTransition*
ViewTransitionSupplement::ResolveCrossDocumentViewTransition() {
  if (!transition_ || !transition_->IsForNavigationOnNewDocument()) {
    return nullptr;
  }

  // We auto-skip *outbound* transitions when the document has not been
  // revealed yet. We expect it to not be revealed yet when resolving the
  // inbound transition.
  CHECK(!GetSupplementable()->domWindow()->HasBeenRevealed());

  if (cross_document_opt_in_ ==
      mojom::blink::ViewTransitionSameOriginOptIn::kDisabled) {
    transition_->SkipTransition();
    CHECK(!ViewTransitionUtils::GetTransition(*GetSupplementable()));
    return nullptr;
  }

  transition_->InitTypes(cross_document_types_);

  // TODO(https://crbug.com/1502628): This is where types from the used
  // @view-transition should be applied.

  return transition_->GetScriptDelegate();
}

viz::ViewTransitionElementResourceId
ViewTransitionSupplement::GenerateResourceId(
    const blink::ViewTransitionToken& transition_token) {
  return viz::ViewTransitionElementResourceId(transition_token,
                                              ++resource_local_id_sequence_);
}

void ViewTransitionSupplement::InitializeResourceIdSequence(
    uint32_t next_local_id) {
  CHECK_GT(next_local_id,
           viz::ViewTransitionElementResourceId::kInvalidLocalId);
  resource_local_id_sequence_ =
      std::max(next_local_id - 1, resource_local_id_sequence_);
}

}  // namespace blink

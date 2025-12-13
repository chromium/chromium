// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"

#include "cc/trees/layer_tree_host.h"
#include "cc/view_transition/view_transition_request.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/core/view_transition/page_swap_event.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"

namespace blink {

namespace {

bool CompareTransitions(const Member<ViewTransition>& left,
                        const Member<ViewTransition>& right) {
  return left->Id() < right->Id();
}

}  // namespace

// static
DOMViewTransition* ViewTransitionSupplement::StartViewTransitionForElement(
    ScriptState* script_state,
    Element* element,
    V8ViewTransitionCallback* callback,
    const std::optional<Vector<String>>& types,
    ExceptionState& exception_state) {
  DCHECK(script_state);
  if (!element) {
    return nullptr;
  }

  return element->GetDocument().GetViewTransitions().StartTransition(
      *element, callback, types, exception_state);
}

DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    V8ViewTransitionCallback* callback,
    ExceptionState& exception_state) {
  return StartViewTransitionForElement(script_state, document.documentElement(),
                                       callback, std::nullopt, exception_state);
}

DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    ViewTransitionOptions* options,
    ExceptionState& exception_state) {
  CHECK(!options || (options->hasUpdate() && options->hasTypes()));
  return StartViewTransitionForElement(
      script_state, document.documentElement(),
      options ? options->update() : nullptr,
      options ? options->types() : std::nullopt, exception_state);
}

DOMViewTransition* ViewTransitionSupplement::startViewTransition(
    ScriptState* script_state,
    Document& document,
    ExceptionState& exception_state) {
  return StartViewTransitionForElement(
      script_state, document.documentElement(),
      static_cast<V8ViewTransitionCallback*>(nullptr), std::nullopt,
      exception_state);
}

// static
DOMViewTransition* ViewTransitionSupplement::activeViewTransition(
    Document& document) {
  auto* supplement = document.GetViewTransitionsIfExists();
  if (!supplement) {
    return nullptr;
  }
  return supplement->document_transition_
             ? supplement->document_transition_->GetScriptDelegate()
             : nullptr;
}

DOMViewTransition* ViewTransitionSupplement::StartTransition(
    Element& element,
    V8ViewTransitionCallback* callback,
    const std::optional<Vector<String>>& types,
    ExceptionState& exception_state) {
  bool for_document = element.IsDocumentElement();
  Document& document = element.GetDocument();

  // Disallow script initiated transitions during a navigation initiated
  // transition.
  if (document_transition_ && !document_transition_->IsCreatedViaScriptAPI()) {
    return ViewTransition::CreateSkipped(&element, callback)
        ->GetScriptDelegate();
  }

  ViewTransition* active_transition = GetTransition(element);
  if (active_transition) {
    // Starting a view-transition skips the currently active view-transition.
    active_transition->SkipTransition();
  } else {
    auto it = skipped_with_pending_dom_callback_.find(&element);
    if (it != skipped_with_pending_dom_callback_.end()) {
      // A recently skipped view transition might not have triggered its DOM
      // callback. This step needs to complete ahead of the capture phase for
      // the new view-transition.
      active_transition = it->value;
    }
  }

  DCHECK(!GetTransition(element))
      << "SkipTransition() should finish previously active view transition";

  // We need to be connected to a view to have a transition.
  if (!document.View()) {
    return nullptr;
  }

  ViewTransition* transition = ViewTransition::CreateFromScript(
      &element, callback, types, this, active_transition);
  DCHECK(transition);

  if (for_document) {
    document_transition_ = transition;
  } else {
    element_transitions_.insert(&element, transition);
  }

  if (document.hidden()) {
    transition->SkipTransition(
        ViewTransition::PromiseResponse::kRejectInvalidState);

    DCHECK(!document_transition_ || !for_document);
    return transition->GetScriptDelegate();
  }

  return transition->GetScriptDelegate();
}

void ViewTransitionSupplement::DidChangeVisibilityState() {
  if (document_->hidden() && document_transition_) {
    document_transition_->SkipTransition(
        ViewTransition::PromiseResponse::kRejectInvalidState);
  }
  SendOptInStatusToHost();
}

void ViewTransitionSupplement::SendOptInStatusToHost() {
  // If we have a frame, notify the frame host that the opt-in has changed.
  if (!document_ || !document_->GetFrame() || !document_->domWindow()) {
    return;
  }

  document_->GetFrame()->GetLocalFrameHostRemote().OnViewTransitionOptInChanged(
      (document_->domWindow()->HasBeenRevealed() && !document_->hidden())
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
  document.GetViewTransitions().StartTransition(
      document, navigation_id, std::move(params), std::move(callback));
}

void ViewTransitionSupplement::StartTransition(
    Document& document,
    const blink::ViewTransitionToken& navigation_id,
    mojom::blink::PageSwapEventParamsPtr params,
    ViewTransition::ViewTransitionStateCallback callback) {
  // TODO(khushalsagar): Per spec, we should be checking the opt-in at this
  // point. See step 2 in
  // https://drafts.csswg.org/css-view-transitions-2/#setup-outbound-transition.

  if (document_transition_) {
    // TODO(nrosenthal): limit eligible animations to those that started after
    // navigation was initiated and have a short while before they are scheduled
    // to end.
    if (RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled() &&
        document_transition_->HasActiveAnimations()) {
      pending_navigation_transition_.emplace(PendingNavigationTransition{
          navigation_id, std::move(params), std::move(callback)});
      return;
    }
    // We should skip a transition if one exists, regardless of how it was
    // created, since navigation transition takes precedence.
    document_transition_->SkipTransition();
  }

  DCHECK(!document_transition_)
      << "SkipTransition() should finish existing |document_transition_|";
  document_transition_ = ViewTransition::CreateForSnapshotForNavigation(
      &document, navigation_id, std::move(callback), cross_document_types_,
      this);

  auto* page_swap_event = MakeGarbageCollected<PageSwapEvent>(
      document, std::move(params), document_transition_->GetScriptDelegate());
  document.domWindow()->DispatchEvent(*page_swap_event);
}

// static
void ViewTransitionSupplement::CreateFromSnapshotForNavigation(
    Document& document,
    ViewTransitionState transition_state) {
  document.GetViewTransitions().StartTransition(document,
                                                std::move(transition_state));
}

// static
void ViewTransitionSupplement::AbortTransition(Document& document) {
  auto* supplement = document.GetViewTransitionsIfExists();
  if (supplement && supplement->document_transition_) {
    supplement->document_transition_->SkipTransition();
    DCHECK(!supplement->document_transition_);
  }
}

void ViewTransitionSupplement::StartTransition(
    Document& document,
    ViewTransitionState transition_state) {
  DCHECK(!document_transition_) << "Existing transition on new Document";
  document_transition_ = ViewTransition::CreateFromSnapshotForNavigation(
      &document, std::move(transition_state), this);
}

void ViewTransitionSupplement::OnTransitionFinished(
    ViewTransition* transition) {
  CHECK(transition);

  // Clear the transition so it can be garbage collected if needed (and to
  // prevent callers of GetTransition thinking there's an ongoing transition).
  if (transition == document_transition_) {
    document_transition_ = nullptr;
    if (pending_navigation_transition_) {
      CHECK(RuntimeEnabledFeatures::TwoPhaseViewTransitionEnabled());
      document_->GetTaskRunner(TaskType::kDOMManipulation)
          ->PostTask(
              FROM_HERE,
              BindOnce(
                  [](ViewTransitionSupplement* supplement,
                     PendingNavigationTransition
                         pending_navigation_transition) {
                    supplement->StartTransition(
                        *supplement->document_,
                        pending_navigation_transition.navigation_id,
                        std::move(pending_navigation_transition.params),
                        std::move(pending_navigation_transition.callback));
                  },
                  WrapWeakPersistent(this),
                  std::move(*pending_navigation_transition_)));
      pending_navigation_transition_.reset();
    }
  } else {
    Element* scope = transition->Scope();
    element_transitions_.erase(scope);
    LayoutObject* layout_object = scope->GetLayoutObject();
    if (scope != scope->GetDocument().documentElement() && layout_object &&
        !(layout_object->StyleRef().Contain() & kContainsViewTransition) &&
        layout_object->HasLayer()) {
      // Element may have had an added stacking context purely for being the
      // scope of a view transition. Ensure correctness of the adjusted style.
      layout_object->EnclosingLayer()->SetNeedsCompositingInputsUpdate();
      scope->SetNeedsStyleRecalc(kLocalStyleChange,
                                 StyleChangeReasonForTracing::Create(
                                     style_change_reason::kViewTransition));
    }
  }

  // Notify the animator if the set of active view transitions is empty.
  if (!document_transition_ && element_transitions_.empty()) {
    if (auto* page = document_->GetPage()) {
      page->Animator().SetHasViewTransition(false);
    }
  }
}

void ViewTransitionSupplement::OnSkipTransitionWithPendingCallback(
    ViewTransition* transition) {
  CHECK(transition);
  skipped_with_pending_dom_callback_.insert(transition->Scope(), transition);
}

void ViewTransitionSupplement::OnSkippedTransitionDOMCallback(
    ViewTransition* transition) {
  CHECK(transition);
  skipped_with_pending_dom_callback_.erase(transition->Scope());
}

void ViewTransitionSupplement::OnTransitionCaptured(
    ViewTransition* transition) {
  CHECK(transition);
  captured_transitions_.push_back(transition);
  if (--in_flight_capture_requests_ == 0) {
    std::sort(captured_transitions_.begin(), captured_transitions_.end(),
              CompareTransitions);
    for (auto captured_transition : captured_transitions_) {
      captured_transition->OnCapturePhaseComplete();
    }
    captured_transitions_.clear();
  }
}

ViewTransition* ViewTransitionSupplement::GetTransition() {
  return document_transition_.Get();
}

ViewTransition* ViewTransitionSupplement::GetTransition(
    const Element& element) {
  if (element.IsDocumentElement()) {
    return document_transition_.Get();
  }
  if (element.IsPseudoElement()) {
    return GetTransition(
        To<PseudoElement>(element).UltimateOriginatingElement());
  }
  auto transition = element_transitions_.find(&element);
  return transition == element_transitions_.end() ? nullptr : transition->value;
}

void ViewTransitionSupplement::ForEachTransition(
    base::FunctionRef<void(ViewTransition&)> function) {
  if (element_transitions_.empty()) {
    if (ViewTransition* document_transition = GetTransition()) {
      function(*document_transition);
    }
    return;
  }

  DCHECK(RuntimeEnabledFeatures::ScopedViewTransitionsEnabled());

  // Local copy of the list, since the function may modify the transition map.
  HeapVector<Member<ViewTransition>> transitions;
  if (ViewTransition* document_transition = GetTransition()) {
    transitions.push_back(document_transition);
  }
  for (auto& element_transition : element_transitions_.Values()) {
    transitions.push_back(element_transition);
  }
  for (auto transition : transitions) {
    function(*transition);
  }
}

void ViewTransitionSupplement::WillEnterGetComputedStyleScope() {
  CHECK(!in_get_computed_style_scope_);
  in_get_computed_style_scope_ = true;

  ForEachTransition([](ViewTransition& transition) {
    transition.WillEnterGetComputedStyleScope();
  });
}

void ViewTransitionSupplement::WillExitGetComputedStyleScope() {
  CHECK(in_get_computed_style_scope_);
  in_get_computed_style_scope_ = false;

  ForEachTransition([](ViewTransition& transition) {
    transition.WillExitGetComputedStyleScope();
  });
}

void ViewTransitionSupplement::WillUpdateStyleAndLayoutTree() {
  if (in_get_computed_style_scope_ == last_update_had_computed_style_scope_) {
    return;
  }
  last_update_had_computed_style_scope_ = in_get_computed_style_scope_;
  ForEachTransition([](ViewTransition& transition) {
    transition.InvalidateInternalPseudoStyle();
  });
}

ViewTransitionSupplement::ViewTransitionSupplement(Document& document)
    : document_(document) {}

ViewTransitionSupplement::~ViewTransitionSupplement() = default;

void ViewTransitionSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(document_transition_);
  visitor->Trace(element_transitions_);
  visitor->Trace(skipped_with_pending_dom_callback_);
  visitor->Trace(captured_transitions_);
}

void ViewTransitionSupplement::AddPendingRequest(
    std::unique_ptr<ViewTransitionRequest> request) {
  if (request->type() == ViewTransitionRequest::Type::kSave) {
    in_flight_capture_requests_++;
  }
  pending_requests_.push_back(std::move(request));

  if (!document_ || !document_->GetPage() || !document_->View()) {
    return;
  }

  // Schedule a new frame.
  document_->View()->ScheduleAnimation();

  // Ensure paint artifact compositor does an update, since that's the mechanism
  // we use to pass transition requests to the compositor.
  document_->View()->SetPaintArtifactCompositorNeedsUpdate();
}

VectorOf<std::unique_ptr<ViewTransitionRequest>>
ViewTransitionSupplement::TakePendingRequests() {
  return std::move(pending_requests_);
}

void ViewTransitionSupplement::OnViewTransitionsStyleUpdated(
    bool cross_document_enabled,
    const Vector<String>& types) {
  SetCrossDocumentOptIn(
      cross_document_enabled
          ? mojom::blink::ViewTransitionSameOriginOptIn::kEnabled
          : mojom::blink::ViewTransitionSameOriginOptIn::kDisabled);
  cross_document_types_ = types;
}

void ViewTransitionSupplement::WillInsertBody() {
  if (!document_transition_ ||
      !document_transition_->IsForNavigationOnNewDocument()) {
    return;
  }

  CHECK(document_);

  // Update active styles will compute the @view-transition
  // navigation opt in.
  // TODO(https://crbug.com/1463966): This is probably a bit of a heavy hammer.
  // In the long term, we probably don't want to make this decision at
  // WillInsertBody or, if we do, we could look specifically for
  // @view-transition rather than all rules. Note: the opt-in is checked below
  // from dispatching the pagereveal event during the first update-the-rendering
  // steps.
  document_->GetStyleEngine().UpdateActiveStyle();
}

DOMViewTransition*
ViewTransitionSupplement::ResolveCrossDocumentViewTransition() {
  if (!document_transition_ ||
      !document_transition_->IsForNavigationOnNewDocument()) {
    return nullptr;
  }

  // We auto-skip *outbound* transitions when the document has not been
  // revealed yet. We expect it to not be revealed yet when resolving the
  // inbound transition.
  CHECK(!document_->domWindow()->HasBeenRevealed());

  if (cross_document_opt_in_ ==
      mojom::blink::ViewTransitionSameOriginOptIn::kDisabled) {
    document_transition_->SkipTransition();
    CHECK(!ViewTransitionUtils::GetTransition(*document_));
    return nullptr;
  }

  document_transition_->InitTypes(cross_document_types_);

  // TODO(https://crbug.com/1502628): This is where types from the used
  // @view-transition should be applied.

  return document_transition_->GetScriptDelegate();
}

viz::ViewTransitionElementResourceId
ViewTransitionSupplement::GenerateResourceId(
    const blink::ViewTransitionToken& transition_token,
    bool for_scope_snapshot) {
  return viz::ViewTransitionElementResourceId(
      transition_token, ++resource_local_id_sequence_, for_scope_snapshot);
}

void ViewTransitionSupplement::InitializeResourceIdSequence(
    uint32_t next_local_id) {
  CHECK_GT(next_local_id,
           viz::ViewTransitionElementResourceId::kInvalidLocalId);
  resource_local_id_sequence_ =
      std::max(next_local_id - 1, resource_local_id_sequence_);
}

}  // namespace blink

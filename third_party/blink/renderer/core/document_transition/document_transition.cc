// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include <vector>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/paint_holding_reason.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

uint32_t NextDocumentTag() {
  static uint32_t next_document_tag = 1u;
  return next_document_tag++;
}

}  // namespace

// DOMChangeFinishedCallback implementation.
DocumentTransition::DOMChangeFinishedCallback::DOMChangeFinishedCallback(
    DocumentTransition* transition,
    ScriptPromiseResolver* dom_updated_resolver,
    bool success)
    : transition_(transition),
      dom_updated_resolver_(dom_updated_resolver),
      success_(success) {}

DocumentTransition::DOMChangeFinishedCallback::~DOMChangeFinishedCallback() =
    default;

ScriptValue DocumentTransition::DOMChangeFinishedCallback::Call(
    ScriptState* script_state,
    ScriptValue value) {
  if (transition_)
    transition_->NotifyDOMCallbackFinished(success_);

  if (success_)
    dom_updated_resolver_->Resolve();
  else
    dom_updated_resolver_->Reject(value);
  return ScriptValue();
}

void DocumentTransition::DOMChangeFinishedCallback::Trace(
    Visitor* visitor) const {
  ScriptFunction::Callable::Trace(visitor);
  visitor->Trace(transition_);
  visitor->Trace(dom_updated_resolver_);
}

// DocumentTransition implementation.
DocumentTransition::DocumentTransition(
    Document* document,
    ScriptState* script_state,
    V8DocumentTransitionCallback* update_dom_callback,
    DocumentTransitionDirectiveStore* directive_store)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      document_(document),
      document_tag_(NextDocumentTag()),
      script_state_(script_state),
      update_dom_callback_(update_dom_callback),
      style_tracker_(
          MakeGarbageCollected<DocumentTransitionStyleTracker>(*document_)),
      dom_updated_promise_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state_)),
      ready_promise_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state_)),
      finished_promise_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state_)),
      directive_store_(directive_store) {
  ProcessCurrentState();
}

void DocumentTransition::skipTransition() {
  if (IsTerminalState(state_))
    return;

  if (static_cast<int>(state_) < static_cast<int>(State::kDOMCallbackRunning)) {
    document_->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(base::IgnoreResult(
                              &DocumentTransition::InvokeDOMChangeCallback),
                          WrapPersistent(this)));
  }

  ResumeRendering();

  style_tracker_->Abort();

  if (static_cast<int>(state_) <
      static_cast<int>(State::kAnimateRequestPending)) {
    // TODO(vmpstr): Add abort error.
    ready_promise_resolver_->Reject();
  }

  AdvanceTo(State::kAborted);

  // TODO(vmpstr): Add abort error.
  finished_promise_resolver_->Reject();
}

ScriptPromise DocumentTransition::finished() const {
  return finished_promise_resolver_->Promise();
}

ScriptPromise DocumentTransition::ready() const {
  return ready_promise_resolver_->Promise();
}

ScriptPromise DocumentTransition::domUpdated() const {
  return dom_updated_promise_resolver_->Promise();
}

bool DocumentTransition::AdvanceTo(State state) {
  DCHECK(CanAdvanceTo(state)) << "Current state " << static_cast<int>(state_)
                              << " new state " << static_cast<int>(state);
  state_ = state;

  // If we need to run in a lifecycle, but we're not in one, then make sure to
  // schedule an animation in case we wouldn't get one naturally.
  if (StateRunsInDocumentTransitionStepsDuringMainFrame(state_) !=
      in_main_lifecycle_update_) {
    if (!in_main_lifecycle_update_) {
      DCHECK(!IsTerminalState(state_));
      document_->View()->ScheduleAnimation();
    } else {
      DCHECK(IsTerminalState(state_) || WaitsForNotification(state_));
    }
    return false;
  }
  // In all other cases, we should be able to process the state immediately. We
  // don't do it in this function so that it's clear what's happening outside of
  // this call.
  return true;
}

bool DocumentTransition::CanAdvanceTo(State state) const {
  // This documents valid state transitions. Note that this does not make a
  // judgement call about whether the state runs synchronously or not,
  // so we allow some transitions that would not be possible in a synchronous
  // run, like kCaptured -> kAborted. This isn't possible in a synchronous call,
  // because kCaptured will always go to kDOMCallbackRunning.

  switch (state_) {
    case State::kInitial:
      return state == State::kCaptureTagDiscovery;
    case State::kCaptureTagDiscovery:
      return state == State::kCaptureRequestPending || state == State::kAborted;
    case State::kCaptureRequestPending:
      return state == State::kCapturing || state == State::kAborted;
    case State::kCapturing:
      return state == State::kCaptured || state == State::kAborted;
    case State::kCaptured:
      return state == State::kDOMCallbackRunning ||
             state == State::kDOMCallbackFinished || state == State::kAborted;
    case State::kDOMCallbackRunning:
      return state == State::kDOMCallbackFinished || state == State::kAborted;
    case State::kDOMCallbackFinished:
      return state == State::kAnimateTagDiscovery || state == State::kAborted;
    case State::kAnimateTagDiscovery:
      return state == State::kAnimateRequestPending || state == State::kAborted;
    case State::kAnimateRequestPending:
      return state == State::kAnimating || state == State::kAborted;
    case State::kAnimating:
      return state == State::kFinished || state == State::kAborted;
    case State::kAborted:
      // We allow aborted to move to timed out state, so that time out can call
      // skipTransition and then change the state to timed out.
      return state == State::kTimedOut;
    case State::kFinished:
    case State::kTimedOut:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
bool DocumentTransition::StateRunsInDocumentTransitionStepsDuringMainFrame(
    State state) {
  switch (state) {
    case State::kInitial:
      return false;
    case State::kCaptureTagDiscovery:
    case State::kCaptureRequestPending:
      return true;
    case State::kCapturing:
    case State::kCaptured:
    case State::kDOMCallbackRunning:
    case State::kDOMCallbackFinished:
    case State::kAnimateTagDiscovery:
    case State::kAnimateRequestPending:
      return false;
    case State::kAnimating:
      return true;
    case State::kFinished:
    case State::kAborted:
    case State::kTimedOut:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
bool DocumentTransition::WaitsForNotification(State state) {
  return state == State::kCapturing || state == State::kDOMCallbackRunning;
}

// static
bool DocumentTransition::IsTerminalState(State state) {
  return state == State::kFinished || state == State::kAborted ||
         state == State::kTimedOut;
}

void DocumentTransition::ProcessCurrentState() {
  bool process_next_state = true;
  while (process_next_state) {
    DCHECK_EQ(in_main_lifecycle_update_,
              StateRunsInDocumentTransitionStepsDuringMainFrame(state_));
    process_next_state = false;
    switch (state_) {
      // Initial state: nothing to do, just advance the state
      case State::kInitial:
        process_next_state = AdvanceTo(State::kCaptureTagDiscovery);
        DCHECK(!process_next_state);
        break;

      // Update the lifecycle if needed and discover the elements (deferred to
      // AddSharedElementsFromCSS).
      case State::kCaptureTagDiscovery:
        DCHECK(in_main_lifecycle_update_);
        DCHECK_GE(document_->Lifecycle().GetState(),
                  DocumentLifecycle::kCompositingInputsClean);
        style_tracker_->AddSharedElementsFromCSS();
        process_next_state = AdvanceTo(State::kCaptureRequestPending);
        DCHECK(process_next_state);
        break;

      // Capture request pending -- create the request
      case State::kCaptureRequestPending:
        if (!style_tracker_->Capture()) {
          skipTransition();
          break;
        }

        directive_store_->AddPendingRequest(
            DocumentTransitionRequest::CreateCapture(
                document_tag_, style_tracker_->CapturedTagCount(),
                style_tracker_->TakeCaptureResourceIds(),
                ConvertToBaseOnceCallback(CrossThreadBindOnce(
                    &DocumentTransition::NotifyCaptureFinished,
                    WrapCrossThreadWeakPersistent(this)))));

        if (document_->GetFrame()->IsLocalRoot()) {
          document_->GetPage()->GetChromeClient().StopDeferringCommits(
              *document_->GetFrame(),
              cc::PaintHoldingCommitTrigger::kDocumentTransition);
        }
        document_->GetPage()->GetChromeClient().RegisterForCommitObservation(
            this);

        process_next_state = AdvanceTo(State::kCapturing);
        DCHECK(!process_next_state);
        break;

      case State::kCapturing:
        DCHECK(WaitsForNotification(state_));
        break;

      case State::kCaptured: {
        style_tracker_->CaptureResolved();

        // TODO(vmpstr): Maybe fold this into InvokeDOMChangeCallback somehow.
        if (!update_dom_callback_) {
          dom_updated_promise_resolver_->Resolve();
          dom_callback_succeeded_ = true;
          process_next_state = AdvanceTo(State::kDOMCallbackFinished);
          DCHECK(process_next_state);
          break;
        }

        if (!InvokeDOMChangeCallback()) {
          dom_updated_promise_resolver_->Reject();
          skipTransition();
          break;
        }
        process_next_state = AdvanceTo(State::kDOMCallbackRunning);
        DCHECK(process_next_state);
        break;
      }

      case State::kDOMCallbackRunning:
        DCHECK(WaitsForNotification(state_));
        break;

      case State::kDOMCallbackFinished:
        ResumeRendering();
        if (!dom_callback_succeeded_) {
          skipTransition();
          break;
        }
        process_next_state = AdvanceTo(State::kAnimateTagDiscovery);
        DCHECK(process_next_state);
        break;

      case State::kAnimateTagDiscovery:
        DCHECK(!in_main_lifecycle_update_);
        document_->View()->UpdateLifecycleToPrePaintClean(
            DocumentUpdateReason::kDocumentTransition);
        DCHECK_GE(document_->Lifecycle().GetState(),
                  DocumentLifecycle::kPrePaintClean);
        style_tracker_->AddSharedElementsFromCSS();
        process_next_state = AdvanceTo(State::kAnimateRequestPending);
        DCHECK(process_next_state);
        break;

      case State::kAnimateRequestPending:
        if (!style_tracker_->Start()) {
          skipTransition();
          break;
        }

        directive_store_->AddPendingRequest(
            DocumentTransitionRequest::CreateAnimateRenderer(document_tag_));
        process_next_state = AdvanceTo(State::kAnimating);
        DCHECK(!process_next_state);

        DCHECK(!in_main_lifecycle_update_);
        ready_promise_resolver_->Resolve();
        break;

      case State::kAnimating:
        if (first_animating_frame_) {
          first_animating_frame_ = false;
          break;
        }

        if (style_tracker_->HasActiveAnimations())
          break;

        style_tracker_->StartFinished();
        finished_promise_resolver_->Resolve();

        style_tracker_ = nullptr;

        directive_store_->AddPendingRequest(
            DocumentTransitionRequest::CreateRelease(document_tag_));
        process_next_state = AdvanceTo(State::kFinished);
        DCHECK(!process_next_state);
        break;

      case State::kFinished:
      case State::kAborted:
      case State::kTimedOut:
        break;
    }
  }
}

void DocumentTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(script_state_);
  visitor->Trace(update_dom_callback_);
  visitor->Trace(style_tracker_);
  visitor->Trace(dom_updated_promise_resolver_);
  visitor->Trace(ready_promise_resolver_);
  visitor->Trace(finished_promise_resolver_);

  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

bool DocumentTransition::InvokeDOMChangeCallback() {
  if (!update_dom_callback_)
    return true;

  v8::Maybe<ScriptPromise> result = update_dom_callback_->Invoke(nullptr);
  // TODO(vmpstr): Should this be a DCHECK?
  if (result.IsNothing())
    return false;

  ScriptState::Scope scope(script_state_);

  result.ToChecked().Then(
      MakeGarbageCollected<ScriptFunction>(
          script_state_, MakeGarbageCollected<DOMChangeFinishedCallback>(
                             this, dom_updated_promise_resolver_, true)),
      MakeGarbageCollected<ScriptFunction>(
          script_state_, MakeGarbageCollected<DOMChangeFinishedCallback>(
                             this, dom_updated_promise_resolver_, false)));
  return true;
}

void DocumentTransition::ContextDestroyed() {
  skipTransition();
}

bool DocumentTransition::HasPendingActivity() const {
  return !IsTerminalState(state_);
}

void DocumentTransition::NotifyCaptureFinished() {
  if (state_ != State::kCapturing) {
    DCHECK(IsTerminalState(state_));
    return;
  }
  bool process_next_state = AdvanceTo(State::kCaptured);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

void DocumentTransition::NotifyDOMCallbackFinished(bool success) {
  dom_callback_succeeded_ = success;
  if (IsTerminalState(state_))
    return;

  bool process_next_state = AdvanceTo(State::kDOMCallbackFinished);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

bool DocumentTransition::NeedsSharedElementEffectNode(
    const LayoutObject& object) const {
  // Layout view always needs an effect node, even if root itself is not
  // transitioning. The reason for this is that we want the root to have an
  // effect which can be hoisted up be the sibling of the layout view. This
  // simplifies calling code to have a consistent stacking context structure.
  if (IsA<LayoutView>(object))
    return !IsTerminalState(state_);

  // Otherwise check if the layout object has an active shared element.
  auto* element = DynamicTo<Element>(object.GetNode());
  return element && style_tracker_ && style_tracker_->IsSharedElement(element);
}

bool DocumentTransition::IsRepresentedViaPseudoElements(
    const LayoutObject& object) const {
  if (IsTerminalState(state_))
    return false;

  if (IsA<LayoutView>(object))
    return style_tracker_->IsRootTransitioning();

  auto* element = DynamicTo<Element>(object.GetNode());
  return element && style_tracker_->IsSharedElement(element);
}

PaintPropertyChangeType DocumentTransition::UpdateEffect(
    const LayoutObject& object,
    const EffectPaintPropertyNodeOrAlias& current_effect,
    const ClipPaintPropertyNodeOrAlias* current_clip,
    const TransformPaintPropertyNodeOrAlias* current_transform) {
  DCHECK(NeedsSharedElementEffectNode(object));
  DCHECK(current_transform);
  DCHECK(current_clip);

  EffectPaintPropertyNode::State state;
  state.direct_compositing_reasons =
      CompositingReason::kDocumentTransitionSharedElement;
  state.local_transform_space = current_transform;
  state.output_clip = current_clip;
  state.document_transition_shared_element_id =
      DocumentTransitionSharedElementId(document_tag_);
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      object.UniqueId(),
      CompositorElementIdNamespace::kSharedElementTransition);
  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    // The only non-element participant is the layout view.
    DCHECK(object.IsLayoutView());

    style_tracker_->UpdateRootIndexAndSnapshotId(
        state.document_transition_shared_element_id,
        state.shared_element_resource_id);
    DCHECK(state.document_transition_shared_element_id.valid() ||
           !style_tracker_->IsRootTransitioning());
    return style_tracker_->UpdateRootEffect(std::move(state), current_effect);
  }

  style_tracker_->UpdateElementIndicesAndSnapshotId(
      element, state.document_transition_shared_element_id,
      state.shared_element_resource_id);
  return style_tracker_->UpdateEffect(element, std::move(state),
                                      current_effect);
}

EffectPaintPropertyNode* DocumentTransition::GetEffect(
    const LayoutObject& object) const {
  DCHECK(NeedsSharedElementEffectNode(object));

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element)
    return style_tracker_->GetRootEffect();
  return style_tracker_->GetEffect(element);
}

void DocumentTransition::VerifySharedElements() {
  if (!IsTerminalState(state_))
    style_tracker_->VerifySharedElements();
}

void DocumentTransition::RunDocumentTransitionStepsDuringMainFrame() {
  base::AutoReset<bool> scope(&in_main_lifecycle_update_, true);
  if (StateRunsInDocumentTransitionStepsDuringMainFrame(state_))
    ProcessCurrentState();
  if (style_tracker_ &&
      document_->Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean) {
    style_tracker_->RunPostPrePaintSteps();
  }
}

bool DocumentTransition::NeedsUpToDateTags() const {
  return state_ == State::kCaptureTagDiscovery ||
         state_ == State::kAnimateTagDiscovery;
}

PseudoElement* DocumentTransition::CreatePseudoElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag) {
  DCHECK(style_tracker_);

  return style_tracker_->CreatePseudoElement(parent, pseudo_id,
                                             document_transition_tag);
}

String DocumentTransition::UAStyleSheet() const {
  // TODO(vmpstr): We can still request getComputedStyle(html,
  // "::page-transition-pseudo") outside of a page transition. What should we
  // return in that case?
  if (!style_tracker_)
    return "";
  return style_tracker_->UAStyleSheet();
}

void DocumentTransition::WillCommitCompositorFrame() {
  // There should only be 1 commit when we're in the capturing phase and
  // rendering is paused immediately after it finishes.
  if (state_ == State::kCapturing)
    PauseRendering();
}

gfx::Rect DocumentTransition::GetSnapshotViewportRect() const {
  if (!style_tracker_)
    return gfx::Rect();

  return style_tracker_->GetSnapshotViewportRect();
}

gfx::Vector2d DocumentTransition::GetRootSnapshotPaintOffset() const {
  if (!style_tracker_)
    return gfx::Vector2d();

  return style_tracker_->GetRootSnapshotPaintOffset();
}

void DocumentTransition::PauseRendering() {
  DCHECK(!rendering_paused_scope_);

  if (!document_->GetPage() || !document_->View())
    return;

  auto& client = document_->GetPage()->GetChromeClient();
  rendering_paused_scope_ = client.PauseRendering(*document_->GetFrame());
  DCHECK(rendering_paused_scope_);
  client.UnregisterFromCommitObservation(this);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("blink",
                                    "DocumentTransition::PauseRendering", this);
  const base::TimeDelta kTimeout = [this]() {
    if (auto* settings = document_->GetFrame()->GetContentSettingsClient();
        settings &&
        settings->IncreaseSharedElementTransitionCallbackTimeout()) {
      return base::Seconds(15);
    } else {
      return base::Seconds(4);
    }
  }();
  document_->GetTaskRunner(TaskType::kInternalFrameLifecycleControl)
      ->PostDelayedTask(
          FROM_HERE,
          WTF::BindOnce(&DocumentTransition::OnRenderingPausedTimeout,
                        WrapWeakPersistent(this)),
          kTimeout);
}

void DocumentTransition::OnRenderingPausedTimeout() {
  if (!rendering_paused_scope_)
    return;

  ResumeRendering();

  skipTransition();

  AdvanceTo(State::kTimedOut);
}

void DocumentTransition::ResumeRendering() {
  if (!rendering_paused_scope_)
    return;

  TRACE_EVENT_NESTABLE_ASYNC_END0("blink", "DocumentTransition::PauseRendering",
                                  this);
  rendering_paused_scope_.reset();
}

}  // namespace blink

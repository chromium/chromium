// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition.h"
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
#include "third_party/blink/renderer/core/execution_context/agent.h"
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
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

const char kAbortedMessage[] = "Transition was skipped";
uint32_t NextDocumentTag() {
  static uint32_t next_document_tag = 1u;
  return next_document_tag++;
}

void RejectWithAbort(ScriptPromiseResolver* resolver) {
  auto* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  ScriptState::Scope scope(script_state);
  resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
      script_state->GetIsolate(), DOMExceptionCode::kAbortError,
      kAbortedMessage));
}

void Resolve(ScriptPromiseResolver* resolver) {
  resolver->Resolve();
}

}  // namespace

// DOMChangeFinishedCallback implementation.
ViewTransition::DOMChangeFinishedCallback::DOMChangeFinishedCallback(
    ViewTransition* transition,
    ScriptPromiseResolver* dom_updated_promise_resolver,
    bool success)
    : transition_(transition),
      dom_updated_promise_resolver_(dom_updated_promise_resolver),
      success_(success) {}

ViewTransition::DOMChangeFinishedCallback::~DOMChangeFinishedCallback() =
    default;

ScriptValue ViewTransition::DOMChangeFinishedCallback::Call(
    ScriptState* script_state,
    ScriptValue value) {
  if (transition_)
    transition_->NotifyDOMCallbackFinished(success_);

  if (success_)
    dom_updated_promise_resolver_->Resolve();
  else
    dom_updated_promise_resolver_->Reject(value);
  return ScriptValue();
}

void ViewTransition::DOMChangeFinishedCallback::Trace(Visitor* visitor) const {
  ScriptFunction::Callable::Trace(visitor);
  visitor->Trace(transition_);
  visitor->Trace(dom_updated_promise_resolver_);
}

ViewTransition::ScriptBoundState::ScriptBoundState(
    ScriptState* state,
    V8ViewTransitionCallback* callback)
    : script_state(state),
      update_dom_callback(callback),
      dom_updated_promise_resolver(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      ready_promise_resolver(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      finished_promise_resolver(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)) {}

void ViewTransition::ScriptBoundState::Trace(Visitor* visitor) const {
  visitor->Trace(script_state);
  visitor->Trace(update_dom_callback);
  visitor->Trace(dom_updated_promise_resolver);
  visitor->Trace(ready_promise_resolver);
  visitor->Trace(finished_promise_resolver);
}

// static
const char* ViewTransition::StateToString(State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kCaptureTagDiscovery:
      return "CaptureTagDiscovery";
    case State::kCaptureRequestPending:
      return "CaptureRequestPending";
    case State::kCapturing:
      return "Capturing";
    case State::kCaptured:
      return "Captured";
    case State::kWaitForRenderBlock:
      return "WaitForRenderBlock";
    case State::kDOMCallbackRunning:
      return "DOMCallbackRunning";
    case State::kDOMCallbackFinished:
      return "DOMCallbackFinished";
    case State::kAnimateTagDiscovery:
      return "AnimateTagDiscovery";
    case State::kAnimateRequestPending:
      return "AnimateRequestPending";
    case State::kAnimating:
      return "Animating";
    case State::kFinished:
      return "Finished";
    case State::kAborted:
      return "Aborted";
    case State::kTimedOut:
      return "TimedOut";
    case State::kTransitionStateCallbackDispatched:
      return "TransitionStateCallbackDispatched";
  };
  NOTREACHED();
  return "";
}

// static
ViewTransition* ViewTransition::CreateFromScript(
    Document* document,
    ScriptState* script_state,
    V8ViewTransitionCallback* callback,
    Delegate* delegate) {
  return MakeGarbageCollected<ViewTransition>(document, script_state, callback,
                                              delegate);
}

ViewTransition::ViewTransition(Document* document,
                               ScriptState* script_state,
                               V8ViewTransitionCallback* update_dom_callback,
                               Delegate* delegate)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      creation_type_(CreationType::kScript),
      document_(document),
      delegate_(delegate),
      document_tag_(NextDocumentTag()),
      script_bound_state_(
          MakeGarbageCollected<ScriptBoundState>(script_state,
                                                 update_dom_callback)),
      style_tracker_(
          MakeGarbageCollected<ViewTransitionStyleTracker>(*document_)) {
  ProcessCurrentState();
}

// static
ViewTransition* ViewTransition::CreateForSnapshotForNavigation(
    Document* document,
    ViewTransitionStateCallback callback,
    Delegate* delegate) {
  return MakeGarbageCollected<ViewTransition>(document, std::move(callback),
                                              delegate);
}

ViewTransition::ViewTransition(Document* document,
                               ViewTransitionStateCallback callback,
                               Delegate* delegate)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      creation_type_(CreationType::kForSnapshot),
      document_(document),
      delegate_(delegate),
      navigation_id_(viz::NavigationID::Create()),
      document_tag_(NextDocumentTag()),
      style_tracker_(
          MakeGarbageCollected<ViewTransitionStyleTracker>(*document_)),
      transition_state_callback_(std::move(callback)) {
  TRACE_EVENT0("blink", "ViewTransition::ViewTransition - CreatedForSnapshot");
  DCHECK(transition_state_callback_);
  ProcessCurrentState();
}

// static
ViewTransition* ViewTransition::CreateFromSnapshotForNavigation(
    Document* document,
    ViewTransitionState transition_state,
    Delegate* delegate) {
  return MakeGarbageCollected<ViewTransition>(
      document, std::move(transition_state), delegate);
}

ViewTransition::ViewTransition(Document* document,
                               ViewTransitionState transition_state,
                               Delegate* delegate)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      creation_type_(CreationType::kFromSnapshot),
      document_(document),
      delegate_(delegate),
      navigation_id_(transition_state.navigation_id),
      document_tag_(NextDocumentTag()),
      style_tracker_(MakeGarbageCollected<ViewTransitionStyleTracker>(
          *document_,
          std::move(transition_state))) {
  TRACE_EVENT0("blink",
               "ViewTransition::ViewTransition - CreatingFromSnapshot");
  bool process_next_state = AdvanceTo(State::kWaitForRenderBlock);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

void ViewTransition::skipTransition() {
  if (IsTerminalState(state_))
    return;

  // If we haven't run the dom change callback yet, schedule a task to do so.
  if (creation_type_ == CreationType::kScript &&
      static_cast<int>(state_) < static_cast<int>(State::kDOMCallbackRunning)) {
    document_->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(base::IgnoreResult(
                                     &ViewTransition::InvokeDOMChangeCallback),
                                 WrapPersistent(this)));
  }

  // If the ready promise has not yet been resolved, reject it. Note that the if
  // check here is an optimization to avoid creating the kAbort error if we've
  // already resolved the promise.
  if (creation_type_ == CreationType::kScript &&
      static_cast<int>(state_) <
          static_cast<int>(State::kAnimateRequestPending)) {
    AtMicrotask(&RejectWithAbort, script_bound_state_->ready_promise_resolver);
  }

  // If we already started processing the transition (i.e. we're beyond capture
  // tag discovery), then send a release directive.
  if (static_cast<int>(state_) >
      static_cast<int>(State::kCaptureTagDiscovery)) {
    delegate_->AddPendingRequest(
        ViewTransitionRequest::CreateRelease(document_tag_));
  }

  // Resume rendering, and finalize the rest of the state.
  ResumeRendering();
  style_tracker_->Abort();

  if (creation_type_ == CreationType::kScript) {
    AtMicrotask(&RejectWithAbort,
                script_bound_state_->finished_promise_resolver);
  }

  delegate_->OnTransitionFinished(this);

  // This should be the last call in this function to avoid erroneously checking
  // the `state_` against the wrong state.
  AdvanceTo(State::kAborted);
}

ScriptPromise ViewTransition::finished() const {
  DCHECK(script_bound_state_);
  return script_bound_state_->finished_promise_resolver->Promise();
}

ScriptPromise ViewTransition::ready() const {
  DCHECK(script_bound_state_);
  return script_bound_state_->ready_promise_resolver->Promise();
}

ScriptPromise ViewTransition::domUpdated() const {
  DCHECK(script_bound_state_);
  return script_bound_state_->dom_updated_promise_resolver->Promise();
}

bool ViewTransition::AdvanceTo(State state) {
  DCHECK(CanAdvanceTo(state)) << "Current state " << static_cast<int>(state_)
                              << " new state " << static_cast<int>(state);
  state_ = state;

  // If we need to run in a lifecycle, but we're not in one, then make sure to
  // schedule an animation in case we wouldn't get one naturally.
  if (StateRunsInViewTransitionStepsDuringMainFrame(state_) !=
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

bool ViewTransition::CanAdvanceTo(State state) const {
  // This documents valid state transitions. Note that this does not make a
  // judgement call about whether the state runs synchronously or not,
  // so we allow some transitions that would not be possible in a synchronous
  // run, like kCaptured -> kAborted. This isn't possible in a synchronous call,
  // because kCaptured will always go to kDOMCallbackRunning.

  switch (state_) {
    case State::kInitial:
      return state == State::kCaptureTagDiscovery ||
             state == State::kWaitForRenderBlock;
    case State::kCaptureTagDiscovery:
      return state == State::kCaptureRequestPending || state == State::kAborted;
    case State::kCaptureRequestPending:
      return state == State::kCapturing || state == State::kAborted;
    case State::kCapturing:
      return state == State::kCaptured || state == State::kAborted;
    case State::kCaptured:
      return state == State::kDOMCallbackRunning ||
             state == State::kDOMCallbackFinished || state == State::kAborted ||
             state == State::kTransitionStateCallbackDispatched;
    case State::kTransitionStateCallbackDispatched:
      // This transition must finish on a ViewTransition bound to the new
      // Document.
      return state == State::kAborted;
    case State::kWaitForRenderBlock:
      return state == State::kAnimateTagDiscovery || state == State::kAborted;
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
bool ViewTransition::StateRunsInViewTransitionStepsDuringMainFrame(
    State state) {
  switch (state) {
    case State::kInitial:
      return false;
    case State::kCaptureTagDiscovery:
    case State::kCaptureRequestPending:
      return true;
    case State::kCapturing:
    case State::kCaptured:
    case State::kWaitForRenderBlock:
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
    case State::kTransitionStateCallbackDispatched:
      return false;
  }
  NOTREACHED();
  return false;
}

// static
bool ViewTransition::WaitsForNotification(State state) {
  return state == State::kCapturing || state == State::kDOMCallbackRunning ||
         state == State::kWaitForRenderBlock ||
         state == State::kTransitionStateCallbackDispatched;
}

// static
bool ViewTransition::IsTerminalState(State state) {
  return state == State::kFinished || state == State::kAborted ||
         state == State::kTimedOut;
}

void ViewTransition::WillDetachFromView() {
  TRACE_EVENT0("blink", "ViewTransition::WillDetachFromView");

  skipTransition();
}

void ViewTransition::ProcessCurrentState() {
  bool process_next_state = true;
  while (process_next_state) {
    DCHECK_EQ(in_main_lifecycle_update_,
              StateRunsInViewTransitionStepsDuringMainFrame(state_));
    TRACE_EVENT1("blink", "ViewTransition::ProcessCurrentState", "state",
                 StateToString(state_));
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

        delegate_->AddPendingRequest(ViewTransitionRequest::CreateCapture(
            document_tag_, style_tracker_->CapturedTagCount(), navigation_id_,
            style_tracker_->TakeCaptureResourceIds(),
            ConvertToBaseOnceCallback(
                CrossThreadBindOnce(&ViewTransition::NotifyCaptureFinished,
                                    WrapCrossThreadWeakPersistent(this)))));

        if (document_->GetFrame()->IsLocalRoot()) {
          document_->GetPage()->GetChromeClient().StopDeferringCommits(
              *document_->GetFrame(),
              cc::PaintHoldingCommitTrigger::kViewTransition);
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

        if (creation_type_ == CreationType::kForSnapshot) {
          DCHECK(transition_state_callback_);
          ViewTransitionState view_transition_state =
              style_tracker_->GetViewTransitionState();
          view_transition_state.navigation_id = navigation_id_;

          process_next_state =
              AdvanceTo(State::kTransitionStateCallbackDispatched);
          DCHECK(process_next_state);

          std::move(transition_state_callback_)
              .Run(std::move(view_transition_state));
          break;
        }

        // The following logic is only executed for ViewTransition objects
        // created by the script API.
        DCHECK(script_bound_state_);

        // TODO(vmpstr): Maybe fold this into InvokeDOMChangeCallback somehow.
        if (!script_bound_state_->update_dom_callback) {
          AtMicrotask(&Resolve,
                      script_bound_state_->dom_updated_promise_resolver);

          dom_callback_succeeded_ = true;
          process_next_state = AdvanceTo(State::kDOMCallbackFinished);
          DCHECK(process_next_state);
          break;
        }

        if (!InvokeDOMChangeCallback()) {
          AtMicrotask(&RejectWithAbort,
                      script_bound_state_->dom_updated_promise_resolver);
          skipTransition();
          break;
        }
        process_next_state = AdvanceTo(State::kDOMCallbackRunning);
        DCHECK(process_next_state);
        break;
      }

      case State::kWaitForRenderBlock:
        DCHECK(WaitsForNotification(state_));
        break;

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
            DocumentUpdateReason::kViewTransition);
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

        delegate_->AddPendingRequest(
            ViewTransitionRequest::CreateAnimateRenderer(document_tag_,
                                                         navigation_id_));
        process_next_state = AdvanceTo(State::kAnimating);
        DCHECK(!process_next_state);

        DCHECK(!in_main_lifecycle_update_);
        if (creation_type_ == CreationType::kScript)
          AtMicrotask(&Resolve, script_bound_state_->ready_promise_resolver);
        break;

      case State::kAnimating: {
        if (first_animating_frame_) {
          first_animating_frame_ = false;
          break;
        }

        if (style_tracker_->HasActiveAnimations())
          break;

        style_tracker_->StartFinished();

        if (creation_type_ == CreationType::kScript)
          AtMicrotask(&Resolve, script_bound_state_->finished_promise_resolver);

        delegate_->AddPendingRequest(
            ViewTransitionRequest::CreateRelease(document_tag_));
        delegate_->OnTransitionFinished(this);

        style_tracker_ = nullptr;
        process_next_state = AdvanceTo(State::kFinished);
        DCHECK(!process_next_state);
        break;
      }
      case State::kFinished:
      case State::kAborted:
      case State::kTimedOut:
      case State::kTransitionStateCallbackDispatched:
        break;
    }
  }
}

void ViewTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(style_tracker_);
  visitor->Trace(script_bound_state_);

  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

bool ViewTransition::InvokeDOMChangeCallback() {
  DCHECK(script_bound_state_);

  if (!script_bound_state_->update_dom_callback)
    return true;

  v8::Maybe<ScriptPromise> result =
      script_bound_state_->update_dom_callback->Invoke(nullptr);
  // TODO(vmpstr): Should this be a DCHECK?
  if (result.IsNothing())
    return false;

  ScriptState::Scope scope(script_bound_state_->script_state);

  result.ToChecked().Then(
      MakeGarbageCollected<ScriptFunction>(
          script_bound_state_->script_state,
          MakeGarbageCollected<DOMChangeFinishedCallback>(
              this, script_bound_state_->dom_updated_promise_resolver, true)),
      MakeGarbageCollected<ScriptFunction>(
          script_bound_state_->script_state,
          MakeGarbageCollected<DOMChangeFinishedCallback>(
              this, script_bound_state_->dom_updated_promise_resolver, false)));
  return true;
}

void ViewTransition::ContextDestroyed() {
  TRACE_EVENT0("blink", "ViewTransition::ContextDestroyed");
  // TODO(khushalsagar): This needs to be called for pages entering BFCache.
  WillDetachFromView();
}

bool ViewTransition::HasPendingActivity() const {
  return !IsTerminalState(state_);
}

void ViewTransition::NotifyCaptureFinished() {
  if (state_ != State::kCapturing) {
    DCHECK(IsTerminalState(state_));
    return;
  }
  bool process_next_state = AdvanceTo(State::kCaptured);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

void ViewTransition::NotifyDOMCallbackFinished(bool success) {
  dom_callback_succeeded_ = success;
  if (IsTerminalState(state_))
    return;

  bool process_next_state = AdvanceTo(State::kDOMCallbackFinished);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

bool ViewTransition::NeedsSharedElementEffectNode(
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

bool ViewTransition::IsRepresentedViaPseudoElements(
    const LayoutObject& object) const {
  if (IsTerminalState(state_))
    return false;

  if (IsA<LayoutView>(object))
    return style_tracker_->IsRootTransitioning();

  auto* element = DynamicTo<Element>(object.GetNode());
  return element && style_tracker_->IsSharedElement(element);
}

PaintPropertyChangeType ViewTransition::UpdateEffect(
    const LayoutObject& object,
    const EffectPaintPropertyNodeOrAlias& current_effect,
    const ClipPaintPropertyNodeOrAlias* current_clip,
    const TransformPaintPropertyNodeOrAlias* current_transform) {
  DCHECK(NeedsSharedElementEffectNode(object));
  DCHECK(current_transform);
  DCHECK(current_clip);

  EffectPaintPropertyNode::State state;
  state.direct_compositing_reasons =
      CompositingReason::kViewTransitionSharedElement;
  state.local_transform_space = current_transform;
  state.output_clip = current_clip;
  state.view_transition_shared_element_id =
      ViewTransitionElementId(document_tag_);
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      object.UniqueId(),
      CompositorElementIdNamespace::kSharedElementTransition);
  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    // The only non-element participant is the layout view.
    DCHECK(object.IsLayoutView());

    style_tracker_->UpdateRootIndexAndSnapshotId(
        state.view_transition_shared_element_id,
        state.view_transition_element_resource_id);
    DCHECK(state.view_transition_shared_element_id.valid() ||
           !style_tracker_->IsRootTransitioning());
    return style_tracker_->UpdateRootEffect(std::move(state), current_effect);
  }

  style_tracker_->UpdateElementIndicesAndSnapshotId(
      element, state.view_transition_shared_element_id,
      state.view_transition_element_resource_id);
  return style_tracker_->UpdateEffect(element, std::move(state),
                                      current_effect);
}

EffectPaintPropertyNode* ViewTransition::GetEffect(
    const LayoutObject& object) const {
  DCHECK(NeedsSharedElementEffectNode(object));

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element)
    return style_tracker_->GetRootEffect();
  return style_tracker_->GetEffect(element);
}

void ViewTransition::VerifySharedElements() {
  if (!IsTerminalState(state_))
    style_tracker_->VerifySharedElements();
}

void ViewTransition::RunViewTransitionStepsDuringMainFrame() {
  base::AutoReset<bool> scope(&in_main_lifecycle_update_, true);
  if (StateRunsInViewTransitionStepsDuringMainFrame(state_))
    ProcessCurrentState();
  if (style_tracker_ &&
      document_->Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean) {
    style_tracker_->RunPostPrePaintSteps();
  }
}

bool ViewTransition::NeedsUpToDateTags() const {
  return state_ == State::kCaptureTagDiscovery ||
         state_ == State::kAnimateTagDiscovery;
}

PseudoElement* ViewTransition::CreatePseudoElement(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) {
  DCHECK(style_tracker_);

  return style_tracker_->CreatePseudoElement(parent, pseudo_id,
                                             view_transition_name);
}

String ViewTransition::UAStyleSheet() const {
  // TODO(vmpstr): We can still request getComputedStyle(html,
  // "::view-transition-pseudo") outside of a page transition. What should we
  // return in that case?
  if (!style_tracker_)
    return "";
  return style_tracker_->UAStyleSheet();
}

void ViewTransition::WillCommitCompositorFrame() {
  // There should only be 1 commit when we're in the capturing phase and
  // rendering is paused immediately after it finishes.
  if (state_ == State::kCapturing)
    PauseRendering();
}

gfx::Rect ViewTransition::GetSnapshotViewportRect() const {
  if (!style_tracker_)
    return gfx::Rect();

  return style_tracker_->GetSnapshotViewportRect();
}

gfx::Vector2d ViewTransition::GetRootSnapshotPaintOffset() const {
  if (!style_tracker_)
    return gfx::Vector2d();

  return style_tracker_->GetRootSnapshotPaintOffset();
}

void ViewTransition::PauseRendering() {
  DCHECK(!rendering_paused_scope_);

  if (!document_->GetPage() || !document_->View())
    return;

  auto& client = document_->GetPage()->GetChromeClient();
  rendering_paused_scope_ = client.PauseRendering(*document_->GetFrame());
  DCHECK(rendering_paused_scope_);
  client.UnregisterFromCommitObservation(this);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("blink", "ViewTransition::PauseRendering",
                                    this);
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
      ->PostDelayedTask(FROM_HERE,
                        WTF::BindOnce(&ViewTransition::OnRenderingPausedTimeout,
                                      WrapWeakPersistent(this)),
                        kTimeout);
}

void ViewTransition::OnRenderingPausedTimeout() {
  if (!rendering_paused_scope_)
    return;

  ResumeRendering();
  skipTransition();
  AdvanceTo(State::kTimedOut);
}

void ViewTransition::ResumeRendering() {
  if (!rendering_paused_scope_)
    return;

  TRACE_EVENT_NESTABLE_ASYNC_END0("blink", "ViewTransition::PauseRendering",
                                  this);
  rendering_paused_scope_.reset();
}

void ViewTransition::AtMicrotask(void callback(ScriptPromiseResolver*),
                                 ScriptPromiseResolver* resolver) {
  document_->GetAgent().event_loop()->EnqueueMicrotask(
      WTF::BindOnce(callback, WrapPersistent(resolver)));
}

void ViewTransition::WillBeginMainFrame() {
  if (state_ != State::kWaitForRenderBlock)
    return;

  // WillBeginMainFrame() implies that rendering has started. If we were waiting
  // for render-blocking resources to be loaded, they must have been fetched (or
  // timed out) before rendering is started.
  DCHECK(document_->RenderingHasBegun());
  bool process_next_state = AdvanceTo(State::kAnimateTagDiscovery);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

}  // namespace blink

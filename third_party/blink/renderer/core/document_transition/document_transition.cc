// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include <vector>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/document_transition/document_transition_request.h"
#include "cc/trees/paint_holding_reason.h"
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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

const char kAbortedFromStart[] = "Aborted due to start() call";
const char kAbortedFromScript[] = "Aborted due to abort() call";
const char kAbortedFromCallback[] =
    "Aborted due to failure in DocumentTransitionCallback";
const char kAbortedFromCallbackTimeout[] =
    "Aborted due to timeout in DocumentTransitionCallback";
const char kAbortedFromInvalidConfigAtStart[] =
    "Start failed: invalid element configuration";

uint32_t NextDocumentTag() {
  static uint32_t next_document_tag = 1u;
  return next_document_tag++;
}

}  // namespace

DocumentTransition::PostCaptureResolved::PostCaptureResolved(
    DocumentTransition* transition,
    bool success,
    Document* document)
    : transition_(transition), success_(success), document_(document) {}

DocumentTransition::PostCaptureResolved::~PostCaptureResolved() = default;

ScriptValue DocumentTransition::PostCaptureResolved::Call(
    ScriptState* script_state,
    ScriptValue value) {
  if (transition_)
    transition_->NotifyPostCaptureCallbackResolved(success_);

  if (!success_) {
    auto* isolate = document_->GetExecutionContext()->GetIsolate();
    v8::Local<v8::Message> message =
        v8::Exception::CreateMessage(isolate, value.V8Value());
    std::unique_ptr<SourceLocation> location = SourceLocation::FromMessage(
        isolate, message, document_->GetExecutionContext());
    ErrorEvent* error = ErrorEvent::Create(
        ToCoreStringWithNullCheck(message->Get()), std::move(location), value,
        &DOMWrapperWorld::MainWorld());
    document_->domWindow()->DispatchErrorEvent(error,
                                               SanitizeScriptErrors::kSanitize);
  }
  return ScriptValue();
}

void DocumentTransition::PostCaptureResolved::Trace(Visitor* visitor) const {
  ScriptFunction::Callable::Trace(visitor);
  visitor->Trace(transition_);
  visitor->Trace(document_);
}

void DocumentTransition::PostCaptureResolved::Cancel() {
  DCHECK(transition_);
  transition_ = nullptr;
}

DocumentTransition::DocumentTransition(Document* document)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      document_(document),
      document_tag_(NextDocumentTag()) {}

void DocumentTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(capture_resolved_callback_);
  visitor->Trace(start_script_state_);
  visitor->Trace(post_capture_success_callable_);
  visitor->Trace(post_capture_reject_callable_);
  visitor->Trace(finished_promise_resolver_);
  visitor->Trace(prepare_promise_resolver_);
  visitor->Trace(style_tracker_);

  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void DocumentTransition::ContextDestroyed() {
  ResetTransitionState();
  ResetScriptState(nullptr);
}

bool DocumentTransition::HasPendingActivity() const {
  return style_tracker_;
}

bool DocumentTransition::StartNewTransition() {
  if (state_ != State::kIdle || style_tracker_)
    return false;

  DCHECK(!capture_resolved_callback_);
  DCHECK(!post_capture_success_callable_);
  DCHECK(!post_capture_reject_callable_);
  DCHECK(!prepare_promise_resolver_);
  DCHECK(!finished_promise_resolver_);
  style_tracker_ =
      MakeGarbageCollected<DocumentTransitionStyleTracker>(*document_);
  return true;
}

ScriptPromise DocumentTransition::start(ScriptState* script_state,
                                        ExceptionState& exception_state) {
  return start(script_state, nullptr, exception_state);
}

ScriptPromise DocumentTransition::start(ScriptState* script_state,
                                        V8DocumentTransitionCallback* callback,
                                        ExceptionState& exception_state) {
  bool success = InitiateTransition(script_state, callback, exception_state);
  DCHECK(!success || finished_promise_resolver_);
  return success ? finished_promise_resolver_->Promise() : ScriptPromise();
}

ScriptPromise DocumentTransition::prepare(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  return prepare(script_state, nullptr, exception_state);
}

ScriptPromise DocumentTransition::prepare(
    ScriptState* script_state,
    V8DocumentTransitionCallback* callback,
    ExceptionState& exception_state) {
  bool success = InitiateTransition(script_state, callback, exception_state);
  DCHECK(!success || prepare_promise_resolver_);
  return success ? prepare_promise_resolver_->Promise() : ScriptPromise();
}

bool DocumentTransition::InitiateTransition(
    ScriptState* script_state,
    V8DocumentTransitionCallback* callback,
    ExceptionState& exception_state) {
  switch (state_) {
    case State::kIdle:
      if (!document_ || !document_->View()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "The document must be connected to a window.");
        return false;
      }
      if (!style_tracker_) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "Transition is aborted.");
        return false;
      }
      break;
    case State::kCapturing:
    case State::kCaptured:
    case State::kStarted:
      CancelPendingTransition(kAbortedFromStart);
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Transition aborted, invalid captureAndHold call");
      return false;
  }

  // Get the sequence id before any early outs so we will correctly process
  // callbacks from previous requests.
  last_prepare_sequence_id_ = next_sequence_id_++;

  style_tracker_->AddSharedElementsFromCSS();
  bool capture_succeeded = style_tracker_->Capture();
  if (!capture_succeeded) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Capture failed: invalid element configuration.");
    ResetTransitionState();
    return false;
  }

  // PREPARE PHASE
  // The capture request below will initiate an async operation to cache
  // textures for the current DOM. The |capture_resolved_callback_| is invoked
  // when that async operation finishes. When the callback is finished,
  // `prepare_promise_resolver_` is resolved.

  //
  // START PHASE
  // When this async callback finishes executing, animations are started using
  // images from old and new DOM elements. The |finished_promise_resolver_|
  // returned here resolves when these animations finish.
  capture_resolved_callback_ = callback;
  start_script_state_ = script_state;
  finished_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  prepare_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  state_ = State::kCapturing;
  pending_request_ = DocumentTransitionRequest::CreateCapture(
      document_tag_, style_tracker_->CapturedTagCount(),
      style_tracker_->TakeCaptureResourceIds(),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          &DocumentTransition::NotifyCaptureFinished,
          WrapCrossThreadWeakPersistent(this), last_prepare_sequence_id_)));

  NotifyHasChangesToCommit();
  return true;
}

void DocumentTransition::abandon(ScriptState*, ExceptionState&) {
  CancelPendingTransition(kAbortedFromScript);
}

ScriptPromise DocumentTransition::finished() const {
  return finished_promise_resolver_ ? finished_promise_resolver_->Promise()
                                    : ScriptPromise();
}

void DocumentTransition::NotifyHasChangesToCommit() {
  if (!document_ || !document_->GetPage() || !document_->View())
    return;

  // Schedule a new frame.
  document_->View()->ScheduleAnimation();

  // Ensure paint artifact compositor does an update, since that's the mechanism
  // we use to pass transition requests to the compositor.
  document_->View()->SetPaintArtifactCompositorNeedsUpdate(
      PaintArtifactCompositorUpdateReason::kDocumentTransitionNotifyChanges);
}

void DocumentTransition::NotifyCaptureFinished(uint32_t sequence_id) {
  // This notification is for a different sequence id.
  if (sequence_id != last_prepare_sequence_id_)
    return;

  // We could have abandoned the transition before capture finishes.
  if (state_ == State::kIdle) {
    return;
  }

  DCHECK(state_ == State::kCapturing);
  if (style_tracker_)
    style_tracker_->CaptureResolved();

  // Defer commits before resolving the promise to ensure any updates made in
  // the callback are deferred.
  StartDeferringCommits();
  if (!capture_resolved_callback_) {
    state_ = State::kCaptured;
    NotifyPostCaptureCallbackResolved(/*success=*/true);
    return;
  }

  v8::Maybe<ScriptPromise> result = capture_resolved_callback_->Invoke(nullptr);
  if (result.IsNothing()) {
    CancelPendingTransition(kAbortedFromCallback);
    return;
  }

  post_capture_success_callable_ =
      MakeGarbageCollected<PostCaptureResolved>(this, true, document_);
  post_capture_reject_callable_ =
      MakeGarbageCollected<PostCaptureResolved>(this, false, document_);

  ScriptState::Scope scope(start_script_state_);
  result.ToChecked().Then(
      MakeGarbageCollected<ScriptFunction>(start_script_state_,
                                           post_capture_success_callable_),
      MakeGarbageCollected<ScriptFunction>(start_script_state_,
                                           post_capture_reject_callable_));

  capture_resolved_callback_ = nullptr;
  state_ = State::kCaptured;
}

void DocumentTransition::NotifyStartFinished(uint32_t sequence_id) {
  // This notification is for a different sequence id.
  if (sequence_id != last_start_sequence_id_)
    return;

  // We could have detached the resolver if the execution context was destroyed.
  if (!finished_promise_resolver_)
    return;

  DCHECK(state_ == State::kStarted);
  DCHECK(finished_promise_resolver_);
  DCHECK(!prepare_promise_resolver_);
  finished_promise_resolver_->Resolve();
  finished_promise_resolver_ = nullptr;
  start_script_state_ = nullptr;

  // Resolve the promise to notify script when animations finish but don't
  // remove the pseudo element tree.
  if (disable_end_transition_)
    return;

  style_tracker_->StartFinished();
  pending_request_ = DocumentTransitionRequest::CreateRelease(document_tag_);
  NotifyHasChangesToCommit();
  ResetTransitionState(/*abort_style_tracker=*/false);
}

void DocumentTransition::NotifyPostCaptureCallbackResolved(bool success) {
  DCHECK_EQ(state_, State::kCaptured);
  DCHECK(style_tracker_);
  DCHECK(finished_promise_resolver_);
  DCHECK(prepare_promise_resolver_);
  DCHECK(!capture_resolved_callback_);

  StopDeferringCommits();

  if (!success) {
    CancelPendingTransition(kAbortedFromCallback);
    return;
  }

  style_tracker_->AddSharedElementsFromCSS();
  bool start_succeeded = style_tracker_->Start();
  if (!start_succeeded) {
    CancelPendingTransition(kAbortedFromInvalidConfigAtStart);
    return;
  }

  last_start_sequence_id_ = next_sequence_id_++;
  state_ = State::kStarted;
  post_capture_success_callable_ = nullptr;
  post_capture_reject_callable_ = nullptr;
  pending_request_ =
      DocumentTransitionRequest::CreateAnimateRenderer(document_tag_);
  NotifyHasChangesToCommit();

  // Resolve the prepare promise, since the animation has started.
  prepare_promise_resolver_->Resolve();
  prepare_promise_resolver_ = nullptr;
}

std::unique_ptr<DocumentTransitionRequest>
DocumentTransition::TakePendingRequest() {
  return std::move(pending_request_);
}

bool DocumentTransition::NeedsSharedElementEffectNode(
    const LayoutObject& object) const {
  // Layout view always needs an effect node, even if root itself is not
  // transitioning. The reason for this is that we want the root to have an
  // effect which can be hoisted up be the sibling of the layout view. This
  // simplifies calling code to have a consistent stacking context structure.
  if (IsA<LayoutView>(object))
    return state_ != State::kIdle;

  // Otherwise check if the layout object has an active shared element.
  auto* element = DynamicTo<Element>(object.GetNode());
  return element && style_tracker_ && style_tracker_->IsSharedElement(element);
}

bool DocumentTransition::IsRepresentedViaPseudoElements(
    const LayoutObject& object) const {
  if (!style_tracker_)
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
  if (state_ != State::kIdle)
    style_tracker_->VerifySharedElements();
}

void DocumentTransition::RunPostPrePaintSteps() {
  DCHECK(document_->Lifecycle().GetState() >=
         DocumentLifecycle::LifecycleState::kPrePaintClean);

  if (style_tracker_) {
    style_tracker_->RunPostPrePaintSteps();
    // If we don't have active animations, schedule a frame to end the
    // transition. Note that if we don't have finished_promise_resolver_ we
    // don't need to finish the animation, since it should already be done. See
    // the DCHECK below.
    //
    // TODO(vmpstr): Note that RunPostPrePaintSteps can happen multiple times
    // during a lifecycle update. These checks don't have to happen here, and
    // could perhaps be moved to DidFinishLifecycleUpdate.
    //
    // We can end up here multiple times, but if we are in a started state and
    // don't have a start promise resolver then the only way we're here is if we
    // disabled end transition.
    DCHECK(state_ != State::kStarted || finished_promise_resolver_ ||
           disable_end_transition_);
    if (state_ == State::kStarted && !style_tracker_->HasActiveAnimations() &&
        finished_promise_resolver_) {
      DCHECK(document_->View());
      document_->View()->RegisterForLifecycleNotifications(this);
      document_->View()->ScheduleAnimation();
    }
  }
}

void DocumentTransition::WillStartLifecycleUpdate(const LocalFrameView&) {
  DCHECK_EQ(state_, State::kStarted);
  DCHECK(document_);
  DCHECK(document_->View());
  DCHECK(style_tracker_);

  if (!style_tracker_->HasActiveAnimations())
    NotifyStartFinished(last_start_sequence_id_);
  document_->View()->UnregisterFromLifecycleNotifications(this);
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

void DocumentTransition::StartDeferringCommits() {
  DCHECK(!deferring_commits_);

  if (!document_->GetPage() || !document_->View())
    return;

  // Don't do paint holding if it could already be in progress for first
  // contentful paint.
  if (document_->View()->WillDoPaintHoldingForFCP())
    return;

  // Based on the viz side timeout to hold snapshots for 5 seconds.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "blink", "DocumentTransition::DeferringCommits", this);
  constexpr base::TimeDelta kTimeout = base::Seconds(4);
  auto& client = document_->GetPage()->GetChromeClient();
  deferring_commits_ =
      client.StartDeferringCommits(*document_->GetFrame(), kTimeout,
                                   cc::PaintHoldingReason::kDocumentTransition);
  DCHECK(deferring_commits_);
  client.RegisterForDeferredCommitObservation(this);
}

void DocumentTransition::WillStopDeferringCommits(
    cc::PaintHoldingCommitTrigger trigger) {
  // We don't expect to have any other triggers here, since we only register for
  // the time we start deferring commits.
  DCHECK(trigger == cc::PaintHoldingCommitTrigger::kDocumentTransition ||
         trigger == cc::PaintHoldingCommitTrigger::kTimeoutDocumentTransition);
  if (trigger == cc::PaintHoldingCommitTrigger::kTimeoutDocumentTransition)
    CancelPendingTransition(kAbortedFromCallbackTimeout);
  document_->GetPage()
      ->GetChromeClient()
      .UnregisterFromDeferredCommitObservation(this);
}

void DocumentTransition::StopDeferringCommits() {
  if (!deferring_commits_)
    return;

  TRACE_EVENT_NESTABLE_ASYNC_END0("blink",
                                  "DocumentTransition::DeferringCommits", this);
  deferring_commits_ = false;
  if (!document_ || !document_->GetPage())
    return;

  document_->GetPage()->GetChromeClient().StopDeferringCommits(
      *document_->GetFrame(),
      cc::PaintHoldingCommitTrigger::kDocumentTransition);
}

void DocumentTransition::CancelPendingTransition(const char* abort_message) {
  bool need_release_directive = state_ == State::kStarted;
  ResetTransitionState();
  ResetScriptState(abort_message);

  if (need_release_directive) {
    pending_request_ = DocumentTransitionRequest::CreateRelease(document_tag_);
    NotifyHasChangesToCommit();
  }
}

void DocumentTransition::ResetTransitionState(bool abort_style_tracker) {
  if (abort_style_tracker) {
    if (style_tracker_)
      style_tracker_->Abort();
    pending_request_.reset();
  }
  style_tracker_ = nullptr;
  StopDeferringCommits();
  state_ = State::kIdle;
}

void DocumentTransition::ResetScriptState(const char* abort_message) {
  capture_resolved_callback_ = nullptr;

  if (post_capture_success_callable_) {
    DCHECK(post_capture_reject_callable_);

    post_capture_success_callable_->Cancel();
    post_capture_success_callable_ = nullptr;

    post_capture_reject_callable_->Cancel();
    post_capture_reject_callable_ = nullptr;
  }

  if (start_script_state_ && start_script_state_->ContextIsValid()) {
    auto finalize = [this, &abort_message](ScriptPromiseResolver* resolver) {
      if (!resolver)
        return;
      ScriptState::Scope scope(start_script_state_);
      if (abort_message) {
        resolver->Reject(V8ThrowDOMException::CreateOrDie(
            resolver->GetScriptState()->GetIsolate(),
            DOMExceptionCode::kAbortError, abort_message));
      } else {
        resolver->Detach();
      }
    };
    finalize(prepare_promise_resolver_);
    finalize(finished_promise_resolver_);
  }
  prepare_promise_resolver_ = nullptr;
  finished_promise_resolver_ = nullptr;
  start_script_state_ = nullptr;
}

}  // namespace blink

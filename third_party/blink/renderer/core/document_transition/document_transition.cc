// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include <vector>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/document_transition/document_transition_request.h"
#include "cc/trees/paint_holding_reason.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_set_element_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

const char kAbortedFromCaptureAndHold[] =
    "Aborted due to captureAndHold() call";
const char kAbortedFromScript[] = "Aborted due to abort() call";

uint32_t NextDocumentTag() {
  static uint32_t next_document_tag = 1u;
  return next_document_tag++;
}

}  // namespace

DocumentTransition::DocumentTransition(Document* document)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      document_(document),
      document_tag_(NextDocumentTag()) {}

void DocumentTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(capture_promise_resolver_);
  visitor->Trace(start_promise_resolver_);
  visitor->Trace(style_tracker_);

  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void DocumentTransition::ContextDestroyed() {
  if (capture_promise_resolver_) {
    capture_promise_resolver_->Detach();
    capture_promise_resolver_ = nullptr;
  }
  if (start_promise_resolver_) {
    start_promise_resolver_->Detach();
    start_promise_resolver_ = nullptr;
  }
  ResetState();
}

bool DocumentTransition::HasPendingActivity() const {
  if (capture_promise_resolver_ || start_promise_resolver_)
    return true;
  return false;
}

void DocumentTransition::AssertNoTransition() {
  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!style_tracker_);
  DCHECK(!capture_promise_resolver_);
  DCHECK(!start_promise_resolver_);
}

void DocumentTransition::StartNewTransition() {
  style_tracker_ =
      MakeGarbageCollected<DocumentTransitionStyleTracker>(*document_);
}

void DocumentTransition::FinalizeNewTransition() {}

void DocumentTransition::setElement(
    ScriptState* script_state,
    Element* element,
    const AtomicString& tag,
    const DocumentTransitionSetElementOptions* opts,
    ExceptionState& exception_state) {
  DCHECK(style_tracker_);
  if (tag.IsNull())
    style_tracker_->RemoveSharedElement(element);
  else
    style_tracker_->AddSharedElement(element, tag);
}

ScriptPromise DocumentTransition::captureAndHold(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // Reject any previous prepare promises.
  if (state_ == State::kCapturing || state_ == State::kCaptured)
    CancelPendingTransition(kAbortedFromCaptureAndHold);

  // Get the sequence id before any early outs so we will correctly process
  // callbacks from previous requests.
  last_prepare_sequence_id_ = next_sequence_id_++;

  // If we are not attached to a view, then we can't prepare a transition.
  // Reject the promise.
  if (!document_ || !document_->View()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The document must be connected to a window.");
    return ScriptPromise();
  }

  // We also reject the promise if we're in any state other than idle.
  if (state_ != State::kIdle) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The document is already executing a transition.");
    return ScriptPromise();
  }

  capture_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  state_ = State::kCapturing;
  pending_request_ = DocumentTransitionRequest::CreateCapture(
      document_tag_, style_tracker_->PendingSharedElementCount() + 1,
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          &DocumentTransition::NotifyCaptureFinished,
          WrapCrossThreadWeakPersistent(this), last_prepare_sequence_id_)));

  style_tracker_->Capture();
  NotifyHasChangesToCommit();

  return capture_promise_resolver_->Promise();
}

ScriptPromise DocumentTransition::start(ScriptState* script_state,
                                        ExceptionState& exception_state) {
  if (state_ != State::kCaptured) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Transition must be prepared before it can be started.");
    return ScriptPromise();
  }

  StopDeferringCommits();

  last_start_sequence_id_ = next_sequence_id_++;
  state_ = State::kStarted;
  start_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_request_ =
      DocumentTransitionRequest::CreateAnimateRenderer(document_tag_);
  style_tracker_->Start();

  NotifyHasChangesToCommit();
  return start_promise_resolver_->Promise();
}

void DocumentTransition::ignoreCSSTaggedElements(ScriptState*,
                                                 ExceptionState&) {}

void DocumentTransition::abandon(ScriptState*, ExceptionState&) {
  CancelPendingTransition(kAbortedFromScript);
}

void DocumentTransition::NotifyHasChangesToCommit() {
  if (!document_ || !document_->GetPage() || !document_->View())
    return;

  // Schedule a new frame.
  document_->View()->ScheduleAnimation();

  // Ensure paint artifact compositor does an update, since that's the mechanism
  // we use to pass transition requests to the compositor.
  document_->View()->SetPaintArtifactCompositorNeedsUpdate();
}

void DocumentTransition::NotifyCaptureFinished(uint32_t sequence_id) {
  // This notification is for a different sequence id.
  if (sequence_id != last_prepare_sequence_id_)
    return;

  // We could have detached the resolver if the execution context was destroyed.
  if (!capture_promise_resolver_)
    return;

  DCHECK(state_ == State::kCapturing);
  DCHECK(capture_promise_resolver_);
  if (style_tracker_)
    style_tracker_->CaptureResolved();

  // Defer commits before resolving the promise to ensure any updates made in
  // the callback are deferred.
  StartDeferringCommits();
  capture_promise_resolver_->Resolve();
  capture_promise_resolver_ = nullptr;
  state_ = State::kCaptured;
}

void DocumentTransition::NotifyStartFinished(uint32_t sequence_id) {
  // This notification is for a different sequence id.
  if (sequence_id != last_start_sequence_id_)
    return;

  // We could have detached the resolver if the execution context was destroyed.
  if (!start_promise_resolver_)
    return;

  DCHECK(state_ == State::kStarted);
  DCHECK(start_promise_resolver_);
  start_promise_resolver_->Resolve();
  start_promise_resolver_ = nullptr;

  // Resolve the promise to notify script when animations finish but don't
  // remove the pseudo element tree.
  if (disable_end_transition_)
    return;

  style_tracker_->StartFinished();
  pending_request_ = DocumentTransitionRequest::CreateRelease(document_tag_);
  NotifyHasChangesToCommit();
  ResetState(/*abort_style_tracker=*/false);
}

std::unique_ptr<DocumentTransitionRequest>
DocumentTransition::TakePendingRequest() {
  return std::move(pending_request_);
}

bool DocumentTransition::IsTransitionParticipant(
    const LayoutObject& object) const {
  // If our state is idle and we're outside of script mutation scope, it implies
  // that we have no style tracker.
  DCHECK(state_ != State::kIdle || script_mutations_allowed_ ||
         !style_tracker_);

  // The layout view is always a participant if there is a transition.
  if (auto* layout_view = DynamicTo<LayoutView>(object))
    return state_ != State::kIdle;

  // Otherwise check if the layout object has an active shared element.
  auto* element = DynamicTo<Element>(object.GetNode());
  return element && style_tracker_ && style_tracker_->IsSharedElement(element);
}

PaintPropertyChangeType DocumentTransition::UpdateEffect(
    const LayoutObject& object,
    const EffectPaintPropertyNodeOrAlias& current_effect,
    const TransformPaintPropertyNodeOrAlias* current_transform) {
  DCHECK(IsTransitionParticipant(object));
  DCHECK(current_transform);

  EffectPaintPropertyNode::State state;
  state.direct_compositing_reasons =
      CompositingReason::kDocumentTransitionSharedElement;
  state.local_transform_space = current_transform;
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
    DCHECK(state.document_transition_shared_element_id.valid());
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
  DCHECK(IsTransitionParticipant(object));

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element)
    return style_tracker_->GetRootEffect();
  return style_tracker_->GetEffect(element);
}

void DocumentTransition::VerifySharedElements() {
  if (state_ != State::kIdle)
    style_tracker_->VerifySharedElements();
}

void DocumentTransition::RunPostLayoutSteps() {
  DCHECK(document_->Lifecycle().GetState() >=
         DocumentLifecycle::LifecycleState::kLayoutClean);

  if (style_tracker_) {
    style_tracker_->RunPostLayoutSteps();
    // If we don't have active animations, schedule a frame to end the
    // transition. Note that if we don't have start_promise_resolver_ we don't
    // need to finish the animation, since it should already be done. See the
    // DCHECK below.
    //
    // TODO(vmpstr): Note that RunPostLayoutSteps can happen multiple times
    // during a lifecycle update. These checks don't have to happen here, and
    // could perhaps be moved to DidFinishLifecycleUpdate.
    //
    // We can end up here multiple times, but if we are in a started state and
    // don't have a start promise resolver then the only way we're here is if we
    // disabled end transition.
    DCHECK(state_ != State::kStarted || start_promise_resolver_ ||
           disable_end_transition_);
    if (state_ == State::kStarted && !style_tracker_->HasActiveAnimations() &&
        start_promise_resolver_) {
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

const String& DocumentTransition::UAStyleSheet() const {
  DCHECK(style_tracker_);
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
  deferring_commits_ =
      document_->GetPage()->GetChromeClient().StartDeferringCommits(
          *document_->GetFrame(), kTimeout,
          cc::PaintHoldingReason::kDocumentTransition);
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
  if (capture_promise_resolver_) {
    capture_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, abort_message));
    capture_promise_resolver_ = nullptr;
  }
  if (start_promise_resolver_) {
    start_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, abort_message));
    start_promise_resolver_ = nullptr;
  }

  ResetState();
}

void DocumentTransition::ResetState(bool abort_style_tracker) {
  if (style_tracker_ && abort_style_tracker)
    style_tracker_->Abort();
  style_tracker_ = nullptr;
  StopDeferringCommits();
  state_ = State::kIdle;
  // If script mutations are still allowed, we recreate the style tracker.
  if (script_mutations_allowed_)
    StartNewTransition();
}

}  // namespace blink

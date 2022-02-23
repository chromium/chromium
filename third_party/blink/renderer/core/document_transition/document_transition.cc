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
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_config.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_prepare_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_start_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
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

const char kAbortedFromPrepare[] = "Aborted due to prepare() call";
const char kAbortedFromSignal[] = "Aborted due to abortSignal";

DocumentTransitionRequest::Effect ParseEffect(const String& input) {
  using MapType = HashMap<String, DocumentTransitionRequest::Effect>;
  DEFINE_STATIC_LOCAL(
      MapType*, lookup_map,
      (new MapType{
          {"cover-down", DocumentTransitionRequest::Effect::kCoverDown},
          {"cover-left", DocumentTransitionRequest::Effect::kCoverLeft},
          {"cover-right", DocumentTransitionRequest::Effect::kCoverRight},
          {"cover-up", DocumentTransitionRequest::Effect::kCoverUp},
          {"explode", DocumentTransitionRequest::Effect::kExplode},
          {"fade", DocumentTransitionRequest::Effect::kFade},
          {"implode", DocumentTransitionRequest::Effect::kImplode},
          {"reveal-down", DocumentTransitionRequest::Effect::kRevealDown},
          {"reveal-left", DocumentTransitionRequest::Effect::kRevealLeft},
          {"reveal-right", DocumentTransitionRequest::Effect::kRevealRight},
          {"reveal-up", DocumentTransitionRequest::Effect::kRevealUp}}));

  auto it = lookup_map->find(input);
  return it != lookup_map->end() ? it->value
                                 : DocumentTransitionRequest::Effect::kNone;
}

DocumentTransitionRequest::Effect ParseRootTransition(
    const DocumentTransitionPrepareOptions* options) {
  return options->hasRootTransition()
             ? ParseEffect(options->rootTransition())
             : DocumentTransitionRequest::Effect::kNone;
}

uint32_t NextDocumentTag() {
  static uint32_t next_document_tag = 1u;
  return next_document_tag++;
}

DocumentTransitionRequest::TransitionConfig ParseTransitionConfig(
    const DocumentTransitionConfig& config) {
  DocumentTransitionRequest::TransitionConfig transition_config;

  if (config.hasDuration()) {
    transition_config.duration = base::Milliseconds(config.duration());
  }

  if (config.hasDelay()) {
    transition_config.delay = base::Milliseconds(config.delay());
  }

  return transition_config;
}

}  // namespace

DocumentTransition::DocumentTransition(Document* document)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      document_(document),
      document_tag_(NextDocumentTag()) {}

void DocumentTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(prepare_promise_resolver_);
  visitor->Trace(start_promise_resolver_);
  visitor->Trace(active_shared_elements_);
  visitor->Trace(signal_);
  visitor->Trace(style_tracker_);

  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void DocumentTransition::ContextDestroyed() {
  if (prepare_promise_resolver_) {
    prepare_promise_resolver_->Detach();
    prepare_promise_resolver_ = nullptr;
  }
  if (start_promise_resolver_) {
    start_promise_resolver_->Detach();
    start_promise_resolver_ = nullptr;
  }
  ResetState();
}

bool DocumentTransition::HasPendingActivity() const {
  if (prepare_promise_resolver_ || start_promise_resolver_)
    return true;
  return false;
}

ScriptPromise DocumentTransition::prepare(
    ScriptState* script_state,
    const DocumentTransitionPrepareOptions* options,
    ExceptionState& exception_state) {
  // Reject any previous prepare promises.
  if (state_ == State::kPreparing || state_ == State::kPrepared)
    CancelPendingTransition(kAbortedFromPrepare);

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

  std::string error;
  DocumentTransitionRequest::TransitionConfig root_config;
  if (options->hasRootConfig())
    root_config = ParseTransitionConfig(*options->rootConfig());
  if (!root_config.IsValid(&error)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      String(error.data(), error.size()));
    return ScriptPromise();
  }

  // This stores a per-shared-element configuration, if specified. Note that
  // this is likely to change when the API is redesigned at
  // https://github.com/WICG/shared-element-transitions.
  //
  // Note that we add one extra config for the "root" element, after parsing the
  // shared elements.
  std::vector<DocumentTransitionRequest::TransitionConfig>
      shared_elements_config;
  if (options->hasSharedElements()) {
    shared_elements_config.resize(options->sharedElements().size());

    // TODO(vmpstr): This is likely to be superceded by CSS customization.
    if (options->hasSharedElementsConfig()) {
      const auto& shared_elements_config_options =
          options->sharedElementsConfig();

      if (shared_elements_config_options.size() !=
          shared_elements_config.size()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "The sharedElementsConfig size must "
                                          "match the list of shared elements");
        return ScriptPromise();
      }

      for (wtf_size_t i = 0; i < shared_elements_config_options.size(); i++) {
        shared_elements_config[i] =
            ParseTransitionConfig(*shared_elements_config_options[i]);
        if (!shared_elements_config[i].IsValid(&error)) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kInvalidStateError,
              String(error.data(), error.size()));
          return ScriptPromise();
        }
      }
    }
  }

  // The root snapshot is handled as a shared element by the compositing stack.
  shared_elements_config.emplace_back();

  if (options->hasAbortSignal()) {
    if (options->abortSignal()->aborted()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                        kAbortedFromSignal);
      return ScriptPromise();
    }

    signal_ = options->abortSignal();
    signal_->AddAlgorithm(WTF::Bind(&DocumentTransition::Abort,
                                    WrapWeakPersistent(this),
                                    WrapWeakPersistent(signal_.Get())));
  }

  // We're going to be creating a new transition, parse the options.
  auto effect = ParseRootTransition(options);
  if (options->hasSharedElements())
    SetActiveSharedElements(options->sharedElements());
  prepare_shared_element_count_ = active_shared_elements_.size();

  prepare_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  state_ = State::kPreparing;
  pending_request_ = DocumentTransitionRequest::CreatePrepare(
      effect, document_tag_, root_config, std::move(shared_elements_config),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          &DocumentTransition::NotifyPrepareFinished,
          WrapCrossThreadWeakPersistent(this), last_prepare_sequence_id_)),
      /*is_renderer_transition=*/true);

  style_tracker_ =
      MakeGarbageCollected<DocumentTransitionStyleTracker>(*document_);
  style_tracker_->Prepare(active_shared_elements_);

  NotifyHasChangesToCommit();
  return prepare_promise_resolver_->Promise();
}

void DocumentTransition::Abort(AbortSignal* signal) {
  // There is no RemoveAlgorithm() method on AbortSignal so compare the signal
  // bound to this callback to the one last passed to start().
  if (signal_ != signal)
    return;

  CancelPendingTransition(kAbortedFromSignal);
}

ScriptPromise DocumentTransition::start(
    ScriptState* script_state,
    const DocumentTransitionStartOptions* options,
    ExceptionState& exception_state) {
  if (state_ != State::kPrepared) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Transition must be prepared before it can be started.");
    return ScriptPromise();
  }

  signal_ = nullptr;
  StopDeferringCommits();

  if (options->hasSharedElements())
    SetActiveSharedElements(options->sharedElements());

  // We need to have the same amount of shared elements (even if null) as the
  // prepared ones.
  if (prepare_shared_element_count_ != active_shared_elements_.size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        String::Format("Start request sharedElement count (%u) must match the "
                       "prepare sharedElement count (%u).",
                       active_shared_elements_.size(),
                       prepare_shared_element_count_));

    // TODO(khushalsagar) : Viz keeps copy results cached for 5 seconds at this
    // point. We should send an early release. See crbug.com/1266500.
    ResetState();
    return ScriptPromise();
  }

  last_start_sequence_id_ = next_sequence_id_++;
  state_ = State::kStarted;
  start_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_request_ =
      DocumentTransitionRequest::CreateAnimateRenderer(document_tag_);
  style_tracker_->Start(active_shared_elements_);

  NotifyHasChangesToCommit();
  return start_promise_resolver_->Promise();
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

void DocumentTransition::NotifyPrepareFinished(uint32_t sequence_id) {
  // This notification is for a different sequence id.
  if (sequence_id != last_prepare_sequence_id_)
    return;

  // We could have detached the resolver if the execution context was destroyed.
  if (!prepare_promise_resolver_)
    return;

  DCHECK(state_ == State::kPreparing);
  DCHECK(prepare_promise_resolver_);
  if (style_tracker_)
    style_tracker_->PrepareResolved();

  // Defer commits before resolving the promise to ensure any updates made in
  // the callback are deferred.
  StartDeferringCommits();
  prepare_promise_resolver_->Resolve();
  prepare_promise_resolver_ = nullptr;
  state_ = State::kPrepared;
  SetActiveSharedElements({});
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
  // If our state is idle it implies that we have no style tracker.
  DCHECK(state_ != State::kIdle || !style_tracker_);

  // The layout view is always a participant if there is a transition.
  if (auto* layout_view = DynamicTo<LayoutView>(object))
    return state_ != State::kIdle;

  // Otherwise check if the layout object has an active shared element.
  auto* element = DynamicTo<Element>(object.GetNode());
  return element && active_shared_elements_.Contains(element);
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
    // This matches one past the size of the shared element configs generated in
    // ::prepare().
    state.document_transition_shared_element_id.AddIndex(
        active_shared_elements_.size());
    state.shared_element_resource_id = style_tracker_->GetLiveRootSnapshotId();
    DCHECK(state.document_transition_shared_element_id.valid());
    return style_tracker_->UpdateRootEffect(std::move(state), current_effect);
  }

  for (wtf_size_t i = 0; i < active_shared_elements_.size(); ++i) {
    if (active_shared_elements_[i] != element)
      continue;
    state.document_transition_shared_element_id.AddIndex(i);

    // This tags the shared element's content with the resource id used by the
    // first pseudo element. This is okay since in the eventual API we should
    // have a 1:1 mapping between shared elements and pseudo elements.
    if (!state.shared_element_resource_id.IsValid()) {
      state.shared_element_resource_id =
          style_tracker_->GetLiveSnapshotId(element);
    }
  }

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
  for (auto& active_element : active_shared_elements_) {
    if (!active_element)
      continue;

    auto* object = active_element->GetLayoutObject();

    // TODO(vmpstr): Should this work for replaced elements as well?
    if (object) {
      if (object->ShouldApplyPaintContainment())
        continue;

      auto* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kRendering,
          mojom::ConsoleMessageLevel::kError,
          "Dropping element from transition. Shared element must have "
          "containt:paint");
      console_message->SetNodes(document_->GetFrame(),
                                {DOMNodeIds::IdForNode(active_element)});
      document_->AddConsoleMessage(console_message);
    }

    // Clear the shared element. Note that we don't remove the element from the
    // vector, since we need to preserve the order of the elements and we
    // support nulls as a valid active element.

    // Invalidate the element since we should no longer be compositing it.
    auto* box = active_element->GetLayoutBox();
    if (box && box->HasSelfPaintingLayer()) {
      box->SetNeedsPaintPropertyUpdate();
      box->Layer()->SetNeedsCompositingInputsUpdate();
    }
    active_element = nullptr;
  }
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

void DocumentTransition::SetActiveSharedElements(
    HeapVector<Member<Element>> elements) {
  // The way this is used, we should never be overriding a non-empty set with
  // another non-empty set of elements.
  DCHECK(elements.IsEmpty() || active_shared_elements_.IsEmpty());

  InvalidateActiveElements();
  active_shared_elements_ = std::move(elements);
  InvalidateActiveElements();
}

void DocumentTransition::InvalidateActiveElements() {
  for (auto& element : active_shared_elements_) {
    // We allow nulls.
    if (!element)
      continue;

    auto* box = element->GetLayoutBox();
    if (!box || !box->HasSelfPaintingLayer())
      continue;

    // We propagate the shared element id on an effect node for the object. This
    // means that we should update the paint properties to update the shared
    // element id.
    box->SetNeedsPaintPropertyUpdate();

    // We might need to composite or decomposite this layer.
    box->Layer()->SetNeedsCompositingInputsUpdate();
  }
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
  DCHECK(state_ == State::kPreparing || state_ == State::kPrepared)
      << "Can not cancel transition at state : " << static_cast<int>(state_);

  if (prepare_promise_resolver_) {
    prepare_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, abort_message));
    prepare_promise_resolver_ = nullptr;
  }

  ResetState();
}

void DocumentTransition::ResetState(bool abort_style_tracker) {
  SetActiveSharedElements({});
  if (style_tracker_ && abort_style_tracker)
    style_tracker_->Abort();
  style_tracker_ = nullptr;
  StopDeferringCommits();
  state_ = State::kIdle;
  signal_ = nullptr;
}

}  // namespace blink

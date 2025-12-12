// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition.h"

#include <algorithm>
#include <vector>

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/paint_holding_reason.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sync_iterator_view_transition_type_set.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/layout_view_transition_root.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "v8-microtask-queue.h"

namespace blink {

// static
int ViewTransition::next_id_ = 0;

ViewTransition::ScopedPauseRendering::ScopedPauseRendering(
    const Element& element,
    bool has_document_scope) {
  const Document& document = element.GetDocument();
  if (!document.GetFrame() || !document.GetFrame()->IsLocalRoot()) {
    return;
  }

  if (!has_document_scope) {
    return;
  }

  auto& client = document.GetPage()->GetChromeClient();
  cc_paused_ = client.PauseRendering(*document.GetFrame());
  DCHECK(cc_paused_);
}

ViewTransition::ScopedPauseRendering::~ScopedPauseRendering() = default;

bool ViewTransition::ScopedPauseRendering::ShouldThrottleRendering() const {
  return !cc_paused_;
}

void ViewTransition::UpdateSnapshotContainingBlockStyle() {
  LayoutViewTransitionRoot* transition_root =
      document_->GetLayoutView()->GetViewTransitionRoot();
  CHECK(transition_root);
  transition_root->UpdateSnapshotStyle(*style_tracker_);
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
    case State::kPendingDone:
      return "PendingDone";
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
}

// static
ViewTransition* ViewTransition::CreateFromScript(
    Element* element,
    V8ViewTransitionCallback* callback,
    const std::optional<Vector<String>>& types,
    Delegate* delegate,
    ViewTransition* previously_active) {
  CHECK(element->GetExecutionContext());
  return MakeGarbageCollected<ViewTransition>(
      PassKey(), element, callback, types, delegate, previously_active);
}

ViewTransition* ViewTransition::CreateSkipped(
    Element* element,
    V8ViewTransitionCallback* callback) {
  return MakeGarbageCollected<ViewTransition>(PassKey(), element, callback);
}

ViewTransition::ViewTransition(PassKey,
                               Element* element,
                               V8ViewTransitionCallback* update_dom_callback,
                               const std::optional<Vector<String>>& types,
                               Delegate* delegate,
                               ViewTransition* previously_active)
    : ExecutionContextLifecycleObserver(element->GetExecutionContext()),
      creation_type_(CreationType::kScript),
      document_(element->GetDocument()),
      scope_(element),
      has_document_scope_(element->IsDocumentElement()),
      delegate_(delegate),
      style_tracker_(
          MakeGarbageCollected<ViewTransitionStyleTracker>(*element,
                                                           transition_token_)),
      script_delegate_(MakeGarbageCollected<DOMViewTransition>(
          *element->GetExecutionContext(),
          *this,
          update_dom_callback)) {
  InitTypes(types.value_or(Vector<String>()));
  if (previously_active && previously_active->PendingDomCallback()) {
    previously_active->blocking_ = this;
    blocked_on_ = previously_active;
  } else {
    ProcessCurrentState();
  }
}

ViewTransition::ViewTransition(PassKey,
                               Element* element,
                               V8ViewTransitionCallback* update_dom_callback)
    : ExecutionContextLifecycleObserver(element->GetExecutionContext()),
      creation_type_(CreationType::kScript),
      document_(element->GetDocument()),
      scope_(element),
      has_document_scope_(element->IsDocumentElement()),
      script_delegate_(MakeGarbageCollected<DOMViewTransition>(
          *element->GetExecutionContext(),
          *this,
          update_dom_callback)) {
  SkipTransition();
}

// static
ViewTransition* ViewTransition::CreateForSnapshotForNavigation(
    Document* document,
    const ViewTransitionToken& transition_token,
    ViewTransitionStateCallback callback,
    const Vector<String>& types,
    Delegate* delegate) {
  return MakeGarbageCollected<ViewTransition>(
      PassKey(), document, transition_token, std::move(callback), types,
      delegate);
}

ViewTransition::ViewTransition(PassKey,
                               Document* document,
                               const ViewTransitionToken& transition_token,
                               ViewTransitionStateCallback callback,
                               const Vector<String>& types,
                               Delegate* delegate)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      creation_type_(CreationType::kForSnapshot),
      document_(document),
      scope_(document->documentElement()),
      has_document_scope_(true),
      delegate_(delegate),
      transition_token_(transition_token),
      style_tracker_(
          MakeGarbageCollected<ViewTransitionStyleTracker>(*document_,
                                                           transition_token_)),
      transition_state_callback_(std::move(callback)),
      script_delegate_(MakeGarbageCollected<DOMViewTransition>(
          *document_->GetExecutionContext(),
          *this)) {
  TRACE_EVENT0("blink", "ViewTransition::ViewTransition - CreatedForSnapshot");
  DCHECK(transition_state_callback_);
  InitTypes(types);
  ProcessCurrentState();
}

// static
ViewTransition* ViewTransition::CreateFromSnapshotForNavigation(
    Document* document,
    ViewTransitionState transition_state,
    Delegate* delegate) {
  return MakeGarbageCollected<ViewTransition>(
      PassKey(), document, std::move(transition_state), delegate);
}

ViewTransition::ViewTransition(PassKey,
                               Document* document,
                               ViewTransitionState transition_state,
                               Delegate* delegate)
    : ExecutionContextLifecycleObserver(document->GetExecutionContext()),
      creation_type_(CreationType::kFromSnapshot),
      document_(document),
      scope_(document->documentElement()),
      has_document_scope_(true),
      delegate_(delegate),
      transition_token_(transition_state.transition_token),
      style_tracker_(MakeGarbageCollected<ViewTransitionStyleTracker>(
          *document_,
          std::move(transition_state))),
      script_delegate_(MakeGarbageCollected<DOMViewTransition>(
          *document_->GetExecutionContext(),
          *this)) {
  TRACE_EVENT0("blink",
               "ViewTransition::ViewTransition - CreatingFromSnapshot");
  bool process_next_state = AdvanceTo(State::kWaitForRenderBlock);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

void ViewTransition::SkipTransition(PromiseResponse response) {
  DCHECK_NE(response, PromiseResponse::kResolve);
  pending_skip_view_transitions_ = false;
  if (IsTerminalState(state_))
    return;

  // If we already started processing the transition (i.e. we're beyond capture
  // tag discovery), then send a release directive. We don't do this, if we're
  // capturing this for a snapshot. The only way that transition is skipped is
  // if we finished capturing.
  if (static_cast<int>(state_) >
          static_cast<int>(State::kCaptureTagDiscovery) &&
      creation_type_ != CreationType::kForSnapshot) {
    delegate_->AddPendingRequest(ViewTransitionRequest::CreateRelease(
        transition_token_, MaybeCrossFrameSink(),
        base::FeatureList::IsEnabled(
            features::kDelayLayerTreeViewDeletionOnLocalSwap)));
  }

  // We always need to call the transition state callback (mojo seems to require
  // this contract), so do so if we have one and we haven't called it yet.
  if (transition_state_callback_) {
    CHECK_EQ(creation_type_, CreationType::kForSnapshot);
    ViewTransitionState view_transition_state;
    view_transition_state.transition_token = transition_token_;
    std::move(transition_state_callback_).Run(std::move(view_transition_state));
  }

  // Resume rendering, and finalize the rest of the state.
  ResumeRendering();
  if (style_tracker_) {
    style_tracker_->Abort();
  }

  if (delegate_) {
    delegate_->OnTransitionFinished(this);
  }

  // TODO(khushalsagar): Figure out the promise handling when this is on the
  // old Document for a cross-document navigation.

  // Cleanup logic which is tied to ViewTransition objects created using the
  // script API. script_delegate_ is cleared when the Document is being torn
  // down and script specific callbacks don't need to be dispatched in that
  // case.
  if (script_delegate_) {
    script_delegate_->DidSkipTransition(response);
  }

  // This should be the last call in this function to avoid erroneously checking
  // the `state_` against the wrong state.
  AdvanceTo(State::kAborted);
}

void ViewTransition::SkipTransitionSoon() {
  pending_skip_view_transitions_ = true;
}

bool ViewTransition::AdvanceTo(State state) {
  CHECK(!blocked_on_ || state == State::kAborted)
      << "Blocked on DOM callback for skipped transition. Attempted to advance "
      << "from " << StateToString(state_) << " to " << StateToString(state);
  DCHECK(CanAdvanceTo(state)) << "Current state " << static_cast<int>(state_)
                              << " new state " << static_cast<int>(state);

  if (state == State::kCapturing || state_ == State::kCapturing) {
    DCHECK(style_tracker_);
    style_tracker_->InvalidateBackdropFilterCompositingProperties();
  }

  bool was_initial = state_ == State::kInitial;
  state_ = state;
  if (!was_initial && IsTerminalState(state_)) {
    if (auto* originating_element = document_->documentElement()) {
      originating_element->ActiveViewTransitionStateChanged();
      if (types_ && !types_->IsEmpty()) {
        originating_element->ActiveViewTransitionTypeStateChanged();
      }
    }
  }

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
             state == State::kWaitForRenderBlock || state == State::kAborted;
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
      return state == State::kPendingDone || state == State::kAborted;
    case State::kPendingDone:
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
    case State::kPendingDone:
      return !RuntimeEnabledFeatures::ViewTransitionAsyncFinishedEnabled();
    case State::kFinished:
    case State::kAborted:
    case State::kTimedOut:
    case State::kTransitionStateCallbackDispatched:
      return false;
  }
  NOTREACHED();
}

// static
bool ViewTransition::WaitsForNotification(State state) {
  return state == State::kCapturing || state == State::kDOMCallbackRunning ||
         state == State::kWaitForRenderBlock ||
         state == State::kTransitionStateCallbackDispatched ||
         (RuntimeEnabledFeatures::ViewTransitionAsyncFinishedEnabled() &&
          state == State::kPendingDone);
}

// static
bool ViewTransition::IsTerminalState(State state) {
  return state == State::kFinished || state == State::kAborted ||
         state == State::kTimedOut;
}

void ViewTransition::ProcessCurrentState() {
  CHECK(!blocked_on_ || state_ == State::kAborted)
      << "ProcessingCurrentState blocked on DOM callback for skipped "
      << "transition while state is " << StateToString(state_);
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
        // We require a new effect node to be generated for the LayoutView when
        // a transition is not in terminal state. Dirty paint to ensure
        // generation of this effect node.
        if (auto* layout_view = document_->GetLayoutView()) {
          layout_view->SetNeedsPaintPropertyUpdate();
        }

        process_next_state = AdvanceTo(State::kCaptureTagDiscovery);
        DCHECK(!process_next_state);
        break;

      // Update the lifecycle if needed and discover the elements (deferred to
      // AddTransitionElementsFromCSS).
      case State::kCaptureTagDiscovery:
        DCHECK(in_main_lifecycle_update_);
        DCHECK_GE(document_->Lifecycle().GetState(),
                  DocumentLifecycle::kCompositingInputsClean);

        if (UnsupportedCapture()) {
          SkipTransition(PromiseResponse::kRejectInvalidState);
          break;
        }

        style_tracker_->AddTransitionElementsFromCSS();
        process_next_state = AdvanceTo(State::kCaptureRequestPending);
        DCHECK(process_next_state);
        break;

      // Capture request pending -- create the request
      case State::kCaptureRequestPending: {
        // If we're capturing during a navigation, browser controls will be
        // forced to show via animation. Ensure they're fully showing when
        // performing the capture.
        bool snap_browser_controls =
            document_->GetFrame()->IsOutermostMainFrame() &&
            document_->GetPage()->GetBrowserControls().PermittedState() !=
                cc::BrowserControlsState::kHidden &&
            creation_type_ == CreationType::kForSnapshot;
        if (!style_tracker_->Capture(snap_browser_controls)) {
          SkipTransition(PromiseResponse::kRejectInvalidState);
          break;
        }

        delegate_->AddPendingRequest(ViewTransitionRequest::CreateCapture(
            transition_token_, MaybeCrossFrameSink(),
            style_tracker_->TakeCaptureResourceIds(),
            ConvertToBaseOnceCallback(
                CrossThreadBindOnce(&ViewTransition::NotifyCaptureFinished,
                                    MakeUnwrappingCrossThreadWeakHandle(this))),
            base::FeatureList::IsEnabled(
                features::kDelayLayerTreeViewDeletionOnLocalSwap)));

        if (document_->GetFrame()->IsLocalRoot()) {
          // We need to ensure commits aren't deferred since we rely on commits
          // to send directives to the compositor and initiate pause of
          // rendering after one frame.
          document_->GetPage()->GetChromeClient().StopDeferringCommits(
              *document_->GetFrame(),
              cc::PaintHoldingCommitTrigger::kViewTransition);
        }
        document_->GetPage()->GetChromeClient().RegisterForCommitObservation(
            this);

        process_next_state = AdvanceTo(State::kCapturing);
        DCHECK(!process_next_state);
        break;
      }
      case State::kCapturing:
        DCHECK(WaitsForNotification(state_));
        break;

      case State::kCaptured: {
        style_tracker_->CaptureResolved();
        if (creation_type_ == CreationType::kForSnapshot) {
          DCHECK(transition_state_callback_);
          ViewTransitionState view_transition_state =
              style_tracker_->GetViewTransitionState();
          CHECK_EQ(view_transition_state.transition_token, transition_token_);

          process_next_state =
              AdvanceTo(State::kTransitionStateCallbackDispatched);
          DCHECK(process_next_state);

          std::move(transition_state_callback_)
              .Run(std::move(view_transition_state));
          break;
        }

        // The following logic is only executed for ViewTransition objects
        // created by the script API.
        CHECK_EQ(creation_type_, CreationType::kScript);
        CHECK(script_delegate_);
        script_delegate_->InvokeDOMChangeCallback();

        // Since invoking the callback could yield (at least when devtools
        // breakpoint is hit, but maybe in other situations), we could have
        // timed out already. Make sure we don't advance the state out of a
        // terminal state.
        if (IsTerminalState(state_)) {
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
        // For testing check: if the flag is enabled, re-create the style
        // tracker with the serialized state that the current style tracker
        // produces. This allows us to use SPA tests for MPA serialization.
        if (RuntimeEnabledFeatures::
                SerializeViewTransitionStateInSPAEnabled()) {
          style_tracker_ = MakeGarbageCollected<ViewTransitionStyleTracker>(
              *document_, style_tracker_->GetViewTransitionState());
        }

        ResumeRendering();

        // Animation and subsequent steps require us to have a view. If after
        // running the callbacks, we don't have a view, skip the transition.
        if (!document_->View()) {
          SkipTransition();
          break;
        }

        process_next_state = AdvanceTo(State::kAnimateTagDiscovery);
        DCHECK(process_next_state);
        break;

      case State::kAnimateTagDiscovery:
        DCHECK(!in_main_lifecycle_update_);
        document_->View()->UpdateAllLifecyclePhasesExceptPaint(
            DocumentUpdateReason::kViewTransition);
        DCHECK_GE(document_->Lifecycle().GetState(),
                  DocumentLifecycle::kPrePaintClean);

        // Note: this happens after updating the lifecycle since the snapshot
        // root can depend on layout when using a mobile viewport (i.e.
        // horizontally overflowing element expanding the size of the frame
        // view). See also: https://crbug.com/1454207.
        if (style_tracker_->SnapshotRootDidChangeSize()) {
          SkipTransition(PromiseResponse::kRejectInvalidState);
          break;
        }

        style_tracker_->AddTransitionElementsFromCSS();
        process_next_state = AdvanceTo(State::kAnimateRequestPending);
        DCHECK(process_next_state);
        break;

      case State::kAnimateRequestPending:
        if (UnsupportedCapture() || !style_tracker_->Start()) {
          SkipTransition(PromiseResponse::kRejectInvalidState);
          break;
        }

        if (RuntimeEnabledFeatures::
                ViewTransitionUpdateLifecycleBeforeReadyEnabled()) {
          document_->View()->UpdateAllLifecyclePhasesExceptPaint(
              DocumentUpdateReason::kViewTransition);
          style_tracker_->RunPostPrePaintSteps();
        }

        delegate_->AddPendingRequest(
            ViewTransitionRequest::CreateAnimateRenderer(
                transition_token_, MaybeCrossFrameSink(),
                base::FeatureList::IsEnabled(
                    features::kDelayLayerTreeViewDeletionOnLocalSwap)));
        process_next_state = AdvanceTo(State::kAnimating);
        DCHECK(!process_next_state);

        DCHECK(!in_main_lifecycle_update_);
        CHECK_NE(creation_type_, CreationType::kForSnapshot);
        CHECK(script_delegate_);
        script_delegate_->DidStartAnimating();
        break;

      case State::kAnimating: {
        if (first_animating_frame_) {
          first_animating_frame_ = false;
          // We need to schedule an animation frame, in case this is the only
          // kAnimating frame we will get, so that we can clean up in the next
          // frame.
          document_->View()->ScheduleAnimation();
          break;
        }

        if (style_tracker_->HasActiveAnimations() ||
            wait_until_pending_promise_count_ > 0) {
          break;
        }

        // Post a task to run the next state (cleanup) outside of the current
        // lifecycle update.
        document_->GetTaskRunner(TaskType::kMiscPlatformAPI)
            ->PostTask(FROM_HERE, BindOnce(&ViewTransition::ProcessCurrentState,
                                           WrapWeakPersistent(this)));

        // Advance to the pending state. This will stop processing for the
        // current lifecycle update since WaitsForNotification(kPendingDone)
        // is true.
        process_next_state = AdvanceTo(State::kPendingDone);
        DCHECK(RuntimeEnabledFeatures::ViewTransitionAsyncFinishedEnabled() ==
               !process_next_state);
        break;
      }
      case State::kPendingDone:
        DCHECK(!RuntimeEnabledFeatures::ViewTransitionAsyncFinishedEnabled() ||
               !in_main_lifecycle_update_);
        style_tracker_->StartFinished();

        delegate_->AddPendingRequest(ViewTransitionRequest::CreateRelease(
            transition_token_, MaybeCrossFrameSink(),
            base::FeatureList::IsEnabled(
                features::kDelayLayerTreeViewDeletionOnLocalSwap)));
        delegate_->OnTransitionFinished(this);
        LogIfDocumentElementChanged();

        style_tracker_ = nullptr;

        CHECK_NE(creation_type_, CreationType::kForSnapshot);
        CHECK(script_delegate_);
        script_delegate_->DidFinishAnimating();

        process_next_state = AdvanceTo(State::kFinished);
        DCHECK(IsTerminalState(state_));
        break;
      case State::kFinished:
      case State::kAborted:
      case State::kTimedOut:
      case State::kTransitionStateCallbackDispatched:
        break;
    }
  }
}

void ViewTransition::LogIfDocumentElementChanged() const {
  if (!has_document_scope_ || !IsCreatedViaScriptAPI()) {
    return;
  }
  if (scope_ && scope_->IsDocumentElement()) {
    return;
  }
  UseCounter::Count(*document_, WebFeature::kViewTransitionChangeRootElement);
}

ViewTransitionTypeSet* ViewTransition::Types() {
  CHECK(types_);
  return types_;
}

void ViewTransition::InitTypes(const Vector<String>& types) {
  types_ = MakeGarbageCollected<ViewTransitionTypeSet>(this, types);
  // Although ViewTransitionTypeSet can invalidate its own style, there are
  // checks that it is in a current view transition. Because InitTypes is
  // called during ctor, the supplement does not yet know that this will become
  // a current view transition, so we need to invalidate explicitly.
  if (auto* originating_element = Scope()) {
    originating_element->ActiveViewTransitionStateChanged();
    if (!types_->IsEmpty()) {
      originating_element->ActiveViewTransitionTypeStateChanged();
    }
  }
}

void ViewTransition::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(scope_);
  visitor->Trace(style_tracker_);
  visitor->Trace(script_delegate_);
  visitor->Trace(types_);
  visitor->Trace(blocked_on_);
  visitor->Trace(blocking_);

  ExecutionContextLifecycleObserver::Trace(visitor);
}

bool ViewTransition::MatchForOnlyChild(
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) const {
  if (!style_tracker_)
    return false;
  return style_tracker_->MatchForOnlyChild(pseudo_id, view_transition_name);
}

bool ViewTransition::MatchForActiveViewTransition() {
  return !IsTerminalState(state_);
}

bool ViewTransition::MatchForActiveViewTransitionType(
    const Vector<AtomicString>& pseudo_types) {
  if (IsTerminalState(state_)) {
    return false;
  }

  CHECK(!pseudo_types.empty());

  // If types are not specified, then there is no match.
  if (!types_ || types_->IsEmpty()) {
    return false;
  }

  // At least one pseudo type has to match at least one of the transition types.
  return std::ranges::any_of(pseudo_types, [&](const String& pseudo_type) {
    return ViewTransitionTypeSet::IsValidType(pseudo_type) &&
           types_->Contains(pseudo_type);
  });
}

void ViewTransition::ContextDestroyed() {
  TRACE_EVENT0("blink", "ViewTransition::ContextDestroyed");

  // Don't try to interact with script after the Document starts shutdown.
  script_delegate_.Clear();

  // TODO(khushalsagar): This needs to be called for pages entering BFCache.
  SkipTransition(PromiseResponse::kRejectAbort);
}

void ViewTransition::NotifyCaptureFinished(
    const std::unordered_map<viz::ViewTransitionElementResourceId, gfx::RectF>&
        capture_rects) {
  if (state_ == State::kCapturing) {
    style_tracker_->SetCaptureRectsFromCompositor(capture_rects);
  } else {
    DCHECK(IsTerminalState(state_));
  }

  // Inform the delegate that the transition has been captured. Once all
  // flight transitions have been captured, processing the next step will resume
  // in creation order ensure deterministic behavior with DOM callbacks. The
  // onus is on the developer in the case of asynchronous callbacks.
  delegate_->OnTransitionCaptured(this);
}

void ViewTransition::OnCapturePhaseComplete() {
  if (state_ != State::kCapturing) {
    DCHECK(IsTerminalState(state_));
    return;
  }
  bool process_next_state = AdvanceTo(State::kCaptured);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

void ViewTransition::NotifyDOMCallbackFinished(bool success) {
  if (IsTerminalState(state_))
    return;

  CHECK_EQ(state_, State::kDOMCallbackRunning);

  bool process_next_state = AdvanceTo(State::kDOMCallbackFinished);
  DCHECK(process_next_state);
  if (!success) {
    SkipTransition(PromiseResponse::kRejectAbort);
  }
  ProcessCurrentState();

  // Succeed or fail, rendering must be resumed after this.
  CHECK(!rendering_paused_scope_);
}

bool ViewTransition::NeedsViewTransitionEffectNode(
    const LayoutObject& object) const {
  // The scope always needs an effect node, even if the scope element is not a
  // participant in the transition. The reason for this is so that we can place
  // the effect node for the ::view-transition pseudo-element as a sibling of
  // the scope's effect. For a document transition, the scope's effect node is
  // associated with the LayoutView rather than the document element.
  if (IsA<LayoutView>(object)) {
    return has_document_scope_ && !IsTerminalState(state_);
  }
  if (!has_document_scope_ && object == scope_->GetLayoutObject()) {
    return !IsTerminalState(state_);
  }

  // Otherwise check if the layout object has a transition element.
  auto* element = DynamicTo<Element>(object.GetNode());
  return element && IsTransitionElementExcludingRoot(*element);
}

bool ViewTransition::NeedsViewTransitionClipNode(
    const LayoutObject& object) const {
  // The root element's painting is already clipped to the snapshot root using
  // LayoutView::ViewRect.
  if (IsA<LayoutView>(object)) {
    return false;
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  return element && style_tracker_ &&
         style_tracker_->NeedsCaptureClipNode(*element);
}

bool ViewTransition::IsRepresentedViaPseudoElements(
    const LayoutObject& object) const {
  if (IsTerminalState(state_)) {
    return false;
  }

  if (IsA<LayoutView>(object)) {
    return document_->documentElement() &&
           style_tracker_->IsTransitionElement(*document_->documentElement());
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  return element && IsTransitionElementExcludingRoot(*element);
}

bool ViewTransition::IsTransitionElementExcludingRoot(
    const Element& node) const {
  if (IsTerminalState(state_)) {
    return false;
  }

  return !node.IsDocumentElement() && style_tracker_->IsTransitionElement(node);
}

viz::ViewTransitionElementResourceId ViewTransition::GetSnapshotId(
    const LayoutObject& object) const {
  DCHECK(NeedsViewTransitionEffectNode(object));

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    // The only non-element participant is the layout view.
    DCHECK(object.IsLayoutView());
    element = document_->documentElement();
  }

  return style_tracker_->GetSnapshotId(*element);
}

const scoped_refptr<cc::ViewTransitionContentLayer>&
ViewTransition::GetScopeSnapshotLayer() const {
  return style_tracker_->GetScopeSnapshotLayer();
}

PaintPropertyChangeType ViewTransition::UpdateCaptureClip(
    const LayoutObject& object,
    const ClipPaintPropertyNodeOrAlias* current_clip,
    const TransformPaintPropertyNodeOrAlias* current_transform) {
  DCHECK(NeedsViewTransitionClipNode(object));
  DCHECK(current_transform);

  auto* element = DynamicTo<Element>(object.GetNode());
  DCHECK(element);
  return style_tracker_->UpdateCaptureClip(*element, current_clip,
                                           current_transform);
}

const ClipPaintPropertyNode* ViewTransition::GetCaptureClip(
    const LayoutObject& object) const {
  DCHECK(NeedsViewTransitionClipNode(object));

  return style_tracker_->GetCaptureClip(*To<Element>(object.GetNode()));
}

void ViewTransition::RunViewTransitionStepsOutsideMainFrame() {
  DCHECK(document_->Lifecycle().GetState() >=
         DocumentLifecycle::kPrePaintClean);
  DCHECK(!in_main_lifecycle_update_);

  if (blocked_on_) {
    // Waiting on a skipped transition to start running its DOM callback.
    return;
  }

  if (pending_skip_view_transitions_ ||
      (state_ == State::kAnimating && style_tracker_ &&
       !style_tracker_->RunPostPrePaintSteps())) {
    SkipTransition(PromiseResponse::kRejectInvalidState);
  }
}

void ViewTransition::RunViewTransitionStepsDuringMainFrame() {
  DCHECK_NE(state_, State::kWaitForRenderBlock);

  DCHECK_GE(document_->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  DCHECK(!in_main_lifecycle_update_);

  if (blocked_on_) {
    // Waiting on a skipped transition to start running its DOM callback.
    return;
  }

  base::AutoReset<bool> scope(&in_main_lifecycle_update_, true);
  if (StateRunsInViewTransitionStepsDuringMainFrame(state_))
    ProcessCurrentState();

  if (pending_skip_view_transitions_ ||
      (style_tracker_ &&
       document_->Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean &&
       !style_tracker_->RunPostPrePaintSteps())) {
    SkipTransition(PromiseResponse::kRejectInvalidState);
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

CSSStyleSheet* ViewTransition::UAStyleSheet() const {
  // TODO(vmpstr): We can still request getComputedStyle(html,
  // "::view-transition-pseudo") outside of a page transition. What should we
  // return in that case?
  if (!style_tracker_)
    return nullptr;
  return &style_tracker_->UAStyleSheet();
}

void ViewTransition::WillCommitCompositorFrame() {
  // There should only be 1 commit when we're in the capturing phase and
  // rendering is paused immediately after it finishes.
  if (state_ == State::kCapturing)
    PauseRendering();
}

gfx::Size ViewTransition::GetSnapshotRootSize() const {
  if (!style_tracker_)
    return gfx::Size();

  return style_tracker_->GetSnapshotRootSize();
}

gfx::Vector2d ViewTransition::GetFrameToSnapshotRootOffset() const {
  if (!style_tracker_)
    return gfx::Vector2d();

  return style_tracker_->GetFrameToSnapshotRootOffset();
}

bool ViewTransition::HasActiveAnimations() const {
  return (state_ == State::kAnimating) && style_tracker_ &&
         style_tracker_->HasActiveAnimations();
}

void ViewTransition::PauseRendering() {
  DCHECK(!rendering_paused_scope_);

  if (!document_->GetPage() || !document_->View())
    return;

  rendering_paused_scope_.emplace(*scope_, has_document_scope_);
  document_->GetPage()->GetChromeClient().UnregisterFromCommitObservation(this);

  if (has_document_scope_ &&
      rendering_paused_scope_->ShouldThrottleRendering() && document_->View()) {
    document_->View()->SetThrottledForViewTransition(true);
  }
  style_tracker_->PauseRendering();

  TRACE_EVENT_BEGIN("blink", "ViewTransition::PauseRendering",
                    perfetto::Track::FromPointer(this));
  static const base::TimeDelta timeout_delay =
      RuntimeEnabledFeatures::
              ViewTransitionLongCallbackTimeoutForTestingEnabled()
          ? base::Seconds(15)
          : base::Seconds(4);
  document_->GetTaskRunner(TaskType::kInternalFrameLifecycleControl)
      ->PostDelayedTask(FROM_HERE,
                        BindOnce(&ViewTransition::OnRenderingPausedTimeout,
                                 WrapWeakPersistent(this)),
                        timeout_delay);
}

void ViewTransition::OnRenderingPausedTimeout() {
  if (!rendering_paused_scope_)
    return;

  ResumeRendering();
  SkipTransition(PromiseResponse::kRejectTimeout);
  AdvanceTo(State::kTimedOut);
}

bool ViewTransition::UnsupportedCapture() {
  if (scope_ && scope_ != scope_->GetDocument().documentElement() &&
      scope_->GetComputedStyle()) {
    // TODO(crbug.com/429763389): image masks are not currently supported on the
    // scoped element. This restriction may be resolved by making the
    // view-transition's layout object a sibling of the scoped element's
    // layout object.For now, skip the transition.
    const ComputedStyle* style = scope_->GetComputedStyle();
    if (style->HasMask()) {
      LogMessageToConsole(
          "Scoped view-transitions do not currently support mask-image.");
      return true;
    }
    // TODO(crbug.com/434891109): Various inline display types are not supported
    // for scoped view transitions. The display type inline-block is an
    // exception since having block characteristics in addition to inline.
    // Depending on spec resolution, we may need to revisit the handling of
    // inline elements.
    if (style->IsDisplayInlineType() && !style->IsDisplayBlockContainer()) {
      LogMessageToConsole(
          "Scoped view-transitions do not currently support inline display "
          "types.");
      return true;
    }

    // TODO(crbug.com/436804019): These elements do not create a layout box.
    if (style->InlinifiesChildren()) {
      LogMessageToConsole(
          "Scoped view-transitions do not currently support elements that "
          "inline their children.");
    }
  }

  return false;
}

void ViewTransition::LogMessageToConsole(const String& message) {
  if (!scope_) {
    return;
  }

  LocalFrame* frame = scope_->GetDocument().GetFrame();
  if (!frame) {
    return;
  }

  frame->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
      ConsoleMessage::Source::kRendering, ConsoleMessage::Level::kWarning,
      message));
}

void ViewTransition::ResumeRendering() {
  if (!rendering_paused_scope_)
    return;

  TRACE_EVENT_END("blink", perfetto::Track::FromPointer(this));
  if (rendering_paused_scope_->ShouldThrottleRendering() && document_->View()) {
    document_->View()->SetThrottledForViewTransition(false);
  }
  rendering_paused_scope_.reset();
}

void ViewTransition::ActivateFromSnapshot() {
  CHECK(IsForNavigationOnNewDocument());
  TRACE_EVENT0("blink", "ViewTransition::ActivateFromSnapshot");

  if (state_ != State::kWaitForRenderBlock)
    return;

  LocalDOMWindow* window = document_->domWindow();
  CHECK(window);

  // This ensures the ViewTransition promises are resolved before the next
  // rendering steps (rAF, style/layout etc) as in the cross-document case
  // activating the view-transition is not called from inside a script. See
  // https://github.com/whatwg/html/pull/10284
  v8::MicrotasksScope microtasks_scope(
      window->GetIsolate(), ToMicrotaskQueue(window),
      v8::MicrotasksScope::Type::kRunMicrotasks);

  // This function implies that rendering has started. If we were waiting
  // for render-blocking resources to be loaded, they must have been fetched (or
  // timed out) before rendering is started.
  DCHECK(document_->RenderingHasBegun());
  bool process_next_state = AdvanceTo(State::kAnimateTagDiscovery);
  DCHECK(process_next_state);
  ProcessCurrentState();
}

bool ViewTransition::MaybeCrossFrameSink() const {
  // Same-document transitions always stay within the Document's widget which
  // also means the same FrameSink.
  if (IsCreatedViaScriptAPI()) {
    return false;
  }

  // We don't support LocalFrame<->RemoteFrame transitions. So if the current
  // Document is a subframe and a LocalFrame, the new Document must also be a
  // LocalFrame. This means this transition must be within the same FrameSink.
  //
  // Note: The limitation above is enforced in
  // content::ViewTransitionCommitDeferringCondition, the browser process
  // doesn't issue a snapshot request for such navigations.
  return document_->GetFrame()->IsLocalRoot();
}

bool ViewTransition::IsGeneratingPseudo(
    const ViewTransitionPseudoElementBase& pseudo_element) const {
  return pseudo_element.IsBoundTo(style_tracker_.Get());
}

void ViewTransition::NotifySkippedTransitionDOMCallbackScheduled() {
  pending_dom_callback_ = true;
  if (delegate_) {
    delegate_->OnSkipTransitionWithPendingCallback(this);
  }
}

void ViewTransition::NotifyInvokeDOMChangeCallback() {
  pending_dom_callback_ = false;
  if (delegate_) {
    delegate_->OnSkippedTransitionDOMCallback(this);
  }
  if (blocking_) {
    blocking_->blocked_on_ = nullptr;
    blocking_->ProcessCurrentState();
    blocking_ = nullptr;
  }
}

bool ViewTransition::PendingDomCallback() {
  return pending_dom_callback_;
}

void ViewTransition::WillEnterGetComputedStyleScope() {
  if (style_tracker_) {
    style_tracker_->WillEnterGetComputedStyleScope();
  }
}
void ViewTransition::WillExitGetComputedStyleScope() {
  if (style_tracker_) {
    style_tracker_->WillExitGetComputedStyleScope();
  }
}

void ViewTransition::InvalidateInternalPseudoStyle() {
  if (style_tracker_) {
    style_tracker_->InvalidateInternalPseudoStyle();
  }
}

void ViewTransition::IncrementWaitUntilPromises() {
  ++wait_until_pending_promise_count_;
}

void ViewTransition::DecrementWaitUntilPromises() {
  CHECK_GT(wait_until_pending_promise_count_, 0);
  // If we reach 0, then schedule an animation so that we process the animation.
  if (--wait_until_pending_promise_count_ == 0) {
    document_->View()->ScheduleAnimation();
  }
}

}  // namespace blink

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include <string>

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/content_visibility_auto_state_change_event.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {
namespace rejection_names {
const char* kContainmentNotSatisfied =
    "Containment requirement is not satisfied.";
const char* kUnsupportedDisplay =
    "Element has unsupported display type (display: contents).";
}  // namespace rejection_names

ScrollableArea* GetScrollableArea(Node* node) {
  if (!node)
    return nullptr;

  LayoutBoxModelObject* object =
      DynamicTo<LayoutBoxModelObject>(node->GetLayoutObject());
  if (!object)
    return nullptr;

  return object->GetScrollableArea();
}

}  // namespace

DisplayLockContext::DisplayLockContext(Element* element)
    : element_(element), document_(&element_->GetDocument()) {
  document_->GetDisplayLockDocumentState().AddDisplayLockContext(this);
  DetermineIfSubtreeHasFocus();
  DetermineIfSubtreeHasSelection();
  DetermineIfSubtreeHasTopLayerElement();
  DetermineIfDescendantIsViewTransitionElement();
}

void DisplayLockContext::SetRequestedState(EContentVisibility state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  base::AutoReset<bool> scope(&set_requested_state_scope_, true);
  switch (state_) {
    case EContentVisibility::kVisible:
      RequestUnlock();
      break;
    case EContentVisibility::kAuto:
      UseCounter::Count(document_, WebFeature::kContentVisibilityAuto);
      had_any_viewport_intersection_notifications_ = false;
      RequestLock(static_cast<uint16_t>(DisplayLockActivationReason::kAny));
      break;
    case EContentVisibility::kHidden:
      UseCounter::Count(document_, WebFeature::kContentVisibilityHidden);
      RequestLock(
          is_hidden_until_found_ || is_details_slot_
              ? static_cast<uint16_t>(DisplayLockActivationReason::kFindInPage)
              : 0u);
      break;
  }
  // In a new state, we might need to either start or stop observing viewport
  // intersections.
  UpdateActivationObservationIfNeeded();

  // If we needed a deferred not intersecting signal from 'auto' mode, we can
  // set that to false, since the mode has switched to something else. If we're
  // switching _to_ 'auto' mode, this should already be false and will be a
  // no-op.
  DCHECK(state_ != EContentVisibility::kAuto ||
         !needs_deferred_not_intersecting_signal_);
  needs_deferred_not_intersecting_signal_ = false;
  UpdateLifecycleNotificationRegistration();

  // Note that we call this here since the |state_| change is a render affecting
  // state, but is tracked independently.
  NotifyRenderAffectingStateChanged();

  // Since our state changed, check if we need to create a scoped force update
  // object.
  element_->GetDocument().GetDisplayLockDocumentState().ForceLockIfNeeded(
      element_.Get());
}

const ComputedStyle* DisplayLockContext::AdjustElementStyle(
    const ComputedStyle* style) const {
  if (state_ == EContentVisibility::kVisible) {
    return style;
  }
  if (IsLocked()) {
    ComputedStyleBuilder builder(*style);
    builder.SetSkipsContents(true);
    return builder.TakeStyle();
  }
  return style;
}

void DisplayLockContext::RequestLock(uint16_t activation_mask) {
  UpdateActivationMask(activation_mask);
  SetRenderAffectingState(RenderAffectingState::kLockRequested, true);
}

void DisplayLockContext::RequestUnlock() {
  SetRenderAffectingState(RenderAffectingState::kLockRequested, false);
}

void DisplayLockContext::UpdateActivationMask(uint16_t activatable_mask) {
  if (activatable_mask == activatable_mask_)
    return;

  bool all_activation_was_blocked = !activatable_mask_;
  bool all_activation_is_blocked = !activatable_mask;
  UpdateDocumentBookkeeping(IsLocked(), all_activation_was_blocked, IsLocked(),
                            all_activation_is_blocked);

  activatable_mask_ = activatable_mask;
}

void DisplayLockContext::UpdateDocumentBookkeeping(
    bool was_locked,
    bool all_activation_was_blocked,
    bool is_locked,
    bool all_activation_is_blocked) {
  if (!document_)
    return;

  if (was_locked != is_locked) {
    if (is_locked)
      document_->GetDisplayLockDocumentState().AddLockedDisplayLock();
    else
      document_->GetDisplayLockDocumentState().RemoveLockedDisplayLock();
  }

  bool was_locked_and_blocking = was_locked && all_activation_was_blocked;
  bool is_locked_and_blocking = is_locked && all_activation_is_blocked;
  if (was_locked_and_blocking != is_locked_and_blocking) {
    if (is_locked_and_blocking) {
      document_->GetDisplayLockDocumentState()
          .IncrementDisplayLockBlockingAllActivation();
    } else {
      document_->GetDisplayLockDocumentState()
          .DecrementDisplayLockBlockingAllActivation();
    }
  }
}

void DisplayLockContext::UpdateActivationObservationIfNeeded() {
  // If we don't have a document, then we don't have an observer so just make
  // sure we're marked as not observing anything and early out.
  if (!document_) {
    is_observed_ = false;
    return;
  }

  // We require observation if we are in 'auto' mode and we're connected to a
  // view.
  bool should_observe =
      state_ == EContentVisibility::kAuto && ConnectedToView();
  if (is_observed_ == should_observe)
    return;
  is_observed_ = should_observe;

  // Reset viewport intersection notification state, so that if we're observing
  // again, the next observation will be synchronous.
  had_any_viewport_intersection_notifications_ = false;

  if (should_observe) {
    document_->GetDisplayLockDocumentState()
        .RegisterDisplayLockActivationObservation(element_);
  } else {
    document_->GetDisplayLockDocumentState()
        .UnregisterDisplayLockActivationObservation(element_);
    // If we're not listening to viewport intersections, then we can assume
    // we're not intersecting:
    // 1. We might not be connected, in which case we're not intersecting.
    // 2. We might not be in 'auto' mode. which means that this doesn't affect
    //    anything consequential but acts as a reset should we switch back to
    //    the 'auto' mode.
    SetRenderAffectingState(RenderAffectingState::kIntersectsViewport, false);
  }
}

bool DisplayLockContext::NeedsLifecycleNotifications() const {
  return needs_deferred_not_intersecting_signal_ ||
         render_affecting_state_[static_cast<int>(
             RenderAffectingState::kAutoStateUnlockedUntilLifecycle)] ||
         has_pending_subtree_checks_ || has_pending_clear_has_top_layer_ ||
         has_pending_top_layer_check_ ||
         anchor_positioning_render_state_may_have_changed_;
}

void DisplayLockContext::UpdateLifecycleNotificationRegistration() {
  if (!document_ || !document_->View()) {
    is_registered_for_lifecycle_notifications_ = false;
    return;
  }

  bool needs_notifications = NeedsLifecycleNotifications();
  if (needs_notifications == is_registered_for_lifecycle_notifications_)
    return;

  is_registered_for_lifecycle_notifications_ = needs_notifications;
  if (needs_notifications) {
    document_->View()->RegisterForLifecycleNotifications(this);
  } else {
    document_->View()->UnregisterFromLifecycleNotifications(this);
  }
}

void DisplayLockContext::Lock() {
  DCHECK(!IsLocked());
  is_locked_ = true;
  UpdateDocumentBookkeeping(false, !activatable_mask_, true,
                            !activatable_mask_);

  // If we're not connected, then we don't have to do anything else. Otherwise,
  // we need to ensure that we update our style to check for containment later,
  // layout size based on the options, and also clear the painted output.
  if (!ConnectedToView()) {
    return;
  }

  // If there are any pending updates, we cancel them, as the fast updates
  // can't detect a locked display.
  // See: ../paint/README.md#Transform-update-optimization for more information
  document_->View()->RemoveAllPendingUpdates();

  // There are two ways we can get locked:
  // 1. A new content-visibility property needs us to be locked.
  // 2. We're in 'auto' mode and we are not intersecting the viewport.
  // In the first case, we are already in style processing, so we don't need to
  // invalidate style. However, in the second case we invalidate style so that
  // `AdjustElementStyle()` can be called.
  if (CanDirtyStyle()) {
    element_->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kDisplayLock));

    MarkForStyleRecalcIfNeeded();
  }

  // TODO(vmpstr): Note when an 'auto' context gets locked, we should clear
  // the ancestor scroll anchors. This is a workaround for a behavior that
  // happens when the user quickly scrolls (e.g. scrollbar scrolls) into an
  // area that only has locked content. We can get into a loop that will
  // keep unlocking an element, which may shrink it to be out of the viewport,
  // and thus relocking it again. It is is also possible that we selected the
  // scroller itself or one of the locked elements as the anchor, so we don't
  // actually shift the scroll and the loop continues indefinitely. The user
  // can easily get out of the loop by scrolling since that triggers a new
  // scroll anchor selection. The work-around for us is also to pick a new
  // scroll anchor for the scroller that has a newly-locked context. The
  // reason it works is that it causes us to pick an anchor while the element
  // is still unlocked, so when it gets relocked we shift the scroll to
  // whatever visible content we had. The TODO here is to figure out if there
  // is a better way to solve this. In either case, we have to select a new
  // scroll anchor to get out of this behavior.
  element_->NotifyPriorityScrollAnchorStatusChanged();

  // We need to notify the AX cache (if it exists) to update |element_|'s
  // children in the AX cache.
  if (AXObjectCache* cache = element_->GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(element_);

  // If we have top layer elements in our subtree, we have to detach their
  // layout objects, since otherwise they would be hoisted out of our subtree.
  DetachDescendantTopLayerElements();

  // Schedule ContentVisibilityAutoStateChange event if needed.
  ScheduleStateChangeEventIfNeeded();

  if (!element_->GetLayoutObject())
    return;

  // If this element is a scroller, then stash its current scroll offset, so
  // that we can restore it when needed.
  // Note that this only applies if the element itself is a scroller. Any
  // subtree scrollers' scroll offsets are not affected.
  StashScrollOffsetIfAvailable();

  MarkNeedsRepaintAndPaintArtifactCompositorUpdate();
}

// Did* function for the lifecycle phases. These functions, along with
// Should* functions in the header, control whether or not to process the
// lifecycle for self or for children.
// =============================================================================
void DisplayLockContext::DidStyleSelf() {
  // If we don't have a style after styling self, it means that we should revert
  // to the default state of being visible. This will get updated when we gain
  // new style.
  if (!element_->GetComputedStyle()) {
    SetRequestedState(EContentVisibility::kVisible);
    return;
  }

  // TODO(vmpstr): This needs to be in the spec.
  if (ForceUnlockIfNeeded())
    return;

  if (!IsLocked() && state_ != EContentVisibility::kVisible) {
    UpdateActivationObservationIfNeeded();
    NotifyRenderAffectingStateChanged();
  }
}

void DisplayLockContext::DidStyleChildren() {
  if (!element_->ChildNeedsReattachLayoutTree())
    return;
  auto* parent = element_->GetReattachParent();
  if (!parent || parent->ChildNeedsReattachLayoutTree())
    return;
  element_->MarkAncestorsWithChildNeedsReattachLayoutTree();
}

void DisplayLockContext::DidLayoutChildren() {
  // Since we did layout on children already, we'll clear this.
  child_layout_was_blocked_ = false;
  had_lifecycle_update_since_last_unlock_ = true;

  // If we're not locked and we laid out the children, then now is a good time
  // to restore the scroll offset.
  if (!is_locked_)
    RestoreScrollOffsetIfStashed();
}
// End Did* functions ==============================================

void DisplayLockContext::CommitForActivation(
    DisplayLockActivationReason reason) {
  DCHECK(element_);
  DCHECK(ConnectedToView());
  DCHECK(IsLocked());
  DCHECK(ShouldCommitForActivation(DisplayLockActivationReason::kAny));

  // The following actions (can) scroll content into view. However, if the
  // position of the target is outside of the bounds that would cause the
  // auto-context to unlock, then we can scroll into wrong content while the
  // context remains lock. To avoid this, unlock it until the next lifecycle.
  // If the scroll is successful, then we will gain visibility anyway so the
  // context will be unlocked for other reasons.
  if (reason == DisplayLockActivationReason::kAccessibility ||
      reason == DisplayLockActivationReason::kFindInPage ||
      reason == DisplayLockActivationReason::kFragmentNavigation ||
      reason == DisplayLockActivationReason::kScrollIntoView ||
      reason == DisplayLockActivationReason::kSimulatedClick) {
    // Note that because the visibility is only determined at the _end_ of the
    // next frame, we need to ensure that we stay unlocked for two frames.
    SetKeepUnlockedUntilLifecycleCount(2);
  }

  if (reason == DisplayLockActivationReason::kFindInPage)
    document_->MarkHasFindInPageContentVisibilityActiveMatch();
}

void DisplayLockContext::SetKeepUnlockedUntilLifecycleCount(int count) {
  DCHECK_GT(count, 0);
  keep_unlocked_count_ = std::max(keep_unlocked_count_, count);
  SetRenderAffectingState(
      RenderAffectingState::kAutoStateUnlockedUntilLifecycle, true);
  UpdateLifecycleNotificationRegistration();
  ScheduleAnimation();
}

void DisplayLockContext::NotifyIsIntersectingViewport() {
  had_any_viewport_intersection_notifications_ = true;
  // If we are now intersecting, then we are definitely not nested in a locked
  // subtree and we don't need to lock as a result.
  needs_deferred_not_intersecting_signal_ = false;
  UpdateLifecycleNotificationRegistration();
  // If we're not connected, then there is no need to change any state.
  // This could be the case if we were disconnected while a viewport
  // intersection notification was pending.
  if (ConnectedToView())
    SetRenderAffectingState(RenderAffectingState::kIntersectsViewport, true);
}

void DisplayLockContext::NotifyIsNotIntersectingViewport() {
  had_any_viewport_intersection_notifications_ = true;

  if (IsLocked()) {
    DCHECK(!needs_deferred_not_intersecting_signal_);
    return;
  }

  // We might have been disconnected while the intersection observation
  // notification was pending. Ensure to unregister from lifecycle
  // notifications if we're doing that, and early out.
  if (!ConnectedToView()) {
    needs_deferred_not_intersecting_signal_ = false;
    UpdateLifecycleNotificationRegistration();
    return;
  }

  // There are two situations we need to consider here:
  // 1. We are off-screen but not nested in any other lock. This means we should
  //    re-lock (also verify that the reason we're in this state is that we're
  //    activated).
  // 2. We are in a nested locked context. This means we don't actually know
  //    whether we should lock or not. In order to avoid needless dirty of the
  //    layout and style trees up to the nested context, we remain unlocked.
  //    However, we also need to ensure that we relock if we become unnested.
  //    So, we simply delay this check to the next frame (via LocalFrameView),
  //    which will call this function again and so we can perform the check
  //    again.
  // Note that we use a signal that we're not painting to defer intersection,
  // since even if we're updating the locked ancestor for style or layout, we
  // should defer intersection notifications.
  auto* locked_ancestor =
      DisplayLockUtilities::LockedAncestorPreventingPaint(*element_);
  if (locked_ancestor) {
    needs_deferred_not_intersecting_signal_ = true;
  } else {
    needs_deferred_not_intersecting_signal_ = false;
    SetRenderAffectingState(RenderAffectingState::kIntersectsViewport, false);
  }
  UpdateLifecycleNotificationRegistration();
}

bool DisplayLockContext::ShouldCommitForActivation(
    DisplayLockActivationReason reason) const {
  return IsActivatable(reason) && IsLocked();
}

void DisplayLockContext::UpgradeForcedScope(ForcedPhase old_phase,
                                            ForcedPhase new_phase,
                                            bool emit_warnings) {
  // Since we're upgrading, it means we have a bigger phase.
  DCHECK_LT(static_cast<int>(old_phase), static_cast<int>(new_phase));

  auto old_forced_info = forced_info_;
  forced_info_.end(old_phase);
  forced_info_.start(new_phase);
  if (IsLocked()) {
    // Now that the update is forced, we should ensure that style layout, and
    // prepaint code can reach it via dirty bits. Note that paint isn't a part
    // of this, since |forced_info_| doesn't force paint to happen. See
    // ShouldPaint(). Also, we could have forced a lock from SetRequestedState
    // during a style update. If that's the case, don't mark style as dirty
    // from within style recalc. We rely on `TakeBlockedStyleRecalcChange`
    // to be called from self style recalc.
    if (CanDirtyStyle() &&
        !old_forced_info.is_forced(ForcedPhase::kStyleAndLayoutTree) &&
        forced_info_.is_forced(ForcedPhase::kStyleAndLayoutTree)) {
      MarkForStyleRecalcIfNeeded();
    }
    if (!old_forced_info.is_forced(ForcedPhase::kLayout) &&
        forced_info_.is_forced(ForcedPhase::kLayout)) {
      MarkForLayoutIfNeeded();
    }
    if (!old_forced_info.is_forced(ForcedPhase::kPrePaint) &&
        forced_info_.is_forced(ForcedPhase::kPrePaint)) {
      MarkAncestorsForPrePaintIfNeeded();
    }

    if (emit_warnings && document_ &&
        document_->GetAgent().isolate()->InContext() && element_ &&
        (!IsActivatable(DisplayLockActivationReason::kAny) ||
         RuntimeEnabledFeatures::
             WarnOnContentVisibilityRenderAccessEnabled())) {
      document_->GetDisplayLockDocumentState().IssueForcedRenderWarning(
          element_);
    }
  }
}

void DisplayLockContext::ScheduleStateChangeEventIfNeeded() {
  if (state_ == EContentVisibility::kAuto &&
      !state_change_task_pending_) {
    document_->GetExecutionContext()
        ->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&DisplayLockContext::DispatchStateChangeEventIfNeeded,
                          WrapPersistent(this)));
    state_change_task_pending_ = true;
  }
}

void DisplayLockContext::DispatchStateChangeEventIfNeeded() {
  DCHECK(state_change_task_pending_);
  state_change_task_pending_ = false;
  // If we're not connected to view, reset the state that we reported so that we
  // can report it again on insertion.
  if (!ConnectedToView()) {
    last_notified_skipped_state_.reset();
    return;
  }

  if (!last_notified_skipped_state_ ||
      *last_notified_skipped_state_ != is_locked_) {
    last_notified_skipped_state_ = is_locked_;
    element_->DispatchEvent(*ContentVisibilityAutoStateChangeEvent::Create(
        event_type_names::kContentvisibilityautostatechange, is_locked_));
  }
}

void DisplayLockContext::NotifyForcedUpdateScopeEnded(ForcedPhase phase) {
  // Since we do perform updates in a locked display if we're in a forced
  // update scope, when ending a forced update scope in a locked display, we
  // remove all pending updates, to prevent them from being executed in a
  // locked display.
  // See: ../paint/README.md#Transform-update-optimization for more information
  if (is_locked_) {
    document_->View()->RemoveAllPendingUpdates();
  }
  forced_info_.end(phase);
}

void DisplayLockContext::Unlock() {
  DCHECK(IsLocked());
  is_locked_ = false;
  had_lifecycle_update_since_last_unlock_ = false;
  UpdateDocumentBookkeeping(true, !activatable_mask_, false,
                            !activatable_mask_);

  if (!ConnectedToView())
    return;

  // There are a few ways we can get unlocked:
  // 1. A new content-visibility property needs us to be ulocked.
  // 2. We're in 'auto' mode and we are intersecting the viewport.
  // In the first case, we are already in style processing, so we don't need to
  // invalidate style. However, in the second case we invalidate style so that
  // `AdjustElementStyle()` can be called.
  if (CanDirtyStyle()) {
    // Since size containment depends on the activatability state, we should
    // invalidate the style for this element, so that the style adjuster can
    // properly remove the containment.
    element_->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kDisplayLock));

    // Also propagate any dirty bits that we have previously blocked.
    // If we're in style recalc, this will be handled by
    // `TakeBlockedStyleRecalcChange()` call from self style recalc.
    MarkForStyleRecalcIfNeeded();
  } else if (SubtreeHasTopLayerElement()) {
    // TODO(vmpstr): This seems like a big hammer, but it's unclear to me how we
    // can mark the dirty bits from the descendant top layer node up to this
    // display lock on the ancestor chain while we're in the middle of style
    // recalc. It seems plausible, but we have to be careful.
    blocked_child_recalc_change_ =
        blocked_child_recalc_change_.ForceRecalcDescendants();
  }

  // We also need to notify the AX cache (if it exists) to update the children
  // of |element_| in the AX cache.
  if (auto* ax_cache = element_->GetDocument().ExistingAXObjectCache()) {
    ax_cache->RemoveSubtree(element_);
  }

  // Schedule ContentVisibilityAutoStateChange event if needed.
  ScheduleStateChangeEventIfNeeded();

  auto* layout_object = element_->GetLayoutObject();
  // We might commit without connecting, so there is no layout object yet.
  if (!layout_object)
    return;

  // Now that we know we have a layout object, we should ensure that we can
  // reach the rest of the phases as well.
  MarkForLayoutIfNeeded();
  MarkAncestorsForPrePaintIfNeeded();
  MarkNeedsRepaintAndPaintArtifactCompositorUpdate();
  MarkNeedsCullRectUpdate();
}

bool DisplayLockContext::CanDirtyStyle() const {
  return !set_requested_state_scope_ && !document_->InStyleRecalc();
}

bool DisplayLockContext::MarkForStyleRecalcIfNeeded() {
  DCHECK(CanDirtyStyle());

  if (IsElementDirtyForStyleRecalc()) {
    // Propagate to the ancestors, since the dirty bit in a locked subtree is
    // stopped at the locked ancestor.
    // See comment in IsElementDirtyForStyleRecalc.
    element_->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kDisplayLock));
    element_->MarkAncestorsWithChildNeedsStyleRecalc();

    // When we're forcing a lock, which is done in a CanDirtyStyle context, we
    // mark the top layers that don't have a computed style as needing a style
    // recalc. This is a heuristic since if a top layer doesn't have a computed
    // style then it is possibly under a content-visibility skipped subtree. The
    // alternative is to figure out exactly which top layer element is under
    // this lock and only dirty those, but that seems unnecessary. If the top
    // layer element is locked under a different lock, then the dirty bit
    // wouldn't propagate anyway.
    for (auto top_layer_element : document_->TopLayerElements()) {
      if (!top_layer_element->GetComputedStyle()) {
        top_layer_element->SetNeedsStyleRecalc(
            kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                   style_change_reason::kDisplayLock));
      }
    }
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkForLayoutIfNeeded() {
  if (IsElementDirtyForLayout()) {
    // Forces the marking of ancestors to happen, even if
    // |DisplayLockContext::ShouldLayout()| returns false.
    class ScopedForceLayout {
      STACK_ALLOCATED();

     public:
      explicit ScopedForceLayout(DisplayLockContext* context)
          : context_(context) {
        context_->forced_info_.start(ForcedPhase::kLayout);
      }
      ~ScopedForceLayout() { context_->forced_info_.end(ForcedPhase::kLayout); }

     private:
      DisplayLockContext* context_;
    } scoped_force(this);

    auto* layout_object = element_->GetLayoutObject();

    // Ensure any layout-type specific caches are dirty.
    layout_object->SetGridPlacementDirty(true);

    if (child_layout_was_blocked_ || HasStashedScrollOffset()) {
      // We've previously blocked a child traversal when doing self-layout for
      // the locked element, so we're marking it with child-needs-layout so that
      // it will traverse to the locked element and do the child traversal
      // again. We don't need to mark it for self-layout (by calling
      // |LayoutObject::SetNeedsLayout()|) because the locked element itself
      // doesn't need to relayout.
      //
      // Note that we also make sure to visit the children when we have a
      // stashed scroll offset. This is so that we can restore the offset after
      // laying out the children. If we try to restore it before the layout, it
      // will be ignored since the scroll area may think that it doesn't have
      // enough contents.
      // TODO(vmpstr): In the scroll offset case, we're doing this just so we
      // can reach DisplayLockContext::DidLayoutChildren where we restore the
      // offset. If performance becomes an issue, then we should think of a
      // different time / opportunity to restore the offset.
      layout_object->SetChildNeedsLayout();
      child_layout_was_blocked_ = false;
    } else {
      // Since the dirty layout propagation stops at the locked element, we need
      // to mark its ancestors as dirty here so that it will be traversed to on
      // the next layout.
      layout_object->MarkContainerChainForLayout();
    }
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkAncestorsForPrePaintIfNeeded() {
  // TODO(vmpstr): We should add a compositing phase for proper bookkeeping.
  bool compositing_dirtied = MarkForCompositingUpdatesIfNeeded();

  if (IsElementDirtyForPrePaint()) {
    auto* layout_object = element_->GetLayoutObject();
    if (auto* parent = layout_object->Parent())
      parent->SetSubtreeShouldCheckForPaintInvalidation();

    // Note that if either we or our descendants are marked as needing this
    // update, then ensure to mark self as needing the update. This sets up the
    // correct flags for PrePaint to recompute the necessary values and
    // propagate the information into the subtree.
    if (needs_effective_allowed_touch_action_update_ ||
        layout_object->EffectiveAllowedTouchActionChanged() ||
        layout_object->DescendantEffectiveAllowedTouchActionChanged()) {
      // Note that although the object itself should have up to date value, in
      // order to force recalc of the whole subtree, we mark it as needing an
      // update.
      layout_object->MarkEffectiveAllowedTouchActionChanged();
    }
    if (needs_blocking_wheel_event_handler_update_ ||
        layout_object->BlockingWheelEventHandlerChanged() ||
        layout_object->DescendantBlockingWheelEventHandlerChanged()) {
      // Note that although the object itself should have up to date value, in
      // order to force recalc of the whole subtree, we mark it as needing an
      // update.
      layout_object->MarkBlockingWheelEventHandlerChanged();
    }
    return true;
  }
  return compositing_dirtied;
}

bool DisplayLockContext::MarkNeedsRepaintAndPaintArtifactCompositorUpdate() {
  DCHECK(ConnectedToView());
  if (auto* layout_object = element_->GetLayoutObject()) {
    layout_object->PaintingLayer()->SetNeedsRepaint();
    document_->View()->SetPaintArtifactCompositorNeedsUpdate();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkNeedsCullRectUpdate() {
  DCHECK(ConnectedToView());
  if (auto* layout_object = element_->GetLayoutObject()) {
    layout_object->PaintingLayer()->SetForcesChildrenCullRectUpdate();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkForCompositingUpdatesIfNeeded() {
  if (!ConnectedToView())
    return false;

  auto* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return false;

  auto* layout_box = DynamicTo<LayoutBoxModelObject>(layout_object);
  if (layout_box && layout_box->HasSelfPaintingLayer()) {
    if (needs_compositing_dependent_flag_update_)
      layout_box->Layer()->SetNeedsCompositingInputsUpdate();
    needs_compositing_dependent_flag_update_ = false;

    return true;
  }
  return false;
}

bool DisplayLockContext::IsElementDirtyForStyleRecalc() const {
  // The |element_| checks could be true even if |blocked_child_recalc_change_|
  // is empty. The reason for this is that the |blocked_child_recalc_change_| is
  // set during the style walk that this display lock blocks. However, we could
  // dirty element style and unlock this context (e.g. by c-v auto visibility
  // change) before ever having gone through the style calc that would have been
  // blocked Also these dirty bits were not propagated to the ancestors, so we
  // do need to update the dirty bit state for ancestors.
  return element_->IsDirtyForStyleRecalc() ||
         element_->ChildNeedsStyleRecalc() ||
         element_->ChildNeedsReattachLayoutTree() ||
         !blocked_child_recalc_change_.IsEmpty() || SubtreeHasTopLayerElement();
}

bool DisplayLockContext::IsElementDirtyForLayout() const {
  if (auto* layout_object = element_->GetLayoutObject()) {
    return layout_object->NeedsLayout() || child_layout_was_blocked_ ||
           HasStashedScrollOffset();
  }
  return false;
}

bool DisplayLockContext::IsElementDirtyForPrePaint() const {
  if (auto* layout_object = element_->GetLayoutObject()) {
    return PrePaintTreeWalk::ObjectRequiresPrePaint(*layout_object) ||
           PrePaintTreeWalk::ObjectRequiresTreeBuilderContext(*layout_object) ||
           needs_prepaint_subtree_walk_ ||
           needs_effective_allowed_touch_action_update_ ||
           needs_blocking_wheel_event_handler_update_;
  }
  return false;
}

void DisplayLockContext::DidMoveToNewDocument(Document& old_document) {
  DCHECK(element_);
  document_ = &element_->GetDocument();

  old_document.GetDisplayLockDocumentState().RemoveDisplayLockContext(this);
  document_->GetDisplayLockDocumentState().AddDisplayLockContext(this);

  if (is_observed_) {
    old_document.GetDisplayLockDocumentState()
        .UnregisterDisplayLockActivationObservation(element_);
    document_->GetDisplayLockDocumentState()
        .RegisterDisplayLockActivationObservation(element_);
  }

  // Since we're observing the lifecycle updates, ensure that we listen to the
  // right document's view.
  if (is_registered_for_lifecycle_notifications_) {
    if (old_document.View())
      old_document.View()->UnregisterFromLifecycleNotifications(this);

    if (document_->View())
      document_->View()->RegisterForLifecycleNotifications(this);
    else
      is_registered_for_lifecycle_notifications_ = false;
  }

  if (IsLocked()) {
    old_document.GetDisplayLockDocumentState().RemoveLockedDisplayLock();
    document_->GetDisplayLockDocumentState().AddLockedDisplayLock();
    if (!IsActivatable(DisplayLockActivationReason::kAny)) {
      old_document.GetDisplayLockDocumentState()
          .DecrementDisplayLockBlockingAllActivation();
      document_->GetDisplayLockDocumentState()
          .IncrementDisplayLockBlockingAllActivation();
    }
  }

  DetermineIfSubtreeHasFocus();
  DetermineIfSubtreeHasSelection();
  DetermineIfSubtreeHasTopLayerElement();
  DetermineIfDescendantIsViewTransitionElement();
}

void DisplayLockContext::WillStartLifecycleUpdate(const LocalFrameView& view) {
  DCHECK(NeedsLifecycleNotifications());
  // We might have delayed processing intersection observation update (signal
  // that we were not intersecting) because this context was nested in another
  // locked context. At the start of the lifecycle, we should check whether
  // that is still true. In other words, this call will check if we're still
  // nested. If we are, we won't do anything. If we're not, then we will lock
  // this context.
  //
  // Note that when we are no longer nested and and we have not received any
  // notifications from the intersection observer, it means that we are not
  // visible.
  if (needs_deferred_not_intersecting_signal_)
    NotifyIsNotIntersectingViewport();

  bool update_registration = false;

  // If we're keeping this context unlocked, update the values.
  if (keep_unlocked_count_) {
    if (--keep_unlocked_count_) {
      ScheduleAnimation();
    } else {
      SetRenderAffectingState(
          RenderAffectingState::kAutoStateUnlockedUntilLifecycle, false);
      update_registration = true;
    }
  } else {
    DCHECK(!render_affecting_state_[static_cast<int>(
        RenderAffectingState::kAutoStateUnlockedUntilLifecycle)]);
  }

  if (has_pending_subtree_checks_ || has_pending_top_layer_check_) {
    DetermineIfSubtreeHasTopLayerElement();
    has_pending_top_layer_check_ = false;
    update_registration = true;
  }

  if (has_pending_subtree_checks_) {
    DetermineIfSubtreeHasFocus();
    DetermineIfSubtreeHasSelection();

    has_pending_subtree_checks_ = false;
    update_registration = true;
  }

  if (has_pending_clear_has_top_layer_) {
    SetRenderAffectingState(RenderAffectingState::kSubtreeHasTopLayerElement,
                            false);
    has_pending_clear_has_top_layer_ = false;
    update_registration = true;
  }

  if (update_registration)
    UpdateLifecycleNotificationRegistration();
}

void DisplayLockContext::DidFinishLayout() {
  if (!anchor_positioning_render_state_may_have_changed_) {
    return;
  }
  anchor_positioning_render_state_may_have_changed_ = false;
  UpdateLifecycleNotificationRegistration();
  if (DescendantIsAnchorTargetFromOutsideDisplayLock()) {
    SetAffectedByAnchorPositioning(true);
  } else {
    SetAffectedByAnchorPositioning(false);
  }
}

void DisplayLockContext::SetAnchorPositioningRenderStateMayHaveChanged() {
  if (anchor_positioning_render_state_may_have_changed_) {
    return;
  }
  anchor_positioning_render_state_may_have_changed_ = true;
  UpdateLifecycleNotificationRegistration();
}

void DisplayLockContext::NotifyWillDisconnect() {
  if (!IsLocked() || !element_ || !element_->GetLayoutObject())
    return;
  // If we're locked while being disconnected, we need to layout the parent.
  // The reason for this is that we might skip the layout if we're empty while
  // locked, but it's important to update IsSelfCollapsingBlock property on
  // the parent so that it's up to date. This property is updated during
  // layout.
  if (auto* parent = element_->GetLayoutObject()->Parent())
    parent->SetNeedsLayout(layout_invalidation_reason::kDisplayLock);
}

void DisplayLockContext::ElementDisconnected() {
  // We remove the style when disconnecting an element, so we should also unlock
  // the context.
  DCHECK(!element_->GetComputedStyle());
  SetRequestedState(EContentVisibility::kVisible);

  if (auto* document_rules =
          DocumentSpeculationRules::FromIfExists(*document_)) {
    document_rules->DisplayLockedElementDisconnected(element_);
  }

  // blocked_child_recalc_change_ must be cleared because things can be in an
  // inconsistent state when we add the element back (e.g. crbug.com/1262742).
  blocked_child_recalc_change_ = StyleRecalcChange();
}

void DisplayLockContext::ElementConnected() {
  // When connecting the element, we should not have a style.
  DCHECK(!element_->GetComputedStyle());

  // We can't check for subtree selection / focus here, since we are likely in
  // slot reassignment forbidden scope. However, walking the subtree may need
  // this reassignment. This is fine, since the state check can be deferred
  // until the beginning of the next frame.
  has_pending_subtree_checks_ = true;
  UpdateLifecycleNotificationRegistration();
  ScheduleAnimation();
}

void DisplayLockContext::DetachLayoutTree() {
  // When |element_| is removed from the flat tree, we need to set this context
  // to visible.
  if (!element_->GetComputedStyle()) {
    SetRequestedState(EContentVisibility::kVisible);
    blocked_child_recalc_change_ = StyleRecalcChange();
  }
}

void DisplayLockContext::ScheduleTopLayerCheck() {
  has_pending_top_layer_check_ = true;
  UpdateLifecycleNotificationRegistration();
  ScheduleAnimation();
}

void DisplayLockContext::ScheduleAnimation() {
  DCHECK(element_);
  if (!ConnectedToView() || !document_ || !document_->GetPage())
    return;

  // Schedule an animation to perform the lifecycle phases.
  document_->GetPage()->Animator().ScheduleVisualUpdate(document_->GetFrame());
}

const char* DisplayLockContext::ShouldForceUnlock() const {
  DCHECK(element_);
  // This function is only called after style, layout tree, or lifecycle
  // updates, so the style should be up-to-date, except in the case of nested
  // locks, where the style recalc will never actually get to |element_|.
  // TODO(vmpstr): We need to figure out what to do here, since we don't know
  // what the style is and whether this element has proper containment. However,
  // forcing an update from the ancestor locks seems inefficient. For now, we
  // just optimistically assume that we have all of the right containment in
  // place. See crbug.com/926276 for more information.
  if (element_->NeedsStyleRecalc()) {
    DCHECK(DisplayLockUtilities::LockedAncestorPreventingStyle(*element_));
    return nullptr;
  }

  if (element_->HasDisplayContentsStyle())
    return rejection_names::kUnsupportedDisplay;

  auto* style = element_->GetComputedStyle();
  DCHECK(style);

  // We need style and layout containment in order to properly lock the subtree.
  if (!style->ContainsStyle() || !style->ContainsLayout())
    return rejection_names::kContainmentNotSatisfied;

  // We allow replaced elements without fallback content to be locked. This
  // check is similar to the check in DefinitelyNewFormattingContext() in
  // element.cc, but in this case we allow object element to get locked.
  if (const auto* object_element = DynamicTo<HTMLObjectElement>(*element_)) {
    if (!object_element->UseFallbackContent())
      return nullptr;
  } else if (IsA<HTMLImageElement>(*element_) ||
             IsA<HTMLCanvasElement>(*element_) ||
             (element_->IsFormControlElement() &&
              !element_->IsOutputElement()) ||
             element_->IsMediaElement() || element_->IsFrameOwnerElement() ||
             element_->IsSVGElement()) {
    return nullptr;
  }

  // From https://www.w3.org/TR/css-contain-1/#containment-layout
  // If the element does not generate a principal box (as is the case with
  // display: contents or display: none), or if the element is an internal
  // table element other than display: table-cell, if the element is an
  // internal ruby element, or if the element’s principal box is a
  // non-atomic inline-level box, layout containment has no effect.
  // (Note we're allowing display:none for display locked elements).
  if ((style->IsDisplayTableType() &&
       style->Display() != EDisplay::kTableCell) ||
      style->Display() == EDisplay::kRubyText ||
      (style->IsDisplayInlineType() && !style->IsDisplayReplacedType())) {
    return rejection_names::kContainmentNotSatisfied;
  }
  return nullptr;
}

bool DisplayLockContext::ForceUnlockIfNeeded() {
  // We must have "contain: style layout", and disallow display:contents
  // for display locking. Note that we should always guarantee this after
  // every style or layout tree update. Otherwise, proceeding with layout may
  // cause unexpected behavior. By rejecting the promise, the behavior can be
  // detected by script.
  // TODO(rakina): If this is after acquire's promise is resolved and update()
  // commit() isn't in progress, the web author won't know that the element
  // got unlocked. Figure out how to notify the author.
  if (ShouldForceUnlock()) {
    if (IsLocked()) {
      Unlock();
      // If we forced unlock, then we need to prevent subsequent calls to
      // Lock() until the next frame.
      SetRequestedState(EContentVisibility::kVisible);
    }
    return true;
  }
  return false;
}

bool DisplayLockContext::ConnectedToView() const {
  return element_ && document_ && element_->isConnected() && document_->View();
}

void DisplayLockContext::NotifySubtreeLostFocus() {
  SetRenderAffectingState(RenderAffectingState::kSubtreeHasFocus, false);
}

void DisplayLockContext::NotifySubtreeGainedFocus() {
  SetRenderAffectingState(RenderAffectingState::kSubtreeHasFocus, true);
}

void DisplayLockContext::DetermineIfSubtreeHasFocus() {
  if (!ConnectedToView()) {
    SetRenderAffectingState(RenderAffectingState::kSubtreeHasFocus, false);
    return;
  }

  bool subtree_has_focus = false;
  // Iterate up the ancestor chain from the currently focused element. If at any
  // time we find our element, then our subtree is focused.
  for (auto* focused = document_->FocusedElement(); focused;
       focused = FlatTreeTraversal::ParentElement(*focused)) {
    if (focused == element_.Get()) {
      subtree_has_focus = true;
      break;
    }
  }
  SetRenderAffectingState(RenderAffectingState::kSubtreeHasFocus,
                          subtree_has_focus);
}

void DisplayLockContext::DetermineIfSubtreeHasTopLayerElement() {
  if (!ConnectedToView())
    return;

  ClearHasTopLayerElement();

  // Iterate up the ancestor chain from each top layer element.
  // Note that this walk is searching for just the |element_| associated with
  // this lock. The walk in DisplayLockDocumentState walks from top layer
  // elements all the way to the ancestors searching for display locks, so if we
  // have nested display locks that walk is more optimal.
  for (auto top_layer_element : document_->TopLayerElements()) {
    auto* ancestor = top_layer_element.Get();
    while ((ancestor = FlatTreeTraversal::ParentElement(*ancestor))) {
      if (ancestor == element_) {
        NotifyHasTopLayerElement();
        return;
      }
    }
  }
}

void DisplayLockContext::DetermineIfDescendantIsViewTransitionElement() {
  ResetDescendantIsViewTransitionElement();
  if (ConnectedToView()) {
    document_->GetDisplayLockDocumentState()
        .UpdateViewTransitionElementAncestorLocks();
  }
}

void DisplayLockContext::ResetDescendantIsViewTransitionElement() {
  SetRenderAffectingState(
      RenderAffectingState::kDescendantIsViewTransitionElement, false);
}

void DisplayLockContext::SetDescendantIsViewTransitionElement() {
  SetRenderAffectingState(
      RenderAffectingState::kDescendantIsViewTransitionElement, true);
}

void DisplayLockContext::ClearHasTopLayerElement() {
  // Note that this is asynchronous because it can happen during a layout detach
  // which is a bad time to relock a content-visibility auto element (since it
  // causes us to potentially access layout objects which are in a state of
  // being destroyed).
  has_pending_clear_has_top_layer_ = true;
  UpdateLifecycleNotificationRegistration();
  ScheduleAnimation();
}

void DisplayLockContext::NotifyHasTopLayerElement() {
  has_pending_clear_has_top_layer_ = false;
  SetRenderAffectingState(RenderAffectingState::kSubtreeHasTopLayerElement,
                          true);
  UpdateLifecycleNotificationRegistration();
}

bool DisplayLockContext::SubtreeHasTopLayerElement() const {
  return render_affecting_state_[static_cast<int>(
      RenderAffectingState::kSubtreeHasTopLayerElement)];
}

void DisplayLockContext::DetachDescendantTopLayerElements() {
  if (!ConnectedToView() || !SubtreeHasTopLayerElement())
    return;

  std::optional<StyleEngine::DetachLayoutTreeScope> detach_scope;
  if (!document_->InStyleRecalc()) {
    detach_scope.emplace(document_->GetStyleEngine());
  }

  // Detach all top layer elements contained by the element inducing this
  // display lock.
  // Detaching a layout tree can cause further top layer elements to be removed
  // from the top layer element's list (in a nested top layer element case --
  // since we would remove the ::backdrop pseudo when the layout object
  // disappears). This means that we're potentially modifying the list as we're
  // traversing it. Instead of doing that, make a copy.
  auto top_layer_elements = document_->TopLayerElements();
  for (auto top_layer_element : top_layer_elements) {
    auto* ancestor = top_layer_element.Get();
    while ((ancestor = FlatTreeTraversal::ParentElement(*ancestor))) {
      if (ancestor == element_) {
        top_layer_element->DetachLayoutTree();
        break;
      }
    }
  }
}

void DisplayLockContext::NotifySubtreeGainedSelection() {
  SetRenderAffectingState(RenderAffectingState::kSubtreeHasSelection, true);
}

void DisplayLockContext::NotifySubtreeLostSelection() {
  SetRenderAffectingState(RenderAffectingState::kSubtreeHasSelection, false);
}

void DisplayLockContext::DetermineIfSubtreeHasSelection() {
  if (!ConnectedToView() || !document_->GetFrame()) {
    SetRenderAffectingState(RenderAffectingState::kSubtreeHasSelection, false);
    return;
  }

  auto range = ToEphemeralRangeInFlatTree(document_->GetFrame()
                                              ->Selection()
                                              .GetSelectionInDOMTree()
                                              .ComputeRange());
  bool subtree_has_selection = false;
  for (auto& node : range.Nodes()) {
    for (auto& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(node)) {
      if (&ancestor == element_.Get()) {
        subtree_has_selection = true;
        break;
      }
    }
    if (subtree_has_selection)
      break;
  }
  SetRenderAffectingState(RenderAffectingState::kSubtreeHasSelection,
                          subtree_has_selection);
}

void DisplayLockContext::SetRenderAffectingState(RenderAffectingState state,
                                                 bool new_flag) {
  // If we have forced activatable locks, it is possible that we're within
  // find-in-page. We cannot lock an object while doing this, since it may
  // invalidate layout and in turn prevent find-in-page from properly finding
  // text (and DCHECK). Since layout is clean for this lock (we're unlocked),
  // keep the context unlocked until the next lifecycle starts.
  if (state == RenderAffectingState::kSubtreeHasSelection && !new_flag &&
      document_->GetDisplayLockDocumentState()
          .ActivatableDisplayLocksForced()) {
    SetKeepUnlockedUntilLifecycleCount(1);
  }
  // If we are changing state due to disappeared anchors, we're in a post-layout
  // state and therefore can't dirty style. Wait until the next lifecycle
  // starts.
  if (state == RenderAffectingState::kDescendantIsAnchorTarget && !new_flag) {
    SetKeepUnlockedUntilLifecycleCount(1);
  }

  render_affecting_state_[static_cast<int>(state)] = new_flag;
  NotifyRenderAffectingStateChanged();
}

void DisplayLockContext::NotifyRenderAffectingStateChanged() {
  auto state = [this](RenderAffectingState state) {
    return render_affecting_state_[static_cast<int>(state)];
  };

  // Check that we're visible if and only if lock has not been requested.
  DCHECK_EQ(state_ == EContentVisibility::kVisible,
            !state(RenderAffectingState::kLockRequested));

  // We should be locked if the lock has been requested (the above DCHECKs
  // verify that this means that we are not 'visible'), and any of the
  // following is true:
  // - We are not in 'auto' mode (meaning 'hidden') or
  // - We are in 'auto' mode and nothing blocks locking: viewport is
  //   not intersecting, subtree doesn't have focus, and subtree doesn't have
  //   selection, etc. See the condition for the full list.
  bool should_be_locked =
      state(RenderAffectingState::kLockRequested) &&
      (state_ != EContentVisibility::kAuto ||
       (!state(RenderAffectingState::kIntersectsViewport) &&
        !state(RenderAffectingState::kSubtreeHasFocus) &&
        !state(RenderAffectingState::kSubtreeHasSelection) &&
        !state(RenderAffectingState::kAutoStateUnlockedUntilLifecycle) &&
        !state(RenderAffectingState::kAutoUnlockedForPrint) &&
        !state(RenderAffectingState::kSubtreeHasTopLayerElement) &&
        !state(RenderAffectingState::kDescendantIsViewTransitionElement) &&
        !state(RenderAffectingState::kDescendantIsAnchorTarget)));

  if (should_be_locked && !IsLocked())
    Lock();
  else if (!should_be_locked && IsLocked())
    Unlock();
}

bool DisplayLockContext::DescendantIsAnchorTargetFromOutsideDisplayLock() {
  for (auto* obj = element_->GetLayoutObject(); obj; obj = obj->Container()) {
    if (const auto* ancestor_box = DynamicTo<LayoutBox>(obj)) {
      // Return true if any out-of-flow positioned elements below this
      // ancestor are anchored to elements below the display lock.
      for (const PhysicalBoxFragment& fragment :
           ancestor_box->PhysicalFragments()) {
        // Early out if there are no anchor targets in the subtree.
        if (!fragment.HasAnchorQuery()) {
          return false;
        }
        // Early out if there are not OOF children.
        if (!fragment.HasOutOfFlowFragmentChild()) {
          continue;
        }
        for (const PhysicalFragmentLink& fragment_child : fragment.Children()) {
          // Skip non-OOF children.
          if (!fragment_child->IsOutOfFlowPositioned()) {
            continue;
          }
          if (auto* box = DynamicTo<LayoutBox>(
                  fragment_child->GetMutableLayoutObject())) {
            if (auto* display_locks = box->DisplayLocksAffectedByAnchors()) {
              if (display_locks->find(element_) != display_locks->end()) {
                return true;
              }
            }
          }
        }
      }
    }
  }
  return false;
}

void DisplayLockContext::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(document_);
  ElementRareDataField::Trace(visitor);
}

void DisplayLockContext::SetShouldUnlockAutoForPrint(bool flag) {
  SetRenderAffectingState(RenderAffectingState::kAutoUnlockedForPrint, flag);
}

const char* DisplayLockContext::RenderAffectingStateName(int state) const {
  switch (static_cast<RenderAffectingState>(state)) {
    case RenderAffectingState::kLockRequested:
      return "LockRequested";
    case RenderAffectingState::kIntersectsViewport:
      return "IntersectsViewport";
    case RenderAffectingState::kSubtreeHasFocus:
      return "SubtreeHasFocus";
    case RenderAffectingState::kSubtreeHasSelection:
      return "SubtreeHasSelection";
    case RenderAffectingState::kAutoStateUnlockedUntilLifecycle:
      return "AutoStateUnlockedUntilLifecycle";
    case RenderAffectingState::kAutoUnlockedForPrint:
      return "AutoUnlockedForPrint";
    case RenderAffectingState::kSubtreeHasTopLayerElement:
      return "SubtreeHasTopLayerElement";
    case RenderAffectingState::kDescendantIsViewTransitionElement:
      return "DescendantIsViewTransitionElement";
    case RenderAffectingState::kDescendantIsAnchorTarget:
      return "kDescendantIsAnchorTarget";
    case RenderAffectingState::kNumRenderAffectingStates:
      break;
  }
  return "<Invalid State>";
}

String DisplayLockContext::RenderAffectingStateToString() const {
  StringBuilder builder;
  for (int i = 0;
       i < static_cast<int>(RenderAffectingState::kNumRenderAffectingStates);
       ++i) {
    builder.Append(RenderAffectingStateName(i));
    builder.Append(": ");
    builder.Append(render_affecting_state_[i] ? "true" : "false");
    builder.Append("\n");
  }
  return builder.ToString();
}

void DisplayLockContext::StashScrollOffsetIfAvailable() {
  if (auto* area = GetScrollableArea(element_)) {
    const ScrollOffset& offset = area->GetScrollOffset();
    // Only store the offset if it's non-zero. This is because scroll
    // restoration has a small performance implication and restoring to a zero
    // offset is the same as not restoring it.
    if (!offset.IsZero()) {
      stashed_scroll_offset_.emplace(offset);
    }
  }
}

void DisplayLockContext::RestoreScrollOffsetIfStashed() {
  if (!stashed_scroll_offset_.has_value()) {
    return;
  }

  // Restore the offset and reset the value.
  if (auto* area = GetScrollableArea(element_)) {
    area->SetScrollOffset(*stashed_scroll_offset_,
                          mojom::blink::ScrollType::kAnchoring);
    stashed_scroll_offset_.reset();
  }
}

bool DisplayLockContext::HasStashedScrollOffset() const {
  return stashed_scroll_offset_.has_value();
}

bool DisplayLockContext::ActivatableDisplayLocksForced() const {
  return document_->GetDisplayLockDocumentState()
      .ActivatableDisplayLocksForced();
}

void DisplayLockContext::SetAffectedByAnchorPositioning(bool val) {
  SetRenderAffectingState(RenderAffectingState::kDescendantIsAnchorTarget, val);
}

}  // namespace blink

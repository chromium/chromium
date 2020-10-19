// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_recalc.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
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

void RecordActivationReason(Document* document,
                            DisplayLockActivationReason reason) {
  int ordered_reason = -1;

  // IMPORTANT: This number needs to be bumped up when adding
  // new reasons.
  static const int number_of_reasons = 9;

  switch (reason) {
    case DisplayLockActivationReason::kAccessibility:
      ordered_reason = 0;
      break;
    case DisplayLockActivationReason::kFindInPage:
      ordered_reason = 1;
      break;
    case DisplayLockActivationReason::kFragmentNavigation:
      ordered_reason = 2;
      break;
    case DisplayLockActivationReason::kScriptFocus:
      ordered_reason = 3;
      break;
    case DisplayLockActivationReason::kScrollIntoView:
      ordered_reason = 4;
      break;
    case DisplayLockActivationReason::kSelection:
      ordered_reason = 5;
      break;
    case DisplayLockActivationReason::kSimulatedClick:
      ordered_reason = 6;
      break;
    case DisplayLockActivationReason::kUserFocus:
      ordered_reason = 7;
      break;
    case DisplayLockActivationReason::kViewportIntersection:
      ordered_reason = 8;
      break;
    case DisplayLockActivationReason::kViewport:
    case DisplayLockActivationReason::kAny:
      NOTREACHED();
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Blink.Render.DisplayLockActivationReason",
                            ordered_reason, number_of_reasons);

  if (document && reason == DisplayLockActivationReason::kFindInPage)
    document->MarkHasFindInPageContentVisibilityActiveMatch();
}
}  // namespace

DisplayLockContext::DisplayLockContext(Element* element)
    : element_(element), document_(&element_->GetDocument()) {
  document_->GetDisplayLockDocumentState().AddDisplayLockContext(this);
  DetermineIfSubtreeHasFocus();
  DetermineIfSubtreeHasSelection();
}

void DisplayLockContext::SetRequestedState(EContentVisibility state) {
  if (state_ == state)
    return;
  state_ = state;
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
      RequestLock(0u);
      break;
    case EContentVisibility::kHiddenMatchable:
      UseCounter::Count(document_,
                        WebFeature::kContentVisibilityHiddenMatchable);
      RequestLock(
          static_cast<uint16_t>(DisplayLockActivationReason::kAny) &
          ~static_cast<uint16_t>(DisplayLockActivationReason::kViewport));
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
  // Note that creating this forced object may cause us to dirty style, which is
  // fine since we are in a style update for this subtree anyway.
  StyleEngine::AllowMarkStyleDirtyFromRecalcScope scope(
      element_->GetDocument().GetStyleEngine());
  element_->GetDocument().GetDisplayLockDocumentState().ForceLockIfNeeded(
      element_.Get());
}

void DisplayLockContext::AdjustElementStyle(ComputedStyle* style) const {
  if (state_ == EContentVisibility::kVisible)
    return;
  // If not visible, element gains style, layout, and paint containment. If
  // skipped, it also gains size containment.
  // https://wicg.github.io/display-locking/#content-visibility
  auto contain =
      style->Contain() | kContainsStyle | kContainsLayout | kContainsPaint;
  if (IsLocked())
    contain |= kContainsSize;
  style->SetContain(contain);
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
             RenderAffectingState::kAutoStateUnlockedUntilLifecycle)];
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
  if (!ConnectedToView())
    return;

  // There are two ways we can get locked:
  // 1. A new content-visibility property needs us to be locked.
  // 2. We're in 'auto' mode and we are not intersecting the viewport.
  // In the first case, we are already in style processing, so we don't need to
  // invalidate style. However, in the second case we invalidate style so that
  // `AdjustElementStyle()` can be called.
  if (!document_->InStyleRecalc()) {
    element_->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kDisplayLock));

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
  }

  // We need to notify the AX cache (if it exists) to update |element_|'s
  // children in the AX cache.
  if (AXObjectCache* cache = element_->GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(element_);

  if (!element_->GetLayoutObject())
    return;

  MarkNeedsRepaintAndPaintArtifactCompositorUpdate();
}

// Should* and Did* function for the lifecycle phases. These functions control
// whether or not to process the lifecycle for self or for children.
// =============================================================================
bool DisplayLockContext::ShouldStyleChildren() const {
  return !is_locked_ || update_forced_ ||
         (document_->GetDisplayLockDocumentState()
              .ActivatableDisplayLocksForced() &&
          IsActivatable(DisplayLockActivationReason::kAny));
}

void DisplayLockContext::DidStyleSelf() {
  // TODO(vmpstr): This needs to be in the spec.
  if (ForceUnlockIfNeeded())
    return;

  if (!IsLocked() && state_ != EContentVisibility::kVisible) {
    UpdateActivationObservationIfNeeded();
    NotifyRenderAffectingStateChanged();
  }

  if (blocked_style_traversal_type_ == kStyleUpdateSelf)
    blocked_style_traversal_type_ = kStyleUpdateNotRequired;
}

void DisplayLockContext::DidStyleChildren() {
  if (element_->ChildNeedsReattachLayoutTree())
    element_->MarkAncestorsWithChildNeedsReattachLayoutTree();
  blocked_style_traversal_type_ = kStyleUpdateNotRequired;
  MarkElementsForWhitespaceReattachment();
}

bool DisplayLockContext::ShouldLayoutChildren() const {
  return !is_locked_ || update_forced_ ||
         (document_->GetDisplayLockDocumentState()
              .ActivatableDisplayLocksForced() &&
          IsActivatable(DisplayLockActivationReason::kAny));
}

void DisplayLockContext::DidLayoutChildren() {
  // Since we did layout on children already, we'll clear this.
  child_layout_was_blocked_ = false;
}

bool DisplayLockContext::ShouldPrePaintChildren() const {
  return !is_locked_ || update_forced_;
}

bool DisplayLockContext::ShouldPaintChildren() const {
  // Note that forced updates should never require us to paint, so we don't
  // check |update_forced_| here. In other words, although |update_forced_|
  // could be true here, we still should not paint.
  return !is_locked_;
}
// End Should* and Did* functions ==============================================

bool DisplayLockContext::IsActivatable(
    DisplayLockActivationReason reason) const {
  return activatable_mask_ & static_cast<uint16_t>(reason);
}

void DisplayLockContext::CommitForActivationWithSignal(
    Element* activated_element,
    DisplayLockActivationReason reason) {
  DCHECK(activated_element);
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

  RecordActivationReason(document_, reason);
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
  auto* locked_ancestor =
      DisplayLockUtilities::NearestLockedExclusiveAncestor(*element_);
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

void DisplayLockContext::NotifyForcedUpdateScopeStarted() {
  ++update_forced_;
  if (update_forced_ == 1) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"), "LockForced",
        TRACE_ID_LOCAL(this));
  }

  if (IsLocked()) {
    // Now that the update is forced, we should ensure that style layout, and
    // prepaint code can reach it via dirty bits. Note that paint isn't a part
    // of this, since |update_forced_| doesn't force paint to happen. See
    // ShouldPaint().
    MarkForStyleRecalcIfNeeded();
    MarkForLayoutIfNeeded();
    MarkAncestorsForPrePaintIfNeeded();
  }
}

void DisplayLockContext::NotifyForcedUpdateScopeEnded() {
  DCHECK(update_forced_);
  --update_forced_;
  if (update_forced_ == 0) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"), "LockForced",
        TRACE_ID_LOCAL(this));
  }
}

void DisplayLockContext::Unlock() {
  DCHECK(IsLocked());
  is_locked_ = false;
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
  if (!document_->InStyleRecalc()) {
    // Since size containment depends on the activatability state, we should
    // invalidate the style for this element, so that the style adjuster can
    // properly remove the containment.
    element_->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kDisplayLock));

    // Also propagate any dirty bits that we have previously blocked.
    // If we're in style recalc, this will be handled by
    // `AdjustStyleRecalcChangeForChildren()`.
    MarkForStyleRecalcIfNeeded();
  }

  // We also need to notify the AX cache (if it exists) to update the childrens
  // of |element_| in the AX cache.
  if (AXObjectCache* cache = element_->GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(element_);

  auto* layout_object = element_->GetLayoutObject();
  // We might commit without connecting, so there is no layout object yet.
  if (!layout_object)
    return;

  // Now that we know we have a layout object, we should ensure that we can
  // reach the rest of the phases as well.
  MarkForLayoutIfNeeded();
  MarkAncestorsForPrePaintIfNeeded();
  MarkNeedsRepaintAndPaintArtifactCompositorUpdate();
}

void DisplayLockContext::AddToWhitespaceReattachSet(Element& element) {
  whitespace_reattach_set_.insert(&element);
}

void DisplayLockContext::MarkElementsForWhitespaceReattachment() {
  for (auto element : whitespace_reattach_set_) {
    if (!element || element->NeedsReattachLayoutTree() ||
        !element->GetLayoutObject())
      continue;

    if (Node* first_child = LayoutTreeBuilderTraversal::FirstChild(*element))
      first_child->MarkAncestorsWithChildNeedsReattachLayoutTree();
  }
  whitespace_reattach_set_.clear();
}

StyleRecalcChange DisplayLockContext::AdjustStyleRecalcChangeForChildren(
    StyleRecalcChange change) {
  // This code is similar to MarkForStyleRecalcIfNeeded, except that it acts on
  // |change| and not on |element_|. This is only called during style recalc.
  // Note that since we're already in self style recalc, this code is shorter
  // since it doesn't have to deal with dirtying self-style.
  DCHECK(document_->InStyleRecalc());

  if (reattach_layout_tree_was_blocked_) {
    change = change.ForceReattachLayoutTree();
    reattach_layout_tree_was_blocked_ = false;
  }

  if (blocked_style_traversal_type_ == kStyleUpdateDescendants)
    change = change.ForceRecalcDescendants();
  else if (blocked_style_traversal_type_ == kStyleUpdateChildren)
    change = change.EnsureAtLeast(StyleRecalcChange::kRecalcChildren);
  blocked_style_traversal_type_ = kStyleUpdateNotRequired;
  return change;
}

bool DisplayLockContext::MarkForStyleRecalcIfNeeded() {
  if (reattach_layout_tree_was_blocked_) {
    // We previously blocked a layout tree reattachment on |element_|'s
    // descendants, so we should mark it for layout tree reattachment now.
    element_->SetForceReattachLayoutTree();
    reattach_layout_tree_was_blocked_ = false;
  }
  if (IsElementDirtyForStyleRecalc()) {
    if (blocked_style_traversal_type_ > kStyleUpdateNotRequired) {
      // We blocked a traversal going to the element previously.
      // Make sure we will traverse this element and maybe its subtree if we
      // previously blocked a style traversal that should've done that.
      element_->SetNeedsStyleRecalc(
          blocked_style_traversal_type_ == kStyleUpdateDescendants
              ? kSubtreeStyleChange
              : kLocalStyleChange,
          StyleChangeReasonForTracing::Create(
              style_change_reason::kDisplayLock));
      if (blocked_style_traversal_type_ == kStyleUpdateChildren)
        element_->SetChildNeedsStyleRecalc();
      blocked_style_traversal_type_ = kStyleUpdateNotRequired;
    } else if (element_->ChildNeedsReattachLayoutTree()) {
      // Mark |element_| as style dirty, as we can't mark for child reattachment
      // before style.
      element_->SetNeedsStyleRecalc(kLocalStyleChange,
                                    StyleChangeReasonForTracing::Create(
                                        style_change_reason::kDisplayLock));
    }
    // Propagate to the ancestors, since the dirty bit in a locked subtree is
    // stopped at the locked ancestor.
    // See comment in IsElementDirtyForStyleRecalc.
    element_->MarkAncestorsWithChildNeedsStyleRecalc();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkForLayoutIfNeeded() {
  if (IsElementDirtyForLayout()) {
    // Forces the marking of ancestors to happen, even if
    // |DisplayLockContext::ShouldLayout()| returns false.
    base::AutoReset<int> scoped_force(&update_forced_, update_forced_ + 1);
    if (child_layout_was_blocked_) {
      // We've previously blocked a child traversal when doing self-layout for
      // the locked element, so we're marking it with child-needs-layout so that
      // it will traverse to the locked element and do the child traversal
      // again. We don't need to mark it for self-layout (by calling
      // |LayoutObject::SetNeedsLayout()|) because the locked element itself
      // doesn't need to relayout.
      element_->GetLayoutObject()->SetChildNeedsLayout();
      child_layout_was_blocked_ = false;
    } else {
      // Since the dirty layout propagation stops at the locked element, we need
      // to mark its ancestors as dirty here so that it will be traversed to on
      // the next layout.
      element_->GetLayoutObject()->MarkContainerChainForLayout();
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

bool DisplayLockContext::MarkForCompositingUpdatesIfNeeded() {
  if (!ConnectedToView())
    return false;

  auto* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return false;

  auto* layout_box = DynamicTo<LayoutBoxModelObject>(layout_object);
  if (layout_box && layout_box->HasSelfPaintingLayer()) {
    if (layout_box->Layer()->ChildNeedsCompositingInputsUpdate() &&
        layout_box->Layer()->Parent()) {
      // Note that if the layer's child needs compositing inputs update, then
      // that layer itself also needs compositing inputs update. In order to
      // propagate the dirty bit, we need to mark this layer's _parent_ as a
      // needing an update.
      layout_box->Layer()->Parent()->SetNeedsCompositingInputsUpdate();
    }
    if (needs_compositing_requirements_update_)
      layout_box->Layer()->SetNeedsCompositingRequirementsUpdate();
    needs_compositing_requirements_update_ = false;

    if (needs_compositing_dependent_flag_update_)
      layout_box->Layer()->SetNeedsCompositingInputsUpdate();
    needs_compositing_dependent_flag_update_ = false;

    if (needs_graphics_layer_rebuild_)
      layout_box->Layer()->SetNeedsGraphicsLayerRebuild();
    needs_graphics_layer_rebuild_ = false;

    if (forced_graphics_layer_update_blocked_) {
      // We only add an extra dirty bit to the compositing state, which is safe
      // since we do this before updating the compositing state.
      DisableCompositingQueryAsserts disabler;

      auto* compositing_parent =
          layout_box->Layer()->EnclosingLayerWithCompositedLayerMapping(
              kIncludeSelf);
      compositing_parent->GetCompositedLayerMapping()
          ->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);
    }
    forced_graphics_layer_update_blocked_ = false;

    return true;
  }
  return false;
}

bool DisplayLockContext::IsElementDirtyForStyleRecalc() const {
  // The |element_| checks could be true even if |blocked_style_traversal_type_|
  // is not required. The reason for this is that the
  // blocked_style_traversal_type_ is set during the style walk that this
  // display lock blocked. However, we could dirty element style and commit
  // before ever having gone through the style calc that would have been
  // blocked, meaning we never blocked style during a walk. Instead we might
  // have not propagated the dirty bits up the tree.
  return element_->NeedsStyleRecalc() || element_->ChildNeedsStyleRecalc() ||
         element_->ChildNeedsReattachLayoutTree() ||
         blocked_style_traversal_type_ > kStyleUpdateNotRequired;
}

bool DisplayLockContext::IsElementDirtyForLayout() const {
  if (auto* layout_object = element_->GetLayoutObject())
    return layout_object->NeedsLayout() || child_layout_was_blocked_;
  return false;
}

bool DisplayLockContext::IsElementDirtyForPrePaint() const {
  if (auto* layout_object = element_->GetLayoutObject()) {
    auto* layout_box = DynamicTo<LayoutBoxModelObject>(layout_object);
    return PrePaintTreeWalk::ObjectRequiresPrePaint(*layout_object) ||
           PrePaintTreeWalk::ObjectRequiresTreeBuilderContext(*layout_object) ||
           needs_prepaint_subtree_walk_ ||
           needs_effective_allowed_touch_action_update_ ||
           needs_blocking_wheel_event_handler_update_ ||
           needs_compositing_requirements_update_ ||
           (layout_box && layout_box->HasSelfPaintingLayer() &&
            layout_box->Layer()->ChildNeedsCompositingInputsUpdate());
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

  // If we're keeping this context unlocked, update the values.
  if (keep_unlocked_count_) {
    if (--keep_unlocked_count_) {
      ScheduleAnimation();
    } else {
      SetRenderAffectingState(
          RenderAffectingState::kAutoStateUnlockedUntilLifecycle, false);
      UpdateLifecycleNotificationRegistration();
    }
  } else {
    DCHECK(!render_affecting_state_[static_cast<int>(
        RenderAffectingState::kAutoStateUnlockedUntilLifecycle)]);
  }
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
  UpdateActivationObservationIfNeeded();
}

void DisplayLockContext::ElementConnected() {
  UpdateActivationObservationIfNeeded();
  DetermineIfSubtreeHasFocus();
  DetermineIfSubtreeHasSelection();
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
  // Note that if for whatever reason we don't have computed style, then
  // optimistically assume that we have containment.
  if (!style)
    return nullptr;
  if (!style->ContainsStyle() || !style->ContainsLayout())
    return rejection_names::kContainmentNotSatisfied;

  // We allow replaced elements to be locked. This check is similar to the check
  // in DefinitelyNewFormattingContext() in element.cc, but in this case we
  // allow object element to get locked.
  if (IsA<HTMLObjectElement>(*element_) || IsA<HTMLImageElement>(*element_) ||
      element_->IsFormControlElement() || element_->IsMediaElement() ||
      element_->IsFrameOwnerElement() || element_->IsSVGElement()) {
    return nullptr;
  }

  // From https://www.w3.org/TR/css-contain-1/#containment-layout
  // If the element does not generate a principal box (as is the case with
  // display: contents or display: none), or if the element is an internal
  // table element other than display: table-cell, if the element is an
  // internal ruby element, or if the element’s principal box is a
  // non-atomic inline-level box, layout containment has no effect.
  // (Note we're allowing display:none for display locked elements, and a bit
  // more restrictive on ruby - banning <ruby> elements entirely).
  auto* html_element = DynamicTo<HTMLElement>(element_.Get());
  if ((style->IsDisplayTableType() &&
       style->Display() != EDisplay::kTableCell) ||
      (!html_element || IsA<HTMLRubyElement>(html_element)) ||
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
  if (auto* reason = ShouldForceUnlock()) {
    if (IsLocked())
      Unlock();
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

  render_affecting_state_[static_cast<int>(state)] = new_flag;
  NotifyRenderAffectingStateChanged();
}

void DisplayLockContext::NotifyRenderAffectingStateChanged() {
  auto state = [this](RenderAffectingState state) {
    return render_affecting_state_[static_cast<int>(state)];
  };

  // Check that we're visible if and only if lock has not been requested.
  DCHECK(state_ == EContentVisibility::kVisible ||
         state(RenderAffectingState::kLockRequested));
  DCHECK(state_ != EContentVisibility::kVisible ||
         !state(RenderAffectingState::kLockRequested));

  // We should be locked if the lock has been requested (the above DCHECKs
  // verify that this means that we are not 'visible'), and any of the
  // following is true:
  // - We are not in 'auto' mode (meaning 'hidden') or
  // - We are in 'auto' mode and nothing blocks locking: viewport is
  //   not intersecting, subtree doesn't have focus, and subtree doesn't have
  //   selection.
  bool should_be_locked =
      state(RenderAffectingState::kLockRequested) &&
      (state_ != EContentVisibility::kAuto ||
       (!state(RenderAffectingState::kIntersectsViewport) &&
        !state(RenderAffectingState::kSubtreeHasFocus) &&
        !state(RenderAffectingState::kSubtreeHasSelection) &&
        !state(RenderAffectingState::kAutoStateUnlockedUntilLifecycle)));

  if (should_be_locked && !IsLocked())
    Lock();
  else if (!should_be_locked && IsLocked())
    Unlock();
}

void DisplayLockContext::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(document_);
  visitor->Trace(whitespace_reattach_set_);
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

}  // namespace blink

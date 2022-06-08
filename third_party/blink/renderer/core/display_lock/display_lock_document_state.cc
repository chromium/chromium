// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/deferred_shaping.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

DisplayLockDocumentState::DisplayLockDocumentState(Document* document)
    : document_(document) {}

void DisplayLockDocumentState::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(intersection_observer_);
  visitor->Trace(display_lock_contexts_);
  visitor->Trace(forced_node_infos_);
  visitor->Trace(forced_range_infos_);
}

void DisplayLockDocumentState::AddDisplayLockContext(
    DisplayLockContext* context) {
  display_lock_contexts_.insert(context);
  context->SetShouldUnlockAutoForPrint(printing_);
}

void DisplayLockDocumentState::RemoveDisplayLockContext(
    DisplayLockContext* context) {
  display_lock_contexts_.erase(context);
}

int DisplayLockDocumentState::DisplayLockCount() const {
  return display_lock_contexts_.size();
}

void DisplayLockDocumentState::AddLockedDisplayLock() {
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
                    "LockedDisplayLockCount", TRACE_ID_LOCAL(this),
                    locked_display_lock_count_);
  ++locked_display_lock_count_;
  last_lock_update_timestamp_ = base::TimeTicks::Now();
}

void DisplayLockDocumentState::RemoveLockedDisplayLock() {
  DCHECK(locked_display_lock_count_);
  --locked_display_lock_count_;
  last_lock_update_timestamp_ = base::TimeTicks::Now();
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
                    "LockedDisplayLockCount", TRACE_ID_LOCAL(this),
                    locked_display_lock_count_);
}

int DisplayLockDocumentState::LockedDisplayLockCount() const {
  return locked_display_lock_count_;
}

void DisplayLockDocumentState::IncrementDisplayLockBlockingAllActivation() {
  ++display_lock_blocking_all_activation_count_;
}

void DisplayLockDocumentState::DecrementDisplayLockBlockingAllActivation() {
  DCHECK(display_lock_blocking_all_activation_count_);
  --display_lock_blocking_all_activation_count_;
}

int DisplayLockDocumentState::DisplayLockBlockingAllActivationCount() const {
  return display_lock_blocking_all_activation_count_;
}

base::TimeTicks DisplayLockDocumentState::GetLockUpdateTimestamp() {
  return last_lock_update_timestamp_;
}

void DisplayLockDocumentState::RegisterDisplayLockActivationObservation(
    Element* element) {
  EnsureIntersectionObserver().observe(element);
}

void DisplayLockDocumentState::UnregisterDisplayLockActivationObservation(
    Element* element) {
  EnsureIntersectionObserver().unobserve(element);
}

IntersectionObserver& DisplayLockDocumentState::EnsureIntersectionObserver() {
  if (!intersection_observer_) {
    // Use kDeliverDuringPostLayoutSteps method, since we will either notify the
    // display lock synchronously and re-run layout, or delay delivering the
    // signal to the display lock context until the next frame's rAF callbacks
    // have run. This means for the duration of the idle time that follows, we
    // should always have clean layout.
    //
    // Note that we use 150% margin (on the viewport) so that we get the
    // observation before the element enters the viewport.
    //
    // Paint containment requires using the overflow clip edge. To do otherwise
    // results in overflow-clip-margin not being painted in certain scenarios.
    intersection_observer_ = IntersectionObserver::Create(
        {Length::Percent(kViewportMarginPercentage)},
        {std::numeric_limits<float>::min()}, document_,
        WTF::BindRepeating(
            &DisplayLockDocumentState::ProcessDisplayLockActivationObservation,
            WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kDisplayLockIntersectionObserver,
        IntersectionObserver::kDeliverDuringPostLayoutSteps,
        IntersectionObserver::kFractionOfTarget, 0 /* delay */,
        false /* track_visibility */, false /* always report_root_bounds */,
        IntersectionObserver::kApplyMarginToTarget,
        true /* use_overflow_clip_edge */);
  }
  return *intersection_observer_;
}

void DisplayLockDocumentState::ProcessDisplayLockActivationObservation(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(document_);
  DCHECK(document_->View());
  bool had_asynchronous_notifications = false;
  for (auto& entry : entries) {
    auto* context = entry->target()->GetDisplayLockContext();
    DCHECK(context);
    if (context->HadAnyViewportIntersectionNotifications()) {
      if (entry->isIntersecting()) {
        document_->View()->EnqueueStartOfLifecycleTask(
            WTF::Bind(&DisplayLockContext::NotifyIsIntersectingViewport,
                      WrapWeakPersistent(context)));
      } else {
        document_->View()->EnqueueStartOfLifecycleTask(
            WTF::Bind(&DisplayLockContext::NotifyIsNotIntersectingViewport,
                      WrapWeakPersistent(context)));
      }
      had_asynchronous_notifications = true;
    } else {
      if (entry->isIntersecting())
        context->NotifyIsIntersectingViewport();
      else
        context->NotifyIsNotIntersectingViewport();
    }
  }

  // If we had any asynchronous notifications, they would be delivered before
  // the next lifecycle. Ensure to schedule a frame so that this process
  // happens.
  if (had_asynchronous_notifications) {
    // Note that since we're processing this from within the lifecycle, post a
    // task to schedule a new frame (direct call would be ignored inside a
    // lifecycle).
    document_->GetTaskRunner(TaskType::kInternalFrameLifecycleControl)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&DisplayLockDocumentState::ScheduleAnimation,
                             WrapWeakPersistent(this)));
  }
}

void DisplayLockDocumentState::ScheduleAnimation() {
  if (document_ && document_->View())
    document_->View()->ScheduleAnimation();
}

DisplayLockDocumentState::ScopedForceActivatableDisplayLocks
DisplayLockDocumentState::GetScopedForceActivatableLocks() {
  return ScopedForceActivatableDisplayLocks(this);
}

bool DisplayLockDocumentState::HasActivatableLocks() const {
  return LockedDisplayLockCount() != DisplayLockBlockingAllActivationCount();
}

bool DisplayLockDocumentState::ActivatableDisplayLocksForced() const {
  return activatable_display_locks_forced_;
}

void DisplayLockDocumentState::ElementAddedToTopLayer(Element* element) {
  // If flat tree traversal is forbidden, then we need to schedule an event to
  // do this work later.
  if (document_->IsFlatTreeTraversalForbidden() ||
      document_->GetSlotAssignmentEngine().HasPendingSlotAssignmentRecalc()) {
    for (auto context : display_lock_contexts_) {
      // If making every DisplayLockContext check whether its in the top layer
      // is too slow, then we could actually repeat
      // MarkAncestorContextsHaveTopLayerElement in the next frame instead.
      context->ScheduleTopLayerCheck();
    }
    return;
  }

  if (MarkAncestorContextsHaveTopLayerElement(element))
    element->DetachLayoutTree();
}

void DisplayLockDocumentState::ElementRemovedFromTopLayer(Element*) {
  // If flat tree traversal is forbidden, then we need to schedule an event to
  // do this work later.
  if (document_->IsFlatTreeTraversalForbidden() ||
      document_->GetSlotAssignmentEngine().HasPendingSlotAssignmentRecalc()) {
    for (auto context : display_lock_contexts_) {
      // If making every DisplayLockContext check whether its in the top layer
      // is too slow, then we could actually repeat
      // MarkAncestorContextsHaveTopLayerElement in the next frame instead.
      context->ScheduleTopLayerCheck();
    }
    return;
  }

  for (auto context : display_lock_contexts_)
    context->ClearHasTopLayerElement();
  // We don't use the given element here, but rather all elements that are still
  // in the top layer.
  for (auto element : document_->TopLayerElements())
    MarkAncestorContextsHaveTopLayerElement(element.Get());
}

bool DisplayLockDocumentState::MarkAncestorContextsHaveTopLayerElement(
    Element* element) {
  if (display_lock_contexts_.IsEmpty())
    return false;

  bool had_locked_ancestor = false;
  auto* ancestor = element;
  while ((ancestor = FlatTreeTraversal::ParentElement(*ancestor))) {
    if (auto* context = ancestor->GetDisplayLockContext()) {
      context->NotifyHasTopLayerElement();
      had_locked_ancestor |= context->IsLocked();
    }
  }
  return had_locked_ancestor;
}

void DisplayLockDocumentState::NotifySelectionRemoved() {
  for (auto context : display_lock_contexts_)
    context->NotifySubtreeLostSelection();
}

void DisplayLockDocumentState::BeginNodeForcedScope(
    const Node* node,
    bool self_was_forced,
    DisplayLockUtilities::ScopedForcedUpdate::Impl* impl) {
  forced_node_infos_.push_back(ForcedNodeInfo(node, self_was_forced, impl));
}

void DisplayLockDocumentState::BeginRangeForcedScope(
    const Range* range,
    DisplayLockUtilities::ScopedForcedUpdate::Impl* impl) {
  forced_range_infos_.push_back(ForcedRangeInfo(range, impl));
}

void DisplayLockDocumentState::EndForcedScope(
    DisplayLockUtilities::ScopedForcedUpdate::Impl* impl) {
  for (wtf_size_t i = 0; i < forced_node_infos_.size(); ++i) {
    if (forced_node_infos_[i].Chain() == impl) {
      forced_node_infos_.EraseAt(i);
      return;
    }
  }
  for (wtf_size_t i = 0; i < forced_range_infos_.size(); ++i) {
    if (forced_range_infos_[i].Chain() == impl) {
      forced_range_infos_.EraseAt(i);
      return;
    }
  }
  // We should always find a scope to erase.
  NOTREACHED();
}

void DisplayLockDocumentState::EnsureMinimumForcedPhase(
    DisplayLockContext::ForcedPhase phase) {
  for (auto& info : forced_node_infos_)
    info.Chain()->EnsureMinimumForcedPhase(phase);
  for (auto& info : forced_range_infos_)
    info.Chain()->EnsureMinimumForcedPhase(phase);
}

void DisplayLockDocumentState::ForceLockIfNeeded(Element* element) {
  DCHECK(element->GetDisplayLockContext());
  for (ForcedNodeInfo& info : forced_node_infos_)
    info.ForceLockIfNeeded(element);
  for (ForcedRangeInfo& info : forced_range_infos_)
    info.ForceLockIfNeeded(element);
}

void DisplayLockDocumentState::ForcedNodeInfo::ForceLockIfNeeded(
    Element* new_locked_element) {
  auto ancestor_view = self_forced_
                           ? FlatTreeTraversal::InclusiveAncestorsOf(*node_)
                           : FlatTreeTraversal::AncestorsOf(*node_);
  for (Node& ancestor : ancestor_view) {
    if (new_locked_element == &ancestor) {
      chain_->AddForcedUpdateScopeForContext(
          new_locked_element->GetDisplayLockContext());
      break;
    }
  }
}

void DisplayLockDocumentState::ForcedRangeInfo::ForceLockIfNeeded(
    Element* new_locked_element) {
  // TODO(crbug.com/1256849): Combine this with the range loop in
  //   DisplayLockUtilities::ScopedForcedUpdate::Impl::Impl.
  // Ranges use NodeTraversal::Next to go in between their start and end nodes,
  // and will access the layout information of each of those nodes. In order to
  // ensure that each of these nodes has unlocked layout information, we have to
  // do a scoped unlock for each of those nodes by unlocking all of their flat
  // tree ancestors.
  for (Node* node = range_->FirstNode(); node != range_->PastLastNode();
       node = NodeTraversal::Next(*node)) {
    if (node->IsChildOfShadowHost()) {
      // This node may be slotted into another place in the flat tree, so we
      // have to do a flat tree parent traversal for it.
      for (Node* ancestor = node; ancestor;
           ancestor = FlatTreeTraversal::Parent(*ancestor)) {
        if (ancestor == new_locked_element) {
          chain_->AddForcedUpdateScopeForContext(
              new_locked_element->GetDisplayLockContext());
          return;
        }
      }
    } else if (node == new_locked_element) {
      chain_->AddForcedUpdateScopeForContext(
          new_locked_element->GetDisplayLockContext());
      return;
    }
  }
  for (Node* node = range_->FirstNode(); node;
       node = FlatTreeTraversal::Parent(*node)) {
    if (node == new_locked_element) {
      chain_->AddForcedUpdateScopeForContext(
          new_locked_element->GetDisplayLockContext());
      return;
    }
  }
}

// ScopedForcedActivatableDisplayLocks implementation -----------
DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::
    ScopedForceActivatableDisplayLocks(DisplayLockDocumentState* state)
    : state_(state) {
  if (++state_->activatable_display_locks_forced_ == 1) {
    for (auto context : state_->display_lock_contexts_) {
      if (context->HasElement()) {
        context->DidForceActivatableDisplayLocks();
      } else {
        NOTREACHED()
            << "The DisplayLockContext's element has been garbage collected or"
            << " otherwise deleted, but the DisplayLockContext is still alive!"
            << " This shouldn't happen and could cause a crash. See"
            << " crbug.com/1230206";
      }
    }
  }
}

DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::
    ScopedForceActivatableDisplayLocks(
        ScopedForceActivatableDisplayLocks&& other)
    : state_(other.state_) {
  other.state_ = nullptr;
}

DisplayLockDocumentState::ScopedForceActivatableDisplayLocks&
DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::operator=(
    ScopedForceActivatableDisplayLocks&& other) {
  state_ = other.state_;
  other.state_ = nullptr;
  return *this;
}

DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::
    ~ScopedForceActivatableDisplayLocks() {
  if (!state_)
    return;
  DCHECK(state_->activatable_display_locks_forced_);
  --state_->activatable_display_locks_forced_;
}

void DisplayLockDocumentState::NotifyPrintingOrPreviewChanged() {
  bool was_printing = printing_;
  printing_ = document_->IsPrintingOrPaintingPreview();
  if (printing_ == was_printing)
    return;

  for (auto& context : display_lock_contexts_) {
    if (printing_ && context->HasElement() && context->IsShapingDeferred())
      context->SetRequestedState(EContentVisibility::kVisible);
    else
      context->SetShouldUnlockAutoForPrint(printing_);
  }
}

void DisplayLockDocumentState::UnlockShapingDeferredElements() {
  if (!RuntimeEnabledFeatures::DeferredShapingEnabled())
    return;
  if (!HasActivatableLocks())
    return;

  size_t count = 0;
  for (auto& context : display_lock_contexts_) {
    if (context->HasElement() && context->IsShapingDeferred()) {
      context->SetRequestedState(EContentVisibility::kVisible);
      ++count;
    }
  }
  if (count > 0) {
    UseCounter::Count(document_,
                      WebFeature::kDeferredShapingReshapedByForceLayout);
    DEFERRED_SHAPING_VLOG(1) << "Unlocked all " << count << " elements.";
  }
}

void DisplayLockDocumentState::UnlockShapingDeferredElements(
    const Node& target,
    CSSPropertyID property_id) {
  if (!RuntimeEnabledFeatures::DeferredShapingEnabled())
    return;
  if (!HasActivatableLocks())
    return;
  // Need to update layout tree because we access the tree and style.
  target.GetDocument().UpdateStyleAndLayoutTreeForNode(&target);
  if (!HasActivatableLocks())
    return;
  LayoutObject* target_object = target.GetLayoutObject();
  if (!target_object)
    return;

  UnlockShapingDeferredInclusiveDescendants(*target_object);
  if (!HasActivatableLocks())
    return;

  const ComputedStyle& style = target_object->StyleRef();
  switch (property_id) {
    case CSSPropertyID::kTop:
      if (!style.Top().IsFixed())
        UnlockToDetermineHeight(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kBottom:
      if (!style.Bottom().IsFixed())
        UnlockToDetermineHeight(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kLeft:
      if (!style.Left().IsFixed())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kRight:
      if (!style.Right().IsFixed())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kHeight:
      if (!style.Height().IsFixed())
        UnlockToDetermineHeight(*target_object);
      return;
    case CSSPropertyID::kWidth:
      if (!style.Width().IsFixed())
        UnlockToDetermineWidth(*target_object);
      return;

    case CSSPropertyID::kPaddingTop:
      if (!style.PaddingTop().IsFixed())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kPaddingBottom:
      if (!style.PaddingBottom().IsFixed())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kPaddingLeft:
      if (!style.PaddingLeft().IsFixed())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kPaddingRight:
      if (!style.PaddingRight().IsFixed())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      return;

    case CSSPropertyID::kMarginTop:
      if (style.MarginTop().IsPercent())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      else if (style.MarginTop().IsAuto())
        UnlockToDetermineHeight(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kMarginBottom:
      if (style.MarginBottom().IsPercent())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      else if (style.MarginBottom().IsAuto())
        UnlockToDetermineHeight(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kMarginLeft:
      if (style.MarginLeft().IsPercent() || style.MarginLeft().IsAuto())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      return;
    case CSSPropertyID::kMarginRight:
      if (style.MarginRight().IsPercent() || style.MarginRight().IsAuto())
        UnlockToDetermineWidth(*target_object->ContainingBlock());
      return;

    default: {
      LayoutObject* object = target_object;
      while (!object->ContainingBlock()->IsLayoutView())
        object = object->ContainingBlock();
      const ComputedStyle& style = object->StyleRef();
      if (object->IsOutOfFlowPositioned() &&
          (!style.Left().IsAuto() || !style.Right().IsAuto()) &&
          (!style.Top().IsAuto() || !style.Bottom().IsAuto()))
        UnlockShapingDeferredInclusiveDescendants(*object);
      else
        UnlockShapingDeferredElements();
    }
  }
}

void DisplayLockDocumentState::UnlockToDetermineWidth(
    const LayoutObject& object) {
  if (!RuntimeEnabledFeatures::DeferredShapingEnabled())
    return;
  if (!HasActivatableLocks())
    return;

  if (object.IsInline()) {
    UnlockShapingDeferredInclusiveDescendants(*object.ContainingBlock());
    return;
  }

  const ComputedStyle& style = object.StyleRef();
  if (style.BoxSizing() == EBoxSizing::kContentBox) {
    if (style.Width().IsFixed())
      return;
  } else {
    if (style.Width().IsFixed() && style.PaddingLeft().IsFixed() &&
        style.PaddingRight().IsFixed())
      return;
    if ((style.PaddingLeft().IsPercent() || style.PaddingRight().IsPercent()) &&
        object.ContainingBlock()) {
      UnlockToDetermineWidth(*object.ContainingBlock());
      return;
    }
  }
  LayoutBlock* cb = object.ContainingBlock();
  if (style.Left().IsAuto() || style.Right().IsAuto() || !cb) {
    UnlockShapingDeferredInclusiveDescendants(object);
    return;
  }
  UnlockToDetermineWidth(*cb);
}

void DisplayLockDocumentState::UnlockToDetermineHeight(
    const LayoutObject& object) {
  if (!RuntimeEnabledFeatures::DeferredShapingEnabled())
    return;
  if (!HasActivatableLocks())
    return;

  if (object.IsInline()) {
    UnlockShapingDeferredInclusiveDescendants(*object.ContainingBlock());
    return;
  }

  const ComputedStyle& style = object.StyleRef();
  if (style.BoxSizing() == EBoxSizing::kContentBox) {
    if (style.Height().IsFixed())
      return;
  } else {
    if (style.Height().IsFixed() && style.PaddingTop().IsFixed() &&
        style.PaddingBottom().IsFixed())
      return;
    if ((style.PaddingTop().IsPercent() || style.PaddingBottom().IsPercent()) &&
        object.ContainingBlock()) {
      UnlockToDetermineWidth(*object.ContainingBlock());
      if (!HasActivatableLocks())
        return;
    }
  }
  LayoutBlock* cb = object.ContainingBlock();
  if (style.Top().IsAuto() || style.Bottom().IsAuto() || !cb) {
    UnlockShapingDeferredInclusiveDescendants(object);
    return;
  }
  UnlockToDetermineHeight(*cb);
}

void DisplayLockDocumentState::UnlockShapingDeferredInclusiveDescendants(
    const LayoutObject& ancestor) {
  DCHECK(RuntimeEnabledFeatures::DeferredShapingEnabled());
  DCHECK(HasActivatableLocks());

  size_t count = 0;
  for (auto& context : display_lock_contexts_) {
    if (context->IsShapingDeferred() &&
        context->IsInclusiveDescendantOf(ancestor)) {
      context->SetRequestedState(EContentVisibility::kVisible);
      ++count;
    }
  }
  if (count > 0) {
    DEFERRED_SHAPING_VLOG(1)
        << "Partially unlocked " << count << " elements ==> remaining="
        << (LockedDisplayLockCount() - DisplayLockBlockingAllActivationCount());
  }
}

}  // namespace blink

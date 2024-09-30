// Copyright 2020 The Chromium Authors
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
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"

namespace {

const char kForcedRendering[] =
    "Rendering was performed in a subtree hidden by content-visibility.";
const char kForcedRenderingMax[] =
    "Rendering was performed in a subtree hidden by content-visibility. "
    "Further messages will be suppressed.";
constexpr unsigned kMaxConsoleMessages = 500;

}  // namespace

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
        *document_,
        WTF::BindRepeating(
            &DisplayLockDocumentState::ProcessDisplayLockActivationObservation,
            WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kDisplayLockIntersectionObserver,
        IntersectionObserver::Params{
            .margin = {Length::Percent(kViewportMarginPercentage)},
            .margin_target = IntersectionObserver::kApplyMarginToTarget,
            .thresholds = {std::numeric_limits<float>::min()},
            .behavior = IntersectionObserver::kDeliverDuringPostLayoutSteps,
            .use_overflow_clip_edge = true,
        });
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
            WTF::BindOnce(&DisplayLockContext::NotifyIsIntersectingViewport,
                          WrapWeakPersistent(context)));
      } else {
        document_->View()->EnqueueStartOfLifecycleTask(
            WTF::BindOnce(&DisplayLockContext::NotifyIsNotIntersectingViewport,
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
                   WTF::BindOnce(&DisplayLockDocumentState::ScheduleAnimation,
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

  if (MarkAncestorContextsHaveTopLayerElement(element)) {
    StyleEngine& style_engine = document_->GetStyleEngine();
    StyleEngine::DetachLayoutTreeScope detach_scope(style_engine);
    element->DetachLayoutTree();
  }
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
  if (display_lock_contexts_.empty())
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

void DisplayLockDocumentState::NotifyViewTransitionPseudoTreeChanged() {
  // Reset the view transition element flag.
  // TODO(vmpstr): This should be optimized to keep track of elements that
  // actually have this flag set.
  for (auto context : display_lock_contexts_)
    context->ResetDescendantIsViewTransitionElement();

  // Process the view transition elements to check if their ancestors are
  // locks that need to be made relevant.
  UpdateViewTransitionElementAncestorLocks();
}

void DisplayLockDocumentState::UpdateViewTransitionElementAncestorLocks() {
  auto* transition = ViewTransitionUtils::GetTransition(*document_);
  if (!transition)
    return;

  const auto& transitioning_elements = transition->GetTransitioningElements();
  for (auto element : transitioning_elements) {
    auto* ancestor = element.Get();
    // When the element which has c-v:auto is itself a view transition element,
    // we keep it locked. So start with the parent.
    while ((ancestor = FlatTreeTraversal::ParentElement(*ancestor))) {
      if (auto* context = ancestor->GetDisplayLockContext())
        context->SetDescendantIsViewTransitionElement();
    }
  }
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
  NOTREACHED_IN_MIGRATION();
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
        // This used to be a DUMP_WILL_BE_NOTREACHED(), but the crash volume was
        // too high. See crbug.com/41494130
        DCHECK(false)
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

  for (auto& context : display_lock_contexts_)
    context->SetShouldUnlockAutoForPrint(printing_);
}

void DisplayLockDocumentState::IssueForcedRenderWarning(Element* element) {
  // Note that this is a verbose level message, since it can happen
  // frequently and is not necessarily a problem if the developer is
  // accessing content-visibility: hidden subtrees intentionally.
  if (forced_render_warnings_ < kMaxConsoleMessages) {
    forced_render_warnings_++;
    auto level =
        RuntimeEnabledFeatures::WarnOnContentVisibilityRenderAccessEnabled()
            ? mojom::blink::ConsoleMessageLevel::kWarning
            : mojom::blink::ConsoleMessageLevel::kVerbose;
    element->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript, level,
        forced_render_warnings_ == kMaxConsoleMessages ? kForcedRenderingMax
                                                       : kForcedRendering);
  }
}

}  // namespace blink

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"

namespace blink {

DisplayLockDocumentState::DisplayLockDocumentState(Document* document)
    : document_(document) {}

void DisplayLockDocumentState::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(intersection_observer_);
  visitor->Trace(display_lock_contexts_);
  visitor->Trace(forced_node_info_);
}

void DisplayLockDocumentState::AddDisplayLockContext(
    DisplayLockContext* context) {
  display_lock_contexts_.insert(context);
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
}

void DisplayLockDocumentState::RemoveLockedDisplayLock() {
  DCHECK(locked_display_lock_count_);
  --locked_display_lock_count_;
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
    intersection_observer_ = IntersectionObserver::Create(
        {Length::Percent(150.f)}, {std::numeric_limits<float>::min()},
        document_,
        WTF::BindRepeating(
            &DisplayLockDocumentState::ProcessDisplayLockActivationObservation,
            WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kDisplayLockIntersectionObserver,
        IntersectionObserver::kDeliverDuringPostLayoutSteps,
        IntersectionObserver::kFractionOfTarget, 0 /* delay */,
        false /* track_visibility */, false /* always report_root_bounds */,
        IntersectionObserver::kApplyMarginToTarget);
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

bool DisplayLockDocumentState::ActivatableDisplayLocksForced() const {
  return activatable_display_locks_forced_;
}

void DisplayLockDocumentState::NotifySelectionRemoved() {
  for (auto context : display_lock_contexts_)
    context->NotifySubtreeLostSelection();
}

void DisplayLockDocumentState::BeginNodeForcedScope(
    const Node* node,
    bool self_was_forced,
    DisplayLockUtilities::ScopedForcedUpdate::Impl* impl) {
  forced_node_info_.push_back(ForcedNodeInfo(node, self_was_forced, impl));
}

void DisplayLockDocumentState::EndNodeForcedScope(
    DisplayLockUtilities::ScopedForcedUpdate::Impl* impl) {
  for (wtf_size_t i = 0; i < forced_node_info_.size(); ++i) {
    if (forced_node_info_[i].chain == impl) {
      forced_node_info_.EraseAt(i);
      return;
    }
  }
  // We should always find a scope to erase.
  NOTREACHED();
}

void DisplayLockDocumentState::ForceLockIfNeeded(Element* element) {
  DCHECK(element->GetDisplayLockContext());
  for (wtf_size_t i = 0; i < forced_node_info_.size(); ++i)
    ForceLockIfNeededForInfo(element, &forced_node_info_[i]);
}

void DisplayLockDocumentState::ForceLockIfNeededForInfo(
    Element* element,
    ForcedNodeInfo* forced_node_info) {
  auto ancestor_view =
      forced_node_info->self_forced
          ? FlatTreeTraversal::InclusiveAncestorsOf(*forced_node_info->node)
          : FlatTreeTraversal::AncestorsOf(*forced_node_info->node);
  for (Node& ancestor : ancestor_view) {
    if (element == &ancestor) {
      forced_node_info->chain->AddForcedUpdateScopeForContext(
          element->GetDisplayLockContext());
      break;
    }
  }
}

// ScopedForcedActivatableDisplayLocks implementation -----------
DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::
    ScopedForceActivatableDisplayLocks(DisplayLockDocumentState* state)
    : state_(state) {
  if (++state_->activatable_display_locks_forced_ == 1) {
    for (auto context : state_->display_lock_contexts_)
      context->DidForceActivatableDisplayLocks();
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

}  // namespace blink

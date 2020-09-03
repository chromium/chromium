// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class Element;
class StyleRecalcChange;

enum class DisplayLockActivationReason {
  // Accessibility driven activation
  kAccessibility = 1 << 0,
  // Activation as a result of find-in-page
  kFindInPage = 1 << 1,
  // Fragment link navigation
  kFragmentNavigation = 1 << 2,
  // Script invoked focus().
  kScriptFocus = 1 << 3,
  // scrollIntoView()
  kScrollIntoView = 1 << 4,
  // User / script selection
  kSelection = 1 << 5,
  // Simulated click (Node::DispatchSimulatedClick)
  kSimulatedClick = 1 << 6,
  // User focus (e.g. tab navigation)
  kUserFocus = 1 << 7,
  // Intersection observer activation
  kViewportIntersection = 1 << 8,

  // Shorthands
  kViewport = static_cast<uint16_t>(kSelection) |
              static_cast<uint16_t>(kUserFocus) |
              static_cast<uint16_t>(kViewportIntersection) |
              static_cast<uint16_t>(kAccessibility),
  kAny = static_cast<uint16_t>(kAccessibility) |
         static_cast<uint16_t>(kFindInPage) |
         static_cast<uint16_t>(kFragmentNavigation) |
         static_cast<uint16_t>(kScriptFocus) |
         static_cast<uint16_t>(kScrollIntoView) |
         static_cast<uint16_t>(kSelection) |
         static_cast<uint16_t>(kSimulatedClick) |
         static_cast<uint16_t>(kUserFocus) |
         static_cast<uint16_t>(kViewportIntersection)
};

// Instead of specifying an underlying type, which would propagate throughout
// forward declarations, we static assert that the activation reasons enum is
// small-ish.
static_assert(static_cast<uint32_t>(DisplayLockActivationReason::kAny) <
                  std::numeric_limits<uint16_t>::max(),
              "DisplayLockActivationReason is too large");

class CORE_EXPORT DisplayLockContext final
    : public GarbageCollected<DisplayLockContext>,
      public LocalFrameView::LifecycleNotificationObserver {
 public:
  // The type of style that was blocked by this display lock.
  enum StyleType {
    kStyleUpdateNotRequired,
    kStyleUpdateSelf,
    kStyleUpdatePseudoElements,
    kStyleUpdateChildren,
    kStyleUpdateDescendants
  };

  explicit DisplayLockContext(Element*);
  ~DisplayLockContext() = default;

  // Called by style to update the current state of content-visibility.
  void SetRequestedState(EContentVisibility state);
  // Called by style to adjust the element's style based on the current state.
  void AdjustElementStyle(ComputedStyle* style) const;

  // Is called by the intersection observer callback to inform us of the
  // intersection state.
  void NotifyIsIntersectingViewport();
  void NotifyIsNotIntersectingViewport();

  // Lifecycle state functions.
  bool ShouldStyleChildren() const;
  void DidStyleSelf();
  void DidStyleChildren();
  bool ShouldLayoutChildren() const;
  void DidLayoutChildren();
  bool ShouldPrePaintChildren() const;
  bool ShouldPaintChildren() const;

  // Returns true if the last style recalc traversal was blocked at this
  // element, either for itself, its children or its descendants.
  bool StyleTraversalWasBlocked() {
    return blocked_style_traversal_type_ != kStyleUpdateNotRequired;
  }

  // Returns true if the contents of the associated element should be visible
  // from and activatable by a specified reason. Note that passing
  // kAny will return true if the lock is activatable for any
  // reason.
  bool IsActivatable(DisplayLockActivationReason reason) const;

  // Trigger commit because of activation from tab order, url fragment,
  // find-in-page, scrolling, etc.
  // This issues a before activate signal with the given element as the
  // activated element.
  // The reason is specified for metrics.
  void CommitForActivationWithSignal(Element* activated_element,
                                     DisplayLockActivationReason reason);

  bool ShouldCommitForActivation(DisplayLockActivationReason reason) const;

  // Returns true if this context is locked.
  bool IsLocked() const { return is_locked_; }

  EContentVisibility GetState() { return state_; }

  bool UpdateForced() const { return update_forced_; }

  // This is called when the element with which this context is associated is
  // moved to a new document. Used to listen to the lifecycle update from the
  // right document's view.
  void DidMoveToNewDocument(Document& old_document);

  void AddToWhitespaceReattachSet(Element& element);

  // LifecycleNotificationObserver overrides.
  void WillStartLifecycleUpdate(const LocalFrameView&) override;

  // Inform the display lock that it prevented a style change. This is used to
  // invalidate style when we need to update it in the future.
  void NotifyStyleRecalcWasBlocked(StyleType type) {
    blocked_style_traversal_type_ =
        std::max(blocked_style_traversal_type_, type);
  }

  void NotifyReattachLayoutTreeWasBlocked() {
    reattach_layout_tree_was_blocked_ = true;
  }

  void NotifyChildLayoutWasBlocked() { child_layout_was_blocked_ = true; }

  // Inform the display lock that it needs a graphics layer collection when it
  // needs to paint.
  void NotifyNeedsGraphicsLayerCollection() {
    needs_graphics_layer_collection_ = true;
  }

  void NotifyCompositingRequirementsUpdateWasBlocked() {
    needs_compositing_requirements_update_ = true;
  }
  void NotifyCompositingDescendantDependentFlagUpdateWasBlocked() {
    needs_compositing_dependent_flag_update_ = true;
  }

  void NotifyGraphicsLayerRebuildBlocked() {
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    needs_graphics_layer_rebuild_ = true;
  }

  // Notify this element will be disconnected.
  void NotifyWillDisconnect();

  // Called when the element disconnects or connects.
  void ElementDisconnected();
  void ElementConnected();

  void NotifySubtreeLostFocus();
  void NotifySubtreeGainedFocus();

  void NotifySubtreeLostSelection();
  void NotifySubtreeGainedSelection();

  void SetNeedsPrePaintSubtreeWalk(
      bool needs_effective_allowed_touch_action_update) {
    needs_effective_allowed_touch_action_update_ =
        needs_effective_allowed_touch_action_update;
    needs_prepaint_subtree_walk_ = true;
  }

  // This is called by the style recalc code in lieu of
  // MarkForStyleRecalcIfNeeded() in order to adjust the child change if we need
  // to recalc children nodes here.
  StyleRecalcChange AdjustStyleRecalcChangeForChildren(
      StyleRecalcChange change);

  void DidForceActivatableDisplayLocks() {
    if (IsLocked() && IsActivatable(DisplayLockActivationReason::kAny)) {
      MarkForStyleRecalcIfNeeded();
      MarkForLayoutIfNeeded();
    }
  }

  bool HadAnyViewportIntersectionNotifications() const {
    return had_any_viewport_intersection_notifications_;
  }

  // GC functions.
  void Trace(Visitor*) const override;

  // Debugging functions.
  String RenderAffectingStateToString() const;

 private:
  // Give access to |NotifyForcedUpdateScopeStarted()| and
  // |NotifyForcedUpdateScopeEnded()|.
  friend class DisplayLockUtilities;

  // Test friends.
  friend class DisplayLockContextRenderingTest;
  friend class DisplayLockContextTest;

  // Request that this context be locked. Called when style determines that the
  // subtree rooted at this element should be skipped, unless things like
  // viewport intersection prevent it from doing so.
  void RequestLock(uint16_t activation_mask);
  // Request that this context be unlocked. Called when style determines that
  // the subtree rooted at this element should be rendered.
  void RequestUnlock();

  // Called in |DisplayLockUtilities| to notify the state of scope.
  void NotifyForcedUpdateScopeStarted();
  void NotifyForcedUpdateScopeEnded();

  // Records the locked context counts on the document as well as context that
  // block all activation.
  void UpdateDocumentBookkeeping(bool was_locked,
                                 bool all_activation_was_blocked,
                                 bool is_locked,
                                 bool all_activation_is_blocked);

  // Set which reasons activate, as a mask of DisplayLockActivationReason enums.
  void UpdateActivationMask(uint16_t activatable_mask);

  // Clear the activated flag.
  void ResetActivation();

  // Marks ancestors of elements in |whitespace_reattach_set_| with
  // ChildNeedsReattachLayoutTree and clears the set.
  void MarkElementsForWhitespaceReattachment();

  // The following functions propagate dirty bits from the locked element up to
  // the ancestors in order to be reached, and update dirty bits for the element
  // as well if needed. They return true if the element or its subtree were
  // dirty, and false otherwise.
  bool MarkForStyleRecalcIfNeeded();
  bool MarkForLayoutIfNeeded();
  bool MarkAncestorsForPrePaintIfNeeded();
  bool MarkPaintLayerNeedsRepaint();
  bool MarkForCompositingUpdatesIfNeeded();

  bool IsElementDirtyForStyleRecalc() const;
  bool IsElementDirtyForLayout() const;
  bool IsElementDirtyForPrePaint() const;

  // Helper to schedule an animation to delay lifecycle updates for the next
  // frame.
  void ScheduleAnimation();

  // Checks whether we should force unlock the lock (due to not meeting
  // containment/display requirements), returns a string from rejection_names
  // if we should, nullptr if not. Note that this can only be called if the
  // style is clean. It checks the layout object if it exists. Otherwise,
  // falls back to checking computed style.
  const char* ShouldForceUnlock() const;

  // Unlocks the lock if the element doesn't meet requirements
  // (containment/display type). Returns true if we did unlock.
  bool ForceUnlockIfNeeded();

  // Returns true if the element is connected to a document that has a view.
  // If we're not connected,  or if we're connected but the document doesn't
  // have a view (e.g. templates) we shouldn't do style calculations etc and
  // when acquiring this lock should immediately resolve the acquire promise.
  bool ConnectedToView() const;

  // Registers or unregisters the element for intersection observations in the
  // document. This is used to activate on visibily changes. This can be safely
  // called even if changes are not required, since it will only act if a
  // register/unregister is required.
  void UpdateActivationObservationIfNeeded();

  // Determines whether or not we need lifecycle notifications.
  bool NeedsLifecycleNotifications() const;
  // Updates the lifecycle notification registration based on whether we need
  // the notifications.
  void UpdateLifecycleNotificationRegistration();

  // Locks the context.
  void Lock();
  // Unlocks the context.
  void Unlock();

  // Determines if the subtree has focus. This is a linear walk from the focused
  // element to its root element.
  void DetermineIfSubtreeHasFocus();

  // Determines if the subtree has selection. This will walk from each of the
  // selected notes up to its root looking for `element_`.
  void DetermineIfSubtreeHasSelection();

  // Keep this context unlocked until the beginning of lifecycle. Effectively
  // keeps this context unlocked for the next `count` frames. It also schedules
  // a frame to ensure the lifecycle happens. Only affects locks with 'auto'
  // setting.
  void SetKeepUnlockedUntilLifecycleCount(int count);

  WeakMember<Element> element_;
  WeakMember<Document> document_;
  EContentVisibility state_ = EContentVisibility::kVisible;

  // See StyleEngine's |whitespace_reattach_set_|.
  // Set of elements that had at least one rendered children removed
  // since its last lifecycle update. For such elements that are located
  // in a locked subtree, we save it here instead of the global set in
  // StyleEngine because we don't want to accidentally mark elements
  // in a locked subtree for layout tree reattachment before we did
  // style recalc on them.
  HeapHashSet<Member<Element>> whitespace_reattach_set_;

  // If non-zero, then the update has been forced.
  int update_forced_ = 0;

  StyleType blocked_style_traversal_type_ = kStyleUpdateNotRequired;
  // Signifies whether we've blocked a layout tree reattachment on |element_|'s
  // descendants or not, so that we can mark |element_| for reattachment when
  // needed.
  bool reattach_layout_tree_was_blocked_ = false;

  bool needs_effective_allowed_touch_action_update_ = false;
  bool needs_prepaint_subtree_walk_ = false;
  bool needs_graphics_layer_collection_ = false;
  bool needs_compositing_requirements_update_ = false;
  bool needs_compositing_dependent_flag_update_ = false;

  // Will be true if child traversal was blocked on a previous layout run on the
  // locked element. We need to keep track of this to ensure that on the next
  // layout run where the descendants of the locked element are allowed to be
  // traversed into, we will traverse to the children of the locked element.
  bool child_layout_was_blocked_ = false;

  // Tracks whether the element associated with this lock is being tracked by a
  // document level intersection observer.
  bool is_observed_ = false;

  uint16_t activatable_mask_ =
      static_cast<uint16_t>(DisplayLockActivationReason::kAny);

  // Is set to true if we are registered for lifecycle notifications.
  bool is_registered_for_lifecycle_notifications_ = false;

  // This is set to true when we have delayed locking ourselves due to viewport
  // intersection (or lack thereof) because we were nested in a locked subtree.
  // In that case, we register for lifecycle notifications and check every time
  // if we are still nested.
  bool needs_deferred_not_intersecting_signal_ = false;

  // Lock has been requested.
  bool is_locked_ = false;

  // If true, this lock is kept unlocked at least until the beginning of the
  // lifecycle. If nothing else is keeping it unlocked, then it will be locked
  // again at the start of the lifecycle.
  bool keep_unlocked_until_lifecycle_ = false;

  bool needs_graphics_layer_rebuild_ = false;

  // This is set to true if we're in the 'auto' mode and had our first
  // intersection / non-intersection notification. This is reset to false if the
  // 'auto' mode is added again (after being removed).
  bool had_any_viewport_intersection_notifications_ = false;

  enum class RenderAffectingState : int {
    kLockRequested,
    kIntersectsViewport,
    kSubtreeHasFocus,
    kSubtreeHasSelection,
    kAutoStateUnlockedUntilLifecycle,
    kNumRenderAffectingStates
  };
  void SetRenderAffectingState(RenderAffectingState state, bool flag);
  void NotifyRenderAffectingStateChanged();
  const char* RenderAffectingStateName(int state) const;

  bool render_affecting_state_[static_cast<int>(
      RenderAffectingState::kNumRenderAffectingStates)] = {false};
  int keep_unlocked_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_

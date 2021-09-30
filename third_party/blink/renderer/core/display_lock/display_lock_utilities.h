// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Static utility class for display-locking related helpers.
class CORE_EXPORT DisplayLockUtilities {
  STATIC_ONLY(DisplayLockUtilities);

 public:
  // This class forces updates on display locks from the given node up the
  // ancestor chain until the local frame root.
  class CORE_EXPORT ScopedForcedUpdate {
    STACK_ALLOCATED();

   public:
    ScopedForcedUpdate(ScopedForcedUpdate&& other) : impl_(other.impl_) {
      other.impl_ = nullptr;
    }
    ScopedForcedUpdate& operator=(ScopedForcedUpdate&& other) {
      impl_ = other.impl_;
      other.impl_ = nullptr;
      return *this;
    }
    ~ScopedForcedUpdate() {
      if (impl_)
        impl_->Destroy();
    }

   private:
    // It is important not to create multiple ScopedChainForcedUpdate scopes.
    // The following functions update some combination of Style, Layout, Paint
    // information after forcing the display locks. It should be enough to use
    // one of the following functions instead of forcing the scope manually.
    friend void Document::UpdateStyleAndLayoutForNode(
        const Node* node,
        DocumentUpdateReason reason);
    friend void Document::UpdateStyleAndLayoutTreeForNode(const Node*);
    friend void Document::UpdateStyleAndLayoutTreeForSubtree(const Node* node);
    friend void Document::EnsurePaintLocationDataValidForNode(
        const Node* node,
        DocumentUpdateReason reason);
    friend VisibleSelection
    FrameSelection::ComputeVisibleSelectionInDOMTreeDeprecated() const;
    friend FloatRect Range::BoundingRect() const;
    friend DOMRectList* Range::getClientRects() const;

    friend class DisplayLockContext;

    // Test friends.
    friend class DisplayLockContextRenderingTest;

    explicit ScopedForcedUpdate(const Node* node,
                                DisplayLockContext::ForcedPhase phase,
                                bool include_self = false)
        : impl_(MakeGarbageCollected<Impl>(node, phase, include_self)) {}
    explicit ScopedForcedUpdate(const Range* range,
                                DisplayLockContext::ForcedPhase phase)
        : impl_(MakeGarbageCollected<Impl>(range, phase)) {}

    friend class DisplayLockDocumentState;

    class CORE_EXPORT Impl final : public GarbageCollected<Impl> {
     public:
      Impl(const Node* node,
           DisplayLockContext::ForcedPhase phase,
           bool include_self = false);
      Impl(const Range* range, DisplayLockContext::ForcedPhase phase);

      // Adds another display-lock scope to this chain. Added when a new lock is
      // created in the ancestor chain of this chain's node.
      void AddForcedUpdateScopeForContext(DisplayLockContext*);

      void Destroy();

      void Trace(Visitor* visitor) const {
        visitor->Trace(node_);
        visitor->Trace(forced_context_set_);
        visitor->Trace(parent_frame_impl_);
      }

     private:
      Member<const Node> node_;
      DisplayLockContext::ForcedPhase phase_;
      HeapHashSet<Member<DisplayLockContext>> forced_context_set_;
      Member<Impl> parent_frame_impl_;
    };

    Impl* impl_ = nullptr;
  };

  // Activates all the nodes within a find-in-page match |range|.
  // Returns true if at least one node gets activated.
  // See: http://bit.ly/2RXULVi, "beforeactivate Event" part.
  static bool ActivateFindInPageMatchRangeIfNeeded(
      const EphemeralRangeInFlatTree& range);

  // Returns activatable-locked inclusive ancestors of |node|.
  // Note that this function will return an empty list if |node| is inside a
  // non-activatable locked subtree (e.g. at least one ancestor is not
  // activatable-locked).
  static const HeapVector<Member<Element>> ActivatableLockedInclusiveAncestors(
      const Node& node,
      DisplayLockActivationReason reason);

  // Returns the nearest inclusive ancestor of |element| that has
  // content-visibility: hidden-matchable.
  // TODO(crbug.com/1249939): Remove this.
  static Element* NearestHiddenMatchableInclusiveAncestor(Element& element);

  // Ancestor navigation functions.

  // Helpers for ancestor navigation to find locks.
  static const Element* LockedInclusiveAncestorPreventingLayout(
      const Node& node);
  static const Element* LockedInclusiveAncestorPreventingPaint(
      const LayoutObject& object);
  static const Element* LockedInclusiveAncestorPreventingPaint(
      const Node& node);

  // The following don't consider the passed argument as a valid lock (i.e. they
  // are exclusive checks).
  static Element* LockedAncestorPreventingLayout(const LayoutObject& object);
  static Element* LockedAncestorPreventingLayout(const Node& node);
  static Element* LockedAncestorPreventingPaint(const LayoutObject& object);
  static Element* LockedAncestorPreventingPaint(const Node& node);
  static Element* LockedAncestorPreventingPrePaint(const LayoutObject& object);
  static Element* LockedAncestorPreventingStyle(const Node& element);

  // Returns the nearest inclusive ancestor of |node| that is display locked
  // and blocks style & layout tree building within the same TreeScope as
  // |node|, meaning that no flat tree traversals are made.
  static Element* LockedInclusiveAncestorPreventingStyleWithinTreeScope(
      const Node& node);

  // Returns the highest exclusive ancestor of |node| that is display locked.
  // Note that this function crosses local frames.
  static Element* HighestLockedExclusiveAncestor(const Node& node);
  static Element* HighestLockedInclusiveAncestor(const Node& node);

  // Returns true if |node| is not in a locked subtree, or if it's possible to
  // activate all of the locked ancestors for |activation_reason|.
  static bool IsInUnlockedOrActivatableSubtree(
      const Node& node,
      DisplayLockActivationReason activation_reason =
          DisplayLockActivationReason::kAny);

  // Returns true if |node| is in a locked subtree, and at least one of its
  // locked ancestors can't be activated with |activation_reason|. In other
  // words, this node should be treated as if it's not in the tree for
  // |activation_reason|.
  static bool ShouldIgnoreNodeDueToDisplayLock(
      const Node& node,
      DisplayLockActivationReason activation_reason) {
    return !IsInUnlockedOrActivatableSubtree(node, activation_reason);
  }

  // Returns true if the element is in a locked subtree (or is self-locked with
  // no self-updates). This crosses frames while navigating the ancestor chain.
  static bool IsInLockedSubtreeCrossingFrames(
      const Node& node,
      IncludeSelfOrNot self = kExcludeSelf);

  // Called when the focused element changes. These functions update locks to
  // ensure that focused element ancestors remain unlocked for 'auto' state.
  static void ElementLostFocus(Element*);
  static void ElementGainedFocus(Element*);

  static void SelectionChanged(const EphemeralRangeInFlatTree& old_selection,
                               const EphemeralRangeInFlatTree& new_selection);
  static void SelectionRemovedFromDocument(Document& document);

  static bool PrePaintBlockedInParentFrame(LayoutView* layout_view);

  static bool IsAutoWithoutLayout(const LayoutObject& object);

  // Walks up the ancestor chain and expands all elements with the
  // hidden=until-found attribute found along by removing the hidden attribute.
  // If any were expanded, returns true.
  // This method may run script because of the mutation events fired when
  // removing the hidden attribute.
  static bool RevealHiddenUntilFoundAncestors(const Node&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_

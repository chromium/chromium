// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_H_

#include "third_party/blink/renderer/core/dom/tree_ordered_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class AXObjectCache;
class HTMLSlotElement;
class Node;
class ShadowRoot;

// Slot assignment for one shadow root.
//
// Dirty/clean model
// -----------------
// DOM mutations do not recompute slot assignment. They only mark the shadow
// root dirty (SetNeedsAssignmentRecalc()), and record it in the document's
// SlotAssignmentEngine. The assignment data (HTMLSlotElement's assigned nodes
// and flat tree children, and each slotted node's FlatTreeNodeData) is
// recomputed lazily, by RecalcAssignment(), either:
//  - from the document lifecycle (SlotAssignmentEngine::
//    RecalcSlotAssignments(), before style recalc), or
//  - from a lazy entry point that needs clean data: HTMLSlotElement::
//    AssignedNodes(), FlatTreeTraversal::TraverseSiblingsForHostChild() (and
//    so all of FlatTreeTraversal), and Node::AssignedSlot().
//
// A recalc always covers every dirty shadow root that a side effect can read
// (the "recalc list"): every dirty connected shadow root in the document, or,
// for a disconnected shadow root, every dirty shadow root with the same
// shadow-including root. It runs in two passes:
//  - Pass 1 computes the assignment data for every root in the list, with no
//    side effects. Flat tree traversal is forbidden. Computing one root's
//    assignment only reads its host's children, its slots, and those slots'
//    children, so no other root's flat tree data is needed.
//  - Pass 2 applies the side effects (style and layout tree invalidation,
//    canvas subtree propagation, accessibility, dir=auto). Every root in the
//    list is clean, so flat tree reads, including the *NoRecalc accessors,
//    are accurate.
//
// Rules
// -----
// R1. No script and no event dispatch during recalc. This covers synchronous
//     DispatchEvent(), DispatchScopedEvent() outside a queue scope,
//     focus/blur, and microtask checkpoints.
// R2. No DOM mutation during recalc: child-list, attribute, and
//     character-data changes, including in UA shadow trees and in other
//     documents.
// R3. No focus change during recalc, even without script, because
//     FocusedElementChangeObservers run.
// R4. Recalc does not dirty slot assignment and does not re-enter recalc.
// R5. Recalc is never triggered while the tree is unstable: in the middle of
//     a DOM mutation, during layout-tree detach, or during style, layout, or
//     paint (see SlotAssignmentRecalcForbiddenScope).
// R6. No partial staleness. Side effects run only after every dirty shadow
//     root they can reach has its assignment data recomputed.
// R7. The flat tree is not read while assignment data is being recomputed
//     (pass 1, where flat tree traversal is forbidden), because that data is
//     only partly updated.
//
// R1-R5 and R7 are checked in release builds by
// SlotAssignmentRecalcInProgressScope, behind the
// EnforceSlotAssignmentInvariants feature. R6 is checked indirectly: a side
// effect that reads a dirty shadow root triggers a re-entrant recalc (R4), and
// reading partly updated data violates R7.
//
// Recalc hooks
// ------------
// Hooks reachable from pass 2 (Node::FlatTreeParentChanged(),
// Node::RemovedFromFlatTree(), Element::DidChangeIsInCanvasSubtree()) and
// from pass 1 (Element::ManuallyAssignSlots()) must follow these rules. In
// practice, they may only mark style or layout-tree dirty, detach layout
// trees, update node flags, notify accessibility (without traversing dirty
// data), and queue work for later. Anything else must be posted as a task,
// or suppressed, by the code that wants it.
//
// Stale accessors
// ---------------
// The *NoRecalc and *WithoutRecalc accessors read cached assignment data
// without recalculating it, so they may return stale data. They are allowed
// only where documented, mostly mutation-time style dirty-marking
// (Node::FlatTreeParentForChildDirty()) and canvas subtree propagation.
class SlotAssignment final : public GarbageCollected<SlotAssignment> {
 public:
  explicit SlotAssignment(ShadowRoot& owner);

  // Relevant DOM Standard: https://dom.spec.whatwg.org/#find-a-slot
  HTMLSlotElement* FindSlot(const Node&);
  HTMLSlotElement* FindSlotByName(const AtomicString& slot_name) const;

  // DOM Standaard defines these two procedures:
  // 1. https://dom.spec.whatwg.org/#assign-a-slot
  //    void assignSlot(const Node& slottable);
  // 2. https://dom.spec.whatwg.org/#assign-slotables
  //    void assignSlotables(HTMLSlotElement&);
  // As an optimization, Blink does not implement these literally.
  // Instead, provide alternative, HTMLSlotElement::hasAssignedNodesSlow()
  // so that slotchange can be detected.

  HeapVector<Member<HTMLSlotElement>>& Slots();

  void DidAddSlot(HTMLSlotElement&);
  void DidRemoveSlot(HTMLSlotElement&);
  void DidRenameSlot(const AtomicString& old_name, HTMLSlotElement&);
  void DidChangeHostChildSlotName(const AtomicString& old_value,
                                  const AtomicString& new_value);

  bool FindHostChildBySlotName(const AtomicString& slot_name) const;

  void Trace(Visitor*) const;

  bool NeedsAssignmentRecalc() const { return needs_assignment_recalc_; }
  void SetNeedsAssignmentRecalc();

  // Lazy entry point: if this shadow root's assignment is dirty, recalculates
  // its recalc list.
  void RecalcAssignment();

  // Recalculates the assignment of the shadow roots in `roots`, in two
  // passes. All of `roots` must be dirty and belong to the same document.
  static void RecalcShadowRoots(const HeapVector<Member<ShadowRoot>>& roots);

 private:
  enum class SlotMutationType {
    kRemoved,
    kRenamed,
  };

  class RecalcSideEffects;

  HTMLSlotElement* FindSlotInManualSlotting(Node&);

  void CollectSlots();
  HTMLSlotElement* GetCachedFirstSlotWithoutAccessingNodeTree(
      const AtomicString& slot_name);

  void DidAddSlotInternal(HTMLSlotElement&);
  void DidRemoveSlotInternal(HTMLSlotElement&,
                             const AtomicString& slot_name,
                             SlotMutationType);

  // Pass 1: computes this shadow root's assignment data, without side
  // effects. Returns the side effects to apply in pass 2.
  RecalcSideEffects* ComputeAssignment();
  // Pass 2.
  static void ApplySideEffects(RecalcSideEffects&, AXObjectCache*);

  HeapVector<Member<HTMLSlotElement>> slots_;
  Member<TreeOrderedMap> slot_map_;
  WeakMember<ShadowRoot> owner_;
  unsigned needs_collect_slots_ : 1;
  unsigned needs_assignment_recalc_ : 1;
  unsigned slot_count_ : 30;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_H_

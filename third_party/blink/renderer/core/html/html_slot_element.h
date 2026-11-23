/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SLOT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SLOT_ELEMENT_H_

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_text.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"

namespace blink {

class AssignedNodesOptions;

// The side effects of recomputing one slot's assigned nodes, recorded during
// pass 1 of slot assignment recalc and applied during pass 2. See
// SlotAssignment.
class SlotRecalcSideEffects final
    : public GarbageCollected<SlotRecalcSideEffects> {
 public:
  void Trace(Visitor* visitor) const {
    visitor->Trace(old_flat_tree_children);
    visitor->Trace(nodes_needing_parent_slot_changed);
  }

  HeapVector<Member<Node>> old_flat_tree_children;
  // Assigned nodes that need ParentSlotChanged() even if the flat tree
  // children diff does not detect them (see
  // UpdateFlatTreeNodeDataForAssignedNodes()).
  HeapVector<Member<Node>> nodes_needing_parent_slot_changed;
  bool switched_between_assignment_and_fallback = false;
  // True if the slot's children are inactive fallback content that may have
  // layout objects, so ApplyRecalcSideEffects() must detach them.
  bool detach_fallback_children = false;
};

class CORE_EXPORT HTMLSlotElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  HTMLSlotElement(Document&);

  ElementType GetElementType() const final {
    return ElementType::kHTMLSlotElement;
  }

  const HeapVector<Member<Node>>& AssignedNodes() const;
  const HeapVector<Member<Node>> AssignedNodesForBinding(
      const AssignedNodesOptions*);
  // Use AssignedNodes() in almost all cases. Only use AssignedNodesNoRecalc()
  // when calling RecalcAssignment() is forbidden or dangerous (e.g., during
  // tree modifications, moveBefore semantics, or state propagation like
  // Element::SetIsCanvasOrInCanvasSubtree where eventual consistency is
  // guaranteed by subsequent FlatTreeParentChanged/RemovedFromFlatTree
  // hooks).
  //
  // May return stale data if the containing shadow root's assignment is
  // dirty (see "Stale accessors" in slot_assignment.h). It is accurate during
  // pass 2 of slot assignment recalc, because every shadow root in the recalc
  // list is clean then. Approved callers: mutation-time style dirty-marking
  // and canvas subtree propagation.
  const HeapVector<Member<Node>>& AssignedNodesNoRecalc() const {
    return assigned_nodes_;
  }
  const HeapVector<Member<Element>> AssignedElementsForBinding(
      const AssignedNodesOptions*);

  Node* FirstAssignedNode() const {
    auto& nodes = AssignedNodes();
    return nodes.empty() ? nullptr : nodes.front().Get();
  }
  Node* LastAssignedNode() const {
    auto& nodes = AssignedNodes();
    return nodes.empty() ? nullptr : nodes.back().Get();
  }

  void AppendAssignedNode(Node&);
  void ClearAssignedNodes();

  const HeapVector<Member<Node>> FlattenedAssignedNodes();

  // Slot assignment recalc runs in two passes; see SlotAssignment.
  //
  // Pass 1 (no side effects): WillRecalcAssignedNodes(), AppendAssignedNode(),
  // then DidRecalcAssignedNodes(), which updates the flat tree node data of the
  // assigned nodes and the flat tree children, and returns the side effects
  // to apply in pass 2.
  void WillRecalcAssignedNodes() { ClearAssignedNodes(); }
  SlotRecalcSideEffects* DidRecalcAssignedNodes();
  // Pass 2: applies the side effects recorded in pass 1. Every shadow root in
  // the recalc list has clean assignment data at this point. Side effects
  // must follow "Recalc hooks" in slot_assignment.h.
  void ApplyRecalcSideEffects(const SlotRecalcSideEffects&,
                              bool display_locked_subtree);

  void AttachLayoutTreeForSlotChildren(AttachContext&);
  void DetachLayoutTree(bool performing_reattach) final;
  void DetachAssignedNodesLayoutTree(bool performing_reattach = false);
  void RebuildDistributedChildrenLayoutTrees(WhitespaceAttacher&);

  void AttributeChanged(const AttributeModificationParams&) final;

  AtomicString GetName() const;

  // This method can be slow because this has to traverse the children of a
  // shadow host.  This method should be used only when |assigned_nodes_| is
  // dirty.  e.g. To detect a slotchange event in DOM mutations.
  bool HasAssignedNodesSlow() const;

  // Returns true if the slot has assigned nodes, without doing assignment
  // recalc. Used by FlatTreeParentForChildDirty() which needs to avoid doing
  // slot assignments while marking the tree style-dirty. May be stale; see
  // AssignedNodesNoRecalc().
  bool HasAssignedNodesNoRecalc() const { return !assigned_nodes_.empty(); }

  // Returns true if FlattenedAssignedNodes() would be non-empty, without
  // doing assignment recalc. Used for :has-slotted matching during style
  // resolution. May be stale; see AssignedNodesNoRecalc().
  bool HasFlattenedAssignedNodesNoRecalc() const;

  bool SupportsAssignment() const { return IsInShadowTree(); }

  void CheckFallbackAfterInsertedIntoShadowTree();
  void CheckFallbackAfterRemovedFromShadowTree();

  void DidSlotChange(SlotChangeType);
  void DidSlotChangeAfterRemovedFromShadowTree();
  void DidSlotChangeAfterRenaming();
  void DispatchSlotChangeEvent();
  void ClearSlotChangeEventEnqueued() { slotchange_event_enqueued_ = false; }

  static AtomicString NormalizeSlotName(const AtomicString&);

  void RecalcStyleForSlotChildren(const StyleRecalcChange,
                                  const StyleRecalcContext&);

  // For imperative Shadow DOM distribution APIs
  // IDL assign() implementation.
  void assign(HeapVector<Member<V8UnionElementOrText>>& nodes, ExceptionState&);
  // assign() c++ implementation.
  void Assign(const HeapVector<Member<Node>>& nodes);
  void Assign(Node* node);

  // For user-agent manual slotting only, from the host's
  // ManuallyAssignSlots() during pass 1 of slot assignment recalc. Unlike
  // Assign(), this only updates the manually assigned nodes: it does not
  // dirty slot assignment (see rule R4 in slot_assignment.h) or signal a slot
  // change. The :has-slotted invalidation is applied in pass 2, by
  // ApplyRecalcSideEffects().
  void SetManuallyAssignedNodesForRecalc(const HeapVector<Member<Node>>& nodes);
  void SetManuallyAssignedNodesForRecalc(Node* node);

  const HeapLinkedHashSet<WeakMember<Node>>& ManuallyAssignedNodes() const {
    return manually_assigned_nodes_;
  }

  // Override in order to defer this work when needed.
  bool CalculateAndAdjustAutoDirectionality() final;

  void Trace(Visitor*) const override;

 private:
  InsertionNotificationRequest InsertedInto(ContainerNode&) final;
  void RemovedFrom(ContainerNode&) final;

  void ChildrenChanged(const ChildrenChange&) override;

  void EnqueueSlotChangeEvent();

  bool HasSlotableChild() const;

  void NotifySlottedNodesOfFlatTreeChange(
      const HeapVector<Member<Node>>& old_slotted,
      const HeapVector<Member<Node>>& new_slotted,
      bool switched_between_assignment_and_fallback);
  static void NotifySlottedNodesOfFlatTreeChangeNaive(
      const HeapVector<Member<Node>>& old_assigned_nodes,
      const HeapVector<Member<Node>>& new_assigned_nodes);
  static void NotifySlottedNodesOfFlatTreeChangeByDynamicProgramming(
      const HeapVector<Member<Node>>& old_slotted,
      const HeapVector<Member<Node>>& new_slotted);

  void SetShadowRootNeedsAssignmentRecalc();

  void RecalcFlatTreeChildren(SlotRecalcSideEffects&);
  void UpdateFlatTreeNodeDataForAssignedNodes(SlotRecalcSideEffects&);
  void ClearAssignedNodesAndFlatTreeChildren();
  void MarkAssignedNodesForDisplayLockDetachCheck();

  void UpdateDirAutoAncestorsForSupportsAssignmentChange();

  HeapVector<Member<Node>> assigned_nodes_;
  HeapVector<Member<Node>> flat_tree_children_;
  // Tracks whether the most recent slot assignment was empty, hence the
  // light-DOM children of the slot element (if any) were active fallback.
  bool flat_tree_children_are_fallback_ = true;
  // Set when the slot's children change (including moveBefore(), which keeps
  // the moved node's layout object) or when the slot leaves its shadow tree
  // (outside a shadow tree, the slot renders its children). Cleared by
  // RecalcFlatTreeChildren(). While false, the children of a slot with
  // assigned nodes are known to be detached.
  bool fallback_children_changed_ = true;

  bool slotchange_event_enqueued_ = false;

  // Set by SetManuallyAssignedNodesForRecalc() when the manually assigned
  // nodes change, and cleared by ApplyRecalcSideEffects() after it
  // invalidates :has-slotted.
  bool has_slotted_invalidation_pending_ = false;

  // Imperative Shadow DOM distribution API.
  HeapLinkedHashSet<WeakMember<Node>> manually_assigned_nodes_;

  template <typename T, wtf_size_t S>
  struct LCSArray {
    LCSArray() : values(S) {}
    T& operator[](wtf_size_t i) { return values[i]; }
    wtf_size_t size() { return values.size(); }
    Vector<T, S> values;
  };

  // TODO(hayato): Move this to more appropriate directory (e.g. platform/wtf)
  // if there are more than one usages.
  template <typename Container, typename LCSTable, typename BacktrackTable>
  static void FillLongestCommonSubsequenceDynamicProgrammingTable(
      const Container& seq1,
      const Container& seq2,
      LCSTable& lcs_table,
      BacktrackTable& backtrack_table) {
    const wtf_size_t rows = base::checked_cast<wtf_size_t>(seq1.size());
    const wtf_size_t columns = base::checked_cast<wtf_size_t>(seq2.size());

    DCHECK_GT(lcs_table.size(), rows);
    DCHECK_GT(lcs_table[0].size(), columns);
    DCHECK_GT(backtrack_table.size(), rows);
    DCHECK_GT(backtrack_table[0].size(), columns);

    for (wtf_size_t r = 0; r <= rows; ++r)
      lcs_table[r][0] = 0;
    for (wtf_size_t c = 0; c <= columns; ++c)
      lcs_table[0][c] = 0;

    for (wtf_size_t r = 1; r <= rows; ++r) {
      for (wtf_size_t c = 1; c <= columns; ++c) {
        if (seq1[r - 1] == seq2[c - 1]) {
          lcs_table[r][c] = lcs_table[r - 1][c - 1] + 1;
          backtrack_table[r][c] = std::make_pair(r - 1, c - 1);
        } else if (lcs_table[r - 1][c] > lcs_table[r][c - 1]) {
          lcs_table[r][c] = lcs_table[r - 1][c];
          backtrack_table[r][c] = std::make_pair(r - 1, c);
        } else {
          lcs_table[r][c] = lcs_table[r][c - 1];
          backtrack_table[r][c] = std::make_pair(r, c - 1);
        }
      }
    }
  }

  friend class HTMLSlotElementTest;
  friend class HTMLSlotElementInDocumentTest;
};

inline const HTMLSlotElement* ToHTMLSlotElementIfSupportsAssignmentOrNull(
    const Node& node) {
  if (auto* slot = DynamicTo<HTMLSlotElement>(node)) {
    if (slot->SupportsAssignment())
      return slot;
  }
  return nullptr;
}

inline HTMLSlotElement* ToHTMLSlotElementIfSupportsAssignmentOrNull(
    Node& node) {
  return const_cast<HTMLSlotElement*>(
      ToHTMLSlotElementIfSupportsAssignmentOrNull(
          static_cast<const Node&>(node)));
}

inline const HTMLSlotElement* ToHTMLSlotElementIfSupportsAssignmentOrNull(
    const Node* node) {
  if (!node)
    return nullptr;
  return ToHTMLSlotElementIfSupportsAssignmentOrNull(*node);
}

inline HTMLSlotElement* ToHTMLSlotElementIfSupportsAssignmentOrNull(
    Node* node) {
  return const_cast<HTMLSlotElement*>(
      ToHTMLSlotElementIfSupportsAssignmentOrNull(
          static_cast<const Node*>(node)));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SLOT_ELEMENT_H_

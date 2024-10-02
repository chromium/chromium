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

#include "third_party/blink/renderer/core/html/html_slot_element.h"

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_assigned_nodes_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {
constexpr size_t kLCSTableSizeLimit = 16;
}

HTMLSlotElement::HTMLSlotElement(Document& document)
    : HTMLElement(html_names::kSlotTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLSlotElement);
}

// static
AtomicString HTMLSlotElement::NormalizeSlotName(const AtomicString& name) {
  return (name.IsNull() || name.empty()) ? g_empty_atom : name;
}

// static
const AtomicString& HTMLSlotElement::UserAgentDefaultSlotName() {
  DEFINE_STATIC_LOCAL(const AtomicString, user_agent_default_slot_name,
                      ("user-agent-default-slot"));
  return user_agent_default_slot_name;
}

// static
const AtomicString& HTMLSlotElement::UserAgentCustomAssignSlotName() {
  DEFINE_STATIC_LOCAL(const AtomicString, user_agent_custom_assign_slot_name,
                      ("user-agent-custom-assign-slot"));
  return user_agent_custom_assign_slot_name;
}

const HeapVector<Member<Node>>& HTMLSlotElement::AssignedNodes() const {
  if (!SupportsAssignment()) {
    DCHECK(assigned_nodes_.empty());
    return assigned_nodes_;
  }
  ContainingShadowRoot()->GetSlotAssignment().RecalcAssignment();
  return assigned_nodes_;
}

namespace {

HeapVector<Member<Node>> CollectFlattenedAssignedNodes(
    const HTMLSlotElement& slot) {
  DCHECK(slot.SupportsAssignment());

  const HeapVector<Member<Node>>& assigned_nodes = slot.AssignedNodes();
  HeapVector<Member<Node>> nodes;
  if (assigned_nodes.empty()) {
    // Fallback contents.
    for (auto& child : NodeTraversal::ChildrenOf(slot)) {
      if (!child.IsSlotable())
        continue;
      if (auto* child_slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(child))
        nodes.AppendVector(CollectFlattenedAssignedNodes(*child_slot));
      else
        nodes.push_back(child);
    }
  } else {
    for (auto& node : assigned_nodes) {
      DCHECK(node->IsSlotable());
      if (auto* assigned_node_slot =
              ToHTMLSlotElementIfSupportsAssignmentOrNull(*node))
        nodes.AppendVector(CollectFlattenedAssignedNodes(*assigned_node_slot));
      else
        nodes.push_back(node);
    }
  }
  return nodes;
}

}  // namespace

const HeapVector<Member<Node>> HTMLSlotElement::FlattenedAssignedNodes() {
  if (!SupportsAssignment()) {
    DCHECK(assigned_nodes_.empty());
    return assigned_nodes_;
  }
  return CollectFlattenedAssignedNodes(*this);
}

const HeapVector<Member<Node>> HTMLSlotElement::AssignedNodesForBinding(
    const AssignedNodesOptions* options) {
  if (options->hasFlatten() && options->flatten())
    return FlattenedAssignedNodes();
  return AssignedNodes();
}

const HeapVector<Member<Element>> HTMLSlotElement::AssignedElements() {
  HeapVector<Member<Element>> elements;
  for (auto& node : AssignedNodes()) {
    if (auto* element = DynamicTo<Element>(node.Get()))
      elements.push_back(*element);
  }
  return elements;
}

const HeapVector<Member<Element>> HTMLSlotElement::AssignedElementsForBinding(
    const AssignedNodesOptions* options) {
  HeapVector<Member<Element>> elements;
  for (auto& node : AssignedNodesForBinding(options)) {
    if (auto* element = DynamicTo<Element>(node.Get()))
      elements.push_back(*element);
  }
  return elements;
}

void HTMLSlotElement::assign(HeapVector<Member<V8UnionElementOrText>>& js_nodes,
                             ExceptionState&) {
  UseCounter::Count(GetDocument(), WebFeature::kSlotAssignNode);
  if (js_nodes.empty() && manually_assigned_nodes_.empty())
    return;

  HeapVector<Member<Node>> nodes;
  for (V8UnionElementOrText* union_node : js_nodes) {
    Node* node = nullptr;
    switch (union_node->GetContentType()) {
      case V8UnionElementOrText::ContentType::kText:
        node = union_node->GetAsText();
        break;
      case V8UnionElementOrText::ContentType::kElement:
        node = union_node->GetAsElement();
        break;
    }
    nodes.push_back(*node);
  }
  Assign(nodes);
}

void HTMLSlotElement::Assign(const HeapVector<Member<Node>>& nodes) {
  if (nodes.empty() && manually_assigned_nodes_.empty())
    return;

  bool updated = false;
  HeapLinkedHashSet<WeakMember<Node>> added_nodes;
  for (Node* node : nodes) {
    added_nodes.insert(node);
    if (auto* previous_slot = node->ManuallyAssignedSlot()) {
      if (previous_slot == this)
        continue;
      previous_slot->manually_assigned_nodes_.erase(node);
      if (previous_slot->SupportsAssignment())
        previous_slot->DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
    }
    updated = true;
    node->SetManuallyAssignedSlot(this);
  }

  HeapLinkedHashSet<WeakMember<Node>> removed_nodes;
  for (Node* node : manually_assigned_nodes_) {
    if (!base::Contains(added_nodes, node)) {
      removed_nodes.insert(node);
    }
  }

  updated |= added_nodes.size() != manually_assigned_nodes_.size();
  if (!updated) {
    for (auto it1 = added_nodes.begin(), it2 = manually_assigned_nodes_.begin();
         it1 != added_nodes.end(); ++it1, ++it2) {
      if (!(*it1 == *it2)) {
        updated = true;
        break;
      }
    }
  }
  DCHECK(updated || removed_nodes.empty());

  if (updated) {
    for (auto removed_node : removed_nodes)
      removed_node->SetManuallyAssignedSlot(nullptr);
    manually_assigned_nodes_.Swap(added_nodes);
    // The slot might not be located in a shadow root yet.
    if (ContainingShadowRoot()) {
      SetShadowRootNeedsAssignmentRecalc();
      DidSlotChange(SlotChangeType::kSignalSlotChangeEvent);
    }
  }
}

void HTMLSlotElement::Assign(Node* node) {
  VectorOf<Node> nodes;
  if (node) {
    nodes.push_back(node);
  }
  Assign(nodes);
}

void HTMLSlotElement::AppendAssignedNode(Node& host_child) {
  DCHECK(host_child.IsSlotable());
  assigned_nodes_.push_back(&host_child);
}

void HTMLSlotElement::ClearAssignedNodes() {
  assigned_nodes_.clear();
}

void HTMLSlotElement::ClearAssignedNodesAndFlatTreeChildren() {
  ClearAssignedNodes();
  flat_tree_children_.clear();
}

void HTMLSlotElement::UpdateFlatTreeNodeDataForAssignedNodes() {
  Node* previous = nullptr;
  for (auto& current : assigned_nodes_) {
    bool flat_tree_parent_changed = false;
    if (!current->NeedsStyleRecalc() && !current->GetComputedStyle()) {
      if (auto* node_data = current->GetFlatTreeNodeData())
        flat_tree_parent_changed = !node_data->AssignedSlot();
    }
    FlatTreeNodeData& flat_tree_node_data = current->EnsureFlatTreeNodeData();
    flat_tree_node_data.SetAssignedSlot(this);
    flat_tree_node_data.SetPreviousInAssignedNodes(previous);
    if (previous) {
      DCHECK(previous->GetFlatTreeNodeData());
      previous->GetFlatTreeNodeData()->SetNextInAssignedNodes(current);
    }
    previous = current;
    if (flat_tree_parent_changed)
      current->ParentSlotChanged();
  }
  if (previous) {
    DCHECK(previous->GetFlatTreeNodeData());
    previous->GetFlatTreeNodeData()->SetNextInAssignedNodes(nullptr);
  }
}

void HTMLSlotElement::DetachDisplayLockedAssignedNodesLayoutTreeIfNeeded() {
  // If the assigned node is now under a display locked subtree and its layout
  // is in 'forced reattach' mode, it means that this node potentially changed
  // slots into a display locked subtree. We would normally update its layout
  // tree during a layout tree update phase, but that is skipped in display
  // locked subtrees. In order to avoid a corrupt layout tree as a result, we
  // detach the node's layout tree.
  StyleEngine& style_engine = GetDocument().GetStyleEngine();
  StyleEngine::DetachLayoutTreeScope detach_scope(style_engine);
  for (auto& current : assigned_nodes_) {
    if (current->GetForceReattachLayoutTree())
      current->DetachLayoutTree();
  }
}

void HTMLSlotElement::RecalcFlatTreeChildren() {
  DCHECK(SupportsAssignment());

  HeapVector<Member<Node>> old_flat_tree_children;
  old_flat_tree_children.swap(flat_tree_children_);

  if (assigned_nodes_.empty()) {
    // Use children as fallback
    for (auto& child : NodeTraversal::ChildrenOf(*this)) {
      if (child.IsSlotable())
        flat_tree_children_.push_back(child);
    }
  } else {
    flat_tree_children_ = assigned_nodes_;
    for (auto& node : old_flat_tree_children) {
      // Detach fallback nodes. Host children which are no longer slotted are
      // detached in SlotAssignment::RecalcAssignment().
      if (node->parentNode() == this)
        node->RemovedFromFlatTree();
    }
  }

  NotifySlottedNodesOfFlatTreeChange(old_flat_tree_children,
                                     flat_tree_children_);
}

void HTMLSlotElement::DispatchSlotChangeEvent() {
  DCHECK(!IsInUserAgentShadowRoot() ||
         ContainingShadowRoot()->IsNamedSlotting());
  Event* event = Event::CreateBubble(event_type_names::kSlotchange);
  event->SetTarget(this);
  DispatchScopedEvent(*event);
}

AtomicString HTMLSlotElement::GetName() const {
  return NormalizeSlotName(FastGetAttribute(html_names::kNameAttr));
}

void HTMLSlotElement::AttachLayoutTreeForSlotChildren(AttachContext& context) {
  for (Node* child : flat_tree_children_) {
    child->AttachLayoutTree(context);
  }
}

void HTMLSlotElement::DetachLayoutTree(bool performing_reattach) {
  if (SupportsAssignment()) {
    auto* host = OwnerShadowHost();
    const HeapVector<Member<Node>>& flat_tree_children = assigned_nodes_;
    for (auto& node : flat_tree_children) {
      // Don't detach the assigned node if the node is no longer a child of the
      // host.
      //
      // 1. It's no long a direct flat-tree child of this slot.
      // 2. It was already detached when removed from the host.
      // 3. It might already have been inserted in a different part of the DOM,
      //    or a new document tree and been attached.
      // 4. It might have been marked style-dirty in its new location and
      //    calling DetachLayoutTree here would have incorrectly cleared those
      //    dirty bits.
      if (host == node->parentNode())
        node->DetachLayoutTree(performing_reattach);
    }
  }
  HTMLElement::DetachLayoutTree(performing_reattach);
}

void HTMLSlotElement::RebuildDistributedChildrenLayoutTrees(
    WhitespaceAttacher& whitespace_attacher) {
  DCHECK(SupportsAssignment());

  // This loop traverses the nodes from right to left for the same reason as the
  // one described in ContainerNode::RebuildChildrenLayoutTrees().
  for (const auto& child : base::Reversed(flat_tree_children_)) {
    RebuildLayoutTreeForChild(child, whitespace_attacher);
  }
}

void HTMLSlotElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kNameAttr) {
    if (ShadowRoot* root = ContainingShadowRoot()) {
      if (params.old_value != params.new_value) {
        root->GetSlotAssignment().DidRenameSlot(
            NormalizeSlotName(params.old_value), *this);
      }
    }
  }
  HTMLElement::AttributeChanged(params);
}

// When the result of `SupportsAssignment()` changes, the behavior of a
// <slot> element for ancestors with dir=auto changes.
void HTMLSlotElement::UpdateDirAutoAncestorsForSupportsAssignmentChange() {
  if (SelfOrAncestorHasDirAutoAttribute()) {
    UpdateAncestorWithDirAuto(UpdateAncestorTraversal::ExcludeSelf);
  }
}

Node::InsertionNotificationRequest HTMLSlotElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  UpdateDirAutoAncestorsForSupportsAssignmentChange();
  if (SupportsAssignment()) {
    ShadowRoot* root = ContainingShadowRoot();
    DCHECK(root);
    if (root == insertion_point.ContainingShadowRoot()) {
      // This slot is inserted into the same tree of |insertion_point|
      root->DidAddSlot(*this);
    } else if (insertion_point.isConnected() &&
               root->NeedsSlotAssignmentRecalc()) {
      // Even when a slot and its containing shadow root is removed together
      // and inserted together again, the slot's cached assigned nodes can be
      // stale if the NeedsSlotAssignmentRecalc flag is set, and it may cause
      // infinite recursion in DetachLayoutTree() when one of the stale node
      // is a shadow-including ancestor of this slot by making a circular
      // reference. Clear the cache here to avoid the situation.
      // See http://crbug.com/849599 for details.
      ClearAssignedNodesAndFlatTreeChildren();
    }
  }
  return kInsertionDone;
}

void HTMLSlotElement::RemovedFrom(ContainerNode& insertion_point) {
  // `removedFrom` is called after the node is removed from the tree.
  // That means:
  // 1. If this slot is still in a tree scope, it means the slot has been in a
  //    shadow tree. An inclusive shadow-including ancestor of the shadow host
  //    was originally removed from its parent. See slot s2 below.
  // 2. Or (this slot is not in a tree scope), this slot's inclusive
  //    ancestor was orginally removed from its parent (== insertion point).
  //    This slot and the originally removed node was in the same tree before
  //    removal. See slot s1 below.

  // For example, given the following trees, (srN: = shadow root, sN: = slot)
  // a
  // |- b --sr1
  // |- c   |--d
  //           |- e-----sr2
  //              |- s1 |--f
  //                    |--s2

  // If we call 'e.remove()', then:
  // - For slot s1, s1.removedFrom(d) is called.
  // - For slot s2, s2.removedFrom(d) is called.

  // ContainingShadowRoot() is okay to use here because 1) It doesn't use
  // kIsInShadowTreeFlag flag, and 2) TreeScope has been already updated for the
  // slot.
  if (ShadowRoot* shadow_root = ContainingShadowRoot()) {
    // In this case, the shadow host (or its shadow-inclusive ancestor) was
    // removed originally. In the above example, (this slot == s2) and
    // (shadow_root == sr2). The shadow tree (sr2)'s structure didn't change at
    // all.
    if (shadow_root->NeedsSlotAssignmentRecalc()) {
      // Clear |assigned_nodes_| here, so that the referenced node can get
      // garbage collected if they no longer needed. See also InsertedInto()'s
      // comment for cases that stale |assigned_nodes| can be problematic.
      ClearAssignedNodesAndFlatTreeChildren();
    } else {
      // We don't need to clear |assigned_nodes_| here. That's an important
      // optimization.
    }
  } else if (insertion_point.IsInShadowTree()) {
    // This slot was in a shadow tree and got disconnected from the shadow tree.
    // In the above example, (this slot == s1), (insertion point == d)
    // and (insertion_point->ContainingShadowRoot == sr1).
    insertion_point.ContainingShadowRoot()->GetSlotAssignment().DidRemoveSlot(
        *this);
    ClearAssignedNodesAndFlatTreeChildren();
  } else {
    DCHECK(assigned_nodes_.empty());
  }

  UpdateDirAutoAncestorsForSupportsAssignmentChange();
  HTMLElement::RemovedFrom(insertion_point);
}

void HTMLSlotElement::RecalcStyleForSlotChildren(
    const StyleRecalcChange change,
    const StyleRecalcContext& style_recalc_context) {
  for (auto& node : flat_tree_children_) {
    if (!change.TraverseChild(*node))
      continue;
    if (auto* element = DynamicTo<Element>(node.Get()))
      element->RecalcStyle(change, style_recalc_context);
    else if (auto* text_node = DynamicTo<Text>(node.Get()))
      text_node->RecalcTextStyle(change);
  }
}

void HTMLSlotElement::NotifySlottedNodesOfFlatTreeChangeByDynamicProgramming(
    const HeapVector<Member<Node>>& old_slotted,
    const HeapVector<Member<Node>>& new_slotted) {
  // Use dynamic programming to minimize the number of nodes being reattached.
  using LCSTable =
      Vector<LCSArray<wtf_size_t, kLCSTableSizeLimit>, kLCSTableSizeLimit>;
  using Backtrack = std::pair<wtf_size_t, wtf_size_t>;
  using BacktrackTable =
      Vector<LCSArray<Backtrack, kLCSTableSizeLimit>, kLCSTableSizeLimit>;

  DEFINE_STATIC_LOCAL(LCSTable*, lcs_table, (new LCSTable(kLCSTableSizeLimit)));
  DEFINE_STATIC_LOCAL(BacktrackTable*, backtrack_table,
                      (new BacktrackTable(kLCSTableSizeLimit)));

  FillLongestCommonSubsequenceDynamicProgrammingTable(
      old_slotted, new_slotted, *lcs_table, *backtrack_table);

  wtf_size_t r = old_slotted.size();
  wtf_size_t c = new_slotted.size();
  while (r > 0 && c > 0) {
    Backtrack backtrack = (*backtrack_table)[r][c];
    if (backtrack == std::make_pair(r - 1, c - 1)) {
      DCHECK_EQ(old_slotted[r - 1], new_slotted[c - 1]);
    } else if (backtrack == std::make_pair(r, c - 1)) {
      new_slotted[c - 1]->ParentSlotChanged();
    }
    std::tie(r, c) = backtrack;
  }
  if (c > 0) {
    for (wtf_size_t i = 0; i < c; ++i)
      new_slotted[i]->ParentSlotChanged();
  }
}

void HTMLSlotElement::NotifySlottedNodesOfFlatTreeChange(
    const HeapVector<Member<Node>>& old_slotted,
    const HeapVector<Member<Node>>& new_slotted) {
  if (old_slotted == new_slotted)
    return;
  probe::DidPerformSlotDistribution(this);

  // It is very important to minimize the number of reattaching nodes in
  // |new_assigned_nodes| here. The following *works*, in terms of the
  // correctness of the rendering,
  //
  // for (auto& node: new_slotted) {
  //   node->ParentSlotChanged();
  // }
  //
  // However, reattaching all ndoes is not good in terms of performance.
  // Reattach is very expensive operation.
  //
  // A possible approach is: Find the Longest Commons Subsequence (LCS) between
  // |old_slotted| and |new_slotted|, and reattach nodes in |new_slotted| which
  // LCS does not include.
  //
  // Note that a relative order between nodes which are not reattached should be
  // preserved in old and new. For example,
  //
  // - old: [1, 4, 2, 3]
  // - new: [3, 1, 2]
  //
  // This case, we must reattach 3 here, as the best possible solution.  If we
  // don't reattach 3, 3's LayoutObject will have an invalid next sibling
  // pointer.  We don't have any chance to update their sibling pointers (3's
  // next and 1's previous).  Sibling pointers between 1 and 2 are correctly
  // updated when we reattach 4, which is done in another code path.
  if (old_slotted.size() + 1 > kLCSTableSizeLimit ||
      new_slotted.size() + 1 > kLCSTableSizeLimit) {
    // Since DP takes O(N^2), we don't use DP if the size is larger than the
    // pre-defined limit.
    NotifySlottedNodesOfFlatTreeChangeNaive(old_slotted, new_slotted);
  } else {
    NotifySlottedNodesOfFlatTreeChangeByDynamicProgramming(old_slotted,
                                                           new_slotted);
  }
}

void HTMLSlotElement::DidSlotChangeAfterRemovedFromShadowTree() {
  DCHECK(!ContainingShadowRoot());
  EnqueueSlotChangeEvent();
  CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
}

void HTMLSlotElement::DidSlotChangeAfterRenaming() {
  DCHECK(SupportsAssignment());
  EnqueueSlotChangeEvent();
  SetShadowRootNeedsAssignmentRecalc();
  CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
}

void HTMLSlotElement::NotifySlottedNodesOfFlatTreeChangeNaive(
    const HeapVector<Member<Node>>& old_assigned_nodes,
    const HeapVector<Member<Node>>& new_assigned_nodes) {
  // Use O(N) naive greedy algorithm to find a *suboptimal* longest common
  // subsequence (LCS), and reattach nodes which are not in suboptimal LCS.  We
  // run a greedy algorithm twice in both directions (scan forward and scan
  // backward), and use the better result.  Though this greedy algorithm is not
  // perfect, it works well in some common cases, such as:

  // Inserting a node:
  // old assigned nodes: [a, b ...., z]
  // new assigned nodes: [a, b ...., z, A]
  // => The algorithm reattaches only node |A|.

  // Removing a node:
  // - old assigned nodes: [a, b, ..., m, n, o, ..., z]
  // - new assigned nodes: [a, b, ..., m, o, ... , z]
  // => The algorithm does not reattach any node.

  // Moving a node:
  // - old assigned nodes: [a, b, ..., z]
  // - new assigned nodes: [b, ..., z, a]
  // => The algorithm reattaches only node |a|.

  // Swapping the first node and the last node
  // - old assigned nodes: [a, b, ..., y, z]
  // - new assigned nodes: [z, b, ..., y, a]
  // => Ideally, we should reattach only |a| and |z|, however, the algorithm
  // does not work well here, reattaching [a, b, ...., y] (or [b, ... y, z]).
  // We could reconsider to support this case if a compelling case arises.

  // TODO(hayato): Consider to write an unit test for the algorithm.  We
  // probably want to make the algorithm templatized so we can test it
  // easily.  Like, Vec<T> greedy_suboptimal_lcs(Vec<T> old, Vec<T> new)

  HeapHashMap<Member<Node>, wtf_size_t> old_index_map;
  for (wtf_size_t i = 0; i < old_assigned_nodes.size(); ++i) {
    old_index_map.insert(old_assigned_nodes[i], i);
  }

  // Scan forward
  HeapVector<Member<Node>> forward_result;

  wtf_size_t i = 0;
  wtf_size_t j = 0;

  while (i < old_assigned_nodes.size() && j < new_assigned_nodes.size()) {
    auto& new_node = new_assigned_nodes[j];
    if (old_assigned_nodes[i] == new_node) {
      ++i;
      ++j;
      continue;
    }
    if (old_index_map.Contains(new_node)) {
      wtf_size_t old_index = old_index_map.at(new_node);
      if (old_index > i) {
        i = old_index_map.at(new_node) + 1;
        ++j;
        continue;
      }
    }
    forward_result.push_back(new_node);
    ++j;
  }

  for (; j < new_assigned_nodes.size(); ++j) {
    forward_result.push_back(new_assigned_nodes[j]);
  }

  // Scan backward
  HeapVector<Member<Node>> backward_result;

  i = old_assigned_nodes.size();
  j = new_assigned_nodes.size();

  while (i > 0 && j > 0) {
    auto& new_node = new_assigned_nodes[j - 1];
    if (old_assigned_nodes[i - 1] == new_node) {
      --i;
      --j;
      continue;
    }
    if (old_index_map.Contains(new_node)) {
      wtf_size_t old_index = old_index_map.at(new_node);
      if (old_index < i - 1) {
        i = old_index;
        --j;
        continue;
      }
    }
    backward_result.push_back(new_node);
    --j;
  }

  for (; j > 0; --j) {
    backward_result.push_back(new_assigned_nodes[j - 1]);
  }

  // Reattach nodes
  if (forward_result.size() <= backward_result.size()) {
    for (auto& node : forward_result) {
      node->ParentSlotChanged();
    }
  } else {
    for (auto& node : backward_result) {
      node->ParentSlotChanged();
    }
  }
}

void HTMLSlotElement::SetShadowRootNeedsAssignmentRecalc() {
  DCHECK(ContainingShadowRoot());
  ContainingShadowRoot()->GetSlotAssignment().SetNeedsAssignmentRecalc();
}

void HTMLSlotElement::DidSlotChange(SlotChangeType slot_change_type) {
  DCHECK(SupportsAssignment());
  PseudoStateChanged(CSSSelector::kPseudoHasSlotted);
  if (slot_change_type == SlotChangeType::kSignalSlotChangeEvent)
    EnqueueSlotChangeEvent();
  SetShadowRootNeedsAssignmentRecalc();
  // Check slotchange recursively since this slotchange may cause another
  // slotchange.
  CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
}

void HTMLSlotElement::CheckFallbackAfterInsertedIntoShadowTree() {
  DCHECK(SupportsAssignment());
  if (HasSlotableChild()) {
    // We use kSuppress here because a slotchange event shouldn't be
    // dispatched if a slot being inserted doesn't get any assigned
    // node, but has a slotable child, according to DOM Standard.
    DidSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
  }
}

void HTMLSlotElement::CheckFallbackAfterRemovedFromShadowTree() {
  if (HasSlotableChild()) {
    // Since a slot was removed from a shadow tree,
    // we don't need to set dirty flag for a disconnected tree.
    // However, we need to call CheckSlotChange because we might need to set a
    // dirty flag for a shadow tree which a parent of the slot may host.
    CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
  }
}

bool HTMLSlotElement::HasSlotableChild() const {
  for (auto& child : NodeTraversal::ChildrenOf(*this)) {
    if (child.IsSlotable())
      return true;
  }
  return false;
}

void HTMLSlotElement::EnqueueSlotChangeEvent() {
  // TODO(kochi): This suppresses slotchange event on user-agent shadows that
  // don't support name based slot assignment, but could be improved further by
  // not running change detection logic in
  // SlotAssignment::Did{Add,Remove}SlotInternal etc., although naive skipping
  // turned out breaking fallback content handling.
  if (IsInUserAgentShadowRoot() && !ContainingShadowRoot()->IsNamedSlotting())
    return;
  if (slotchange_event_enqueued_)
    return;
  MutationObserver::EnqueueSlotChange(*this);
  slotchange_event_enqueued_ = true;
}

bool HTMLSlotElement::HasAssignedNodesSlow() const {
  ShadowRoot* root = ContainingShadowRoot();
  DCHECK(root) << "This should only be called on slots inside a shadow tree";
  SlotAssignment& assignment = root->GetSlotAssignment();
  if (assignment.FindSlotByName(GetName()) != this)
    return false;
  return assignment.FindHostChildBySlotName(GetName());
}

void HTMLSlotElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  if (SupportsAssignment())
    SetShadowRootNeedsAssignmentRecalc();
}

bool HTMLSlotElement::CalculateAndAdjustAutoDirectionality() {
  if (SupportsAssignment() &&
      ContainingShadowRoot()->GetSlotAssignment().NeedsAssignmentRecalc()) {
    // It might not be safe to do an auto directionality update right now
    // since it might run RecalcAssignment at a bad time; we should wait until
    // RecalcAssignment runs.  RecalcAssignment needs to update directionality
    // anyway, so we don't need to invalidate anything.

    // This dependency on NeedsAssignmentRecalc() is a little bit ugly, but it
    // seems far less problematic than other solutions.
    return false;
  }
  return HTMLElement::CalculateAndAdjustAutoDirectionality();
}

void HTMLSlotElement::Trace(Visitor* visitor) const {
  visitor->Trace(assigned_nodes_);
  visitor->Trace(flat_tree_children_);
  visitor->Trace(manually_assigned_nodes_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink

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

#include <array>
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/assigned_nodes_options.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

using namespace HTMLNames;

namespace {
constexpr size_t kLCSTableSizeLimit = 16;
}

HTMLSlotElement* HTMLSlotElement::Create(Document& document) {
  return new HTMLSlotElement(document);
}

HTMLSlotElement* HTMLSlotElement::CreateUserAgentDefaultSlot(
    Document& document) {
  HTMLSlotElement* slot = new HTMLSlotElement(document);
  slot->setAttribute(nameAttr, UserAgentDefaultSlotName());
  return slot;
}

HTMLSlotElement* HTMLSlotElement::CreateUserAgentCustomAssignSlot(
    Document& document) {
  HTMLSlotElement* slot = new HTMLSlotElement(document);
  slot->setAttribute(nameAttr, UserAgentCustomAssignSlotName());
  return slot;
}

inline HTMLSlotElement::HTMLSlotElement(Document& document)
    : HTMLElement(slotTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLSlotElement);
  SetHasCustomStyleCallbacks();
}

// static
AtomicString HTMLSlotElement::NormalizeSlotName(const AtomicString& name) {
  return (name.IsNull() || name.IsEmpty()) ? g_empty_atom : name;
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
    DCHECK(assigned_nodes_.IsEmpty());
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
  if (assigned_nodes.IsEmpty()) {
    // Fallback contents.
    for (auto& child : NodeTraversal::ChildrenOf(slot)) {
      if (!child.IsSlotable())
        continue;
      if (auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(child))
        nodes.AppendVector(CollectFlattenedAssignedNodes(*slot));
      else
        nodes.push_back(child);
    }
  } else {
    for (auto& node : assigned_nodes) {
      DCHECK(node->IsSlotable());
      if (auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(*node))
        nodes.AppendVector(CollectFlattenedAssignedNodes(*slot));
      else
        nodes.push_back(node);
    }
  }
  return nodes;
}

}  // namespace

const HeapVector<Member<Node>> HTMLSlotElement::FlattenedAssignedNodes() {
  if (!SupportsAssignment()) {
    DCHECK(assigned_nodes_.IsEmpty());
    return assigned_nodes_;
  }
  return CollectFlattenedAssignedNodes(*this);
}

const HeapVector<Member<Node>> HTMLSlotElement::AssignedNodesForBinding(
    const AssignedNodesOptions& options) {
  if (options.hasFlatten() && options.flatten())
    return FlattenedAssignedNodes();
  return AssignedNodes();
}

const HeapVector<Member<Element>> HTMLSlotElement::AssignedElements() {
  HeapVector<Member<Element>> elements;
  for (auto& node : AssignedNodes()) {
    if (Element* element = ToElementOrNull(node))
      elements.push_back(element);
  }
  return elements;
}

const HeapVector<Member<Element>> HTMLSlotElement::AssignedElementsForBinding(
    const AssignedNodesOptions& options) {
  HeapVector<Member<Element>> elements;
  for (auto& node : AssignedNodesForBinding(options)) {
    if (Element* element = ToElementOrNull(node))
      elements.push_back(element);
  }
  return elements;
}

void HTMLSlotElement::assign(HeapVector<Member<Node>> nodes) {
  if (SupportsAssignment())
    ContainingShadowRoot()->GetSlotAssignment().SetNeedsAssignmentRecalc();
  assigned_nodes_candidates_.clear();
  for (auto& node : nodes) {
    assigned_nodes_candidates_.insert(node);
  }
}

void HTMLSlotElement::AppendAssignedNode(Node& host_child) {
  DCHECK(host_child.IsSlotable());
  assigned_nodes_.push_back(&host_child);
}

void HTMLSlotElement::ClearAssignedNodes() {
  assigned_nodes_.clear();
}

void HTMLSlotElement::ClearAssignedNodesAndFlatTreeChildren() {
  assigned_nodes_.clear();
  flat_tree_children_.clear();
}

void HTMLSlotElement::RecalcFlatTreeChildren() {
  DCHECK(SupportsAssignment());

  HeapVector<Member<Node>> old_flat_tree_children;
  old_flat_tree_children.swap(flat_tree_children_);

  if (assigned_nodes_.IsEmpty()) {
    // Use children as fallback
    for (auto& child : NodeTraversal::ChildrenOf(*this))
      flat_tree_children_.push_back(child);
  } else {
    flat_tree_children_ = assigned_nodes_;
  }

  LazyReattachNodesIfNeeded(old_flat_tree_children, flat_tree_children_);
}

void HTMLSlotElement::DispatchSlotChangeEvent() {
  DCHECK(!IsInUserAgentShadowRoot());
  Event* event = Event::CreateBubble(EventTypeNames::slotchange);
  event->SetTarget(this);
  DispatchScopedEvent(*event);
}

Node* HTMLSlotElement::AssignedNodeNextTo(const Node& node) const {
  DCHECK(SupportsAssignment());
  ContainingShadowRoot()->GetSlotAssignment().RecalcAssignment();
  // TODO(crbug.com/776656): Use {node -> index} map to avoid O(N) lookup
  wtf_size_t index = assigned_nodes_.Find(&node);
  DCHECK(index != WTF::kNotFound);
  if (index + 1 == assigned_nodes_.size())
    return nullptr;
  return assigned_nodes_[index + 1].Get();
}

Node* HTMLSlotElement::AssignedNodePreviousTo(const Node& node) const {
  DCHECK(SupportsAssignment());
  ContainingShadowRoot()->GetSlotAssignment().RecalcAssignment();
  // TODO(crbug.com/776656): Use {node -> index} map to avoid O(N) lookup
  wtf_size_t index = assigned_nodes_.Find(&node);
  DCHECK(index != WTF::kNotFound);
  if (index == 0)
    return nullptr;
  return assigned_nodes_[index - 1].Get();
}

AtomicString HTMLSlotElement::GetName() const {
  return NormalizeSlotName(FastGetAttribute(nameAttr));
}

void HTMLSlotElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);

  if (SupportsAssignment()) {
    AttachContext children_context(context);

    for (auto& node : AssignedNodes()) {
      if (node->NeedsAttach())
        node->AttachLayoutTree(children_context);
    }
    if (children_context.previous_in_flow)
      context.previous_in_flow = children_context.previous_in_flow;
  }
}

void HTMLSlotElement::DetachLayoutTree(const AttachContext& context) {
  if (SupportsAssignment()) {
    const HeapVector<Member<Node>>& flat_tree_children = assigned_nodes_;
    for (auto& node : flat_tree_children)
      node->LazyReattachIfAttached();
  }
  HTMLElement::DetachLayoutTree(context);
}

void HTMLSlotElement::RebuildDistributedChildrenLayoutTrees(
    WhitespaceAttacher& whitespace_attacher) {
  if (!SupportsAssignment())
    return;

  const HeapVector<Member<Node>>& assigned_nodes = AssignedNodes();

  // This loop traverses the nodes from right to left for the same reason as the
  // one described in ContainerNode::RebuildChildrenLayoutTrees().
  for (auto it = assigned_nodes.rbegin(); it != assigned_nodes.rend(); ++it) {
    RebuildLayoutTreeForChild(*it, whitespace_attacher);
  }
}

void HTMLSlotElement::AttributeChanged(
    const AttributeModificationParams& params) {
  if (params.name == nameAttr) {
    if (ShadowRoot* root = ContainingShadowRoot()) {
      if (root->IsV1() && params.old_value != params.new_value) {
        root->GetSlotAssignment().DidRenameSlot(
            NormalizeSlotName(params.old_value), *this);
      }
    }
  }
  HTMLElement::AttributeChanged(params);
}

Node::InsertionNotificationRequest HTMLSlotElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  if (SupportsAssignment()) {
    ShadowRoot* root = ContainingShadowRoot();
    DCHECK(root);
    DCHECK(root->IsV1());
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
  // shadow tree. An inclusive shadow-including ancestor of the shadow host was
  // originally removed from its parent.
  // 2. Or (this slot is not in a tree scope), this slot's inclusive
  // ancestor was orginally removed from its parent (== insertion point). This
  // slot and the originally removed node was in the same tree before removal.

  // For exmaple, given the following trees, (srN: = shadow root, sN: = slot)
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
  } else if (insertion_point.IsInV1ShadowTree()) {
    // This slot was in a shadow tree and got disconnected from the shadow tree.
    // In the above example, (this slot == s1), (insertion point == d)
    // and (insertion_point->ContainingShadowRoot == sr1).
    insertion_point.ContainingShadowRoot()->GetSlotAssignment().DidRemoveSlot(
        *this);
    ClearAssignedNodesAndFlatTreeChildren();
  } else {
    DCHECK(assigned_nodes_.IsEmpty());
  }

  HTMLElement::RemovedFrom(insertion_point);
}

void HTMLSlotElement::DidRecalcStyle(StyleRecalcChange change) {
  if (change < kIndependentInherit)
    return;
  for (auto& node : assigned_nodes_) {
    if (change == kReattach && node->IsElementNode()) {
      DCHECK(node->ShouldCallRecalcStyle(kReattach));
      ToElement(node)->RecalcStyle(kReattach);
      continue;
    }
    // We only need to pick up changes for inherited style, we do not actually
    // need to match rules against this element but we do that for
    // simplicity. If we ever stop doing this then we need to update
    // StyleInvalidator::Invalidate as described in the comment there.
    node->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(
            style_change_reason::kPropagateInheritChangeToDistributedNodes));
  }
}

void HTMLSlotElement::LazyReattachNodesByDynamicProgramming(
    const HeapVector<Member<Node>>& nodes1,
    const HeapVector<Member<Node>>& nodes2) {
  // Use dynamic programming to minimize the number of nodes being reattached.
  using LCSTable = std::array<std::array<wtf_size_t, kLCSTableSizeLimit>,
                              kLCSTableSizeLimit>;
  using Backtrack = std::pair<wtf_size_t, wtf_size_t>;
  using BacktrackTable =
      std::array<std::array<Backtrack, kLCSTableSizeLimit>, kLCSTableSizeLimit>;

  DEFINE_STATIC_LOCAL(LCSTable*, lcs_table, (new LCSTable));
  DEFINE_STATIC_LOCAL(BacktrackTable*, backtrack_table, (new BacktrackTable));

  FillLongestCommonSubsequenceDynamicProgrammingTable(
      nodes1, nodes2, *lcs_table, *backtrack_table);

  wtf_size_t r = nodes1.size();
  wtf_size_t c = nodes2.size();
  while (r > 0 && c > 0) {
    Backtrack backtrack = (*backtrack_table)[r][c];
    if (backtrack == std::make_pair(r - 1, c - 1)) {
      DCHECK_EQ(nodes1[r - 1], nodes2[c - 1]);
    } else if (backtrack == std::make_pair(r - 1, c)) {
      nodes1[r - 1]->LazyReattachIfAttached();
    } else {
      DCHECK(backtrack == std::make_pair(r, c - 1));
      nodes2[c - 1]->LazyReattachIfAttached();
    }
    std::tie(r, c) = backtrack;
  }
  if (r > 0) {
    for (wtf_size_t i = 0; i < r; ++i)
      nodes1[i]->LazyReattachIfAttached();
  } else if (c > 0) {
    for (wtf_size_t i = 0; i < c; ++i)
      nodes2[i]->LazyReattachIfAttached();
  }
}

void HTMLSlotElement::LazyReattachNodesIfNeeded(
    const HeapVector<Member<Node>>& nodes1,
    const HeapVector<Member<Node>>& nodes2) {
  if (nodes1 == nodes2)
    return;
  probe::didPerformSlotDistribution(this);

  if (nodes1.size() + 1 > kLCSTableSizeLimit ||
      nodes2.size() + 1 > kLCSTableSizeLimit) {
    // Since DP takes O(N^2), we don't use DP if the size is larger than the
    // pre-defined limit.
    LazyReattachNodesNaive(nodes1, nodes2);
  } else {
    LazyReattachNodesByDynamicProgramming(nodes1, nodes2);
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
  SetNeedsDistributionRecalcWillBeSetNeedsAssignmentRecalc();
  CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
}

void HTMLSlotElement::LazyReattachNodesNaive(
    const HeapVector<Member<Node>>& nodes1,
    const HeapVector<Member<Node>>& nodes2) {
  // TODO(hayato): Use some heuristic to avoid reattaching all nodes
  for (auto& node : nodes1)
    node->LazyReattachIfAttached();
  for (auto& node : nodes2)
    node->LazyReattachIfAttached();
}

void HTMLSlotElement::
    SetNeedsDistributionRecalcWillBeSetNeedsAssignmentRecalc() {
  ContainingShadowRoot()->GetSlotAssignment().SetNeedsAssignmentRecalc();
}

void HTMLSlotElement::DidSlotChange(SlotChangeType slot_change_type) {
  DCHECK(SupportsAssignment());
  if (slot_change_type == SlotChangeType::kSignalSlotChangeEvent)
    EnqueueSlotChangeEvent();
  SetNeedsDistributionRecalcWillBeSetNeedsAssignmentRecalc();
  // Check slotchange recursively since this slotchange may cause another
  // slotchange.
  CheckSlotChange(SlotChangeType::kSuppressSlotChangeEvent);
}

void HTMLSlotElement::CheckFallbackAfterInsertedIntoShadowTree() {
  DCHECK(SupportsAssignment());
  if (HasSlotableChild()) {
    // We use kSuppress here because a slotchange event shouldn't be
    // dispatched if a slot being inserted don't get any assigned
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
  // TODO(kochi): This suppresses slotchange event on user-agent shadows,
  // but could be improved further by not running change detection logic
  // in SlotAssignment::Did{Add,Remove}SlotInternal etc., although naive
  // skipping turned out breaking fallback content handling.
  if (IsInUserAgentShadowRoot())
    return;
  if (slotchange_event_enqueued_)
    return;
  MutationObserver::EnqueueSlotChange(*this);
  slotchange_event_enqueued_ = true;
}

bool HTMLSlotElement::HasAssignedNodesSlow() const {
  ShadowRoot* root = ContainingShadowRoot();
  DCHECK(root);
  DCHECK(root->IsV1());
  SlotAssignment& assignment = root->GetSlotAssignment();
  if (assignment.FindSlotByName(GetName()) != this)
    return false;
  return assignment.FindHostChildBySlotName(GetName());
}

bool HTMLSlotElement::FindHostChildWithSameSlotName() const {
  ShadowRoot* root = ContainingShadowRoot();
  DCHECK(root);
  DCHECK(root->IsV1());
  SlotAssignment& assignment = root->GetSlotAssignment();
  return assignment.FindHostChildBySlotName(GetName());
}

int HTMLSlotElement::tabIndex() const {
  return Element::tabIndex();
}

void HTMLSlotElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(assigned_nodes_);
  visitor->Trace(flat_tree_children_);
  visitor->Trace(assigned_nodes_candidates_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink

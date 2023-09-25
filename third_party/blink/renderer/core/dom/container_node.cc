/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/container_node.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/selector_query.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/child_frame_disconnector.h"
#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/class_collection.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/name_node_list.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_child_removal_tracker.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_move_scope.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_recalc_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/events/mutation_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/radio_node_list.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_tag_collection.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

static void DispatchChildInsertionEvents(Node&);
static void DispatchChildRemovalEvents(Node&);

namespace {

// This class is helpful to detect necessity of
// RecheckNodeInsertionStructuralPrereq() after removeChild*() inside
// InsertBefore(), AppendChild(), and ReplaceChild().
//
// After removeChild*(), we can detect necessity of
// RecheckNodeInsertionStructuralPrereq() by
//  - DOM tree version of |node_document_| was increased by at most one.
//  - If |node| and |parent| are in different documents, Document for
//    |parent| must not be changed.
class DOMTreeMutationDetector {
  STACK_ALLOCATED();

 public:
  DOMTreeMutationDetector(const Node& node, const Node& parent)
      : node_(&node),
        node_document_(&node.GetDocument()),
        parent_document_(&parent.GetDocument()),
        parent_(&parent),
        original_node_document_version_(node_document_->DomTreeVersion()),
        original_parent_document_version_(parent_document_->DomTreeVersion()) {}

  bool NeedsRecheck() {
    if (node_document_ != node_->GetDocument()) {
      return false;
    }
    if (node_document_->DomTreeVersion() > original_node_document_version_ + 1)
      return false;
    if (parent_document_ != parent_->GetDocument())
      return false;
    if (node_document_ == parent_document_)
      return true;
    return parent_document_->DomTreeVersion() ==
           original_parent_document_version_;
  }

 private:
  const Node* const node_;
  Document* const node_document_;
  Document* const parent_document_;
  const Node* const parent_;
  const uint64_t original_node_document_version_;
  const uint64_t original_parent_document_version_;
};

inline bool CheckReferenceChildParent(const Node& parent,
                                      const Node* next,
                                      const Node* old_child,
                                      ExceptionState& exception_state) {
  if (next && next->parentNode() != &parent) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "The node before which the new node is "
                                      "to be inserted is not a child of this "
                                      "node.");
    return false;
  }
  if (old_child && old_child->parentNode() != &parent) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The node to be replaced is not a child of this node.");
    return false;
  }
  return true;
}

}  // namespace

// This dispatches various events; DOM mutation events, blur events, IFRAME
// unload events, etc.
// Returns true if DOM mutation should be proceeded.
static inline bool CollectChildrenAndRemoveFromOldParent(
    Node& node,
    NodeVector& nodes,
    ExceptionState& exception_state) {
  NodeMoveScope::SetCurrentNodeBeingRemoved(node);
  if (auto* fragment = DynamicTo<DocumentFragment>(node)) {
    GetChildNodes(*fragment, nodes);
    fragment->RemoveChildren();
    return !nodes.empty();
  }
  nodes.push_back(&node);
  if (ContainerNode* old_parent = node.parentNode())
    old_parent->RemoveChild(&node, exception_state);
  return !exception_state.HadException() && !nodes.empty();
}

void ContainerNode::ParserTakeAllChildrenFrom(ContainerNode& old_parent) {
  while (Node* child = old_parent.firstChild()) {
    // Explicitly remove since appending can fail, but this loop shouldn't be
    // infinite.
    old_parent.ParserRemoveChild(*child);
    ParserAppendChild(child);
  }
}

ContainerNode::~ContainerNode() {
  DCHECK(isConnected() || !NeedsStyleRecalc());
}

DISABLE_CFI_PERF
bool ContainerNode::IsChildTypeAllowed(const Node& child) const {
  auto* child_fragment = DynamicTo<DocumentFragment>(child);
  if (!child_fragment)
    return ChildTypeAllowed(child.getNodeType());

  for (Node* node = child_fragment->firstChild(); node;
       node = node->nextSibling()) {
    if (!ChildTypeAllowed(node->getNodeType()))
      return false;
  }
  return true;
}

// Returns true if |new_child| contains this node. In that case,
// |exception_state| has an exception.
// https://dom.spec.whatwg.org/#concept-tree-host-including-inclusive-ancestor
bool ContainerNode::IsHostIncludingInclusiveAncestorOfThis(
    const Node& new_child,
    ExceptionState& exception_state) const {
  // Non-ContainerNode can contain nothing.
  if (!new_child.IsContainerNode())
    return false;

  bool child_contains_parent = false;
  if (IsInShadowTree() || GetDocument().IsTemplateDocument()) {
    child_contains_parent = new_child.ContainsIncludingHostElements(*this);
  } else {
    const Node& root = TreeRoot();
    auto* fragment = DynamicTo<DocumentFragment>(root);
    if (fragment && fragment->IsTemplateContent()) {
      child_contains_parent = new_child.ContainsIncludingHostElements(*this);
    } else {
      child_contains_parent = new_child.contains(this);
    }
  }
  if (child_contains_parent) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "The new child element contains the parent.");
  }
  return child_contains_parent;
}

// EnsurePreInsertionValidity() is an implementation of step 2 to 6 of
// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity and
// https://dom.spec.whatwg.org/#concept-node-replace .
DISABLE_CFI_PERF
bool ContainerNode::EnsurePreInsertionValidity(
    const Node& new_child,
    const Node* next,
    const Node* old_child,
    ExceptionState& exception_state) const {
  DCHECK(!(next && old_child));

  // Use common case fast path if possible.
  if ((new_child.IsElementNode() || new_child.IsTextNode()) &&
      IsElementNode()) {
    DCHECK(IsChildTypeAllowed(new_child));
    // 2. If node is a host-including inclusive ancestor of parent, throw a
    // HierarchyRequestError.
    if (IsHostIncludingInclusiveAncestorOfThis(new_child, exception_state))
      return false;
    // 3. If child is not null and its parent is not parent, then throw a
    // NotFoundError.
    return CheckReferenceChildParent(*this, next, old_child, exception_state);
  }

  // This should never happen, but also protect release builds from tree
  // corruption.
  DCHECK(!new_child.IsPseudoElement());
  if (new_child.IsPseudoElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "The new child element is a pseudo-element.");
    return false;
  }

  if (auto* document = DynamicTo<Document>(this)) {
    // Step 2 is unnecessary. No one can have a Document child.
    // Step 3:
    if (!CheckReferenceChildParent(*this, next, old_child, exception_state))
      return false;
    // Step 4-6.
    return document->CanAcceptChild(new_child, next, old_child,
                                    exception_state);
  }

  // 2. If node is a host-including inclusive ancestor of parent, throw a
  // HierarchyRequestError.
  if (IsHostIncludingInclusiveAncestorOfThis(new_child, exception_state))
    return false;

  // 3. If child is not null and its parent is not parent, then throw a
  // NotFoundError.
  if (!CheckReferenceChildParent(*this, next, old_child, exception_state))
    return false;

  // 4. If node is not a DocumentFragment, DocumentType, Element, Text,
  // ProcessingInstruction, or Comment node, throw a HierarchyRequestError.
  // 5. If either node is a Text node and parent is a document, or node is a
  // doctype and parent is not a document, throw a HierarchyRequestError.
  if (!IsChildTypeAllowed(new_child)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "Nodes of type '" + new_child.nodeName() +
            "' may not be inserted inside nodes of type '" + nodeName() + "'.");
    return false;
  }

  // Step 6 is unnecessary for non-Document nodes.
  return true;
}

// We need this extra structural check because prior DOM mutation operations
// dispatched synchronous events, so their handlers may have modified DOM
// trees.
bool ContainerNode::RecheckNodeInsertionStructuralPrereq(
    const NodeVector& new_children,
    const Node* next,
    ExceptionState& exception_state) {
  for (const auto& child : new_children) {
    if (child->parentNode()) {
      // A new child was added to another parent before adding to this
      // node.  Firefox and Edge don't throw in this case.
      return false;
    }
    if (auto* document = DynamicTo<Document>(this)) {
      // For Document, no need to check host-including inclusive ancestor
      // because a Document node can't be a child of other nodes.
      // However, status of existing doctype or root element might be changed
      // and we need to check it again.
      if (!document->CanAcceptChild(*child, next, nullptr, exception_state))
        return false;
    } else {
      if (IsHostIncludingInclusiveAncestorOfThis(*child, exception_state))
        return false;
    }
  }
  return CheckReferenceChildParent(*this, next, nullptr, exception_state);
}

template <typename Functor>
void ContainerNode::InsertNodeVector(
    const NodeVector& targets,
    Node* next,
    const Functor& mutator,
    NodeVector* post_insertion_notification_targets) {
  DCHECK(post_insertion_notification_targets);
  probe::WillInsertDOMNode(this);
  {
    EventDispatchForbiddenScope assert_no_event_dispatch;
    ScriptForbiddenScope forbid_script;
    for (const auto& target_node : targets) {
      DCHECK(target_node);
      DCHECK(!target_node->parentNode());
      Node& child = *target_node;
      mutator(*this, child, next);
      ChildListMutationScope(*this).ChildAdded(child);
      if (GetDocument().MayContainShadowRoots())
        child.CheckSlotChangeAfterInserted();
      probe::DidInsertDOMNode(&child);
      NotifyNodeInsertedInternal(child, *post_insertion_notification_targets);
    }
  }
}

void ContainerNode::DidInsertNodeVector(
    const NodeVector& targets,
    Node* next,
    const NodeVector& post_insertion_notification_targets) {
  Node* unchanged_previous =
      targets.size() > 0 ? targets[0]->previousSibling() : nullptr;
  for (const auto& target_node : targets) {
    ChildrenChanged(ChildrenChange::ForInsertion(
        *target_node, unchanged_previous, next, ChildrenChangeSource::kAPI));
  }
  for (const auto& descendant : post_insertion_notification_targets) {
    if (descendant->isConnected())
      descendant->DidNotifySubtreeInsertionsToDocument();
  }
  for (const auto& target_node : targets) {
    if (target_node->parentNode() == this)
      DispatchChildInsertionEvents(*target_node);
  }
  DispatchSubtreeModifiedEvent();
}

class ContainerNode::AdoptAndInsertBefore {
 public:
  inline void operator()(ContainerNode& container,
                         Node& child,
                         Node* next) const {
    DCHECK(next);
    DCHECK_EQ(next->parentNode(), &container);
    container.GetTreeScope().AdoptIfNeeded(child);
    container.InsertBeforeCommon(*next, child);
  }
};

class ContainerNode::AdoptAndAppendChild {
 public:
  inline void operator()(ContainerNode& container, Node& child, Node*) const {
    container.GetTreeScope().AdoptIfNeeded(child);
    container.AppendChildCommon(child);
  }
};

Node* ContainerNode::InsertBefore(Node* new_child,
                                  Node* ref_child,
                                  ExceptionState& exception_state) {
  DCHECK(new_child);
  // https://dom.spec.whatwg.org/#concept-node-pre-insert

  // insertBefore(node, null) is equivalent to appendChild(node)
  if (!ref_child)
    return AppendChild(new_child, exception_state);

  // 1. Ensure pre-insertion validity of node into parent before child.
  if (!EnsurePreInsertionValidity(*new_child, ref_child, nullptr,
                                  exception_state))
    return new_child;

  // 2. Let reference child be child.
  // 3. If reference child is node, set it to node’s next sibling.
  if (ref_child == new_child) {
    if (!new_child->HasNextSibling()) {
      return AppendChild(new_child, exception_state);
    }
    ref_child = new_child->nextSibling();
  }

  // 4. Adopt node into parent’s node document.
  NodeVector targets;
  DOMTreeMutationDetector detector(*new_child, *this);
  NodeMoveScope node_move_scope(
      *this, firstChild() == ref_child
                 ? NodeMoveScopeType::kInsertBeforeAllChildren
                 : NodeMoveScopeType::kOther);
  if (!CollectChildrenAndRemoveFromOldParent(*new_child, targets,
                                             exception_state))
    return new_child;
  if (!detector.NeedsRecheck()) {
    if (!RecheckNodeInsertionStructuralPrereq(targets, ref_child,
                                              exception_state))
      return new_child;
  }

  // 5. Insert node into parent before reference child.
  NodeVector post_insertion_notification_targets;
  {
    SlotAssignmentRecalcForbiddenScope forbid_slot_recalc(GetDocument());
    ChildListMutationScope mutation(*this);
    InsertNodeVector(targets, ref_child, AdoptAndInsertBefore(),
                     &post_insertion_notification_targets);
  }
  DidInsertNodeVector(targets, ref_child, post_insertion_notification_targets);
  return new_child;
}

Node* ContainerNode::InsertBefore(Node* new_child, Node* ref_child) {
  return InsertBefore(new_child, ref_child, ASSERT_NO_EXCEPTION);
}

void ContainerNode::InsertBeforeCommon(Node& next_child, Node& new_child) {
#if DCHECK_IS_ON()
  DCHECK(EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(ScriptForbiddenScope::IsScriptForbidden());
  // Use insertBefore if you need to handle reparenting (and want DOM mutation
  // events).
  DCHECK(!new_child.parentNode());
  DCHECK(!new_child.HasNextSibling());
  DCHECK(!new_child.HasPreviousSibling());
  DCHECK(!new_child.IsShadowRoot());

  Node* prev = next_child.previousSibling();
  DCHECK_NE(last_child_, prev);
  next_child.SetPreviousSibling(&new_child);
  if (prev) {
    DCHECK_NE(firstChild(), next_child);
    DCHECK_EQ(prev->nextSibling(), next_child);
    prev->SetNextSibling(&new_child);
  } else {
    DCHECK(firstChild() == next_child);
    SetFirstChild(&new_child);
  }
  new_child.SetParentOrShadowHostNode(this);
  new_child.SetPreviousSibling(prev);
  new_child.SetNextSibling(&next_child);
}

void ContainerNode::AppendChildCommon(Node& child) {
#if DCHECK_IS_ON()
  DCHECK(EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(ScriptForbiddenScope::IsScriptForbidden());

  child.SetParentOrShadowHostNode(this);
  if (last_child_) {
    child.SetPreviousSibling(last_child_);
    last_child_->SetNextSibling(&child);
  } else {
    SetFirstChild(&child);
  }
  SetLastChild(&child);
}

bool ContainerNode::CheckParserAcceptChild(const Node& new_child) const {
  auto* document = DynamicTo<Document>(this);
  if (!document)
    return true;
  // TODO(esprehn): Are there other conditions where the parser can create
  // invalid trees?
  return document->CanAcceptChild(new_child, nullptr, nullptr,
                                  IGNORE_EXCEPTION_FOR_TESTING);
}

void ContainerNode::ParserInsertBefore(Node* new_child, Node& next_child) {
  DCHECK(new_child);
  DCHECK(next_child.parentNode() == this ||
         (DynamicTo<DocumentFragment>(this) &&
          DynamicTo<DocumentFragment>(this)->IsTemplateContent()));
  DCHECK(!new_child->IsDocumentFragment());
  DCHECK(!IsA<HTMLTemplateElement>(this));

  if (next_child.previousSibling() == new_child ||
      &next_child == new_child)  // nothing to do
    return;

  if (!CheckParserAcceptChild(*new_child))
    return;

  // FIXME: parserRemoveChild can run script which could then insert the
  // newChild back into the page. Loop until the child is actually removed.
  // See: fast/parser/execute-script-during-adoption-agency-removal.html
  while (ContainerNode* parent = new_child->parentNode())
    parent->ParserRemoveChild(*new_child);

  // This can happen if foster parenting moves nodes into a template
  // content document, but next_child is still a "direct" child of the
  // template.
  if (next_child.parentNode() != this)
    return;

  if (GetDocument() != new_child->GetDocument())
    GetDocument().adoptNode(new_child, ASSERT_NO_EXCEPTION);

  {
    EventDispatchForbiddenScope assert_no_event_dispatch;
    ScriptForbiddenScope forbid_script;

    AdoptAndInsertBefore()(*this, *new_child, &next_child);
    DCHECK_EQ(new_child->ConnectedSubframeCount(), 0u);
    ChildListMutationScope(*this).ChildAdded(*new_child);
  }

  NotifyNodeInserted(*new_child, ChildrenChangeSource::kParser);
}

Node* ContainerNode::ReplaceChild(Node* new_child,
                                  Node* old_child,
                                  ExceptionState& exception_state) {
  DCHECK(new_child);
  // https://dom.spec.whatwg.org/#concept-node-replace

  if (!old_child) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "The node to be replaced is null.");
    return nullptr;
  }

  // Step 2 to 6.
  if (!EnsurePreInsertionValidity(*new_child, nullptr, old_child,
                                  exception_state))
    return old_child;

  // 7. Let reference child be child’s next sibling.
  Node* next = old_child->nextSibling();
  // 8. If reference child is node, set it to node’s next sibling.
  if (next == new_child)
    next = new_child->nextSibling();

  bool needs_recheck = false;
  // 10. Adopt node into parent’s node document.
  // TODO(tkent): Actually we do only RemoveChild() as a part of 'adopt'
  // operation.
  //
  // Though the following CollectChildrenAndRemoveFromOldParent() also calls
  // RemoveChild(), we'd like to call RemoveChild() here to make a separated
  // MutationRecord.
  if (ContainerNode* new_child_parent = new_child->parentNode()) {
    DOMTreeMutationDetector detector(*new_child, *this);
    new_child_parent->RemoveChild(new_child, exception_state);
    if (exception_state.HadException())
      return nullptr;
    if (!detector.NeedsRecheck())
      needs_recheck = true;
  }

  NodeVector targets;
  NodeVector post_insertion_notification_targets;
  {
    // 9. Let previousSibling be child’s previous sibling.
    // 11. Let removedNodes be the empty list.
    // 15. Queue a mutation record of "childList" for target parent with
    // addedNodes nodes, removedNodes removedNodes, nextSibling reference child,
    // and previousSibling previousSibling.
    ChildListMutationScope mutation(*this);

    // 12. If child’s parent is not null, run these substeps:
    //    1. Set removedNodes to a list solely containing child.
    //    2. Remove child from its parent with the suppress observers flag set.
    if (ContainerNode* old_child_parent = old_child->parentNode()) {
      DOMTreeMutationDetector detector(*old_child, *this);
      old_child_parent->RemoveChild(old_child, exception_state);
      if (exception_state.HadException())
        return nullptr;
      if (!detector.NeedsRecheck())
        needs_recheck = true;
    }

    SlotAssignmentRecalcForbiddenScope forbid_slot_recalc(GetDocument());

    // 13. Let nodes be node’s children if node is a DocumentFragment node, and
    // a list containing solely node otherwise.
    DOMTreeMutationDetector detector(*new_child, *this);
    NodeMoveScope node_move_scope(
        *this, !next ? NodeMoveScopeType::kAppendAfterAllChildren
                     : (firstChild() == next
                            ? NodeMoveScopeType::kInsertBeforeAllChildren
                            : NodeMoveScopeType::kOther));
    if (!CollectChildrenAndRemoveFromOldParent(*new_child, targets,
                                               exception_state))
      return old_child;
    if (!detector.NeedsRecheck() || needs_recheck) {
      if (!RecheckNodeInsertionStructuralPrereq(targets, next, exception_state))
        return old_child;
    }

    // 10. Adopt node into parent’s node document.
    // 14. Insert node into parent before reference child with the suppress
    // observers flag set.
    if (next) {
      InsertNodeVector(targets, next, AdoptAndInsertBefore(),
                       &post_insertion_notification_targets);
    } else {
      InsertNodeVector(targets, nullptr, AdoptAndAppendChild(),
                       &post_insertion_notification_targets);
    }
  }
  DidInsertNodeVector(targets, next, post_insertion_notification_targets);

  // 16. Return child.
  return old_child;
}

Node* ContainerNode::ReplaceChild(Node* new_child, Node* old_child) {
  return ReplaceChild(new_child, old_child, ASSERT_NO_EXCEPTION);
}

void ContainerNode::WillRemoveChild(Node& child) {
  DCHECK_EQ(child.parentNode(), this);
  ChildListMutationScope(*this).WillRemoveChild(child);
  child.NotifyMutationObserversNodeWillDetach();
  DispatchChildRemovalEvents(child);
  ChildFrameDisconnector(child).Disconnect();
  if (GetDocument() != child.GetDocument()) {
    // |child| was moved to another document by the DOM mutation event handler.
    return;
  }

  // |nodeWillBeRemoved()| must be run after |ChildFrameDisconnector|, because
  // |ChildFrameDisconnector| may remove the node, resulting in an invalid
  // state.
  ScriptForbiddenScope script_forbidden_scope;
  EventDispatchForbiddenScope assert_no_event_dispatch;
  // e.g. mutation event listener can create a new range.
  GetDocument().NodeWillBeRemoved(child);

  if (auto* child_element = DynamicTo<Element>(child)) {
    if (auto* context = child_element->GetDisplayLockContext())
      context->NotifyWillDisconnect();
  }
}

void ContainerNode::WillRemoveChildren() {
  NodeVector children;
  GetChildNodes(*this, children);

  ChildListMutationScope mutation(*this);
  for (const auto& node : children) {
    DCHECK(node);
    Node& child = *node;
    mutation.WillRemoveChild(child);
    child.NotifyMutationObserversNodeWillDetach();
    DispatchChildRemovalEvents(child);
  }

  ChildFrameDisconnector(*this).Disconnect(
      ChildFrameDisconnector::kDescendantsOnly);
}

LayoutBox* ContainerNode::GetLayoutBoxForScrolling() const {
  return GetLayoutBox();
}

void ContainerNode::Trace(Visitor* visitor) const {
  visitor->Trace(first_child_);
  visitor->Trace(last_child_);
  Node::Trace(visitor);
}

static bool ShouldMergeCombinedTextAfterRemoval(const Node& old_child) {
  DCHECK(!old_child.parentNode()->GetForceReattachLayoutTree());

  auto* const layout_object = old_child.GetLayoutObject();
  if (!layout_object)
    return false;

  // Request to merge previous and next |LayoutNGTextCombine| of |child|.
  // See http:://crbug.com/1227066
  auto* const previous_sibling = layout_object->PreviousSibling();
  if (!previous_sibling)
    return false;
  auto* const next_sibling = layout_object->NextSibling();
  if (!next_sibling)
    return false;
  if (UNLIKELY(IsA<LayoutTextCombine>(previous_sibling)) &&
      UNLIKELY(IsA<LayoutTextCombine>(next_sibling))) {
    return true;
  }

  // Request to merge combined texts in anonymous block.
  // See http://crbug.com/1233432
  if (!previous_sibling->IsAnonymousBlock() ||
      !next_sibling->IsAnonymousBlock())
    return false;

  return UNLIKELY(IsA<LayoutTextCombine>(previous_sibling->SlowLastChild())) &&
         UNLIKELY(IsA<LayoutTextCombine>(next_sibling->SlowFirstChild()));
}

Node* ContainerNode::RemoveChild(Node* old_child,
                                 ExceptionState& exception_state) {
  // NotFoundError: Raised if oldChild is not a child of this node.
  // FIXME: We should never really get PseudoElements in here, but editing will
  // sometimes attempt to remove them still. We should fix that and enable this
  // DCHECK.  DCHECK(!oldChild->isPseudoElement())
  if (!old_child || old_child->parentNode() != this ||
      old_child->IsPseudoElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The node to be removed is not a child of this node.");
    return nullptr;
  }

  Node* child = old_child;

  GetDocument().RemoveFocusedElementOfSubtree(*child);

  // Events fired when blurring currently focused node might have moved this
  // child into a different parent.
  if (child->parentNode() != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The node to be removed is no longer a "
        "child of this node. Perhaps it was moved "
        "in a 'blur' event handler?");
    return nullptr;
  }

  WillRemoveChild(*child);

  // TODO(crbug.com/927646): |WillRemoveChild()| may dispatch events that set
  // focus to a node that will be detached, leaving behind a detached focused
  // node. Fix it.

  // Mutation events might have moved this child into a different parent.
  if (child->parentNode() != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The node to be removed is no longer a "
        "child of this node. Perhaps it was moved "
        "in response to a mutation?");
    return nullptr;
  }

  if (!GetForceReattachLayoutTree() &&
      UNLIKELY(ShouldMergeCombinedTextAfterRemoval(*child)))
    SetForceReattachLayoutTree();

  {
    HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
    TreeOrderedMap::RemoveScope tree_remove_scope;
    StyleEngine& engine = GetDocument().GetStyleEngine();
    StyleEngine::DetachLayoutTreeScope detach_scope(engine);
    Node* prev = child->previousSibling();
    Node* next = child->nextSibling();
    {
      SlotAssignmentRecalcForbiddenScope forbid_slot_recalc(GetDocument());
      StyleEngine::DOMRemovalScope style_scope(engine);
      RemoveBetween(prev, next, *child);
      NotifyNodeRemoved(*child);
    }
    ChildrenChanged(ChildrenChange::ForRemoval(*child, prev, next,
                                               ChildrenChangeSource::kAPI));
  }
  DispatchSubtreeModifiedEvent();
  return child;
}

Node* ContainerNode::RemoveChild(Node* old_child) {
  return RemoveChild(old_child, ASSERT_NO_EXCEPTION);
}

void ContainerNode::RemoveBetween(Node* previous_child,
                                  Node* next_child,
                                  Node& old_child) {
  EventDispatchForbiddenScope assert_no_event_dispatch;

  DCHECK_EQ(old_child.parentNode(), this);

  if (InActiveDocument())
    old_child.DetachLayoutTree();

  if (next_child)
    next_child->SetPreviousSibling(previous_child);
  if (previous_child)
    previous_child->SetNextSibling(next_child);
  if (first_child_ == &old_child)
    SetFirstChild(next_child);
  if (last_child_ == &old_child)
    SetLastChild(previous_child);

  old_child.SetPreviousSibling(nullptr);
  old_child.SetNextSibling(nullptr);
  old_child.SetParentOrShadowHostNode(nullptr);

  GetDocument().AdoptIfNeeded(old_child);
}

void ContainerNode::ParserRemoveChild(Node& old_child) {
  DCHECK_EQ(old_child.parentNode(), this);
  DCHECK(!old_child.IsDocumentFragment());

  // This may cause arbitrary Javascript execution via onunload handlers.
  if (old_child.ConnectedSubframeCount())
    ChildFrameDisconnector(old_child).Disconnect();

  if (old_child.parentNode() != this)
    return;

  ChildListMutationScope(*this).WillRemoveChild(old_child);
  old_child.NotifyMutationObserversNodeWillDetach();

  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  TreeOrderedMap::RemoveScope tree_remove_scope;
  StyleEngine& engine = GetDocument().GetStyleEngine();
  StyleEngine::DetachLayoutTreeScope detach_scope(engine);

  Node* prev = old_child.previousSibling();
  Node* next = old_child.nextSibling();
  {
    StyleEngine::DOMRemovalScope style_scope(engine);
    RemoveBetween(prev, next, old_child);
    NotifyNodeRemoved(old_child);
  }
  ChildrenChanged(ChildrenChange::ForRemoval(old_child, prev, next,
                                             ChildrenChangeSource::kParser));
}

// This differs from other remove functions because it forcibly removes all the
// children, regardless of read-only status or event exceptions, e.g.
void ContainerNode::RemoveChildren(SubtreeModificationAction action) {
  if (!first_child_)
    return;

  // Do any prep work needed before actually starting to detach
  // and remove... e.g. stop loading frames, fire unload events.
  WillRemoveChildren();

  {
    // Removing focus can cause frames to load, either via events (focusout,
    // blur) or widget updates (e.g., for <embed>).
    SubframeLoadingDisabler disabler(*this);

    // Exclude this node when looking for removed focusedElement since only
    // children will be removed.
    // This must be later than willRemoveChildren, which might change focus
    // state of a child.
    GetDocument().RemoveFocusedElementOfSubtree(*this, true);

    // Removing a node from a selection can cause widget updates.
    GetDocument().NodeChildrenWillBeRemoved(*this);
  }

  HeapVector<Member<Node>> removed_nodes;
  const bool children_changed = ChildrenChangedAllChildrenRemovedNeedsList();
  {
    HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
    TreeOrderedMap::RemoveScope tree_remove_scope;
    StyleEngine& engine = GetDocument().GetStyleEngine();
    StyleEngine::DetachLayoutTreeScope detach_scope(engine);
    bool has_element_child = false;
    {
      SlotAssignmentRecalcForbiddenScope forbid_slot_recalc(GetDocument());
      StyleEngine::DOMRemovalScope style_scope(engine);
      EventDispatchForbiddenScope assert_no_event_dispatch;
      ScriptForbiddenScope forbid_script;

      while (Node* child = first_child_) {
        if (child->IsElementNode()) {
          has_element_child = true;
        }
        RemoveBetween(nullptr, child->nextSibling(), *child);
        NotifyNodeRemoved(*child);
        if (children_changed)
          removed_nodes.push_back(child);
      }
    }

    ChildrenChange change = {
        .type = ChildrenChangeType::kAllChildrenRemoved,
        .by_parser = ChildrenChangeSource::kAPI,
        .affects_elements = has_element_child
                                ? ChildrenChangeAffectsElements::kYes
                                : ChildrenChangeAffectsElements::kNo,
        .removed_nodes = std::move(removed_nodes)};
    ChildrenChanged(change);
  }

  if (action == kDispatchSubtreeModifiedEvent)
    DispatchSubtreeModifiedEvent();
}

Node* ContainerNode::AppendChild(Node* new_child,
                                 ExceptionState& exception_state) {
  DCHECK(new_child);
  // Make sure adding the new child is ok
  if (!EnsurePreInsertionValidity(*new_child, nullptr, nullptr,
                                  exception_state))
    return new_child;

  NodeVector targets;
  DOMTreeMutationDetector detector(*new_child, *this);
  NodeMoveScope node_move_scope(*this,
                                NodeMoveScopeType::kAppendAfterAllChildren);
  if (!CollectChildrenAndRemoveFromOldParent(*new_child, targets,
                                             exception_state))
    return new_child;
  if (!detector.NeedsRecheck()) {
    if (!RecheckNodeInsertionStructuralPrereq(targets, nullptr,
                                              exception_state))
      return new_child;
  }

  NodeVector post_insertion_notification_targets;
  {
    SlotAssignmentRecalcForbiddenScope forbid_slot_recalc(GetDocument());
    ChildListMutationScope mutation(*this);
    InsertNodeVector(targets, nullptr, AdoptAndAppendChild(),
                     &post_insertion_notification_targets);
  }
  DidInsertNodeVector(targets, nullptr, post_insertion_notification_targets);
  return new_child;
}

Node* ContainerNode::AppendChild(Node* new_child) {
  return AppendChild(new_child, ASSERT_NO_EXCEPTION);
}

void ContainerNode::ParserAppendChild(Node* new_child) {
  DCHECK(new_child);
  DCHECK(!new_child->IsDocumentFragment());
  DCHECK(!IsA<HTMLTemplateElement>(this));

  RUNTIME_CALL_TIMER_SCOPE(GetDocument().GetAgent().isolate(),
                           RuntimeCallStats::CounterId::kParserAppendChild);

  if (!CheckParserAcceptChild(*new_child))
    return;

  // FIXME: parserRemoveChild can run script which could then insert the
  // newChild back into the page. Loop until the child is actually removed.
  // See: fast/parser/execute-script-during-adoption-agency-removal.html
  while (ContainerNode* parent = new_child->parentNode())
    parent->ParserRemoveChild(*new_child);

  if (GetDocument() != new_child->GetDocument())
    GetDocument().adoptNode(new_child, ASSERT_NO_EXCEPTION);

  {
    EventDispatchForbiddenScope assert_no_event_dispatch;
    ScriptForbiddenScope forbid_script;

    AdoptAndAppendChild()(*this, *new_child, nullptr);
    DCHECK_EQ(new_child->ConnectedSubframeCount(), 0u);
    ChildListMutationScope(*this).ChildAdded(*new_child);
  }

  NotifyNodeInserted(*new_child, ChildrenChangeSource::kParser);
}

DISABLE_CFI_PERF
void ContainerNode::NotifyNodeInserted(Node& root,
                                       ChildrenChangeSource source) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(!root.IsShadowRoot());

  if (GetDocument().MayContainShadowRoots())
    root.CheckSlotChangeAfterInserted();

  probe::DidInsertDOMNode(&root);

  NodeVector post_insertion_notification_targets;
  NotifyNodeInsertedInternal(root, post_insertion_notification_targets);

  ChildrenChanged(ChildrenChange::ForInsertion(root, root.previousSibling(),
                                               root.nextSibling(), source));

  for (const auto& target_node : post_insertion_notification_targets) {
    if (target_node->isConnected())
      target_node->DidNotifySubtreeInsertionsToDocument();
  }
}

DISABLE_CFI_PERF
void ContainerNode::NotifyNodeInsertedInternal(
    Node& root,
    NodeVector& post_insertion_notification_targets) {
  EventDispatchForbiddenScope assert_no_event_dispatch;
  ScriptForbiddenScope forbid_script;

  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    // As an optimization we don't notify leaf nodes when when inserting
    // into detached subtrees that are not in a shadow tree, unless the
    // node has DOM Parts attached.
    if (!isConnected() && !IsInShadowTree() && !node.IsContainerNode() &&
        !node.GetDOMParts()) {
      continue;
    }
    if (Node::kInsertionShouldCallDidNotifySubtreeInsertions ==
        node.InsertedInto(*this))
      post_insertion_notification_targets.push_back(&node);
    if (ShadowRoot* shadow_root = node.GetShadowRoot())
      NotifyNodeInsertedInternal(*shadow_root,
                                 post_insertion_notification_targets);
  }
}

void ContainerNode::NotifyNodeRemoved(Node& root) {
  ScriptForbiddenScope forbid_script;
  EventDispatchForbiddenScope assert_no_event_dispatch;

  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    // As an optimization we skip notifying Text nodes and other leaf nodes
    // of removal when they're not in the Document tree, not in a shadow root,
    // and don't have DOM Parts, since the virtual call to removedFrom is not
    // needed.
    if (!node.IsContainerNode() && !node.IsInTreeScope() &&
        !node.GetDOMParts()) {
      continue;
    }
    node.RemovedFrom(*this);
    if (ShadowRoot* shadow_root = node.GetShadowRoot())
      NotifyNodeRemoved(*shadow_root);
  }
}

void ContainerNode::RemovedFrom(ContainerNode& insertion_point) {
  if (isConnected()) {
    if (NeedsStyleInvalidation()) {
      GetDocument()
          .GetStyleEngine()
          .GetPendingNodeInvalidations()
          .ClearInvalidation(*this);
      ClearNeedsStyleInvalidation();
    }
    ClearChildNeedsStyleInvalidation();
  }
  Node::RemovedFrom(insertion_point);
}

DISABLE_CFI_PERF
void ContainerNode::AttachLayoutTree(AttachContext& context) {
  for (Node* child = firstChild(); child; child = child->nextSibling())
    child->AttachLayoutTree(context);
  Node::AttachLayoutTree(context);
  ClearChildNeedsReattachLayoutTree();
}

void ContainerNode::DetachLayoutTree(bool performing_reattach) {
  for (Node* child = firstChild(); child; child = child->nextSibling())
    child->DetachLayoutTree(performing_reattach);
  Node::DetachLayoutTree(performing_reattach);
}

void ContainerNode::ChildrenChanged(const ChildrenChange& change) {
  GetDocument().IncDOMTreeVersion();
  GetDocument().NotifyChangeChildren(*this, change);
  InvalidateNodeListCachesInAncestors(nullptr, nullptr, &change);
  if (change.IsChildRemoval() ||
      change.type == ChildrenChangeType::kAllChildrenRemoved) {
    GetDocument().GetStyleEngine().ChildrenRemoved(*this);
    return;
  }
  if (!change.IsChildInsertion())
    return;
  Node* inserted_node = change.sibling_changed;
  if (inserted_node->IsContainerNode() || inserted_node->IsTextNode())
    inserted_node->ClearFlatTreeNodeDataIfHostChanged(*this);
  if (!InActiveDocument())
    return;
  if (IsElementNode() && !GetComputedStyle()) {
    // There is no need to mark for style recalc if the parent element does not
    // Already have a ComputedStyle. For instance if we insert nodes into a
    // display:none subtree. If this ContainerNode gets a ComputedStyle during
    // the next style recalc, we will traverse into the inserted children since
    // the ComputedStyle goes from null to non-null.
    return;
  }
  if (inserted_node->IsContainerNode() || inserted_node->IsTextNode())
    inserted_node->SetStyleChangeOnInsertion();
}

bool ContainerNode::ChildrenChangedAllChildrenRemovedNeedsList() const {
  return false;
}

void ContainerNode::ClonePartsFrom(const ContainerNode& node,
                                   NodeCloningData& data) {
  if (!data.Has(CloneOption::kPreserveDOMParts)) {
    return;
  }
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  if (auto* document = DynamicTo<Document>(const_cast<ContainerNode&>(node))) {
    data.ConnectPartRootToClone(document->getPartRoot(),
                                To<Document>(this)->getPartRoot());
  } else if (auto* document_fragment = DynamicTo<DocumentFragment>(
                 const_cast<ContainerNode&>(node))) {
    data.ConnectPartRootToClone(document_fragment->getPartRoot(),
                                To<DocumentFragment>(this)->getPartRoot());
  }
  if (auto* parts = node.GetDOMParts()) {
    data.ConnectNodeToClone(node, *this);
    for (Part* part : *parts) {
      if (part->NodeToSortBy() == node) {
        data.QueueForCloning(*part);
      }
    }
  }
}

void ContainerNode::CloneChildNodesFrom(const ContainerNode& node,
                                        NodeCloningData& data) {
  CHECK(data.Has(CloneOption::kIncludeDescendants));
  for (const Node& child : NodeTraversal::ChildrenOf(node)) {
    child.Clone(GetDocument(), data, this);
  }
}

PhysicalRect ContainerNode::BoundingBox() const {
  if (!GetLayoutObject())
    return PhysicalRect();
  return GetLayoutObject()->AbsoluteBoundingBoxRectHandlingEmptyInline();
}

// This is used by FrameSelection to denote when the active-state of the page
// has changed independent of the focused element changing.
void ContainerNode::FocusStateChanged() {
  // If we're just changing the window's active state and the focused node has
  // no layoutObject we can just ignore the state change.
  if (!GetLayoutObject())
    return;

  StyleChangeType change_type =
      GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
          ? kSubtreeStyleChange
          : kLocalStyleChange;
  SetNeedsStyleRecalc(
      change_type,
      StyleChangeReasonForTracing::CreateWithExtraData(
          style_change_reason::kPseudoClass, style_change_extra_data::g_focus));

  if (auto* this_element = DynamicTo<Element>(this))
    this_element->PseudoStateChanged(CSSSelector::kPseudoFocus);

  InvalidateIfHasEffectiveAppearance();
  FocusVisibleStateChanged();
  FocusWithinStateChanged();
}

void ContainerNode::FocusVisibleStateChanged() {
  if (!RuntimeEnabledFeatures::CSSFocusVisibleEnabled())
    return;
  StyleChangeType change_type =
      GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
          ? kSubtreeStyleChange
          : kLocalStyleChange;
  SetNeedsStyleRecalc(change_type,
                      StyleChangeReasonForTracing::CreateWithExtraData(
                          style_change_reason::kPseudoClass,
                          style_change_extra_data::g_focus_visible));

  if (auto* this_element = DynamicTo<Element>(this))
    this_element->PseudoStateChanged(CSSSelector::kPseudoFocusVisible);
}

void ContainerNode::FocusWithinStateChanged() {
  if (GetComputedStyle() && GetComputedStyle()->AffectedByFocusWithin()) {
    StyleChangeType change_type =
        GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
            ? kSubtreeStyleChange
            : kLocalStyleChange;
    SetNeedsStyleRecalc(change_type,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_focus_within));
  }
  if (auto* this_element = DynamicTo<Element>(this))
    this_element->PseudoStateChanged(CSSSelector::kPseudoFocusWithin);
}

void ContainerNode::SetFocused(bool received,
                               mojom::blink::FocusType focus_type) {
  // Recurse up author shadow trees to mark shadow hosts if it matches :focus.
  // TODO(kochi): Handle UA shadows which marks multiple nodes as focused such
  // as <input type="date"> the same way as author shadow.
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (!root->IsUserAgent())
      OwnerShadowHost()->SetFocused(received, focus_type);
  }

  if (IsFocused() == received)
    return;

  Node::SetFocused(received, focus_type);

  FocusStateChanged();

  if (GetLayoutObject() || received)
    return;

  auto* this_element = DynamicTo<Element>(this);
  // If :focus sets display: none, we lose focus but still need to recalc our
  // style.
  if (!this_element || !this_element->ChildrenOrSiblingsAffectedByFocus()) {
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_focus));
  }
  if (this_element)
    this_element->PseudoStateChanged(CSSSelector::kPseudoFocus);

  if (RuntimeEnabledFeatures::CSSFocusVisibleEnabled()) {
    if (!this_element ||
        !this_element->ChildrenOrSiblingsAffectedByFocusVisible()) {
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::CreateWithExtraData(
                              style_change_reason::kPseudoClass,
                              style_change_extra_data::g_focus_visible));
    }
    if (this_element)
      this_element->PseudoStateChanged(CSSSelector::kPseudoFocusVisible);
  }

  if (!this_element ||
      !this_element->ChildrenOrSiblingsAffectedByFocusWithin()) {
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_focus_within));
  }
  if (this_element)
    this_element->PseudoStateChanged(CSSSelector::kPseudoFocusWithin);
}

void ContainerNode::SetHasFocusWithinUpToAncestor(bool flag, Node* ancestor) {
  for (ContainerNode* node = this; node && node != ancestor;
       node = FlatTreeTraversal::Parent(*node)) {
    node->SetHasFocusWithin(flag);
    node->FocusWithinStateChanged();
  }
}

void ContainerNode::SetDragged(bool new_value) {
  if (new_value == IsDragged())
    return;

  Node::SetDragged(new_value);

  // If :-webkit-drag sets display: none we lose our dragging but still need
  // to recalc our style.
  if (!GetLayoutObject()) {
    if (new_value)
      return;
    auto* this_element = DynamicTo<Element>(this);
    if (this_element && this_element->ChildrenOrSiblingsAffectedByDrag()) {
      this_element->PseudoStateChanged(CSSSelector::kPseudoDrag);

    } else {
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::CreateWithExtraData(
                              style_change_reason::kPseudoClass,
                              style_change_extra_data::g_drag));
    }
    return;
  }

  if (GetComputedStyle()->AffectedByDrag()) {
    StyleChangeType change_type =
        GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
            ? kSubtreeStyleChange
            : kLocalStyleChange;
    SetNeedsStyleRecalc(change_type,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_drag));
  }
  auto* this_element = DynamicTo<Element>(this);
  if (this_element && this_element->ChildrenOrSiblingsAffectedByDrag())
    this_element->PseudoStateChanged(CSSSelector::kPseudoDrag);
}

HTMLCollection* ContainerNode::children() {
  return EnsureCachedCollection<HTMLCollection>(kNodeChildren);
}

Element* ContainerNode::firstElementChild() {
  return ElementTraversal::FirstChild(*this);
}

Element* ContainerNode::lastElementChild() {
  return ElementTraversal::LastChild(*this);
}

unsigned ContainerNode::childElementCount() {
  unsigned count = 0;
  for (Element* child = ElementTraversal::FirstChild(*this); child;
       child = ElementTraversal::NextSibling(*child)) {
    ++count;
  }
  return count;
}

Element* ContainerNode::querySelector(const AtomicString& selectors,
                                      ExceptionState& exception_state) {
  return QuerySelector(selectors, exception_state);
}

StaticElementList* ContainerNode::querySelectorAll(
    const AtomicString& selectors,
    ExceptionState& exception_state) {
  return QuerySelectorAll(selectors, exception_state);
}

unsigned ContainerNode::CountChildren() const {
  unsigned count = 0;
  for (Node* node = firstChild(); node; node = node->nextSibling())
    count++;
  return count;
}

Element* ContainerNode::QuerySelector(const AtomicString& selectors,
                                      ExceptionState& exception_state) {
  SelectorQuery* selector_query = GetDocument().GetSelectorQueryCache().Add(
      selectors, GetDocument(), exception_state);
  if (!selector_query)
    return nullptr;
  return selector_query->QueryFirst(*this);
}

Element* ContainerNode::QuerySelector(const AtomicString& selectors) {
  return QuerySelector(selectors, ASSERT_NO_EXCEPTION);
}

StaticElementList* ContainerNode::QuerySelectorAll(
    const AtomicString& selectors,
    ExceptionState& exception_state) {
  SelectorQuery* selector_query = GetDocument().GetSelectorQueryCache().Add(
      selectors, GetDocument(), exception_state);
  if (!selector_query)
    return nullptr;
  return selector_query->QueryAll(*this);
}

StaticElementList* ContainerNode::QuerySelectorAll(
    const AtomicString& selectors) {
  return QuerySelectorAll(selectors, ASSERT_NO_EXCEPTION);
}

static void DispatchChildInsertionEvents(Node& child) {
  Document& document = child.GetDocument();
  if (child.IsInShadowTree() || document.ShouldSuppressMutationEvents()) {
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif

  Node* c = &child;

  if (c->parentNode() &&
      document.HasListenerType(Document::kDOMNodeInsertedListener)) {
    c->DispatchScopedEvent(
        *MutationEvent::Create(event_type_names::kDOMNodeInserted,
                               Event::Bubbles::kYes, c->parentNode()));
  }

  // dispatch the DOMNodeInsertedIntoDocument event to all descendants
  if (c->isConnected() && document.HasListenerType(
                              Document::kDOMNodeInsertedIntoDocumentListener)) {
    for (; c; c = NodeTraversal::Next(*c, &child)) {
      c->DispatchScopedEvent(*MutationEvent::Create(
          event_type_names::kDOMNodeInsertedIntoDocument, Event::Bubbles::kNo));
    }
  }
}

static void DispatchChildRemovalEvents(Node& child) {
  probe::WillRemoveDOMNode(&child);

  Document& document = child.GetDocument();
  if (child.IsInShadowTree() || document.ShouldSuppressMutationEvents()) {
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif

  Node* c = &child;

  // Dispatch pre-removal mutation events.
  if (c->parentNode() &&
      document.HasListenerType(Document::kDOMNodeRemovedListener)) {
    NodeChildRemovalTracker scope(child);
    c->DispatchScopedEvent(
        *MutationEvent::Create(event_type_names::kDOMNodeRemoved,
                               Event::Bubbles::kYes, c->parentNode()));
  }

  // Dispatch the DOMNodeRemovedFromDocument event to all descendants.
  if (c->isConnected() &&
      document.HasListenerType(Document::kDOMNodeRemovedFromDocumentListener)) {
    NodeChildRemovalTracker scope(child);
    for (; c; c = NodeTraversal::Next(*c, &child)) {
      c->DispatchScopedEvent(*MutationEvent::Create(
          event_type_names::kDOMNodeRemovedFromDocument, Event::Bubbles::kNo));
    }
  }
}

bool ContainerNode::HasRestyleFlagInternal(DynamicRestyleFlags mask) const {
  return RareData()->HasRestyleFlag(mask);
}

bool ContainerNode::HasRestyleFlagsInternal() const {
  return RareData()->HasRestyleFlags();
}

void ContainerNode::SetRestyleFlag(DynamicRestyleFlags mask) {
  DCHECK(IsElementNode() || IsShadowRoot());
  EnsureRareData().SetRestyleFlag(mask);
}

void ContainerNode::RecalcDescendantStyles(
    const StyleRecalcChange change,
    const StyleRecalcContext& style_recalc_context) {
  DCHECK(GetDocument().InStyleRecalc());
  DCHECK(!NeedsStyleRecalc());

  for (Node* child = firstChild(); child; child = child->nextSibling()) {
    if (!change.TraverseChild(*child)) {
      continue;
    }
    if (auto* child_text_node = DynamicTo<Text>(child))
      child_text_node->RecalcTextStyle(change);

    if (auto* child_element = DynamicTo<Element>(child)) {
      child_element->RecalcStyle(change, style_recalc_context);
    }
  }
}

void ContainerNode::RebuildLayoutTreeForChild(
    Node* child,
    WhitespaceAttacher& whitespace_attacher) {
  if (auto* child_text_node = DynamicTo<Text>(child)) {
    if (child->NeedsReattachLayoutTree())
      child_text_node->RebuildTextLayoutTree(whitespace_attacher);
    else
      whitespace_attacher.DidVisitText(child_text_node);
    return;
  }

  auto* element = DynamicTo<Element>(child);
  if (!element)
    return;

  if (element->NeedsRebuildLayoutTree(whitespace_attacher))
    element->RebuildLayoutTree(whitespace_attacher);
  else
    whitespace_attacher.DidVisitElement(element);
}

void ContainerNode::RebuildChildrenLayoutTrees(
    WhitespaceAttacher& whitespace_attacher) {
  DCHECK(!NeedsReattachLayoutTree());

  if (IsActiveSlot()) {
    if (auto* slot = DynamicTo<HTMLSlotElement>(this)) {
      slot->RebuildDistributedChildrenLayoutTrees(whitespace_attacher);
    }
    return;
  }

  // This loop is deliberately backwards because we use insertBefore in the
  // layout tree, and want to avoid a potentially n^2 loop to find the insertion
  // point while building the layout tree.  Having us start from the last child
  // and work our way back means in the common case, we'll find the insertion
  // point in O(1) time.  See crbug.com/288225
  for (Node* child = lastChild(); child; child = child->previousSibling())
    RebuildLayoutTreeForChild(child, whitespace_attacher);
}

void ContainerNode::CheckForSiblingStyleChanges(SiblingCheckType change_type,
                                                Element* changed_element,
                                                Node* node_before_change,
                                                Node* node_after_change) {
  if (!InActiveDocument() || GetDocument().HasPendingForcedStyleRecalc() ||
      GetStyleChangeType() == kSubtreeStyleChange)
    return;

  if (!HasRestyleFlag(DynamicRestyleFlags::kChildrenAffectedByStructuralRules))
    return;

  auto* element_after_change = DynamicTo<Element>(node_after_change);
  if (node_after_change && !element_after_change)
    element_after_change = ElementTraversal::NextSibling(*node_after_change);
  auto* element_before_change = DynamicTo<Element>(node_before_change);
  if (node_before_change && !element_before_change) {
    element_before_change =
        ElementTraversal::PreviousSibling(*node_before_change);
  }

  // TODO(futhark@chromium.org): move this code into StyleEngine and collect the
  // various invalidation sets into a single InvalidationLists object and
  // schedule with a single scheduleInvalidationSetsForNode for efficiency.

  // Forward positional selectors include :nth-child, :nth-of-type,
  // :first-of-type, and only-of-type. Backward positional selectors include
  // :nth-last-child, :nth-last-of-type, :last-of-type, and :only-of-type.
  if ((ChildrenAffectedByForwardPositionalRules() && element_after_change) ||
      (ChildrenAffectedByBackwardPositionalRules() && element_before_change)) {
    GetDocument().GetStyleEngine().ScheduleNthPseudoInvalidations(*this);
  }

  if (ChildrenAffectedByFirstChildRules() && !element_before_change &&
      element_after_change &&
      element_after_change->AffectedByFirstChildRules()) {
    DCHECK_NE(change_type, kFinishedParsingChildren);
    element_after_change->PseudoStateChanged(CSSSelector::kPseudoFirstChild);
    element_after_change->PseudoStateChanged(CSSSelector::kPseudoOnlyChild);
  }

  if (ChildrenAffectedByLastChildRules() && !element_after_change &&
      element_before_change &&
      element_before_change->AffectedByLastChildRules()) {
    element_before_change->PseudoStateChanged(CSSSelector::kPseudoLastChild);
    element_before_change->PseudoStateChanged(CSSSelector::kPseudoOnlyChild);
  }

  // For ~ and + combinators, succeeding siblings may need style invalidation
  // after an element is inserted or removed.

  if (!element_after_change)
    return;

  if (!ChildrenAffectedByIndirectAdjacentRules() &&
      !ChildrenAffectedByDirectAdjacentRules())
    return;

  if (change_type == kSiblingElementInserted) {
    GetDocument().GetStyleEngine().ScheduleInvalidationsForInsertedSibling(
        element_before_change, *changed_element);
    return;
  }

  DCHECK(change_type == kSiblingElementRemoved);
  GetDocument().GetStyleEngine().ScheduleInvalidationsForRemovedSibling(
      element_before_change, *changed_element, *element_after_change);
}

void ContainerNode::InvalidateNodeListCachesInAncestors(
    const QualifiedName* attr_name,
    Element* attribute_owner_element,
    const ChildrenChange* change) {
  // This is a performance optimization, NodeList cache invalidation is
  // not necessary for a text change.
  if (change && change->type == ChildrenChangeType::kTextChanged)
    return;

  if (HasRareData() && (!attr_name || IsAttributeNode())) {
    if (NodeListsNodeData* lists = RareData()->NodeLists()) {
      if (ChildNodeList* child_node_list = lists->GetChildNodeList(*this)) {
        if (change) {
          child_node_list->ChildrenChanged(*change);
        } else {
          child_node_list->InvalidateCache();
        }
      }
    }
  }

  // This is a performance optimization, NodeList cache invalidation is
  // not necessary for non-element nodes.
  if (change && change->affects_elements == ChildrenChangeAffectsElements::kNo)
    return;

  // Modifications to attributes that are not associated with an Element can't
  // invalidate NodeList caches.
  if (attr_name && !attribute_owner_element)
    return;

  if (!GetDocument().ShouldInvalidateNodeListCaches(attr_name))
    return;

  GetDocument().InvalidateNodeListCaches(attr_name);

  for (ContainerNode* node = this; node; node = node->parentNode()) {
    if (NodeListsNodeData* lists = node->NodeLists())
      lists->InvalidateCaches(attr_name);
  }
}

HTMLCollection* ContainerNode::getElementsByTagName(
    const AtomicString& qualified_name) {
  DCHECK(!qualified_name.IsNull());

  if (IsA<HTMLDocument>(GetDocument())) {
    return EnsureCachedCollection<HTMLTagCollection>(kHTMLTagCollectionType,
                                                     qualified_name);
  }
  return EnsureCachedCollection<TagCollection>(kTagCollectionType,
                                               qualified_name);
}

HTMLCollection* ContainerNode::getElementsByTagNameNS(
    const AtomicString& namespace_uri,
    const AtomicString& local_name) {
  return EnsureCachedCollection<TagCollectionNS>(
      kTagCollectionNSType, namespace_uri.empty() ? g_null_atom : namespace_uri,
      local_name);
}

// Takes an AtomicString in argument because it is common for elements to share
// the same name attribute.  Therefore, the NameNodeList factory function
// expects an AtomicString type.
NodeList* ContainerNode::getElementsByName(const AtomicString& element_name) {
  return EnsureCachedCollection<NameNodeList>(kNameNodeListType, element_name);
}

// Takes an AtomicString in argument because it is common for elements to share
// the same set of class names.  Therefore, the ClassNodeList factory function
// expects an AtomicString type.
HTMLCollection* ContainerNode::getElementsByClassName(
    const AtomicString& class_names) {
  return EnsureCachedCollection<ClassCollection>(kClassCollectionType,
                                                 class_names);
}

RadioNodeList* ContainerNode::GetRadioNodeList(const AtomicString& name,
                                               bool only_match_img_elements) {
  DCHECK(IsA<HTMLFormElement>(this) || IsA<HTMLFieldSetElement>(this));
  CollectionType type =
      only_match_img_elements ? kRadioImgNodeListType : kRadioNodeListType;
  return EnsureCachedCollection<RadioNodeList>(type, name);
}

Element* ContainerNode::getElementById(const AtomicString& id) const {
  // According to https://dom.spec.whatwg.org/#concept-id, empty IDs are
  // treated as equivalent to the lack of an id attribute.
  if (id.empty()) {
    return nullptr;
  }

  if (IsInTreeScope()) {
    // Fast path if we are in a tree scope: call getElementById() on tree scope
    // and check if the matching element is in our subtree.
    Element* element = ContainingTreeScope().getElementById(id);
    if (!element)
      return nullptr;
    if (element->IsDescendantOf(this))
      return element;
  }

  // Fall back to traversing our subtree. In case of duplicate ids, the first
  // element found will be returned.
  for (Element& element : ElementTraversal::DescendantsOf(*this)) {
    if (element.GetIdAttribute() == id)
      return &element;
  }
  return nullptr;
}

NodeListsNodeData& ContainerNode::EnsureNodeLists() {
  return EnsureRareData().EnsureNodeLists();
}

// https://html.spec.whatwg.org/C/#autofocus-delegate
Element* ContainerNode::GetAutofocusDelegate() const {
  Element* element = ElementTraversal::Next(*this, this);
  while (element) {
    if (!element->IsAutofocusable()) {
      element = ElementTraversal::Next(*element, this);
      continue;
    }

    Element* focusable_area =
        element->IsFocusable() ? element : element->GetFocusableArea();
    if (!focusable_area) {
      element = ElementTraversal::Next(*element, this);
      continue;
    }

    // The spec says to continue instead of returning focusable_area if
    // focusable_area is not click-focusable and the call was initiated by the
    // user clicking. I don't believe this is currently possible, so DCHECK
    // instead.
    DCHECK(focusable_area->IsFocusable());

    return focusable_area;
  }

  return nullptr;
}

// https://dom.spec.whatwg.org/#dom-parentnode-replacechildren
void ContainerNode::ReplaceChildren(const VectorOf<Node>& nodes,
                                    ExceptionState& exception_state) {
  // 1. Let node be the result of converting nodes into a node given nodes and
  // this’s node document.
  Node* node =
      ConvertNodesIntoNode(this, nodes, GetDocument(), exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 2. Ensure pre-insertion validity of node into this before null.
  if (!EnsurePreInsertionValidity(*node, nullptr, nullptr, exception_state)) {
    return;
  }

  // 3. Replace all with node within this.
  ChildListMutationScope mutation(*this);
  while (Node* first_child = firstChild()) {
    RemoveChild(first_child, exception_state);
    if (exception_state.HadException()) {
      return;
    }
  }

  AppendChild(node, exception_state);
}

}  // namespace blink

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/menu_mutation_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/html/forms/select_mutation_observer.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

MenuMutationObserver::MenuMutationObserver(HTMLMenuOwnerElement& menu_owner)
    : menu_owner_(menu_owner), observer_(MutationObserver::Create(this)) {
  MutationObserverInit* init = MutationObserverInit::Create();
  init->setChildList(true);
  init->setSubtree(true);
  init->setAttributes(true);
  observer_->observe(menu_owner_, init, ASSERT_NO_EXCEPTION);
}

ExecutionContext* MenuMutationObserver::GetExecutionContext() const {
  return menu_owner_->GetExecutionContext();
}

void MenuMutationObserver::Deliver(const MutationRecordVector& records,
                                   MutationObserver&) {
  HeapHashSet<Member<Node>> visited_nodes;

  for (const auto& record : records) {
    if (record->type() == "childList") {
      CheckAddedNodes(record.Get(), visited_nodes);
      CheckRemovedNodes(record.Get());
    } else if (record->type() == "attributes") {
      if (record->attributeName() == html_names::kTabindexAttr ||
          record->attributeName() == html_names::kContenteditableAttr) {
        AddDescendantDisallowedErrorToNode(*record->target(), visited_nodes);
      }
    }
  }
}

void MenuMutationObserver::Disconnect() {
  observer_->disconnect();
}

void MenuMutationObserver::Trace(Visitor* visitor) const {
  visitor->Trace(menu_owner_);
  visitor->Trace(observer_);
  MutationObserver::Delegate::Trace(visitor);
}

void MenuMutationObserver::CheckAddedNodes(
    MutationRecord* record,
    HeapHashSet<Member<Node>>& visited_nodes) {
  DCHECK(record);
  auto* added_nodes = record->addedNodes();
  for (unsigned i = 0; i < added_nodes->length(); ++i) {
    auto* descendant = added_nodes->item(i);
    DCHECK(descendant);
    CheckNodeAndDescendantsForViolations(descendant, visited_nodes,
                                         record->target());
  }
}

void MenuMutationObserver::CheckRemovedNodes(MutationRecord* record) {
  DCHECK(record);
  auto* removed_nodes = record->removedNodes();
  DCHECK(removed_nodes);
  for (unsigned i = 0; i < removed_nodes->length(); ++i) {
    auto* removed_node = removed_nodes->item(i);
    DCHECK(removed_node);
    if (!SelectMutationObserver::IsWhitespaceOrEmpty(*removed_node)) {
      if (CheckForIssue(*removed_node, removed_node, record->target()) !=
          ElementAccessibilityIssueReason::kValidChild) {
        menu_owner_->DecreaseContentModelViolationCount();
      }
    }

    for (Node* nested = NodeTraversal::FirstWithin(*removed_node); nested;
         nested = NodeTraversal::Next(*nested, removed_node)) {
      if (!SelectMutationObserver::IsWhitespaceOrEmpty(*nested)) {
        if (CheckForIssue(*nested, removed_node, record->target()) !=
            ElementAccessibilityIssueReason::kValidChild) {
          menu_owner_->DecreaseContentModelViolationCount();
        }
      }
    }
  }
}

void MenuMutationObserver::CheckNodeAndDescendantsForViolations(
    Node* node,
    HeapHashSet<Member<Node>>& visited_nodes,
    const Node* disconnected_parent) {
  const Node* disconnected_root = disconnected_parent ? node : nullptr;
  for (Node* curr = node; curr; curr = NodeTraversal::Next(*curr, node)) {
    if (!SelectMutationObserver::IsWhitespaceOrEmpty(*curr)) {
      AddDescendantDisallowedErrorToNode(
          *curr, visited_nodes, disconnected_root, disconnected_parent);
    }
  }
}

void MenuMutationObserver::AddDescendantDisallowedErrorToNode(
    Node& node,
    HeapHashSet<Member<Node>>& visited_nodes,
    const Node* disconnected_root,
    const Node* disconnected_parent) {
  if (visited_nodes.Contains(&node)) {
    return;
  }
  visited_nodes.insert(&node);
  if (CheckForIssue(node, disconnected_root, disconnected_parent) !=
      ElementAccessibilityIssueReason::kValidChild) {
    menu_owner_->IncreaseContentModelViolationCount();
  }
}

ElementAccessibilityIssueReason MenuMutationObserver::CheckForIssue(
    const Node& descendant,
    const Node* disconnected_root,
    const Node* disconnected_parent) {
  if (descendant.getNodeType() == Node::kCommentNode) {
    return ElementAccessibilityIssueReason::kValidChild;
  }

  // TODO(crbug.com/513637242): Return a menu-specific issue reason instead of
  // kDisallowedSelectChild.
  const ElementAccessibilityIssueReason kPlaceholderIssue =
      ElementAccessibilityIssueReason::kDisallowedSelectChild;

  if (IsA<HTMLAnchorElement>(descendant)) {
    // This is extra strict. SelectMutationObserver::IsInteractiveElement
    // returns true only if the anchor tag has an href attribute, so this check
    // ensures <a>Blah</a> is also considered invalid.
    return kPlaceholderIssue;
  }
  if (SelectMutationObserver::IsInteractiveElement(descendant)) {
    return kPlaceholderIssue;
  }

  DCHECK(!!disconnected_root == !!disconnected_parent)
      << "CheckRemovedNodes has to pass both, and neither can be nullptr";
  auto get_parent_even_if_disconnected =
      [disconnected_root, disconnected_parent](const Node* n) -> const Node* {
    if (n == disconnected_root) {
      return disconnected_parent;
    }
    return n->parentNode();
  };

  // Text nodes must be inside <menuitem> or <legend>.
  if (descendant.getNodeType() == Node::kTextNode) {
    for (const Node* ancestor = get_parent_even_if_disconnected(&descendant);
         ancestor; ancestor = get_parent_even_if_disconnected(ancestor)) {
      if (auto* ancestor_element = DynamicTo<Element>(ancestor)) {
        if (ancestor_element->HasTagName(html_names::kMenuitemTag) ||
            ancestor_element->HasTagName(html_names::kLegendTag)) {
          return ElementAccessibilityIssueReason::kValidChild;
        }
        if (IsA<HTMLMenuOwnerElement>(*ancestor)) {
          return kPlaceholderIssue;
        }
      }
    }
    // We reach here if the node is in a detached subtree and we reached the
    // root of that subtree without finding a <menuitem> or <legend>. (e.g.
    // external/wpt/html/semantics/menu/tentative/menu-content-model-violation.html
    // -- "Removing a node from a detached valid subtree should not crash")
    // Because the subtree is detached, we do not increment or decrement the
    // menu's violation count for mutations happening entirely within it.
    // Return kValidChild so that these detached mutations are ignored.
    DCHECK(disconnected_root);
    return ElementAccessibilityIssueReason::kValidChild;
  }

  return ElementAccessibilityIssueReason::kValidChild;
}

}  // namespace blink

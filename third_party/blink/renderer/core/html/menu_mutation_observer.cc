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

  HeapHashSet<Member<Node>> visited_nodes;
  TraverseNodeDescendants(menu_owner_, visited_nodes);
}

ExecutionContext* MenuMutationObserver::GetExecutionContext() const {
  return menu_owner_->GetExecutionContext();
}

void MenuMutationObserver::Deliver(const MutationRecordVector& records,
                                   MutationObserver&) {
  HeapHashSet<Member<Node>> visited_nodes;
  HeapHashSet<Member<const Node>> detached_roots_we_own;

  // Pass 1: Build the set of detached roots our particular menu owns. Omit
  // nodes that belong to any of our submenus.
  // Transient observers can queue a mutation record for an inner node before
  // the record that removes its outer container. If we process the inner record
  // first, IsRecordOwnedByThisMenu will return false because the outer
  // container is not yet in the detached_roots_we_own set. We loop until the
  // set stabilizes so that the outer container is added to the set, which
  // allows the inner node's record to be correctly identified as owned.
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto& record : records) {
      if (record->type() == "childList") {
        // Because we observe the subtree, mutations inside an inner nested
        // menu bubble up to this outer menu's observer. We must ignore records
        // that belong to inner menus so we don't falsely claim ownership of
        // their detached subtrees.
        if (!IsRecordOwnedByThisMenu(record.Get(), detached_roots_we_own)) {
          continue;
        }
        auto* removed = record->removedNodes();
        for (unsigned i = 0; i < removed->length(); ++i) {
          const Node* detached_root = removed->item(i);
          changed |= detached_roots_we_own.insert(detached_root).is_new_entry;
        }
      }
    }
  }

  // Pass 2: Check for content model violations.
  for (const auto& record : records) {
    // Because we observe the subtree, mutations inside a nested sub-menu will
    // bubble up to outer menu observers. This check ensures only the innermost
    // owning menu processes the mutation to prevent double-counting violations.
    if (!IsRecordOwnedByThisMenu(record.Get(), detached_roots_we_own)) {
      continue;
    }
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

bool MenuMutationObserver::IsRecordOwnedByThisMenu(
    MutationRecord* record,
    const HeapHashSet<Member<const Node>>& detached_roots_we_own) const {
  for (const Node* ancestor = record->target();;
       ancestor = ancestor->parentNode()) {
    DCHECK(ancestor);
    if (ancestor == menu_owner_) {
      return true;
    }
    if (IsA<HTMLMenuOwnerElement>(*ancestor)) {
      return false;
    }
    if (!ancestor->parentNode()) {
      // If the target is detached, it is the root of a subtree that was removed
      // from the DOM. Because transient observers fire for all ancestor
      // observers, an outer menu will receive records for subtrees detached
      // from an inner menu. We must return true ONLY if this specific menu
      // observer was the one that actually removed this root in the current
      // delivery batch.
      return detached_roots_we_own.Contains(ancestor);
    }
  }
  NOTREACHED();
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
    if (SelectMutationObserver::IsWhitespaceOrEmpty(*descendant)) {
      continue;
    }

    AddDescendantDisallowedErrorToNode(*descendant, visited_nodes);

    if (IsA<HTMLMenuOwnerElement>(*descendant)) {
      // Violations in this submenu don't affect our count, so skip it.
      continue;
    }

    TraverseNodeDescendants(descendant, visited_nodes);
  }
}

void MenuMutationObserver::CheckRemovedNodes(MutationRecord* record) {
  DCHECK(record);
  auto* removed_nodes = record->removedNodes();
  DCHECK(removed_nodes);
  for (unsigned i = 0; i < removed_nodes->length(); ++i) {
    auto* removed_node = removed_nodes->item(i);
    DCHECK(removed_node);
    if (SelectMutationObserver::IsWhitespaceOrEmpty(*removed_node)) {
      continue;
    }

    if (CheckForIssue(*removed_node, removed_node, record->target()) !=
        ElementAccessibilityIssueReason::kValidChild) {
      menu_owner_->DecreaseContentModelViolationCount();
    }

    if (IsA<HTMLMenuOwnerElement>(*removed_node)) {
      // Violations in this submenu don't affect our count, so skip it.
      continue;
    }

    for (Node* nested = NodeTraversal::FirstWithin(*removed_node); nested;) {
      if (!SelectMutationObserver::IsWhitespaceOrEmpty(*nested)) {
        if (CheckForIssue(*nested, removed_node, record->target()) !=
            ElementAccessibilityIssueReason::kValidChild) {
          menu_owner_->DecreaseContentModelViolationCount();
        }
      }

      if (IsA<HTMLMenuOwnerElement>(*nested)) {
        nested = NodeTraversal::NextSkippingChildren(*nested, removed_node);
      } else {
        nested = NodeTraversal::Next(*nested, removed_node);
      }
    }
  }
}

void MenuMutationObserver::TraverseNodeDescendants(
    const Node* node,
    HeapHashSet<Member<Node>>& visited_nodes) {
  for (Node* descendant = NodeTraversal::FirstWithin(*node); descendant;) {
    if (!SelectMutationObserver::IsWhitespaceOrEmpty(*descendant)) {
      AddDescendantDisallowedErrorToNode(*descendant, visited_nodes);
    }

    DCHECK_NE(descendant, menu_owner_) << "No cycles";
    if (IsA<HTMLMenuOwnerElement>(*descendant)) {
      descendant = NodeTraversal::NextSkippingChildren(*descendant, node);
    } else {
      descendant = NodeTraversal::Next(*descendant, node);
    }
  }
}

void MenuMutationObserver::AddDescendantDisallowedErrorToNode(
    Node& node,
    HeapHashSet<Member<Node>>& visited_nodes) {
  if (visited_nodes.Contains(&node)) {
    return;
  }
  visited_nodes.insert(&node);
  if (CheckForIssue(node) != ElementAccessibilityIssueReason::kValidChild) {
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
    // We only reach here if this is a transient mutation removal and the node
    // was in violation.
    DCHECK(disconnected_root);
    return kPlaceholderIssue;
  }

  return ElementAccessibilityIssueReason::kValidChild;
}

}  // namespace blink

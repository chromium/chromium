// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_containment_scope_tree.h"

#include "third_party/blink/renderer/core/css/style_containment_scope.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"

namespace blink {

void StyleContainmentScopeTree::Trace(Visitor* visitor) const {
  visitor->Trace(root_scope_);
  visitor->Trace(outermost_quotes_dirty_scope_);
  visitor->Trace(scopes_);
}

StyleContainmentScope*
StyleContainmentScopeTree::FindOrCreateEnclosingScopeForElement(
    const Element& element) {
  // Traverse the ancestors and see if there is any with contain style.
  // The search is started from the parent of the element as the style
  // containment is scoped to the element’s sub-tree, meaning that the
  // element itself is not the part of its scope subtree.
  for (const Element* it = LayoutTreeBuilderTraversal::ParentElement(element);
       it; it = LayoutTreeBuilderTraversal::ParentElement(*it)) {
    if (!it->ComputedStyleRef().ContainsStyle()) {
      continue;
    }
    // Try to find if the element is a scope root.
    if (auto scope = scopes_.find(it); scope != scopes_.end()) {
      return scope->value;
    }
    // Create a new scope if the element is not a root to any.
    StyleContainmentScope* parent = FindOrCreateEnclosingScopeForElement(*it);
    StyleContainmentScope* scope =
        MakeGarbageCollected<StyleContainmentScope>(it);
    parent->AppendChild(scope);
    scopes_.insert(it, scope);
    return scope;
  }
  // Return root scope if nothing found.
  return root_scope_;
}

void StyleContainmentScopeTree::DestroyScopeForElement(const Element& element) {
  auto entry = scopes_.find(&element);
  if (entry == scopes_.end()) {
    return;
  }
  StyleContainmentScope* scope = entry->value;
  StyleContainmentScope* parent = scope->Parent();
  scope->ReattachToParent();
  scopes_.erase(&element);
  UpdateOutermostQuotesDirtyScope(parent);
}

void StyleContainmentScopeTree::CreateScopeForElement(const Element& element) {
  if (scopes_.find(&element) != scopes_.end()) {
    return;
  }
  StyleContainmentScope* scope =
      MakeGarbageCollected<StyleContainmentScope>(&element);
  StyleContainmentScope* parent = FindOrCreateEnclosingScopeForElement(element);
  parent->AppendChild(scope);
  scopes_.insert(&element, scope);
  // Try to find if we create a scope anywhere between the parent and existing
  // children. If so, reattach the child and the quotes.
  auto children = parent->Children();
  for (StyleContainmentScope* child : children) {
    if (child != scope &&
        scope->IsAncestorOf(child->GetElement(), parent->GetElement())) {
      parent->RemoveChild(child);
      scope->AppendChild(child);
    }
  }
  auto quotes = parent->Quotes();
  for (LayoutQuote* quote : quotes) {
    if (scope->IsAncestorOf(quote->GetOwningPseudo(), parent->GetElement())) {
      parent->DetachQuote(*quote);
      scope->AttachQuote(*quote);
    }
  }
  UpdateOutermostQuotesDirtyScope(parent);
}

void StyleContainmentScopeTree::ElementWillBeRemoved(const Element& element) {
  if (auto it = scopes_.find(&element); it != scopes_.end()) {
    // If the element that will be removed is a scope owner,
    // we need to delete this scope and reattach its quotes and children
    // to its parent, and mark its parent dirty.
    StyleContainmentScope* scope = it->value;
    UpdateOutermostQuotesDirtyScope(scope->Parent());
    scope->ReattachToParent();
    scopes_.erase(it);
  }
}

namespace {

StyleContainmentScope* FindCommonAncestor(StyleContainmentScope* scope1,
                                          StyleContainmentScope* scope2) {
  if (!scope1) {
    return scope2;
  }
  if (!scope2) {
    return scope1;
  }
  HeapVector<StyleContainmentScope*> ancestors1, ancestors2;
  for (StyleContainmentScope* it = scope1; it; it = it->Parent()) {
    if (it == scope2) {
      return scope2;
    }
    ancestors1.emplace_back(it);
  }
  for (StyleContainmentScope* it = scope2; it; it = it->Parent()) {
    if (it == scope1) {
      return scope1;
    }
    ancestors2.emplace_back(it);
  }
  int anc1 = ancestors1.size() - 1;
  int anc2 = ancestors2.size() - 1;
  while (anc1 >= 0 && anc2 >= 0 && ancestors1[anc1] == ancestors2[anc2]) {
    --anc1;
    --anc2;
  }
  int pos = anc1 == int(ancestors1.size()) - 1 ? anc1 : anc1 + 1;
  return ancestors1[pos];
}

}  // namespace

void StyleContainmentScopeTree::UpdateOutermostQuotesDirtyScope(
    StyleContainmentScope* scope) {
  outermost_quotes_dirty_scope_ =
      FindCommonAncestor(scope, outermost_quotes_dirty_scope_);
}

void StyleContainmentScopeTree::UpdateQuotes() {
  if (!outermost_quotes_dirty_scope_) {
    return;
  }
  outermost_quotes_dirty_scope_->UpdateQuotes();
  outermost_quotes_dirty_scope_ = nullptr;
}

}  // namespace blink

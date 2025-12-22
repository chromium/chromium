// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ORDERED_SCOPE_TREE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ORDERED_SCOPE_TREE_H_

#include "third_party/blink/renderer/core/css/ordered_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// Internal helpers for `OrderedScope<T>` and `OrderedScopeTree<T>`.
//
// These helpers are implementation details shared between the ordered
// scope templates and are not part of the public API. Keep changes here
// minimal and avoid depending on this namespace from unrelated code.
namespace ordered_scope_internal {

// Finds the common ancestor of two scopes. Returns the scope that is an
// ancestor of both, or one of them if one is an ancestor of the other.
template <typename ScopeType>
ScopeType* FindCommonAncestor(ScopeType* scope1, ScopeType* scope2) {
  if (!scope1) {
    return scope2;
  }
  if (!scope2) {
    return scope1;
  }

  HeapHashSet<Member<ScopeType>> ancestors;
  for (ScopeType* s = scope1; s; s = s->Parent()) {
    ancestors.insert(s);
  }
  for (ScopeType* s = scope2; s; s = s->Parent()) {
    if (ancestors.Contains(s)) {
      return s;
    }
  }
  return nullptr;
}

}  // namespace ordered_scope_internal

// A tree of OrderedScope<T> instances that manages scope creation and cleanup.
//
// Scopes are created for elements where ScopeType::CreatesScope() returns true.
// The root scope captures items that don't have a scope-creating ancestor.
// Supports dirty scope tracking for efficient batch updates via UpdateItems().
template <typename T>
class OrderedScopeTree : public GarbageCollected<OrderedScopeTree<T>> {
 public:
  using ScopeType = OrderedScope<T>;

  OrderedScopeTree()
      : root_scope_(MakeGarbageCollected<ScopeType>(nullptr, this)) {}

  OrderedScopeTree(const OrderedScopeTree&) = delete;
  OrderedScopeTree& operator=(const OrderedScopeTree&) = delete;

  // Finds the scope that encloses the given element, creating it if needed.
  ScopeType* FindOrCreateEnclosingScopeForElement(const Element& element) {
    for (const Element* it = LayoutTreeBuilderTraversal::ParentElement(element);
         it; it = LayoutTreeBuilderTraversal::ParentElement(*it)) {
      if (!ScopeType::CreatesScope(*it)) {
        continue;
      }
      auto scope_it = scopes_.find(it);
      if (scope_it != scopes_.end()) {
        return scope_it->value.Get();
      }
      return CreateScopeForElement(*it);
    }
    return root_scope_.Get();
  }

  // Creates a scope for an element. The element must satisfy CreatesScope().
  ScopeType* CreateScopeForElement(const Element& element) {
    CHECK(ScopeType::CreatesScope(element));

    auto it = scopes_.find(&element);
    if (it != scopes_.end()) {
      return it->value.Get();
    }

    ScopeType* parent_scope = FindOrCreateEnclosingScopeForElement(element);
    ScopeType* new_scope = MakeGarbageCollected<ScopeType>(&element, this);
    scopes_.insert(&element, new_scope);

    // Move items from parent that belong to this new scope.
    bool parent_has_changed = false;
    HeapVector<Member<T>> items_to_move;
    for (T* item : parent_scope->Items()) {
      if (new_scope->IsAncestorOf(ScopeType::GetItemElement(item),
                                  parent_scope->GetScopeRoot())) {
        items_to_move.push_back(item);
      }
    }
    for (T* item : items_to_move) {
      parent_has_changed = true;
      parent_scope->DetachItem(*item);
      new_scope->AttachItem(*item);
    }

    // Move child scopes from parent that belong to this new scope.
    HeapVector<Member<ScopeType>> children_to_move;
    for (ScopeType* child : parent_scope->Children()) {
      if (new_scope->IsAncestorOf(child->GetScopeRoot(),
                                  parent_scope->GetScopeRoot())) {
        children_to_move.push_back(child);
      }
    }
    for (ScopeType* child : children_to_move) {
      parent_has_changed = true;
      parent_scope->RemoveChild(child);
      new_scope->AppendChild(child);
    }

    parent_scope->AppendChild(new_scope);

    // Mark parent dirty if items or children were moved.
    if (parent_has_changed) {
      UpdateOutermostDirtyScope(parent_scope);
    }
    return new_scope;
  }

  // Destroys the scope for an element, reattaching its contents to parent.
  // Use this when the element's style changes (e.g., contain:style removed).
  void DestroyScopeForElement(const Element& element) {
    RemoveScopeForElementInternal(element, /*reattach_to_parent=*/true);
  }

  // Removes the scope for an element without reattaching contents.
  // Use this when the element is being removed from the DOM tree entirely.
  void RemoveScopeForElement(const Element& element) {
    RemoveScopeForElementInternal(element, /*reattach_to_parent=*/false);
  }

  // Updates all items starting from the outermost dirty scope.
  void UpdateItems() {
    if (outermost_dirty_scope_) {
      ScopeType::UpdateItemsInScope(outermost_dirty_scope_.Get());
      outermost_dirty_scope_ = nullptr;
    }
  }

  // Marks a scope as needing updates. Tracks the outermost dirty scope.
  void UpdateOutermostDirtyScope(ScopeType* scope) {
    if (!outermost_dirty_scope_) {
      outermost_dirty_scope_ = scope;
      return;
    }
    outermost_dirty_scope_ = blink::ordered_scope_internal::FindCommonAncestor(
        outermost_dirty_scope_.Get(), scope);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(root_scope_);
    visitor->Trace(scopes_);
    visitor->Trace(outermost_dirty_scope_);
  }

#if DCHECK_IS_ON()
  String ToString(ScopeType* scope = nullptr, wtf_size_t depth = 0u) const {
    if (!scope) {
      scope = root_scope_.Get();
    }
    StringBuilder builder;
    for (wtf_size_t i = 0; i < depth; ++i) {
      builder.Append("  ");
    }
    builder.Append("Scope");
    if (scope->GetScopeRoot()) {
      builder.Append(" <");
      builder.Append(scope->GetScopeRoot()->DebugName());
      builder.Append(">");
    } else {
      builder.Append(" (root)");
    }
    builder.Append(" items=");
    builder.AppendNumber(scope->Items().size());
    builder.Append("\n");

    for (ScopeType* child : scope->Children()) {
      builder.Append(ToString(child, depth + 1));
    }
    return builder.ToString();
  }
#endif

 private:
  // If reattach_to_parent is true, items and children are moved to parent.
  // If false, the scope is simply cleared (for DOM removal).
  void RemoveScopeForElementInternal(const Element& element,
                                     bool reattach_to_parent) {
    auto it = scopes_.find(&element);
    if (it == scopes_.end()) {
      return;
    }
    ScopeType* scope = it->value.Get();
    UpdateOutermostDirtyScope(scope->Parent());
    if (reattach_to_parent) {
      scope->ReattachToParent();
    } else {
      scope->Clear();
    }
    scopes_.erase(it);
  }

  Member<ScopeType> root_scope_;
  HeapHashMap<WeakMember<const Element>, Member<ScopeType>> scopes_;
  Member<ScopeType> outermost_dirty_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ORDERED_SCOPE_TREE_H_

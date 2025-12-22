// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ORDERED_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ORDERED_SCOPE_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

template <typename T>
class OrderedScopeTree;

// Internal helpers for `OrderedScope<T>` and `OrderedScopeTree<T>`.
//
// These helpers are implementation details shared between the ordered
// scope templates and are not part of the public API. Keep changes here
// minimal and avoid depending on this namespace from unrelated code.
namespace ordered_scope_internal {

// Comparator for finding insertion position in tree order.
template <typename T, typename GetElement>
struct TreeOrderComparator {
  GetElement get_element;

  bool operator()(const Element* element, const T* item) const {
    return LayoutTreeBuilderTraversal::ComparePreorderTreePosition(
               *element, *get_element(item)) < 0;
  }

  bool operator()(const Element& element, const T* item) const {
    return LayoutTreeBuilderTraversal::ComparePreorderTreePosition(
               element, *get_element(item)) < 0;
  }
};

}  // namespace ordered_scope_internal

// A scope that maintains items of type T ordered by flat tree position.
//
// This is a generic template. To use with a specific type T, provide explicit
// specializations for the following static methods:
//   - GetItemElement(const T*) - returns the Element associated with an item
//   - OnItemAttached(T*, OrderedScope<T>*) - called when item is attached
//   - OnItemDetached(T*) - called when item is detached
//   - CreatesScope(const Element&) - returns true if element creates a scope
//   - UpdateItemsInScope(const OrderedScope<T>*) - updates items recursively
template <typename T>
class OrderedScope : public GarbageCollected<OrderedScope<T>> {
 public:
  using TreeType = OrderedScopeTree<T>;

  OrderedScope(const Element* scope_root, TreeType* tree)
      : scope_root_(scope_root), tree_(tree) {}

  // Static methods requiring explicit specialization for each T.
  static const Element* GetItemElement(const T* item);
  static void OnItemAttached(T* item, OrderedScope<T>* scope);
  static void OnItemDetached(T* item);
  static bool CreatesScope(const Element& element);
  static void UpdateItemsInScope(const OrderedScope<T>* scope);

  // Attaches an item to this scope, maintaining tree order.
  void AttachItem(T& item) {
    const Element* item_element = GetItemElement(&item);
    CHECK(item_element);

    // Find insertion position in tree order, which is the first item that goes
    // after `item_element`, and then insert before it.
    auto comp = blink::ordered_scope_internal::TreeOrderComparator<
        T, decltype(&GetItemElement)>{&GetItemElement};
    auto it =
        std::upper_bound(items_.begin(), items_.end(), item_element, comp);

    items_.insert(static_cast<wtf_size_t>(it - items_.begin()), &item);
    OnItemAttached(&item, this);
  }

  // Detaches an item from this scope.
  void DetachItem(T& item) {
    wtf_size_t pos = items_.Find(&item);
    CHECK_NE(pos, kNotFound);
    items_.EraseAt(pos);
    OnItemDetached(&item);
  }

  // Reattaches all items and children to the parent scope.
  // Used when this scope is being removed.
  void ReattachToParent() {
    if (!parent_) {
      return;
    }

    // Find position for the first item, then insert all items sequentially.
    if (!items_.empty()) {
      const Element* first_element = GetItemElement(items_.front());
      auto comp = blink::ordered_scope_internal::TreeOrderComparator<
          T, decltype(&GetItemElement)>{&GetItemElement};
      auto it = std::upper_bound(parent_->items_.begin(), parent_->items_.end(),
                                 first_element, comp);
      wtf_size_t insert_pos =
          static_cast<wtf_size_t>(it - parent_->items_.begin());

      for (T* item : items_) {
        OnItemDetached(item);
        parent_->items_.insert(insert_pos++, item);
        OnItemAttached(item, parent_);
      }
      items_.clear();
    }

    auto children = std::move(children_);
    for (OrderedScope<T>* child : children) {
      child->SetParent(nullptr);
      parent_->AppendChild(child);
    }
    parent_->RemoveChild(this);
  }

  // Clears the scope without reattaching items to parent.
  // Used when the element is being removed from the DOM tree entirely.
  void Clear() {
    if (parent_) {
      parent_->RemoveChild(this);
    }
    for (OrderedScope<T>* child : children_) {
      child->SetParent(nullptr);
    }
    children_.clear();
    for (T* item : items_) {
      OnItemDetached(item);
    }
    items_.clear();
  }

  // Appends a child scope.
  void AppendChild(OrderedScope<T>* child) {
    CHECK(!child->Parent());
    children_.emplace_back(child);
    child->SetParent(this);
  }

  // Removes a child scope.
  void RemoveChild(OrderedScope<T>* child) {
    wtf_size_t pos = children_.Find(child);
    CHECK_NE(pos, kNotFound);
    children_.EraseAt(pos);
    child->SetParent(nullptr);
  }

  // Returns true if this scope is an ancestor of the given element.
  bool IsAncestorOf(const Element* element,
                    const Element* stay_within = nullptr) const {
    if (!scope_root_) {
      return true;  // Root scope is ancestor of everything.
    }
    for (const Element* it = element; it && it != stay_within;
         it = LayoutTreeBuilderTraversal::ParentElement(*it)) {
      if (it == scope_root_) {
        return true;
      }
    }
    return false;
  }

  // Finds the item that precedes the given element in tree order.
  T* FindItemPrecedingElement(const Element& element) const {
    // comp returns true if `element` goes before item in preorder tree
    // traversal.
    auto comp = blink::ordered_scope_internal::TreeOrderComparator<
        T, decltype(&GetItemElement)>{&GetItemElement};
    // Find the first item that goes after `element`.
    auto it = std::upper_bound(items_.begin(), items_.end(), element, comp);
    // Return the item before it, or nullptr if none.
    return it == items_.begin() ? nullptr : *std::prev(it);
  }

  const Element* GetScopeRoot() const { return scope_root_.Get(); }
  OrderedScope<T>* Parent() const { return parent_.Get(); }
  const HeapVector<Member<T>>& Items() const { return items_; }
  const HeapVector<Member<OrderedScope<T>>>& Children() const {
    return children_;
  }
  TreeType* GetTree() const { return tree_.Get(); }

  void Trace(Visitor* visitor) const {
    visitor->Trace(scope_root_);
    visitor->Trace(parent_);
    visitor->Trace(items_);
    visitor->Trace(children_);
    visitor->Trace(tree_);
  }

 private:
  friend class OrderedScopeTree<T>;

  void SetParent(OrderedScope<T>* parent) { parent_ = parent; }

  WeakMember<const Element> scope_root_;
  Member<OrderedScope<T>> parent_;
  HeapVector<Member<T>> items_;
  HeapVector<Member<OrderedScope<T>>> children_;
  WeakMember<TreeType> tree_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ORDERED_SCOPE_H_

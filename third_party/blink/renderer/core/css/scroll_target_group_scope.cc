// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/scroll_target_group_scope.h"

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_data.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

// Gets ScrollMarkerGroupData from a scope, returns nullptr if not available.
ScrollMarkerGroupData* GetScrollMarkerGroupDataForScope(
    const OrderedScope<HTMLAnchorElement>* scope) {
  const Element* scope_root = scope->GetScopeRoot();
  if (!scope_root) {
    return nullptr;
  }
  // scope_root is const, but we need non-const access to modify data.
  return const_cast<Element*>(scope_root)->GetScrollTargetGroupData();
}

// Ensures ScrollMarkerGroupData exists for a scope.
ScrollMarkerGroupData& EnsureScrollMarkerGroupDataForScope(
    const OrderedScope<HTMLAnchorElement>* scope) {
  const Element* scope_root = scope->GetScopeRoot();
  CHECK(scope_root);
  return const_cast<Element*>(scope_root)->EnsureScrollTargetGroupData();
}

}  // namespace

// Returns the element associated with an HTMLAnchorElement item.
template <>
const Element* OrderedScope<HTMLAnchorElement>::GetItemElement(
    const HTMLAnchorElement* anchor) {
  return anchor;
}

// Called when an anchor element is attached to a scope.
// Adds the anchor to the scope root's ScrollMarkerGroupData.
template <>
void OrderedScope<HTMLAnchorElement>::OnItemAttached(
    HTMLAnchorElement* anchor,
    OrderedScope<HTMLAnchorElement>* scope) {
  if (!scope->GetScopeRoot()) {
    // Root scope has no associated element, so nothing to attach to.
    return;
  }
  ScrollMarkerGroupData& data = EnsureScrollMarkerGroupDataForScope(scope);
  data.AddToFocusGroup(*anchor);
  data.SetNeedsScrollersMapUpdate();
}

// Called when an anchor element is detached from a scope.
// Removes the anchor from its ScrollMarkerGroupData.
template <>
void OrderedScope<HTMLAnchorElement>::OnItemDetached(
    HTMLAnchorElement* anchor) {
  if (ScrollMarkerGroupData* data =
          anchor->GetScrollTargetGroupContainerData()) {
    data->RemoveFromFocusGroup(*anchor);
    // Mark that scrollable area subscriptions need updating.
    data->SetNeedsScrollersMapUpdate();
  }
}

// Returns true if the element creates a scroll-target-group scope.
// Elements with scroll-target-group: auto create scopes.
template <>
bool OrderedScope<HTMLAnchorElement>::CreatesScope(const Element& element) {
  const ComputedStyle* style = element.GetComputedStyle();
  return style && !style->ScrollTargetGroupNone();
}

// For scroll-target-group, we don't store items in items_.
// Anchors are stored in ScrollMarkerGroupData::focus_group_ instead.
template <>
bool OrderedScope<HTMLAnchorElement>::StoresItemsInScope() {
  return false;
}

// Called when scope is cleared (element removed from DOM).
// Clears the focus_group_ in ScrollMarkerGroupData.
template <>
void OrderedScope<HTMLAnchorElement>::OnScopeCleared(
    OrderedScope<HTMLAnchorElement>* scope) {
  if (ScrollMarkerGroupData* data = GetScrollMarkerGroupDataForScope(scope)) {
    data->ClearFocusGroup();
  }
}

// Called when scope items should be reattached to parent.
// Moves anchors from current scope's focus_group_ to parent's focus_group_.
template <>
void OrderedScope<HTMLAnchorElement>::OnScopeReattachItems(
    OrderedScope<HTMLAnchorElement>* scope,
    OrderedScope<HTMLAnchorElement>* parent) {
  ScrollMarkerGroupData* data = GetScrollMarkerGroupDataForScope(scope);
  if (!data) {
    return;
  }

  // Make a copy since we'll be modifying the focus_group_.
  HeapVector<Member<Element>> anchors_to_move = data->ScrollMarkers();
  for (Element* anchor : anchors_to_move) {
    OnItemDetached(To<HTMLAnchorElement>(anchor));
    OnItemAttached(To<HTMLAnchorElement>(anchor), parent);
  }
}

// Called when a new scope is created to move items from parent.
// Moves anchors from parent's focus_group_ that belong to the new scope.
template <>
void OrderedScope<HTMLAnchorElement>::OnScopeMoveItemsFromParent(
    OrderedScope<HTMLAnchorElement>* new_scope,
    OrderedScope<HTMLAnchorElement>* parent_scope) {
  ScrollMarkerGroupData* parent_data =
      GetScrollMarkerGroupDataForScope(parent_scope);
  if (!parent_data) {
    // Parent is root scope or has no data - anchors will be added via
    // OnScopeCreated traversal.
    return;
  }

  const Element* parent_root = parent_scope->GetScopeRoot();
  HeapVector<Member<Element>> anchors_to_move;
  for (Element* anchor : parent_data->ScrollMarkers()) {
    if (new_scope->IsAncestorOf(anchor, parent_root)) {
      anchors_to_move.push_back(anchor);
    }
  }

  for (Element* anchor : anchors_to_move) {
    parent_scope->DetachItem(*To<HTMLAnchorElement>(anchor));
    new_scope->AttachItem(*To<HTMLAnchorElement>(anchor));
  }
}

// Updates items in the scope recursively.
// For scroll-target-group, we update the ScrollMarkerGroupData
// subscriptions to scrollable areas and update the selected marker.
template <>
void OrderedScope<HTMLAnchorElement>::UpdateItemsInScope(
    const OrderedScope<HTMLAnchorElement>* scope) {
  if (ScrollMarkerGroupData* data = GetScrollMarkerGroupDataForScope(scope)) {
    data->UpdateScrollableAreaSubscriptions();
    data->UpdateSelectedScrollMarker();
  }

  // Recursively update child scopes.
  for (const OrderedScope<HTMLAnchorElement>* child : scope->Children()) {
    UpdateItemsInScope(child);
  }
}

// Called when a new scroll-target-group scope is created.
// Traverses descendants in layout tree order, skipping nested scopes,
// and attaches anchors with valid scroll targets.
// Does nothing for root scope or non-root scopes with a non-root scope parent,
// since those are covered by reparenting.
template <>
void OrderedScope<HTMLAnchorElement>::OnScopeCreated(
    OrderedScope<HTMLAnchorElement>* scope) {
  const Element* scope_root = scope->GetScopeRoot();
  if (!scope_root || (scope->Parent() && scope->Parent()->GetScopeRoot())) {
    return;
  }

  // Flat tree traversal within scope_root, skipping nested scope subtrees.
  for (Node* node = LayoutTreeBuilderTraversal::FirstChild(*scope_root);
       node;) {
    Element* element = DynamicTo<Element>(node);

    // If this element creates a nested scope, skip its entire subtree.
    if (element && CreatesScope(*element)) {
      node =
          LayoutTreeBuilderTraversal::NextSkippingChildren(*node, scope_root);
      continue;
    }

    // Check if this is an anchor element we should attach.
    if (auto* anchor = DynamicTo<HTMLAnchorElement>(element)) {
      if (anchor->ScrollTargetElement()) {
        DCHECK(!anchor->GetScrollTargetGroupContainerData());
        scope->AttachItem(*anchor);
      }
    }

    node = LayoutTreeBuilderTraversal::Next(*node, scope_root);
  }
}

// Explicit template instantiation definitions.
// These provide the actual template implementations for HTMLAnchorElement
// instantiations, preventing code duplication across translation units.
template class OrderedScope<HTMLAnchorElement>;
template class OrderedScopeTree<HTMLAnchorElement>;

}  // namespace blink

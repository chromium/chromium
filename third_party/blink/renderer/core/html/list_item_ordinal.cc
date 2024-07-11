// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/list_item_ordinal.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_menu_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/layout/list/layout_inline_list_item.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ListItemOrdinal::ListItemOrdinal() : type_(kNeedsUpdate) {}

bool ListItemOrdinal::IsListOwner(const Node& node) {
  // Counters must not cross the list owner, which can be either <ol>, <ul>,
  // or <menu> element. Additionally, counters should not cross elements that
  // have style containment, hence we pretend such elements are list owners for
  // the purposes of calculating ordinal values.
  // See https://html.spec.whatwg.org/#the-li-element and
  // https://drafts.csswg.org/css-contain-2/#containment-style for more details.
  return IsA<HTMLUListElement>(node) || IsA<HTMLOListElement>(node) ||
         (RuntimeEnabledFeatures::HTMLMenuElementIsListOwnerEnabled() &&
          IsA<HTMLMenuElement>(node)) ||
         HasStyleContainment(node);
}

bool ListItemOrdinal::IsListItem(const LayoutObject* layout_object) {
  return layout_object && layout_object->IsListItem();
}

bool ListItemOrdinal::IsListItem(const Node& node) {
  return IsListItem(node.GetLayoutObject());
}

bool ListItemOrdinal::IsInReversedOrderedList(const Node& node) {
  const Node* list = EnclosingList(&node);
  auto* olist = DynamicTo<HTMLOListElement>(list);
  return olist && olist->IsReversed();
}

ListItemOrdinal* ListItemOrdinal::Get(const Node& item_node) {
  auto* object = item_node.GetLayoutObject();
  if (auto* list_item = DynamicTo<LayoutListItem>(object)) {
    return &list_item->Ordinal();
  } else if (auto* inline_list_item = DynamicTo<LayoutInlineListItem>(object)) {
    return &inline_list_item->Ordinal();
  }
  return nullptr;
}

bool ListItemOrdinal::HasStyleContainment(const Node& node) {
  if (LayoutObject* layout_object = node.GetLayoutObject()) {
    return layout_object->ShouldApplyStyleContainment();
  }
  return false;
}

// Returns the enclosing list with respect to the DOM order.
Node* ListItemOrdinal::EnclosingList(const Node* list_item_node) {
  if (!list_item_node)
    return nullptr;
  Node* first_node = nullptr;
  // We use parentNode because the enclosing list could be a ShadowRoot that's
  // not Element.
  for (Node* parent = FlatTreeTraversal::Parent(*list_item_node); parent;
       parent = FlatTreeTraversal::Parent(*parent)) {
    if (IsListOwner(*parent)) {
      return parent;
    }
    if (!first_node)
      first_node = parent;
  }

  // If there is no actual list element such as <ul>, <ol>, or <menu>, then the
  // first found node acts as our list for purposes of determining what other
  // list items should be numbered as part of the same list.
  return first_node;
}

// Returns the next list item with respect to the DOM order.
ListItemOrdinal::NodeAndOrdinal ListItemOrdinal::NextListItem(
    const Node* list_node,
    const Node* item) {
  if (!list_node)
    return {};

  const Node* current = item ? item : list_node;
  DCHECK(current);
  current = LayoutTreeBuilderTraversal::Next(*current, list_node);

  while (current) {
    if (IsListOwner(*current)) {
      // We've found a nested, independent list: nothing to do here.
      current =
          LayoutTreeBuilderTraversal::NextSkippingChildren(*current, list_node);
      continue;
    }

    if (ListItemOrdinal* ordinal = Get(*current))
      return {current, ordinal};

    // FIXME: Can this be optimized to skip the children of the elements without
    // a layoutObject?
    current = LayoutTreeBuilderTraversal::Next(*current, list_node);
  }

  return {};
}

// Returns the previous list item with respect to the DOM order.
ListItemOrdinal::NodeAndOrdinal ListItemOrdinal::PreviousListItem(
    const Node* list_node,
    const Node* item) {
  const Node* current = item;
  DCHECK(current);
  for (current = LayoutTreeBuilderTraversal::Previous(*current, list_node);
       current && current != list_node;
       current = LayoutTreeBuilderTraversal::Previous(*current, list_node)) {
    ListItemOrdinal* ordinal = Get(*current);
    if (!ordinal)
      continue;
    const Node* other_list = EnclosingList(current);
    // This item is part of our current list, so it's what we're looking for.
    if (list_node == other_list)
      return {current, ordinal};
    // We found ourself inside another list; lets skip the rest of it.
    // Use nextIncludingPseudo() here because the other list itself may actually
    // be a list item itself. We need to examine it, so we do this to counteract
    // the previousIncludingPseudo() that will be done by the loop.
    if (other_list)
      current = LayoutTreeBuilderTraversal::Next(*other_list, list_node);
  }
  return {};
}

// Returns the item for the next ordinal value. It is usually the next list
// item, except when the <ol> element has the 'reversed' attribute.
ListItemOrdinal::NodeAndOrdinal ListItemOrdinal::NextOrdinalItem(
    bool is_list_reversed,
    const Node* list,
    const Node* item) {
  return is_list_reversed ? PreviousListItem(list, item)
                          : NextListItem(list, item);
}

std::optional<int> ListItemOrdinal::ExplicitValue() const {
  if (!HasExplicitValue())
    return {};
  return value_;
}

int ListItemOrdinal::CalcValue(const Node& item_node) const {
  if (HasExplicitValue())
    return value_;

  Node* list = EnclosingList(&item_node);
  auto* o_list_element = DynamicTo<HTMLOListElement>(list);
  const bool is_reversed = o_list_element && o_list_element->IsReversed();
  int value_step = is_reversed ? -1 : 1;
  if (const auto* style = To<Element>(item_node).GetComputedStyle()) {
    const auto directives =
        style->GetCounterDirectives(AtomicString("list-item"));
    if (directives.IsSet())
      return directives.CombinedValue();
    if (directives.IsIncrement())
      value_step = directives.CombinedValue();
  }

  int64_t base_value = 0;
  // FIXME: This recurses to a possible depth of the length of the list.
  // That's not good -- we need to change this to an iterative algorithm.
  if (NodeAndOrdinal previous = PreviousListItem(list, &item_node)) {
    base_value = previous.ordinal->Value(*previous.node);
  } else if (o_list_element) {
    base_value = o_list_element->StartConsideringItemCount();
    base_value += (is_reversed ? 1 : -1);
  }
  return base::saturated_cast<int>(base_value + value_step);
}

int ListItemOrdinal::Value(const Node& item_node) const {
  if (Type() != kNeedsUpdate)
    return value_;
  value_ = CalcValue(item_node);
  SetType(kUpdated);
  return value_;
}

// Invalidate one instance of |ListItemOrdinal|.
void ListItemOrdinal::InvalidateSelf(const Node& item_node, ValueType type) {
  DCHECK_NE(type, kUpdated);
  SetType(type);

  auto* object = item_node.GetLayoutObject();
  if (auto* list_item = DynamicTo<LayoutListItem>(object)) {
    list_item->OrdinalValueChanged();
  } else if (auto* inline_list_item = DynamicTo<LayoutInlineListItem>(object)) {
    inline_list_item->OrdinalValueChanged();
  }
}

// Invalidate items after |item_node| in the DOM order.
void ListItemOrdinal::InvalidateAfter(const Node* list_node,
                                      const Node* item_node) {
  for (NodeAndOrdinal item = NextListItem(list_node, item_node); item;
       item = NextListItem(list_node, item.node)) {
    DCHECK(item.ordinal);
    if (item.ordinal->Type() == kUpdated)
      item.ordinal->InvalidateSelf(*item.node);
  }
}

// Invalidate items after |item_node| in the ordinal order.
void ListItemOrdinal::InvalidateOrdinalsAfter(bool is_reversed,
                                              const Node* list_node,
                                              const Node* item_node) {
  for (NodeAndOrdinal item = NextOrdinalItem(is_reversed, list_node, item_node);
       item; item = NextOrdinalItem(is_reversed, list_node, item.node)) {
    DCHECK(item.ordinal);
    if (item.ordinal->Type() != kUpdated) {
      // If an item has been marked for update before, we can safely
      // assume that all the following ones have too.
      // This gives us the opportunity to stop here and avoid
      // marking the same nodes again.
      return;
    }
    item.ordinal->InvalidateSelf(*item.node);
  }
}

void ListItemOrdinal::SetExplicitValue(int value, const Node& item_node) {
  if (HasExplicitValue() && value_ == value)
    return;
  value_ = value;
  InvalidateSelf(item_node, kExplicit);
  InvalidateAfter(EnclosingList(&item_node), &item_node);
}

void ListItemOrdinal::ClearExplicitValue(const Node& item_node) {
  if (!HasExplicitValue())
    return;
  InvalidateSelf(item_node);
  InvalidateAfter(EnclosingList(&item_node), &item_node);
}

unsigned ListItemOrdinal::ItemCountForOrderedList(
    const HTMLOListElement* list_node) {
  DCHECK(list_node);

  unsigned item_count = 0;
  for (NodeAndOrdinal list_item = NextListItem(list_node); list_item;
       list_item = NextListItem(list_node, list_item.node))
    item_count++;

  return item_count;
}

void ListItemOrdinal::InvalidateAllItemsForOrderedList(
    const HTMLOListElement* list_node) {
  DCHECK(list_node);

  if (NodeAndOrdinal list_item = NextListItem(list_node)) {
    list_item.ordinal->InvalidateSelf(*list_item.node);
    InvalidateAfter(list_node, list_item.node);
  }
}

// TODO(layout-dev): We should use layout tree traversal instead of flat tree
// traversal to invalidate ordinal number cache since lite items in unassigned
// slots don't have cached value. See http://crbug.com/844277 for details.
void ListItemOrdinal::ItemUpdated(const LayoutObject* layout_list_item,
                                  UpdateType type) {
  const Node* item_node = layout_list_item->GetNode();
  if (item_node->GetDocument().IsSlotAssignmentDirty())
    return;
  if (item_node->GetDocument().IsFlatTreeTraversalForbidden())
    return;

  Node* list_node = EnclosingList(item_node);
  CHECK(list_node);

  bool is_list_reversed = false;
  if (auto* o_list_element = DynamicTo<HTMLOListElement>(list_node)) {
    if (type == kInsertedOrRemoved)
      o_list_element->ItemCountChanged();
    is_list_reversed = o_list_element->IsReversed();
  }

  // FIXME: The n^2 protection below doesn't help if the elements were inserted
  // after the the list had already been displayed.

  // Avoid an O(n^2) walk over the children below when they're all known to be
  // attaching.
  if (list_node->NeedsReattachLayoutTree())
    return;

  if (type == kCounterStyle) {
    ListItemOrdinal* ordinal = Get(*item_node);
    DCHECK(ordinal);
    ordinal->InvalidateSelf(*item_node);
  }
  InvalidateOrdinalsAfter(is_list_reversed, list_node, item_node);
}

void ListItemOrdinal::ItemInsertedOrRemoved(
    const LayoutObject* layout_list_item) {
  ItemUpdated(layout_list_item, kInsertedOrRemoved);
}

void ListItemOrdinal::ItemCounterStyleUpdated(
    const LayoutObject& layout_list_item) {
  ItemUpdated(&layout_list_item, kCounterStyle);
}

}  // namespace blink

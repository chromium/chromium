/**
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/layout/layout_counter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/layout/counter_node.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#if DCHECK_IS_ON()
#include <stdio.h>
#endif

namespace blink {

typedef HeapHashMap<WeakMember<const LayoutObject>, Member<CounterMap>>
    CounterMaps;

namespace {

CounterNode* MakeCounterNodeIfNeeded(LayoutObject&,
                                     const AtomicString& identifier,
                                     bool always_create_counter);

// See class definition as to why we have this map.
CounterMaps& GetCounterMaps() {
  DEFINE_STATIC_LOCAL(Persistent<CounterMaps>, static_counter_maps,
                      (MakeGarbageCollected<CounterMaps>()));
  return *static_counter_maps;
}

Element* AncestorStyleContainmentObject(const Element& element) {
  for (Element* ancestor = element.GetStyleRecalcParent(); ancestor;
       ancestor = ancestor->GetStyleRecalcParent()) {
    if (const ComputedStyle* style = ancestor->GetComputedStyle()) {
      if (style->ContainsStyle())
        return ancestor;
    }
  }
  return nullptr;
}

int ValueForText(CounterNode* node) {
  return node->ActsAsReset() ? node->Value() : node->CountInParent();
}

// This function processes the DOM tree including pseudo elements as defined in
// CSS 2.1. This method will always return either a previous element within the
// same contain: style scope or nullptr.
Element* PreviousInPreOrderRespectingContainment(const Element& element) {
  Element* previous = ElementTraversal::PreviousIncludingPseudo(element);
  Element* style_contain_ancestor = AncestorStyleContainmentObject(element);

  while (true) {
    // Find the candidate previous element.
    while (previous && !previous->GetLayoutObject() &&
           !previous->HasDisplayContentsStyle())
      previous = ElementTraversal::PreviousIncludingPseudo(*previous);
    if (!previous)
      return nullptr;
    Element* previous_style_contain_ancestor =
        AncestorStyleContainmentObject(*previous);
    // If the candidate's containment ancestor is the same as elements, then
    // that's a valid candidate.
    if (previous_style_contain_ancestor == style_contain_ancestor)
      return previous;

    // Otherwise, if previous does not have a containment ancestor, it means
    // that we have already escaped `element`'s containment ancestor, so return
    // nullptr.
    if (!previous_style_contain_ancestor)
      return nullptr;

    // If, however, the candidate does have a containment ancestor, it could be
    // that we entered a new sub-containment. Try again starting from the
    // contain ancestor.
    previous = previous_style_contain_ancestor;
  }
}

// This function processes the DOM including pseudo elements as defined in
// CSS 2.1. This method avoids crossing contain: style boundaries.
Element* PreviousSiblingOrParentRespectingContainment(const Element& element) {
  Element* previous = ElementTraversal::PseudoAwarePreviousSibling(element);
  // Skip display:none elements.
  while (previous && !previous->GetLayoutObject() &&
         !previous->HasDisplayContentsStyle())
    previous = ElementTraversal::PseudoAwarePreviousSibling(*previous);
  if (previous)
    return previous;
  previous = element.parentElement();
  if (previous) {
    if (const ComputedStyle* style = previous->GetComputedStyle()) {
      if (style->ContainsStyle())
        return nullptr;
    }
  }
  return previous;
}

inline bool AreElementsSiblings(const Element& first, const Element& second) {
  return first.parentElement() == second.parentElement();
}

// This function processes the the DOM tree including pseudo elements as defined
// in CSS 2.1.
LayoutObject* NextInPreOrder(const LayoutObject& object,
                             const Element* stay_within,
                             bool skip_descendants = false) {
  auto* self = To<Element>(object.GetNode());
  DCHECK(self);
  Element* next =
      skip_descendants
          ? ElementTraversal::NextIncludingPseudoSkippingChildren(*self,
                                                                  stay_within)
          : ElementTraversal::NextIncludingPseudo(*self, stay_within);
  while (next && !next->GetLayoutObject())
    next = skip_descendants
               ? ElementTraversal::NextIncludingPseudoSkippingChildren(
                     *next, stay_within)
               : ElementTraversal::NextIncludingPseudo(*next, stay_within);
  return next ? next->GetLayoutObject() : nullptr;
}

bool PlanCounter(LayoutObject& object,
                 const AtomicString& identifier,
                 unsigned& type_mask,
                 int& value) {
  // Real text nodes don't have their own style so they can't have counters.
  // We can't even look at their styles or we'll see extra resets and
  // increments!
  if (object.IsText() && !object.IsBR())
    return false;
  Node* generating_node = object.GeneratingNode();
  // We must have a generating node or else we cannot have a counter.
  if (!generating_node)
    return false;
  const ComputedStyle& style = object.StyleRef();

  switch (style.StyleType()) {
    case kPseudoIdNone:
      // Sometimes nodes have more than one layout object. Only the first one
      // gets the counter. See web_tests/http/tests/css/counter-crash.html
      if (generating_node->GetLayoutObject() != &object)
        return false;
      break;
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdMarker:
      break;
    default:
      return false;  // Counters are forbidden from all other pseudo elements.
  }

  type_mask = 0;
  const CounterDirectives directives = style.GetCounterDirectives(identifier);
  if (directives.IsDefined()) {
    value = directives.CombinedValue();
    type_mask |= directives.IsIncrement() ? CounterNode::kIncrementType : 0;
    type_mask |= directives.IsReset() ? CounterNode::kResetType : 0;
    type_mask |= directives.IsSet() ? CounterNode::kSetType : 0;
    return true;
  }

  if (identifier == "list-item") {
    if (Node* e = object.GetNode()) {
      if (ListItemOrdinal* ordinal = ListItemOrdinal::Get(*e)) {
        if (const auto& explicit_value = ordinal->ExplicitValue()) {
          value = explicit_value.value();
          type_mask = CounterNode::kResetType;
          return true;
        }
        value = ListItemOrdinal::IsInReversedOrderedList(*e) ? -1 : 1;
        type_mask = CounterNode::kIncrementType;
        return true;
      }
      if (auto* olist = DynamicTo<HTMLOListElement>(*e)) {
        value = base::ClampAdd(olist->StartConsideringItemCount(),
                               olist->IsReversed() ? 1 : -1);
        type_mask = CounterNode::kResetType;
        return true;
      }
      if (IsA<HTMLUListElement>(*e) || IsA<HTMLMenuElement>(*e) ||
          IsA<HTMLDirectoryElement>(*e)) {
        value = 0;
        type_mask = CounterNode::kResetType;
        return true;
      }
    }
  }

  return false;
}

// - Finds the insertion point for the counter described by counter_owner,
//   IsReset and identifier in the CounterNode tree for identifier and sets
//   parent and previous_sibling accordingly.
// - The function returns true if the counter whose insertion point is searched
//   is NOT the root of the tree.
// - The root of the tree is a counter reference that is not in the scope of any
//   other counter with the same identifier.
// - All the counter references with the same identifier as this one that are in
//   children or subsequent siblings of the layout object that owns the root of
//   the tree form the rest of of the nodes of the tree.
// - The root of the tree is always a reset type reference.
// - A subtree rooted at any reset node in the tree is equivalent to all counter
//   references that are in the scope of the counter or nested counter defined
//   by that reset node.
// - Non-reset CounterNodes cannot have descendants.
bool FindPlaceForCounter(LayoutObject& counter_owner,
                         const AtomicString& identifier,
                         bool is_reset,
                         CounterNode** parent,
                         CounterNode** previous_sibling) {
  // We cannot stop searching for counters with the same identifier before
  // we also check this layout object, because it may affect the positioning
  // in the tree of our counter.
  auto* counter_owner_element = To<Element>(counter_owner.GetNode());
  Element* search_end_element =
      PreviousSiblingOrParentRespectingContainment(*counter_owner_element);
  Element* current_element =
      PreviousInPreOrderRespectingContainment(*counter_owner_element);
  *previous_sibling = nullptr;
  CounterNode* previous_sibling_protector = nullptr;
  while (current_element) {
    CounterNode* current_counter = nullptr;
    if (LayoutObject* current_layout_object =
            current_element->GetLayoutObject()) {
      current_counter =
          MakeCounterNodeIfNeeded(*current_layout_object, identifier, false);
    }
    if (search_end_element == current_element) {
      // We may be at the end of our search.
      if (current_counter) {
        // We have a suitable counter on the search_end_element.
        if (previous_sibling_protector) {
          // But we already found another counter that we come after.
          if (current_counter->ActsAsReset()) {
            // We found a reset counter that is on a layout object that is a
            // sibling of ours or a parent.
            if (is_reset &&
                AreElementsSiblings(*current_element, *counter_owner_element)) {
              // We are also a reset counter and the previous reset was on a
              // sibling layout object hence we are the next sibling of that
              // counter if that reset is not a root or we are a root node if
              // that reset is a root.
              *parent = current_counter->Parent();
              *previous_sibling = *parent ? current_counter : nullptr;
              return *parent;
            }
            // We are not a reset node or the previous reset must be on an
            // ancestor of our owner layout object hence we must be a child of
            // that reset counter.
            *parent = current_counter;
            // In some cases layout objects can be reparented (ex. nodes inside
            // a table but not in a column or row). In these cases the
            // identified previous_sibling will be invalid as its parent is
            // different from our identified parent.
            if (previous_sibling_protector->Parent() != current_counter)
              previous_sibling_protector = nullptr;

            *previous_sibling = previous_sibling_protector;
            return true;
          }
          // CurrentCounter, the counter at the EndSearchLayoutObject, is not
          // reset.
          if (!is_reset ||
              !AreElementsSiblings(*current_element, *counter_owner_element)) {
            // If the node we are placing is not reset or we have found a
            // counter that is attached to an ancestor of the placed counter's
            // owner layout object we know we are a sibling of that node.
            if (current_counter->Parent() !=
                previous_sibling_protector->Parent())
              return false;

            *parent = current_counter->Parent();
            *previous_sibling = previous_sibling_protector;
            return true;
          }
        } else {
          // We are at the potential end of the search, but we had no previous
          // sibling candidate. In this case we follow pretty much the same
          // logic as above but no ASSERTs about previous_sibling, and when we
          // are a sibling of the end counter we must set previous_sibling to
          // current_counter.
          if (current_counter->ActsAsReset()) {
            if (is_reset &&
                AreElementsSiblings(*current_element, *counter_owner_element)) {
              *parent = current_counter->Parent();
              *previous_sibling = current_counter;
              return *parent;
            }
            *parent = current_counter;
            *previous_sibling = previous_sibling_protector;
            return true;
          }
          if (!is_reset ||
              !AreElementsSiblings(*current_element, *counter_owner_element)) {
            *parent = current_counter->Parent();
            *previous_sibling = current_counter;
            return true;
          }
          previous_sibling_protector = current_counter;
        }
      }
      // We come here if the previous sibling or parent of our owner
      // layout_object had no good counter, or we are a reset node and the
      // counter on the previous sibling of our owner layout_object was not a
      // reset counter. Set a new goal for the end of the search.
      search_end_element =
          PreviousSiblingOrParentRespectingContainment(*current_element);
    } else {
      // We are searching descendants of a previous sibling of the layout object
      // that the
      // counter being placed is attached to.
      if (current_counter) {
        // We found a suitable counter.
        if (previous_sibling_protector) {
          // Since we had a suitable previous counter before, we should only
          // consider this one as our previous_sibling if it is a reset counter
          // and hence the current previous_sibling is its child.
          if (current_counter->ActsAsReset()) {
            previous_sibling_protector = current_counter;
            // We are no longer interested in previous siblings of the
            // current_element or their children as counters they may have
            // attached cannot be the previous sibling of the counter we are
            // placing.
            current_element = current_element->parentElement();
            continue;
          }
        } else {
          previous_sibling_protector = current_counter;
        }
        current_element =
            PreviousSiblingOrParentRespectingContainment(*current_element);
        continue;
      }
    }
    // This function is designed so that the same test is not done twice in an
    // iteration, except for this one which may be done twice in some cases.
    // Rearranging the decision points though, to accommodate this performance
    // improvement would create more code duplication than is worthwhile in my
    // opinion and may further impede the readability of this already complex
    // algorithm.
    if (previous_sibling_protector) {
      current_element =
          PreviousSiblingOrParentRespectingContainment(*current_element);
    } else {
      current_element =
          PreviousInPreOrderRespectingContainment(*current_element);
    }
  }
  return false;
}

inline Element* ParentElement(LayoutObject& object) {
  return To<Element>(object.GetNode())->parentElement();
}

CounterNode* MakeCounterNodeIfNeeded(LayoutObject& object,
                                     const AtomicString& identifier,
                                     bool always_create_counter) {
  if (object.HasCounterNodeMap()) {
    auto it_counter = GetCounterMaps().find(&object);
    if (it_counter != GetCounterMaps().end()) {
      auto it_node = it_counter->value->find(identifier);
      if (it_node != it_counter->value->end())
        return &*it_node->value;
    }
  }

  unsigned type_mask = 0;
  int value = 0;
  if (!PlanCounter(object, identifier, type_mask, value) &&
      !always_create_counter)
    return nullptr;

  CounterNode* new_parent = nullptr;
  CounterNode* new_previous_sibling = nullptr;
  CounterNode* new_node =
      MakeGarbageCollected<CounterNode>(object, type_mask, value);

  if (type_mask & CounterNode::kResetType) {
    // Find the place where we would've inserted the new node if it was a
    // non-reset node. We have to move every non-reset sibling after the
    // insertion point to a child of the new node.
    CounterNode* old_parent = nullptr;
    CounterNode* old_previous_sibling = nullptr;
    if (FindPlaceForCounter(object, identifier, false, &old_parent,
                            &old_previous_sibling)) {
      if (!object.IsDescendantOf(&old_parent->Owner())) {
        CounterNode* first_node_to_move =
            old_previous_sibling ? old_previous_sibling->NextSibling()
                                 : old_parent->FirstChild();
        CounterNode::MoveNonResetSiblingsToChildOf(first_node_to_move,
                                                   *new_node, identifier);
      }
    }
  }

  if (FindPlaceForCounter(object, identifier,
                          type_mask & CounterNode::kResetType, &new_parent,
                          &new_previous_sibling))
    new_parent->InsertAfter(new_node, new_previous_sibling, identifier);
  CounterMap* node_map = nullptr;
  if (object.HasCounterNodeMap()) {
    node_map = GetCounterMaps().at(&object);
  } else {
    node_map = MakeGarbageCollected<CounterMap>();
    GetCounterMaps().Set(&object, node_map);
    object.SetHasCounterNodeMap(true);
  }
  node_map->Set(identifier, new_node);
  // If the new node has a parent, that means any descendant would have been
  // updated by `CounterNode::MoveNonResetSiblingsToChildOf()` above, so we
  // don't need to update descendants. Likewise, if the object has style
  // containment, any descendant should not become parented across the boundary.
  if (new_node->Parent() || object.ShouldApplyStyleContainment())
    return new_node;

  // Checking if some nodes that were previously counter tree root nodes
  // should become children of this node now.
  CounterMaps& maps = GetCounterMaps();
  Element* stay_within = ParentElement(object);
  bool skip_descendants;
  for (LayoutObject* current_layout_object =
           NextInPreOrder(object, stay_within);
       current_layout_object;
       current_layout_object = NextInPreOrder(*current_layout_object,
                                              stay_within, skip_descendants)) {
    // We'll update the current object and we might recurse into the
    // descendants. However, if the object has style containment then we do not
    // cross the boundary which begins right after the object. In other words we
    // skip the descendants of this object.
    skip_descendants = current_layout_object->ShouldApplyStyleContainment();
    if (!current_layout_object->HasCounterNodeMap())
      continue;
    auto* current_object = maps.at(current_layout_object);
    auto it = current_object->find(identifier);
    CounterNode* current_counter =
        it != current_object->end() ? &*it->value : nullptr;
    if (!current_counter)
      continue;
    // At this point we found a counter to reparent. So we don't need to descend
    // into the layout tree further, since any further counters we find would be
    // at most parented to `current_counter` we just found.
    skip_descendants = true;
    if (current_counter->Parent())
      continue;
    if (stay_within == ParentElement(*current_layout_object) &&
        current_counter->HasResetType())
      break;
    new_node->InsertAfter(current_counter, new_node->LastChild(), identifier);
  }
  return new_node;
}

String GenerateCounterText(const CounterStyle* counter_style, int value) {
  if (!counter_style)
    return g_empty_string;
  return counter_style->GenerateRepresentation(value);
}

}  // namespace

LayoutCounter::LayoutCounter(PseudoElement& pseudo,
                             const CounterContentData& counter)
    : LayoutText(nullptr, StringImpl::empty_),
      counter_(counter),
      counter_node_(nullptr),
      next_for_same_counter_(nullptr) {
  SetDocumentForAnonymous(&pseudo.GetDocument());
  View()->AddLayoutCounter();
}

LayoutCounter::~LayoutCounter() = default;

void LayoutCounter::Trace(Visitor* visitor) const {
  visitor->Trace(counter_);
  visitor->Trace(counter_node_);
  visitor->Trace(next_for_same_counter_);
  LayoutText::Trace(visitor);
}

void LayoutCounter::WillBeDestroyed() {
  NOT_DESTROYED();
  if (counter_node_) {
    counter_node_->RemoveLayoutObject(this);
    DCHECK(!counter_node_);
  }
  if (View())
    View()->RemoveLayoutCounter();
  LayoutText::WillBeDestroyed();
}

String LayoutCounter::OriginalText() const {
  NOT_DESTROYED();
  // Child will be the base of our text that we report. First, we need to find
  // an appropriate child.
  CounterNode* child = nullptr;

  // Find a container on which to create the counter if one needs creating.
  LayoutObject* container = Parent();
  bool should_create_counter = counter_->Separator().IsNull();
  // Optimization: the only reason we need a proper container is if we might not
  // need to create a counter (in which case, we navigate container's
  // ancestors), or if we don't have a counter_node_ (in which case we need to
  // find the container to place the counter on).
  if (!should_create_counter || !counter_node_) {
    while (true) {
      if (!container)
        return String();
      if (!container->IsAnonymous() && !container->IsPseudoElement())
        return String();  // LayoutCounters are restricted to before, after and
                          // marker pseudo elements
      PseudoId container_style = container->StyleRef().StyleType();
      if ((container_style == kPseudoIdBefore) ||
          (container_style == kPseudoIdAfter) ||
          (container_style == kPseudoIdMarker))
        break;
      container = container->Parent();
    }
  }

  // Now that we have a container, check if the counter directives are
  // defined between us and the first style containment element, meaning that
  // the counter would be created for our scope even if there is no content
  // request. If not, and if the separator is not null, meaning the request was
  // for something like counters(n, "."), then we first have to check our
  // ancestors across the style containment boundary. If the ancestors have the
  // value for our identifier, then we don't need a counter here and it is
  // instead omitted. See counter-scoping-001.html WPT and crbug.com/882383#c11
  // for more context.
  if (!should_create_counter) {
    for (auto* scope_ancestor = container; scope_ancestor;
         scope_ancestor = scope_ancestor->Parent()) {
      auto& style = scope_ancestor->StyleRef();
      if (style.ContainsStyle())
        break;
      const CounterDirectives directives =
          style.GetCounterDirectives(counter_->Identifier());
      if (directives.IsDefined()) {
        should_create_counter = true;
        break;
      }
    }
  }

  if (!should_create_counter) {
    // If we have an ancestor across the the containment boundary, then use it
    // as the child, without needing to create a counter on `this`. If we don't
    // have such an ancestor, we need to create a `counter_node_` on `this`.
    if (auto* node = CounterNode::AncestorNodeAcrossStyleContainment(
            *this, counter_->Identifier())) {
      child = node;
    } else {
      should_create_counter = true;
    }
  }

  if (should_create_counter) {
    if (!counter_node_) {
      MakeCounterNodeIfNeeded(*container, counter_->Identifier(), true)
          ->AddLayoutObject(const_cast<LayoutCounter*>(this));
      DCHECK(counter_node_);
    }
    child = counter_node_;
  }

  // In all cases we should end up with a `child` which is the base of our
  // navigation.
  DCHECK(child);

  int value = ValueForText(child);
  const CounterStyle* counter_style = NullableCounterStyle();
  String text = GenerateCounterText(counter_style, value);
  // If the separator exists, we need to append all of the parent values as
  // well, including the ones that cross the style containment boundary.
  if (!counter_->Separator().IsNull()) {
    if (!child->ActsAsReset())
      child = child->ParentCrossingStyleContainment(counter_->Identifier());
    bool next_result_uses_parent_value = !child->Parent();
    while (CounterNode* parent =
               child->ParentCrossingStyleContainment(counter_->Identifier())) {
      int next_value = next_result_uses_parent_value ? ValueForText(parent)
                                                     : child->CountInParent();
      text = GenerateCounterText(counter_style, next_value) +
             counter_->Separator() + text;
      child = parent;
      next_result_uses_parent_value = !child->Parent();
    }
  }

  return text;
}

void LayoutCounter::UpdateCounter() {
  NOT_DESTROYED();
  SetTextIfNeeded(OriginalText());
}

void LayoutCounter::Invalidate() {
  NOT_DESTROYED();
  counter_node_->RemoveLayoutObject(this);
  DCHECK(!counter_node_);
  if (DocumentBeingDestroyed())
    return;
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kCountersChanged);
}

static void DestroyCounterNodeWithoutMapRemoval(const AtomicString& identifier,
                                                CounterNode* node) {
  CounterNode* previous = nullptr;
  for (CounterNode* child = node->LastDescendant(); child && child != node;
       child = previous) {
    previous = child->PreviousInPreOrder();
    child->Parent()->RemoveChild(child);
    DCHECK(GetCounterMaps().at(&child->Owner())->at(identifier) == child);
    GetCounterMaps().at(&child->Owner())->Take(identifier)->Destroy();
  }
  if (CounterNode* parent = node->Parent()) {
    parent->RemoveChild(node);
  }
  node->Destroy();
}

void LayoutCounter::DestroyCounterNodes(LayoutObject& owner) {
  CounterMaps& maps = GetCounterMaps();
  CounterMaps::iterator maps_iterator = maps.find(&owner);
  if (maps_iterator == maps.end())
    return;
  CounterMap* map = maps_iterator->value;
  CounterMap::const_iterator end = map->end();
  for (CounterMap::const_iterator it = map->begin(); it != end; ++it) {
    DestroyCounterNodeWithoutMapRemoval(it->key, it->value);
  }
  maps.erase(maps_iterator);
  owner.SetHasCounterNodeMap(false);
  if (owner.View())
    owner.View()->SetNeedsMarkerOrCounterUpdate();
}

void LayoutCounter::DestroyCounterNode(LayoutObject& owner,
                                       const AtomicString& identifier) {
  auto it = GetCounterMaps().find(&owner);
  CounterMap* map = it != GetCounterMaps().end() ? &*it->value : nullptr;
  if (!map)
    return;
  CounterMap::iterator map_iterator = map->find(identifier);
  if (map_iterator == map->end())
    return;
  DestroyCounterNodeWithoutMapRemoval(identifier, map_iterator->value);
  map->erase(map_iterator);
  // We do not delete "map" here even if empty because we expect to reuse
  // it soon. In order for a layout object to lose all its counters permanently,
  // a style change for the layout object involving removal of all counter
  // directives must occur, in which case, LayoutCounter::DestroyCounterNodes()
  // must be called.
  // The destruction of the LayoutObject (possibly caused by the removal of its
  // associated DOM node) is the other case that leads to the permanent
  // destruction of all counters attached to a LayoutObject. In this case
  // LayoutCounter::DestroyCounterNodes() must be and is now called, too.
  // LayoutCounter::DestroyCounterNodes() handles destruction of the counter
  // map associated with a layout object, so there is no risk in leaking the
  // map.
}

void LayoutCounter::LayoutObjectSubtreeWillBeDetached(
    LayoutObject* layout_object) {
  DCHECK(layout_object->View());
  // View should never be non-zero. crbug.com/546939
  if (!layout_object->View() || !layout_object->View()->HasLayoutCounters())
    return;

  LayoutObject* current_layout_object = layout_object->LastLeafChild();
  if (!current_layout_object)
    current_layout_object = layout_object;
  while (true) {
    DestroyCounterNodes(*current_layout_object);
    if (current_layout_object == layout_object)
      break;
    current_layout_object = current_layout_object->PreviousInPreOrder();
  }
}

static void UpdateCounters(LayoutObject& layout_object) {
  DCHECK(layout_object.Style());
  const CounterDirectiveMap* directive_map =
      layout_object.StyleRef().GetCounterDirectives();
  if (!directive_map)
    return;
  CounterDirectiveMap::const_iterator end = directive_map->end();
  if (!layout_object.HasCounterNodeMap()) {
    for (CounterDirectiveMap::const_iterator it = directive_map->begin();
         it != end; ++it)
      MakeCounterNodeIfNeeded(layout_object, it->key, false);
    return;
  }
  auto it_counter = GetCounterMaps().find(&layout_object);
  CounterMap* counter_map =
      it_counter != GetCounterMaps().end() ? &*it_counter->value : nullptr;
  DCHECK(counter_map);
  for (CounterDirectiveMap::const_iterator it = directive_map->begin();
       it != end; ++it) {
    auto it_node = counter_map->find(it->key);
    CounterNode* node =
        it_node != counter_map->end() ? it_node->value : nullptr;
    if (!node) {
      MakeCounterNodeIfNeeded(layout_object, it->key, false);
      continue;
    }
    CounterNode* new_parent = nullptr;
    CounterNode* new_previous_sibling = nullptr;

    FindPlaceForCounter(layout_object, it->key, node->HasResetType(),
                        &new_parent, &new_previous_sibling);
    auto it_node2 = counter_map->find(it->key);
    if (it_node2 == counter_map->end() || (node != it_node2->value))
      continue;
    CounterNode* parent = node->Parent();
    if (new_parent == parent && new_previous_sibling == node->PreviousSibling())
      continue;
    if (parent)
      parent->RemoveChild(node);
    if (new_parent)
      new_parent->InsertAfter(node, new_previous_sibling, it->key);
  }
}

void LayoutCounter::LayoutObjectSubtreeAttached(LayoutObject* layout_object) {
  DCHECK(layout_object->View());
  // Only update counters if we have LayoutCounter which is created when we have
  // a content: field with a counter requirement.
  if (!layout_object->View()->HasLayoutCounters())
    return;
  Node* node = layout_object->GetNode();
  if (node)
    node = node->parentNode();
  else
    node = layout_object->GeneratingNode();
  if (node && node->NeedsReattachLayoutTree())
    return;  // No need to update if the parent is not attached yet

  // Update the descendants.
  for (LayoutObject* descendant = layout_object; descendant;
       descendant = descendant->NextInPreOrder(layout_object))
    UpdateCounters(*descendant);

  bool crossed_boundary = false;
  // Since we skipped counter updates if there were no counters, we might need
  // to update parent counters that lie beyond the style containment boundary.
  for (LayoutObject* parent = layout_object->Parent(); parent;
       parent = parent->Parent()) {
    crossed_boundary |= parent->ShouldApplyStyleContainment();
    if (crossed_boundary)
      UpdateCounters(*parent);
  }
}

void LayoutCounter::LayoutObjectStyleChanged(LayoutObject& layout_object,
                                             const ComputedStyle* old_style,
                                             const ComputedStyle& new_style) {
  if (layout_object.IsListItemIncludingNG())
    ListItemOrdinal::ItemCounterStyleUpdated(layout_object);
  Node* node = layout_object.GeneratingNode();
  if (!node || node->NeedsReattachLayoutTree())
    return;  // cannot have generated content or if it can have, it will be
             // handled during attaching
  const CounterDirectiveMap* old_counter_directives =
      old_style ? old_style->GetCounterDirectives() : nullptr;
  const CounterDirectiveMap* new_counter_directives =
      new_style.GetCounterDirectives();
  if (old_counter_directives) {
    if (new_counter_directives) {
      CounterDirectiveMap::const_iterator new_map_end =
          new_counter_directives->end();
      CounterDirectiveMap::const_iterator old_map_end =
          old_counter_directives->end();
      for (CounterDirectiveMap::const_iterator it =
               new_counter_directives->begin();
           it != new_map_end; ++it) {
        CounterDirectiveMap::const_iterator old_map_it =
            old_counter_directives->find(it->key);
        if (old_map_it != old_map_end) {
          if (old_map_it->value == it->value)
            continue;
          LayoutCounter::DestroyCounterNode(layout_object, it->key);
        }
        // We must create this node here, because the changed node may be a node
        // with no display such as as those created by the increment or reset
        // directives and the re-layout that will happen will not catch the
        // change if the node had no children.
        MakeCounterNodeIfNeeded(layout_object, it->key, false);
      }
      // Destroying old counters that do not exist in the new counterDirective
      // map.
      for (CounterDirectiveMap::const_iterator it =
               old_counter_directives->begin();
           it != old_map_end; ++it) {
        if (!new_counter_directives->Contains(it->key))
          LayoutCounter::DestroyCounterNode(layout_object, it->key);
      }
    } else {
      if (layout_object.HasCounterNodeMap())
        LayoutCounter::DestroyCounterNodes(layout_object);
    }
  } else if (new_counter_directives) {
    if (layout_object.HasCounterNodeMap())
      LayoutCounter::DestroyCounterNodes(layout_object);
    CounterDirectiveMap::const_iterator new_map_end =
        new_counter_directives->end();
    for (CounterDirectiveMap::const_iterator it =
             new_counter_directives->begin();
         it != new_map_end; ++it) {
      // We must create this node here, because the added node may be a node
      // with no display such as as those created by the increment or reset
      // directives and the re-layout that will happen will not catch the change
      // if the node had no children.
      MakeCounterNodeIfNeeded(layout_object, it->key, false);
    }
  }
}

// static
CounterMap* LayoutCounter::GetCounterMap(LayoutObject* object) {
  if (object->HasCounterNodeMap())
    return GetCounterMaps().at(object);
  return nullptr;
}

const CounterStyle* LayoutCounter::NullableCounterStyle() const {
  // Note: CSS3 spec doesn't allow 'none' but CSS2.1 allows it. We currently
  // allow it for backward compatibility.
  // See https://github.com/w3c/csswg-drafts/issues/5795 for details.
  if (counter_->ListStyle() == "none") {
    return nullptr;
  }
  return &GetDocument().GetStyleEngine().FindCounterStyleAcrossScopes(
      counter_->ListStyle(), counter_->GetTreeScope());
}

bool LayoutCounter::IsDirectionalSymbolMarker() const {
  const auto* counter_style = NullableCounterStyle();
  if (!counter_style || !counter_style->IsPredefinedSymbolMarker()) {
    return false;
  }
  const AtomicString& list_style = counter_->ListStyle();
  return list_style == keywords::kDisclosureOpen ||
         list_style == keywords::kDisclosureClosed;
}

const AtomicString& LayoutCounter::Separator() const {
  return counter_->Separator();
}

// static
const AtomicString& LayoutCounter::ListStyle(const LayoutObject* object,
                                             const ComputedStyle& style) {
  if (const auto* counter = DynamicTo<LayoutCounter>(object)) {
    return counter->counter_->ListStyle();
  }
  return style.ListStyleType()->GetCounterStyleName();
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowCounterLayoutTree(const blink::LayoutObject* layout_object,
                           const char* counter_name) {
  if (!layout_object)
    return;
  const blink::LayoutObject* root = layout_object;
  while (root->Parent())
    root = root->Parent();

  AtomicString identifier(counter_name);
  for (const blink::LayoutObject* current = root; current;
       current = current->NextInPreOrder()) {
    fprintf(stderr, "%c", (current == layout_object) ? '*' : ' ');
    for (const blink::LayoutObject* parent = current; parent && parent != root;
         parent = parent->Parent())
      fprintf(stderr, "    ");
    fprintf(stderr, "%p %s", current, current->DebugName().Utf8().c_str());
    auto* counter_node =
        current->HasCounterNodeMap() && current
            ? blink::GetCounterMaps().at(current)->at(identifier)
            : nullptr;
    if (counter_node) {
      fprintf(stderr, " counter:%p parent:%p value:%d countInParent:%d\n",
              counter_node, counter_node->Parent(), counter_node->Value(),
              counter_node->CountInParent());
    } else {
      fprintf(stderr, "\n");
    }
  }
}

#endif  // DCHECK_IS_ON()

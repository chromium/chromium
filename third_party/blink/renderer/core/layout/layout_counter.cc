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
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/counter_node.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

using namespace HTMLNames;

typedef HashMap<AtomicString, scoped_refptr<CounterNode>> CounterMap;
typedef HashMap<const LayoutObject*, std::unique_ptr<CounterMap>> CounterMaps;

static CounterNode* MakeCounterNodeIfNeeded(LayoutObject&,
                                            const AtomicString& identifier,
                                            bool always_create_counter);

// See class definition as to why we have this map.
static CounterMaps& GetCounterMaps() {
  DEFINE_STATIC_LOCAL(CounterMaps, static_counter_maps, ());
  return static_counter_maps;
}

Element* AncestorStyleContainmentObject(const Element& element) {
  for (Element* ancestor = FlatTreeTraversal::ParentElement(element); ancestor;
       ancestor = FlatTreeTraversal::ParentElement(*ancestor)) {
    if (const ComputedStyle* style = ancestor->GetComputedStyle()) {
      if (style->ContainsStyle())
        return ancestor;
    }
  }
  return nullptr;
}

// This function processes the DOM tree including pseudo elements as defined in
// CSS 2.1. This method will always return either a previous element within the
// same contain: style scope or nullptr.
static Element* PreviousInPreOrderRespectingContainment(
    const Element& element) {
  Element* previous = ElementTraversal::PreviousIncludingPseudo(element);
  Element* style_contain_ancestor = AncestorStyleContainmentObject(element);

  while (true) {
    while (previous && !previous->GetLayoutObject() &&
           !previous->HasDisplayContentsStyle())
      previous = ElementTraversal::PreviousIncludingPseudo(*previous);
    if (!previous)
      return nullptr;
    Element* previous_style_contain_ancestor =
        AncestorStyleContainmentObject(*previous);
    if (previous_style_contain_ancestor == style_contain_ancestor)
      return previous;
    if (!previous_style_contain_ancestor)
      return nullptr;
    previous = previous_style_contain_ancestor;
  }
}

// This function processes the DOM including pseudo elements as defined in
// CSS 2.1. This method avoids crossing contain: style boundaries.
static Element* PreviousSiblingOrParentRespectingContainment(
    const Element& element) {
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
      if (style->Contain() & kContainsStyle)
        return nullptr;
    }
  }
  return previous;
}

static inline bool AreElementsSiblings(const Element& first,
                                       const Element& second) {
  return first.parentElement() == second.parentElement();
}

// This function processes the the DOM tree including pseudo elements as defined
// in CSS 2.1.
static LayoutObject* NextInPreOrder(const LayoutObject& object,
                                    const Element* stay_within,
                                    bool skip_descendants = false) {
  Element* self = ToElement(object.GetNode());
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

static bool PlanCounter(LayoutObject& object,
                        const AtomicString& identifier,
                        bool& is_reset,
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
      // gets the counter. See LayoutTests/http/tests/css/counter-crash.html
      if (generating_node->GetLayoutObject() != &object)
        return false;
      break;
    case kPseudoIdBefore:
    case kPseudoIdAfter:
      break;
    default:
      return false;  // Counters are forbidden from all other pseudo elements.
  }

  const CounterDirectives directives = style.GetCounterDirectives(identifier);
  if (directives.IsDefined()) {
    value = directives.CombinedValue();
    is_reset = directives.IsReset();
    return true;
  }

  if (identifier == "list-item") {
    if (Node* e = object.GetNode()) {
      if (ListItemOrdinal* ordinal = ListItemOrdinal::Get(*e)) {
        if (const auto& explicit_value = ordinal->ExplicitValue()) {
          value = explicit_value.value();
          is_reset = true;
          return true;
        }
        value = 1;
        is_reset = false;
        return true;
      }
      if (auto* olist = ToHTMLOListElementOrNull(*e)) {
        value = olist->StartConsideringItemCount();
        is_reset = true;
        return true;
      }
      if (IsHTMLUListElement(*e) || IsHTMLMenuElement(*e) ||
          IsHTMLDirectoryElement(*e)) {
        value = 0;
        is_reset = true;
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
static bool FindPlaceForCounter(LayoutObject& counter_owner,
                                const AtomicString& identifier,
                                bool is_reset,
                                scoped_refptr<CounterNode>& parent,
                                scoped_refptr<CounterNode>& previous_sibling) {
  // We cannot stop searching for counters with the same identifier before we
  // also check this layout object, because it may affect the positioning in the
  // tree of our counter.
  Element* counter_owner_element = ToElement(counter_owner.GetNode());
  Element* search_end_element =
      PreviousSiblingOrParentRespectingContainment(*counter_owner_element);
  Element* current_element =
      PreviousInPreOrderRespectingContainment(*counter_owner_element);
  previous_sibling = nullptr;
  scoped_refptr<CounterNode> previous_sibling_protector = nullptr;
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
              parent = current_counter->Parent();
              previous_sibling = parent ? current_counter : nullptr;
              return parent.get();
            }
            // We are not a reset node or the previous reset must be on an
            // ancestor of our owner layout object hence we must be a child of
            // that reset counter.
            parent = current_counter;
            // In some cases layout objects can be reparented (ex. nodes inside
            // a table but not in a column or row). In these cases the
            // identified previous_sibling will be invalid as its parent is
            // different from our identified parent.
            if (previous_sibling_protector->Parent() != current_counter)
              previous_sibling_protector = nullptr;

            previous_sibling = previous_sibling_protector.get();
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

            parent = current_counter->Parent();
            previous_sibling = previous_sibling_protector.get();
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
              parent = current_counter->Parent();
              previous_sibling = current_counter;
              return parent.get();
            }
            parent = current_counter;
            previous_sibling = previous_sibling_protector.get();
            return true;
          }
          if (!is_reset ||
              !AreElementsSiblings(*current_element, *counter_owner_element)) {
            parent = current_counter->Parent();
            previous_sibling = current_counter;
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

static inline Element* ParentElement(LayoutObject& object) {
  return ToElement(object.GetNode())->parentElement();
}

static CounterNode* MakeCounterNodeIfNeeded(LayoutObject& object,
                                            const AtomicString& identifier,
                                            bool always_create_counter) {
  if (object.HasCounterNodeMap()) {
    if (CounterMap* node_map = GetCounterMaps().at(&object)) {
      if (CounterNode* node = node_map->at(identifier))
        return node;
    }
  }

  bool is_reset = false;
  int value = 0;
  if (!PlanCounter(object, identifier, is_reset, value) &&
      !always_create_counter)
    return nullptr;

  scoped_refptr<CounterNode> new_parent = nullptr;
  scoped_refptr<CounterNode> new_previous_sibling = nullptr;
  scoped_refptr<CounterNode> new_node =
      CounterNode::Create(object, is_reset, value);

  if (is_reset) {
    // Find the place where we would've inserted the new node if it was a
    // non-reset node. We have to move every non-reset sibling after the
    // insertion point to a child of the new node.
    scoped_refptr<CounterNode> old_parent = nullptr;
    scoped_refptr<CounterNode> old_previous_sibling = nullptr;
    if (FindPlaceForCounter(object, identifier, false, old_parent,
                            old_previous_sibling)) {
      if (!object.IsDescendantOf(&old_parent->Owner())) {
        CounterNode* first_node_to_move =
            old_previous_sibling ? old_previous_sibling->NextSibling()
                                 : old_parent->FirstChild();
        CounterNode::MoveNonResetSiblingsToChildOf(first_node_to_move,
                                                   *new_node, identifier);
      }
    }
  }

  if (FindPlaceForCounter(object, identifier, is_reset, new_parent,
                          new_previous_sibling))
    new_parent->InsertAfter(new_node.get(), new_previous_sibling.get(),
                            identifier);
  CounterMap* node_map;
  if (object.HasCounterNodeMap()) {
    node_map = GetCounterMaps().at(&object);
  } else {
    node_map = new CounterMap;
    GetCounterMaps().Set(&object, base::WrapUnique(node_map));
    object.SetHasCounterNodeMap(true);
  }
  node_map->Set(identifier, new_node);
  if (new_node->Parent())
    return new_node.get();
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
    skip_descendants = false;
    if (!current_layout_object->HasCounterNodeMap())
      continue;
    CounterNode* current_counter =
        maps.at(current_layout_object)->at(identifier);
    if (!current_counter)
      continue;
    skip_descendants = true;
    if (current_counter->Parent())
      continue;
    if (stay_within == ParentElement(*current_layout_object) &&
        current_counter->HasResetType())
      break;
    new_node->InsertAfter(current_counter, new_node->LastChild(), identifier);
  }
  return new_node.get();
}

LayoutCounter::LayoutCounter(PseudoElement& pseudo,
                             const CounterContent& counter)
    : LayoutText(nullptr, StringImpl::empty_),
      counter_(counter),
      counter_node_(nullptr),
      next_for_same_counter_(nullptr) {
  SetDocumentForAnonymous(&pseudo.GetDocument());
  View()->AddLayoutCounter();
}

LayoutCounter::~LayoutCounter() = default;

void LayoutCounter::WillBeDestroyed() {
  if (counter_node_) {
    counter_node_->RemoveLayoutObject(this);
    DCHECK(!counter_node_);
  }
  if (View())
    View()->RemoveLayoutCounter();
  LayoutText::WillBeDestroyed();
}

scoped_refptr<StringImpl> LayoutCounter::OriginalText() const {
  if (!counter_node_) {
    LayoutObject* before_after_container = Parent();
    while (true) {
      if (!before_after_container)
        return nullptr;
      if (!before_after_container->IsAnonymous() &&
          !before_after_container->IsPseudoElement())
        return nullptr;  // LayoutCounters are restricted to before and after
                         // pseudo elements
      PseudoId container_style = before_after_container->StyleRef().StyleType();
      if ((container_style == kPseudoIdBefore) ||
          (container_style == kPseudoIdAfter))
        break;
      before_after_container = before_after_container->Parent();
    }
    MakeCounterNodeIfNeeded(*before_after_container, counter_.Identifier(),
                            true)
        ->AddLayoutObject(const_cast<LayoutCounter*>(this));
    DCHECK(counter_node_);
  }
  CounterNode* child = counter_node_;
  int value = child->ActsAsReset() ? child->Value() : child->CountInParent();

  String text = list_marker_text::GetText(counter_.ListStyle(), value);

  if (!counter_.Separator().IsNull()) {
    if (!child->ActsAsReset())
      child = child->Parent();
    while (CounterNode* parent = child->Parent()) {
      text = list_marker_text::GetText(counter_.ListStyle(),
                                       child->CountInParent()) +
             counter_.Separator() + text;
      child = parent;
    }
  }

  return text.ReleaseImpl();
}

void LayoutCounter::UpdateCounter() {
  SetText(OriginalText());
}

void LayoutCounter::Invalidate() {
  counter_node_->RemoveLayoutObject(this);
  DCHECK(!counter_node_);
  if (DocumentBeingDestroyed())
    return;
  SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReason::kCountersChanged);
}

static void DestroyCounterNodeWithoutMapRemoval(const AtomicString& identifier,
                                                CounterNode* node) {
  CounterNode* previous;
  for (scoped_refptr<CounterNode> child = node->LastDescendant();
       child && child != node; child = previous) {
    previous = child->PreviousInPreOrder();
    child->Parent()->RemoveChild(child.get());
    DCHECK(GetCounterMaps().at(&child->Owner())->at(identifier) == child);
    GetCounterMaps().at(&child->Owner())->erase(identifier);
  }
  if (CounterNode* parent = node->Parent())
    parent->RemoveChild(node);
}

void LayoutCounter::DestroyCounterNodes(LayoutObject& owner) {
  CounterMaps& maps = GetCounterMaps();
  CounterMaps::iterator maps_iterator = maps.find(&owner);
  if (maps_iterator == maps.end())
    return;
  CounterMap* map = maps_iterator->value.get();
  CounterMap::const_iterator end = map->end();
  for (CounterMap::const_iterator it = map->begin(); it != end; ++it) {
    DestroyCounterNodeWithoutMapRemoval(it->key, it->value.get());
  }
  maps.erase(maps_iterator);
  owner.SetHasCounterNodeMap(false);
  if (owner.View())
    owner.View()->SetNeedsCounterUpdate();
}

void LayoutCounter::DestroyCounterNode(LayoutObject& owner,
                                       const AtomicString& identifier) {
  CounterMap* map = GetCounterMaps().at(&owner);
  if (!map)
    return;
  CounterMap::iterator map_iterator = map->find(identifier);
  if (map_iterator == map->end())
    return;
  DestroyCounterNodeWithoutMapRemoval(identifier, map_iterator->value.get());
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
  CounterMap* counter_map = GetCounterMaps().at(&layout_object);
  DCHECK(counter_map);
  for (CounterDirectiveMap::const_iterator it = directive_map->begin();
       it != end; ++it) {
    scoped_refptr<CounterNode> node = counter_map->at(it->key);
    if (!node) {
      MakeCounterNodeIfNeeded(layout_object, it->key, false);
      continue;
    }
    scoped_refptr<CounterNode> new_parent = nullptr;
    scoped_refptr<CounterNode> new_previous_sibling = nullptr;

    FindPlaceForCounter(layout_object, it->key, node->HasResetType(),
                        new_parent, new_previous_sibling);
    if (node != counter_map->at(it->key))
      continue;
    CounterNode* parent = node->Parent();
    if (new_parent == parent && new_previous_sibling == node->PreviousSibling())
      continue;
    if (parent)
      parent->RemoveChild(node.get());
    if (new_parent)
      new_parent->InsertAfter(node.get(), new_previous_sibling.get(), it->key);
  }
}

void LayoutCounter::LayoutObjectSubtreeAttached(LayoutObject* layout_object) {
  DCHECK(layout_object->View());
  if (!layout_object->View()->HasLayoutCounters())
    return;
  Node* node = layout_object->GetNode();
  if (node)
    node = node->parentNode();
  else
    node = layout_object->GeneratingNode();
  if (node && node->NeedsAttach())
    return;  // No need to update if the parent is not attached yet
  for (LayoutObject* descendant = layout_object; descendant;
       descendant = descendant->NextInPreOrder(layout_object))
    UpdateCounters(*descendant);
}

void LayoutCounter::LayoutObjectStyleChanged(LayoutObject& layout_object,
                                             const ComputedStyle* old_style,
                                             const ComputedStyle& new_style) {
  Node* node = layout_object.GeneratingNode();
  if (!node || node->NeedsAttach())
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

}  // namespace blink

#ifndef NDEBUG

void showCounterLayoutObjectTree(const blink::LayoutObject* layout_object,
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
    fprintf(
        stderr, "%p N:%p P:%p PS:%p NS:%p C:%p\n", current, current->GetNode(),
        current->Parent(), current->PreviousSibling(), current->NextSibling(),
        current->HasCounterNodeMap()
            ? counter_name ? blink::GetCounterMaps().at(current)->at(identifier)
                           : (blink::CounterNode*)1
            : (blink::CounterNode*)nullptr);
  }
  fflush(stderr);
}

#endif  // NDEBUG

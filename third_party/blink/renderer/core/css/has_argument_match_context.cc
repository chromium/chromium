// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/has_argument_match_context.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"

namespace blink {

namespace {

inline const CSSSelector* GetCurrentRelationAndNextCompound(
    const CSSSelector* compound_selector,
    CSSSelector::RelationType& relation) {
  DCHECK(compound_selector);
  for (; compound_selector;
       compound_selector = compound_selector->TagHistory()) {
    relation = compound_selector->Relation();
    if (relation != CSSSelector::kSubSelector)
      return compound_selector->TagHistory();
  }
  return nullptr;
}

}  // namespace

HasArgumentMatchContext::HasArgumentMatchContext(const CSSSelector* selector) {
  CSSSelector::RelationType relation = CSSSelector::kSubSelector;
  depth_limit_ = 0;
  adjacent_distance_limit_ = 0;
  // The explicit ':scope' in ':has' argument selector is not considered
  // for getting the depth and adjacent distance.
  // TODO(blee@igalia.com) Need to clarify the :scope dependency in relative
  // selector definition.
  // - spec : https://www.w3.org/TR/selectors-4/#relative
  // - csswg issue : https://github.com/w3c/csswg-drafts/issues/6399
  for (selector = GetCurrentRelationAndNextCompound(selector, relation);
       selector;
       selector = GetCurrentRelationAndNextCompound(selector, relation)) {
    switch (relation) {
      case CSSSelector::kRelativeDescendant:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kDescendant:
        depth_limit_ = kInfiniteDepth;
        adjacent_distance_limit_ = 0;
        break;

      case CSSSelector::kRelativeChild:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kChild:
        if (DepthFixed()) {
          depth_limit_++;
          adjacent_distance_limit_ = 0;
        }
        break;

      case CSSSelector::kRelativeDirectAdjacent:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kDirectAdjacent:
        if (AdjacentDistanceFixed())
          adjacent_distance_limit_++;
        break;

      case CSSSelector::kRelativeIndirectAdjacent:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kIndirectAdjacent:
        adjacent_distance_limit_ = kInfiniteAdjacentDistance;
        break;

      default:
        NOTREACHED();
        return;
    }
  }
}

HasArgumentSubtreeIterator::HasArgumentSubtreeIterator(
    Element& has_scope_element,
    HasArgumentMatchContext& context)
    : has_scope_element_(&has_scope_element), context_(context) {
  if (!context_.AdjacentDistanceFixed()) {
    // Set the traversal_end_ as the next sibling of the :has scope element,
    // and move to the last sibling of the :has scope element, and move again
    // to the last descendant of the last sibling.
    traversal_end_ = ElementTraversal::NextSibling(*has_scope_element_);
    if (!traversal_end_) {
      current_ = nullptr;
      return;
    }
    Element* last_sibling =
        Traversal<Element>::LastChild(*has_scope_element_->parentNode());
    current_ = LastWithin(last_sibling);
    if (!current_)
      current_ = last_sibling;
  } else if (context_.AdjacentDistanceLimit() == 0) {
    DCHECK_GT(context_.DepthLimit(), 0);
    // Set the traversal_end_ as the first child of the :has scope element,
    // and move to the last descendant of the :has scope element without
    // exceeding the depth limit.
    traversal_end_ = ElementTraversal::FirstChild(*has_scope_element_);
    if (!traversal_end_) {
      current_ = nullptr;
      return;
    }
    current_ = LastWithin(has_scope_element_);
    DCHECK(current_);
  } else {
    // Set the traversal_end_ as the element at the adjacent distance of the
    // :has scope element, and move to the last descendant of the element
    // without exceeding the depth limit.
    int distance = 1;
    for (traversal_end_ = ElementTraversal::NextSibling(*has_scope_element_);
         distance < context_.AdjacentDistanceLimit() && traversal_end_;
         distance++,
        traversal_end_ = ElementTraversal::NextSibling(*traversal_end_)) {
    }
    if (!traversal_end_) {
      current_ = nullptr;
      return;
    }
    if ((current_ = LastWithin(traversal_end_)))
      return;
    current_ = traversal_end_;
  }
}

Element* HasArgumentSubtreeIterator::LastWithin(Element* element) {
  // If the current depth is at the depth limit, return null.
  if (depth_ == context_.DepthLimit())
    return nullptr;

  // Return the last element of the pre-order traversal starting from the passed
  // in element without exceeding the depth limit.
  Element* last_descendant = nullptr;
  for (Element* descendant = ElementTraversal::LastChild(*element); descendant;
       descendant = ElementTraversal::LastChild(*descendant)) {
    last_descendant = descendant;
    if (++depth_ == context_.DepthLimit())
      break;
  }
  return last_descendant;
}

void HasArgumentSubtreeIterator::operator++() {
  DCHECK(current_);
  DCHECK_NE(current_, has_scope_element_);
  if (current_ == traversal_end_) {
    current_ = nullptr;
    return;
  }

  // Move to the previous element in DOM tree order within the depth limit.
  if (Element* next = Traversal<Element>::PreviousSibling(*current_)) {
    Element* last_descendant = LastWithin(next);
    current_ = last_descendant ? last_descendant : next;
  } else {
    DCHECK_GT(depth_, 0);
    depth_--;
    current_ = current_->parentElement();
  }
  DCHECK(current_);
}

}  // namespace blink

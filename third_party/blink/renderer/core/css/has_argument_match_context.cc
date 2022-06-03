// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/has_argument_match_context.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"

namespace {  // anonymous namespace for file-local method and constant

using blink::CSSSelector;
using blink::Element;
using blink::To;
using blink::Traversal;

const int kInfiniteDepth = -1;
const int kInfiniteAdjacentDistance = -1;

inline Element* LastDescendantOf(const Element& element,
                                 int& depth,
                                 const int& depth_limit) {
  // If the current depth is at the depth limit, return null.
  if (depth == depth_limit)
    return nullptr;

  // Return the rightmost bottom element of the element without exceeding the
  // depth limit.
  Element* last_descendant = nullptr;
  for (Element* descendant = Traversal<Element>::LastChild(element); descendant;
       descendant = Traversal<Element>::LastChild(*descendant)) {
    last_descendant = descendant;
    if (++depth == depth_limit)
      break;
  }
  return last_descendant;
}

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

namespace blink {

HasArgumentMatchContext::HasArgumentMatchContext(const CSSSelector* selector)
    : leftmost_relation_(CSSSelector::kSubSelector),
      adjacent_traversal_distance_(0),
      descendant_traversal_depth_(0) {
  CSSSelector::RelationType relation = CSSSelector::kSubSelector;
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
        FALLTHROUGH;
      case CSSSelector::kDescendant:
        descendant_traversal_depth_ = kInfiniteDepth;
        adjacent_traversal_distance_ = 0;
        break;

      case CSSSelector::kRelativeChild:
        leftmost_relation_ = relation;
        FALLTHROUGH;
      case CSSSelector::kChild:
        if (descendant_traversal_depth_ != kInfiniteDepth) {
          descendant_traversal_depth_++;
          adjacent_traversal_distance_ = 0;
        }
        break;

      case CSSSelector::kRelativeDirectAdjacent:
        leftmost_relation_ = relation;
        FALLTHROUGH;
      case CSSSelector::kDirectAdjacent:
        if (adjacent_traversal_distance_ != kInfiniteAdjacentDistance)
          adjacent_traversal_distance_++;
        break;

      case CSSSelector::kRelativeIndirectAdjacent:
        leftmost_relation_ = relation;
        FALLTHROUGH;
      case CSSSelector::kIndirectAdjacent:
        adjacent_traversal_distance_ = kInfiniteAdjacentDistance;
        break;

      case CSSSelector::kUAShadow:
      case CSSSelector::kShadowSlot:
      case CSSSelector::kShadowPart:
        // TODO(blee@igalia.com) Need to check how to handle the shadow tree
        // (e.g. ':has(::slotted(img))', ':has(component::part(my-part))')
        return;
      default:
        NOTREACHED();
        break;
    }
  }
}

CSSSelector::RelationType HasArgumentMatchContext::GetLeftMostRelation() const {
  return leftmost_relation_;
}

bool HasArgumentMatchContext::GetDepthFixed() const {
  return descendant_traversal_depth_ != kInfiniteDepth;
}

bool HasArgumentMatchContext::GetAdjacentDistanceFixed() const {
  return adjacent_traversal_distance_ != kInfiniteAdjacentDistance;
}

HasArgumentSubtreeIterator::HasArgumentSubtreeIterator(
    Element& scope_element,
    HasArgumentMatchContext& context)
    : scope_element_(&scope_element),
      adjacent_distance_fixed_(context.GetAdjacentDistanceFixed()),
      adjacent_distance_limit_(adjacent_distance_fixed_
                                   ? context.adjacent_traversal_distance_
                                   : std::numeric_limits<int>::max()),
      depth_limit_(context.GetDepthFixed() ? context.descendant_traversal_depth_
                                           : std::numeric_limits<int>::max()),
      depth_(0) {
  if (!adjacent_distance_fixed_) {
    // Set the traversal_end_ as the next sibling of the :has scope element,
    // and move to the last sibling of the :has scope element, and move again
    // to the last descendant of the last sibling.
    traversal_end_ = Traversal<Element>::NextSibling(*scope_element_);
    if (!traversal_end_) {
      current_ = nullptr;
      return;
    }
    Element* last_sibling =
        Traversal<Element>::LastChild(*scope_element_->parentNode());
    current_ = LastDescendantOf(*last_sibling, depth_, depth_limit_);
    if (!current_)
      current_ = last_sibling;
  } else if (adjacent_distance_limit_ == 0) {
    DCHECK_GT(depth_limit_, 0);
    // Set the traversal_end_ as the first child of the :has scope element,
    // and move to the last descendant of the :has scope element without
    // exceeding the depth limit.
    traversal_end_ = Traversal<Element>::FirstChild(*scope_element_);
    if (!traversal_end_) {
      current_ = nullptr;
      return;
    }
    current_ = LastDescendantOf(*scope_element_, depth_, depth_limit_);
    DCHECK(current_);
  } else {
    // Set the traversal_end_ as the element at the adjacent distance of the
    // :has scope element, and move to the last descendant of the element
    // without exceeding the depth limit.
    int distance;
    for (distance = 1,
        traversal_end_ = Traversal<Element>::NextSibling(*scope_element_);
         distance < adjacent_distance_limit_ && traversal_end_; distance++,
        traversal_end_ = Traversal<Element>::NextSibling(*traversal_end_)) {
    }
    if (!traversal_end_) {
      current_ = nullptr;
      return;
    }
    if ((current_ = LastDescendantOf(*traversal_end_, depth_, depth_limit_)))
      return;
    current_ = traversal_end_;
  }
}

void HasArgumentSubtreeIterator::operator++() {
  DCHECK(current_);
  DCHECK_NE(current_, scope_element_);
  if (current_ == traversal_end_) {
    current_ = nullptr;
    return;
  }

  // Move to the previous element in DOM tree order within the depth limit.
  if (Element* next = Traversal<Element>::PreviousSibling(*current_)) {
    Element* last_descendant = LastDescendantOf(*next, depth_, depth_limit_);
    current_ = last_descendant ? last_descendant : next;
  } else {
    DCHECK_GT(depth_, 0);
    depth_--;
    current_ = current_->parentElement();
  }
  DCHECK(current_);
}

}  // namespace blink

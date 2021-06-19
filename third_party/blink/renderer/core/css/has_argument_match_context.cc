// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/has_argument_match_context.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"

namespace {  // anonymous namespace for file-local method and constant

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

}  // namespace

namespace blink {

HasArgumentMatchContext::HasArgumentMatchContext(const CSSSelector* selector) {
  const CSSSelector* leftmost_compound = selector;
  const CSSSelector* leftmost_compound_containing_scope = nullptr;

  while (leftmost_compound) {
    const CSSSelector* simple_selector = leftmost_compound;
    CSSSelector::RelationType relation;
    while (simple_selector) {
      if (leftmost_compound_containing_scope)
        contains_compounded_scope_selector_ = true;
      if (simple_selector->GetPseudoType() == CSSSelector::kPseudoScope) {
        if (leftmost_compound_containing_scope &&
            leftmost_compound_containing_scope != leftmost_compound) {
          // Selectors that contains multiple :scope pseudo classes separated
          // by combinators will never match.
          // (e.g. :has(:scope > .a > :scope))
          SetNeverMatch();
          return;
        }
        if (simple_selector != leftmost_compound)
          contains_compounded_scope_selector_ = true;
        leftmost_compound_containing_scope = leftmost_compound;
      }
      relation = simple_selector->Relation();
      if (relation != CSSSelector::kSubSelector)
        break;
      simple_selector = simple_selector->TagHistory();
    }

    if (!simple_selector)
      break;

    if (leftmost_compound_containing_scope) {
      // Skip to update the context if it already found the :scope
      leftmost_compound = simple_selector->TagHistory();
      DCHECK(leftmost_compound);
      continue;
    }

    switch (relation) {
      case CSSSelector::kDescendant:
        descendant_traversal_depth_ = kInfiniteDepth;
        adjacent_traversal_distance_ = 0;
        leftmost_relation_ = relation;
        break;
      case CSSSelector::kChild:
        if (descendant_traversal_depth_ != kInfiniteDepth) {
          descendant_traversal_depth_++;
          adjacent_traversal_distance_ = 0;
        }
        leftmost_relation_ = relation;
        break;
      case CSSSelector::kDirectAdjacent:
        if (adjacent_traversal_distance_ != kInfiniteAdjacentDistance)
          adjacent_traversal_distance_++;
        leftmost_relation_ = relation;
        break;
      case CSSSelector::kIndirectAdjacent:
        adjacent_traversal_distance_ = kInfiniteAdjacentDistance;
        leftmost_relation_ = relation;
        break;
      case CSSSelector::kUAShadow:
      case CSSSelector::kShadowSlot:
      case CSSSelector::kShadowPart:
        // TODO(blee@igalia.com) Need to check how to handle the shadow tree
        // (e.g. ':has(::slotted(img))', ':has(component::part(my-part))')
        SetNeverMatch();
        return;
      default:
        NOTREACHED();
        break;
    }

    leftmost_compound = simple_selector->TagHistory();
    DCHECK(leftmost_compound);
  }

  if (!leftmost_compound_containing_scope) {
    // Always set descendant relative selector because the relative selector
    // spec is not supported yet.
    leftmost_relation_ = CSSSelector::kDescendant;
    descendant_traversal_depth_ = kInfiniteDepth;
    adjacent_traversal_distance_ = 0;
    return;
  }

  contains_no_leftmost_scope_selector_ =
      leftmost_compound_containing_scope != leftmost_compound;
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

bool HasArgumentMatchContext::WillNeverMatch() const {
  return leftmost_relation_ == CSSSelector::kSubSelector;
}

bool HasArgumentMatchContext::ContainsCompoundedScopeSelector() const {
  return contains_compounded_scope_selector_;
}

bool HasArgumentMatchContext::ContainsNoLeftmostScopeSelector() const {
  return contains_no_leftmost_scope_selector_;
}

void HasArgumentMatchContext::SetNeverMatch() {
  leftmost_relation_ = CSSSelector::kSubSelector;
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

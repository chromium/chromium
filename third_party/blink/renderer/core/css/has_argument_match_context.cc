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
  bool contains_child_or_descendant_combinator = false;
  bool sibling_combinator_at_leftmost = false;

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
        if (sibling_combinator_at_leftmost) {
          sibling_combinator_at_leftmost = false;
          sibling_combinator_between_child_or_descendant_combinator_ = true;
        }
        contains_child_or_descendant_combinator = true;
        depth_limit_ = kInfiniteDepth;
        adjacent_distance_limit_ = 0;
        break;

      case CSSSelector::kRelativeChild:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kChild:
        if (sibling_combinator_at_leftmost) {
          sibling_combinator_at_leftmost = false;
          sibling_combinator_between_child_or_descendant_combinator_ = true;
        }
        contains_child_or_descendant_combinator = true;
        if (DepthFixed()) {
          depth_limit_++;
          adjacent_distance_limit_ = 0;
        }
        break;

      case CSSSelector::kRelativeDirectAdjacent:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kDirectAdjacent:
        if (contains_child_or_descendant_combinator)
          sibling_combinator_at_leftmost = true;
        else
          sibling_combinator_at_rightmost_ = true;
        if (AdjacentDistanceFixed())
          adjacent_distance_limit_++;
        break;

      case CSSSelector::kRelativeIndirectAdjacent:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kIndirectAdjacent:
        if (contains_child_or_descendant_combinator)
          sibling_combinator_at_leftmost = true;
        else
          sibling_combinator_at_rightmost_ = true;
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

AffectedByHasIterator::AffectedByHasIterator(
    HasArgumentSubtreeIterator& iterator_at_matched)
    : iterator_at_matched_(iterator_at_matched),
      depth_(iterator_at_matched_.Depth()),
      current_(iterator_at_matched_.CurrentElement()) {
  DCHECK_GE(depth_, 0);
}

bool AffectedByHasIterator::NeedsTraverseSiblings() {
  // When the current element is at the same depth of the subselector-matched
  // element, we can determine whether the sibling traversal is needed or not
  // by checking whether the rightmost combinator is an adjacent combinator.
  // When the current element is not at the same depth of the subselector-
  // matched element, we can determine whether the sibling traversal is needed
  // or not by checking whether an adjacent combinator is between child or
  // descendant combinator.
  DCHECK_LE(depth_, iterator_at_matched_.Depth());
  return iterator_at_matched_.Depth() == depth_
             ? iterator_at_matched_.Context().SiblingCombinatorAtRightmost()
             : iterator_at_matched_.Context()
                   .SiblingCombinatorBetweenChildOrDescendantCombinator();
}

bool AffectedByHasIterator::AtEnd() const {
  DCHECK_GE(iterator_at_matched_.Depth(), 0);
  return current_ == iterator_at_matched_.ScopeElement();
}

void AffectedByHasIterator::operator++() {
  DCHECK(current_);
  DCHECK_GE(iterator_at_matched_.Depth(), 0);

  if (depth_ == 0) {
    current_ = Traversal<Element>::PreviousSibling(*current_);
    DCHECK(current_);
    return;
  }

  Element* previous = nullptr;
  if (NeedsTraverseSiblings() &&
      (previous = Traversal<Element>::PreviousSibling(*current_))) {
    current_ = previous;
    DCHECK(current_);
    return;
  }

  DCHECK_GT(depth_, 0);
  depth_--;
  current_ = current_->parentElement();
  DCHECK(current_);
}

}  // namespace blink

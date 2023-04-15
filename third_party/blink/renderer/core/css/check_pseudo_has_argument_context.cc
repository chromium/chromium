// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/check_pseudo_has_argument_context.h"

#include "third_party/blink/renderer/core/css/check_pseudo_has_fast_reject_filter.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"

namespace blink {

const CSSSelector*
CheckPseudoHasArgumentContext::GetCurrentRelationAndNextCompound(
    const CSSSelector* compound_selector,
    CSSSelector::RelationType& relation) {
  DCHECK(compound_selector);
  for (const CSSSelector* simple_selector = compound_selector; simple_selector;
       simple_selector = simple_selector->NextSimpleSelector()) {
    CheckPseudoHasFastRejectFilter::CollectPseudoHasArgumentHashes(
        pseudo_has_argument_hashes_, simple_selector);

    relation = simple_selector->Relation();
    if (relation != CSSSelector::kSubSelector) {
      return simple_selector->NextSimpleSelector();
    }
  }
  return nullptr;
}

CheckPseudoHasArgumentContext::CheckPseudoHasArgumentContext(
    const CSSSelector* selector)
    : has_argument_(selector) {
  CSSSelector::RelationType relation = CSSSelector::kSubSelector;
  depth_limit_ = 0;
  adjacent_distance_limit_ = 0;
  bool contains_child_or_descendant_combinator = false;
  bool sibling_combinator_at_leftmost = false;

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
        }
        adjacent_distance_limit_ = 0;
        break;

      case CSSSelector::kRelativeDirectAdjacent:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kDirectAdjacent:
        if (contains_child_or_descendant_combinator) {
          sibling_combinator_at_leftmost = true;
        } else {
          sibling_combinator_at_rightmost_ = true;
        }
        if (AdjacentDistanceFixed()) {
          adjacent_distance_limit_++;
        }
        break;

      case CSSSelector::kRelativeIndirectAdjacent:
        leftmost_relation_ = relation;
        [[fallthrough]];
      case CSSSelector::kIndirectAdjacent:
        if (contains_child_or_descendant_combinator) {
          sibling_combinator_at_leftmost = true;
        } else {
          sibling_combinator_at_rightmost_ = true;
        }
        adjacent_distance_limit_ = kInfiniteAdjacentDistance;
        break;

      default:
        NOTREACHED();
        return;
    }
  }
  DCHECK_NE(leftmost_relation_, CSSSelector::kSubSelector);
  DCHECK_LE(adjacent_distance_limit_, kInfiniteAdjacentDistance);
  DCHECK_LE(depth_limit_, kInfiniteDepth);

  switch (leftmost_relation_) {
    case CSSSelector::kRelativeDescendant:
    case CSSSelector::kRelativeChild:
      if (DepthFixed()) {
        traversal_scope_ = kFixedDepthDescendants;
      } else {
        traversal_scope_ = kSubtree;
      }
      siblings_affected_by_has_flags_ =
          SiblingsAffectedByHasFlags::kNoSiblingsAffectedByHasFlags;
      break;
    case CSSSelector::kRelativeIndirectAdjacent:
    case CSSSelector::kRelativeDirectAdjacent:
      if (DepthLimit() == 0) {
        if (AdjacentDistanceFixed()) {
          traversal_scope_ = kOneNextSibling;
        } else {
          traversal_scope_ = kAllNextSiblings;
        }
        siblings_affected_by_has_flags_ =
            SiblingsAffectedByHasFlags::kFlagForSiblingRelationship;
      } else {
        if (AdjacentDistanceFixed()) {
          if (DepthFixed()) {
            traversal_scope_ = kOneNextSiblingFixedDepthDescendants;
          } else {
            traversal_scope_ = kOneNextSiblingSubtree;
          }
        } else {
          if (DepthFixed()) {
            traversal_scope_ = kAllNextSiblingsFixedDepthDescendants;
          } else {
            traversal_scope_ = kAllNextSiblingSubtrees;
          }
        }
        siblings_affected_by_has_flags_ =
            SiblingsAffectedByHasFlags::kFlagForSiblingDescendantRelationship;
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

CheckPseudoHasArgumentTraversalIterator::
    CheckPseudoHasArgumentTraversalIterator(
        Element& has_anchor_element,
        CheckPseudoHasArgumentContext& context)
    : has_anchor_element_(&has_anchor_element),
      depth_limit_(context.DepthLimit()) {
  if (!context.AdjacentDistanceFixed()) {
    // Set the last_element_ as the next sibling of the :has() anchor element,
    // and move to the last sibling of the :has() anchor element, and move again
    // to the last descendant of the last sibling.
    last_element_ = ElementTraversal::NextSibling(*has_anchor_element_);
    if (!last_element_) {
      DCHECK_EQ(current_element_, nullptr);
      return;
    }
    Element* last_sibling =
        ElementTraversal::LastChild(*has_anchor_element_->parentNode());
    current_element_ = LastWithin(last_sibling);
    if (!current_element_) {
      current_element_ = last_sibling;
    }
  } else if (context.AdjacentDistanceLimit() == 0) {
    DCHECK_GT(context.DepthLimit(), 0);
    // Set the last_element_ as the first child of the :has() anchor element,
    // and move to the last descendant of the :has() anchor element without
    // exceeding the depth limit.
    last_element_ = ElementTraversal::FirstChild(*has_anchor_element_);
    if (!last_element_) {
      DCHECK_EQ(current_element_, nullptr);
      return;
    }
    current_element_ = LastWithin(has_anchor_element_);
    DCHECK(current_element_);
  } else {
    // Set last_element_ as the next sibling of the :has() anchor element, set
    // the sibling_at_fixed_distance_ as the element at the adjacent distance
    // of the :has() anchor element, and move to the last descendant of the
    // sibling at fixed distance without exceeding the depth limit.
    int distance = 1;
    Element* old_sibling = nullptr;
    Element* sibling = ElementTraversal::NextSibling(*has_anchor_element_);
    for (; distance < context.AdjacentDistanceLimit() && sibling;
         distance++, sibling = ElementTraversal::NextSibling(*sibling)) {
      old_sibling = sibling;
    }
    if (sibling) {
      sibling_at_fixed_distance_ = sibling;
      current_element_ = LastWithin(sibling_at_fixed_distance_);
      if (!current_element_) {
        current_element_ = sibling_at_fixed_distance_;
      }
    } else {
      current_element_ = old_sibling;
      if (!current_element_) {
        return;
      }
      // set the depth_limit_ to 0 so that the iterator only traverse to the
      // siblings of the :has() anchor element.
      depth_limit_ = 0;
    }
    last_element_ = ElementTraversal::NextSibling(*has_anchor_element_);
  }
}

Element* CheckPseudoHasArgumentTraversalIterator::LastWithin(Element* element) {
  // If the current depth is at the depth limit, return null.
  if (current_depth_ == depth_limit_) {
    return nullptr;
  }

  // Return the last element of the pre-order traversal starting from the passed
  // in element without exceeding the depth limit.
  Element* last_descendant = nullptr;
  for (Element* descendant = ElementTraversal::LastChild(*element); descendant;
       descendant = ElementTraversal::LastChild(*descendant)) {
    last_descendant = descendant;
    if (++current_depth_ == depth_limit_) {
      break;
    }
  }
  return last_descendant;
}

void CheckPseudoHasArgumentTraversalIterator::operator++() {
  DCHECK(current_element_);
  DCHECK_NE(current_element_, has_anchor_element_);
  if (current_element_ == last_element_) {
    current_element_ = nullptr;
    return;
  }

  // If current element is the sibling at fixed distance, set the depth_limit_
  // to 0 so that the iterator only traverse to the siblings of the :has()
  // anchor element.
  if (current_depth_ == 0 && sibling_at_fixed_distance_ == current_element_) {
    sibling_at_fixed_distance_ = nullptr;
    depth_limit_ = 0;
  }

  // Move to the previous element in DOM tree order within the depth limit.
  if (Element* next = ElementTraversal::PreviousSibling(*current_element_)) {
    Element* last_descendant = LastWithin(next);
    current_element_ = last_descendant ? last_descendant : next;
  } else {
    DCHECK_GT(current_depth_, 0);
    current_depth_--;
    current_element_ = current_element_->parentElement();
  }
  DCHECK(current_element_);
}

}  // namespace blink

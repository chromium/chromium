// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_ARGUMENT_MATCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_ARGUMENT_MATCH_CONTEXT_H_

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

class HasArgumentSubtreeIterator;

class HasArgumentMatchContext {
  STACK_ALLOCATED();

 public:
  explicit HasArgumentMatchContext(const CSSSelector* selector);
  CSSSelector::RelationType GetLeftMostRelation() const;
  bool GetDepthFixed() const;
  bool GetAdjacentDistanceFixed() const;

 private:
  // Indicate the :has argument relative type and subtree traversal scope.
  // If 'adjacent_traversal_distance_' is greater than 0, then it means that
  // it is enough to traverse the adjacent subtree at that distance.
  // If it is -1, it means that all the adjacent subtree need to be traversed.
  // If 'descendant_traversal_depth_' is greater than 0, then it means that
  // it is enough to traverse elements at the certain depth. If it is -1,
  // it means that all of the descendant subtree need to be traversed.
  //
  // Case 1:  (kDescendant, 0, -1)
  //   - Argument selector conditions
  //     - Starts with descendant combinator.
  //   - E.g. ':has(.a)', ':has(:scope .a)', ':has(.a ~ .b > .c)'
  //   - Traverse all descendants of the :has scope element.
  // Case 2:  (kChild, 0, -1)
  //   - Argument selector conditions
  //     - Starts with child combinator.
  //     - At least one descendant combinator.
  //   - E.g. ':has(:scope > .a .b)', ':has(:scope > .a ~ .b .c)'
  //   - Traverse all descendants of the :has scope element.
  // Case 3:  (kChild, 0, n)
  //   - Argument selector conditions
  //     - Starts with child combinator.
  //     - n number of child combinator. (n > 0)
  //     - No descendant combinator.
  //   - E.g.
  //     - ':has(:scope > .a)'            : (kChild, 0, 1)
  //     - ':has(:scope > .a ~ .b > .c)'  : (kChild, 0, 2)
  //   - Traverse the depth n descendants of the :has scope element.
  // Case 4:  (kIndirectAdjacent, -1, -1)
  //   - Argument selector conditions
  //     - Starts with subsequent-sibling combinator.
  //     - At least one descendant combinator.
  //   - E.g. ':has(:scope ~ .a .b)', ':has(:scope ~ .a + .b > .c ~ .d .e)'
  //   - Traverse all the subsequent sibling subtrees of the :has scope element.
  //     (all subsequent siblings and it's descendants)
  // Case 5:  (kIndirectAdjacent, -1, 0)
  //   - Argument selector conditions
  //     - Starts with subsequent-sibling combinator.
  //     - No descendant/child combinator.
  //   - E.g. ':has(:scope ~ .a)', ':has(:scope ~ .a + .b ~ .c)'
  //   - Traverse all subsequent siblings of the :has scope element.
  // Case 6:  (kIndirectAdjacent, -1, n)
  //   - Argument selector conditions
  //     - Starts with subsequent-sibling combinator.
  //     - n number of child combinator. (n > 0)
  //     - No descendant combinator.
  //   - E.g.
  //     - ':has(:scope ~ .a > .b)'                 : (kIndirectAdjacent, -1, 1)
  //     - ':has(:scope ~ .a + .b > .c ~ .d > .e)'  : (kIndirectAdjacent, -1, 2)
  //   - Traverse depth n elements of all subsequent sibling subtree of the
  //     :has scope element.
  // Case 7:  (kDirectAdjacent, -1, -1)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - At least one subsequent-sibling combinator to the left of every
  //       descendant or child combinator.
  //     - At least 1 descendant combinator.
  //   - E.g. ':has(:scope + .a ~ .b .c)', ':has(:scope + .a ~ .b > .c + .e .f)'
  //   - Traverse all the subsequent sibling subtrees of the :has scope element.
  //     (all subsequent siblings and it's descendants)
  // Case 8:  (kDirectAdjacent, -1, 0)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - At least one subsequent-sibling combinator.
  //     - No descendant/child combinator.
  //   - E.g. ':has(:scope + .a ~ .b)', ':has(:scope + .a + .b ~ .c)'
  //   - Traverse all subsequent siblings of the :has scope element.
  // Case 9:  (kDirectAdjacent, -1, n)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - At least one subsequent-sibling combinator to the left of every
  //       descendant or child combinator.
  //     - n number of child combinator. (n > 0)
  //     - No descendant combinator.
  //   - E.g.
  //     - ':has(:scope + .a ~ .b > .c)'            : (kDirectAdjacent, -1, 1)
  //     - ':has(:scope + .a ~ .b > .c + .e >.f)'   : (kDirectAdjacent, -1, 2)
  //   - Traverse depth n elements of all subsequent sibling subtree of the
  //     :has scope element.
  // Case 10:  (kDirectAdjacent, n, -1)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - n number of next-sibling combinator to the left of the leftmost
  //       child(or descendant) combinator. (n > 0)
  //     - No subsequent-sibling combinator to the left of the leftmost child
  //       (or descendant) combinator.
  //     - At least 1 descendant combinator.
  //   - E.g.
  //     - ':has(:scope + .a .b)'            : (kDirectAdjacent, 1, -1)
  //     - ':has(:scope + .a > .b + .c .d)'  : (kDirectAdjacent, 1, -1)
  //     - ':has(:scope + .a + .b > .c .d)'  : (kDirectAdjacent, 2, -1)
  //   - Traverse the distance n sibling subtree of the :has scope element.
  //     (sibling element at distance n, and it's descendants).
  // Case 11:  (kDirectAdjacent, n, 0)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - n number of next-sibling combinator. (n > 0)
  //     - No child/descendant/subsequent-sibling combinator.
  //   - E.g.
  //     - ':has(:scope + .a)'            : (kDirectAdjacent, 1, 0)
  //     - ':has(:scope + .a + .b + .c)'  : (kDirectAdjacent, 3, 0)
  //   - Traverse the distance n sibling element of the :has scope element.
  // Case 12:  (kDirectAdjacent, n, m)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - n number of next-sibling combinator to the left of the leftmost
  //       child combinator. (n > 0)
  //     - No subsequent-sibling combinator to the left of the leftmost child
  //       combinator.
  //     - n number of child combinator. (n > 0)
  //     - No descendant combinator.
  //   - E.g.
  //     - ':has(:scope + .a > .b)'                 : (kDirectAdjacent, 1, 1)
  //     - ':has(:scope + .a + .b > .c ~ .d > .e)'  : (kDirectAdjacent, 2, 2)
  //   - Traverse the depth m elements of the distance n sibling subtree of
  //     the :has scope element. (elements at depth m of the descendant subtree
  //     of the sibling element at distance n)
  CSSSelector::RelationType leftmost_relation_;
  int adjacent_traversal_distance_;
  int descendant_traversal_depth_;

  friend class HasArgumentSubtreeIterator;
};

// Subtree traversal iterator class for ':has' argument matching. To
// solve the following problems, this traversal uses the right-to-left
// postorder tree traversal, and provides a functionality to limit the
// traversal depth.
//
// 1. Prevent incorrect 'NotMatched' cache status marked in the ':has'
// argument selector matching iteration.
//
// With the pre-order tree traversal, the previous ':has' matching logic
// cannot guarantee that an element with 'NotMatched' status is actually
// 'checked the :has selector on the element but not matched'.
// To skip the duplicated argument selector matching on the descendant
// subtree of an element, in the :has argument matching iteration,
// SelectorChecker marks every descendant elements as 'NotMatched' if
// the element status is not 'Matched'. This logic works when the subtree
// doesn't have any argument matched element, or only 1 element. But
// if the subtree has more than 2 argument matching elements and one of
// them is an ancestor of the other, the pre-order tree traversal cannot
// guarantee the 'NotMatched' status of the ancestor element because it
// traverse root first before traversing it's descendants.
// The right-to-left post-order traversal can guarantee the logic of
// marking 'NotMatched' in the ':has' argument matching iteration
// because it guarantee that the descendant subtree of the element and
// the downward subtree(succeeding siblings and it's descendants) of the
// element was already checked. (If any of the previous traversals have
// matched the argument selector, the element marked as 'Matched' when
// it was the :has scope element of the match)
//
// 2. Prevent unnecessary subtree traversal when it can be limited with
// child combinator or direct sibling combinator.
//
// We can limit the tree traversal range when we count the leftmost
// combinators of a ':has' argument selector. For example, when we have
// 'div:has(:scope > .a > .b)', instead of traversing all the descendants
// of div element, we can limit the traversal only for the elements at
// depth 2 of the div element. When we have 'div:has(:scope + .a > .b)',
// we can limit the traversal only for the child elements of the direct
// adjacent sibling of the div element. To implement this, we need a
// way to limit the traversal depth and a way to check whether the
// iterator is currently at the fixed depth or not.
//
// TODO(blee@igalia.com) Need to check how to handle the shadow tree
// cases (e.g. ':has(::slotted(img))', ':has(component::part(my-part))')
class HasArgumentSubtreeIterator {
  STACK_ALLOCATED();

 public:
  HasArgumentSubtreeIterator(Element&, HasArgumentMatchContext&);
  void operator++();
  Element* Get() const { return current_; }
  bool IsEnd() const { return !current_; }
  bool IsAtFixedDepth() const { return depth_ == depth_limit_; }
  int IsAtSiblingOfHasScope() const { return depth_ == 0; }

 private:
  Element* const scope_element_;
  const bool adjacent_distance_fixed_;
  const int adjacent_distance_limit_;
  const int depth_limit_;
  int depth_;
  Element* current_;
  Element* traversal_end_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_ARGUMENT_MATCH_CONTEXT_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_ARGUMENT_MATCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_ARGUMENT_MATCH_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

enum HasArgumentMatchTraversalScope {
  // Case 1: subselector starts with child or descendant combinator, and depth
  //         is not fixed.
  //         (e.g. :has(.a), :has(.a > .b), :has(.a + .b), :has(> .a .b) ...))
  kSubtree,

  // Case 2: subselector starts with direct or indirect adjacent combinator
  //         and adjacent distance is not fixed and depth is fixed and child
  //         combinator not exists.
  //         (.e.g. :has(~ .a), :has(~ .a ~ .b), :has(~ .a + .b))
  kAllNextSiblings,

  // Case 3: subselector starts with direct adjacent combinator and adjacent
  //         distance is fixed and depth is not fixed.
  //         (.e.g. :has(+ .a .b), :has(+ .a > .b .c)), :has(+ .a .b > .c)
  //                :has(+ .a .b ~ .c), :has(+ .a + .b .c))
  kOneNextSiblingSubtree,

  // Case 4: subselector starts with direct or indirect adjacent combinator
  //         and adjacent distance and depth are not fixed.
  //         (.e.g. :has(~ .a .b), :has(+ .a ~ .b .c))
  kAllNextSiblingSubtrees,

  // Case 5: subselector starts with direct adjacent combinator and both
  //         adjacent distance and depth are fixed and no child combinator.
  //          (.e.g. :has(+ .a), :has(+ .a + .b))
  kOneNextSibling,

  // Case 6: subselector starts with child combinator and depth is fixed.
  //         (.e.g. :has(> .a), :has(> .a > .b), :has(> .a + .b),
  //                :has(> .a ~ .b))
  kFixedDepthDescendants,

  // Case 7: subselector starts with direct adjacent combinator and both
  //         adjacent distance and depth are fixed and child combinator exists.
  //          (.e.g. :has(+ .a > .b), :has(+ .a > .b ~ .c))
  kOneNextSiblingFixedDepthDescendants,

  // Case 8: subselector starts with direct or indirect adjacent combinator
  //         and adjacent distance is not fixed and depth is fixed and child
  //         combinator exists.
  //            (.e.g. :has(~ .a > .b), :has(+ .a ~ .b > .c),
  //                   :has(~ .a > .b ~ .c), :has(+ .a ~ .b > .c ~ .d),
  kAllNextSiblingsFixedDepthDescendants,
};

class CORE_EXPORT HasArgumentMatchContext {
  STACK_ALLOCATED();

 public:
  explicit HasArgumentMatchContext(const CSSSelector* selector);

  inline bool AdjacentDistanceFixed() const {
    return adjacent_distance_limit_ != kInfiniteAdjacentDistance;
  }
  inline int AdjacentDistanceLimit() const { return adjacent_distance_limit_; }
  inline bool DepthFixed() const { return depth_limit_ != kInfiniteDepth; }
  inline int DepthLimit() const { return depth_limit_; }

  inline CSSSelector::RelationType LeftmostRelation() const {
    return leftmost_relation_;
  }

  inline bool SiblingCombinatorAtRightmost() const {
    return sibling_combinator_at_rightmost_;
  }
  inline bool SiblingCombinatorBetweenChildOrDescendantCombinator() const {
    return sibling_combinator_between_child_or_descendant_combinator_;
  }

  HasArgumentMatchTraversalScope TraversalScope() const {
    return traversal_scope_;
  }

  const CSSSelector* HasArgument() const { return has_argument_; }

 private:
  const static int kInfiniteDepth = std::numeric_limits<int>::max();
  const static int kInfiniteAdjacentDistance = std::numeric_limits<int>::max();

  // Indicate the :has argument relative type and subtree traversal scope.
  // If 'adjacent_distance_limit' is integer max, it means that all the
  // adjacent subtrees need to be traversed. otherwise, it means that it is
  // enough to traverse the adjacent subtree at that distance.
  // If 'descendant_traversal_depth_' is integer max, it means that all of the
  // descendant subtree need to be traversed. Otherwise, it means that it is
  // enough to traverse elements at the certain depth.
  //
  // Case 1:  (kDescendant, 0, max)
  //   - Argument selector conditions
  //     - Starts with descendant combinator.
  //   - E.g. ':has(.a)', ':has(.a ~ .b)', ':has(.a ~ .b > .c)'
  //   - Traverse all descendants of the :has scope element.
  // Case 2:  (kChild, 0, max)
  //   - Argument selector conditions
  //     - Starts with child combinator.
  //     - At least one descendant combinator.
  //   - E.g. ':has(> .a .b)', ':has(> .a ~ .b .c)', ':has(> .a + .b .c)'
  //   - Traverse all descendants of the :has scope element.
  // Case 3:  (kChild, 0, n)
  //   - Argument selector conditions
  //     - Starts with child combinator.
  //     - n number of child combinator. (n > 0)
  //     - No descendant combinator.
  //   - E.g.
  //     - ':has(> .a)'            : (kChild, 0, 1)
  //     - ':has(> .a ~ .b > .c)'  : (kChild, 0, 2)
  //   - Traverse the depth n descendants of the :has scope element.
  // Case 4:  (kIndirectAdjacent, max, max)
  //   - Argument selector conditions
  //     - Starts with subsequent-sibling combinator.
  //     - At least one descendant combinator.
  //   - E.g. ':has(~ .a .b)', ':has(~ .a + .b > .c ~ .d .e)'
  //   - Traverse all the subsequent sibling subtrees of the :has scope element.
  //     (all subsequent siblings and it's descendants)
  // Case 5:  (kIndirectAdjacent, max, 0)
  //   - Argument selector conditions
  //     - Starts with subsequent-sibling combinator.
  //     - No descendant/child combinator.
  //   - E.g. ':has(~ .a)', ':has(~ .a + .b ~ .c)'
  //   - Traverse all subsequent siblings of the :has scope element.
  // Case 6:  (kIndirectAdjacent, max, n)
  //   - Argument selector conditions
  //     - Starts with subsequent-sibling combinator.
  //     - n number of child combinator. (n > 0)
  //     - No descendant combinator.
  //   - E.g.
  //     - ':has(~ .a > .b)'                 : (kIndirectAdjacent, max, 1)
  //     - ':has(~ .a + .b > .c ~ .d > .e)'  : (kIndirectAdjacent, max, 2)
  //   - Traverse depth n elements of all subsequent sibling subtree of the
  //     :has scope element.
  // Case 7:  (kDirectAdjacent, max, max)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - At least one subsequent-sibling combinator to the left of every
  //       descendant or child combinator.
  //     - At least 1 descendant combinator.
  //   - E.g. ':has(+ .a ~ .b .c)', ':has(+ .a ~ .b > .c + .d .e)'
  //   - Traverse all the subsequent sibling subtrees of the :has scope element.
  //     (all subsequent siblings and it's descendants)
  // Case 8:  (kDirectAdjacent, max, 0)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - At least one subsequent-sibling combinator.
  //     - No descendant/child combinator.
  //   - E.g. ':has(+ .a ~ .b)', ':has(+ .a + .b ~ .c)'
  //   - Traverse all subsequent siblings of the :has scope element.
  // Case 9:  (kDirectAdjacent, max, n)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - At least one subsequent-sibling combinator to the left of every
  //       descendant or child combinator.
  //     - n number of child combinator. (n > 0)
  //     - No descendant combinator.
  //   - E.g.
  //     - ':has(+ .a ~ .b > .c)'            : (kDirectAdjacent, max, 1)
  //     - ':has(+ .a ~ .b > .c + .d >.e)'   : (kDirectAdjacent, max, 2)
  //   - Traverse depth n elements of all subsequent sibling subtree of the
  //     :has scope element.
  // Case 10:  (kDirectAdjacent, n, max)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - n number of next-sibling combinator to the left of the leftmost
  //       child(or descendant) combinator. (n > 0)
  //     - No subsequent-sibling combinator to the left of the leftmost child
  //       (or descendant) combinator.
  //     - At least 1 descendant combinator.
  //   - E.g.
  //     - ':has(+ .a .b)'            : (kDirectAdjacent, 1, max)
  //     - ':has(+ .a > .b + .c .d)'  : (kDirectAdjacent, 1, max)
  //     - ':has(+ .a + .b > .c .d)'  : (kDirectAdjacent, 2, max)
  //   - Traverse the distance n sibling subtree of the :has scope element.
  //     (sibling element at distance n, and it's descendants).
  // Case 11:  (kDirectAdjacent, n, 0)
  //   - Argument selector conditions
  //     - Starts with next-sibling combinator.
  //     - n number of next-sibling combinator. (n > 0)
  //     - No child/descendant/subsequent-sibling combinator.
  //   - E.g.
  //     - ':has(+ .a)'            : (kDirectAdjacent, 1, 0)
  //     - ':has(+ .a + .b + .c)'  : (kDirectAdjacent, 3, 0)
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
  //     - ':has(+ .a > .b)'                 : (kDirectAdjacent, 1, 1)
  //     - ':has(+ .a + .b > .c ~ .d > .e)'  : (kDirectAdjacent, 2, 2)
  //   - Traverse the depth m elements of the distance n sibling subtree of
  //     the :has scope element. (elements at depth m of the descendant subtree
  //     of the sibling element at distance n)
  CSSSelector::RelationType leftmost_relation_{CSSSelector::kSubSelector};
  int adjacent_distance_limit_;
  int depth_limit_;

  // Indicates the selector's combinator information which can be used for
  // sibling traversal after subselector matched.
  bool sibling_combinator_at_rightmost_{false};
  bool sibling_combinator_between_child_or_descendant_combinator_{false};
  HasArgumentMatchTraversalScope traversal_scope_;
  const CSSSelector* has_argument_;
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
// 'div:has(> .a > .b)', instead of traversing all the descendants
// of div element, we can limit the traversal only for the elements at
// depth 2 of the div element. When we have 'div:has(+ .a > .b)',
// we can limit the traversal only for the child elements of the direct
// adjacent sibling of the div element. To implement this, we need a
// way to limit the traversal depth and a way to check whether the
// iterator is currently at the fixed depth or not.
class HasArgumentSubtreeIterator {
  STACK_ALLOCATED();

 public:
  HasArgumentSubtreeIterator(Element&, HasArgumentMatchContext&);
  void operator++();
  Element* CurrentElement() const { return current_; }
  bool AtEnd() const { return !current_; }
  bool AtFixedDepth() const { return depth_ == context_.DepthLimit(); }
  bool UnderDepthLimit() const { return depth_ <= context_.DepthLimit(); }
  bool AtSiblingOfHasScope() const { return depth_ == 0; }
  inline int Depth() const { return depth_; }
  inline Element* ScopeElement() const { return has_scope_element_; }
  inline const HasArgumentMatchContext& Context() const { return context_; }

 private:
  inline Element* LastWithin(Element*);

  Element* const has_scope_element_;
  const HasArgumentMatchContext& context_;
  int depth_{0};
  Element* current_{nullptr};
  Element* traversal_end_{nullptr};
};

// Iterator class to traverse siblings, ancestors and ancestor siblings of the
// HasArgumentSubtreeIterator's current element until reach to the scope
// element.
// This iterator is used to set the 'AncestorsOrAncestorSiblingsAffectedByHas'
// or 'SiblingsAffectedByHas' flags of those elements before returning early
// from the ':has()' argument subtree traversal.
class AffectedByHasIterator {
  STACK_ALLOCATED();

 public:
  explicit AffectedByHasIterator(HasArgumentSubtreeIterator&);
  void operator++();
  Element* CurrentElement() const { return current_; }
  bool AtEnd() const;
  bool AtSiblingOfHasScope() const { return depth_ == 0; }

 private:
  inline bool NeedsTraverseSiblings();

  const HasArgumentSubtreeIterator& iterator_at_matched_;
  int depth_;
  Element* current_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_HAS_ARGUMENT_MATCH_CONTEXT_H_

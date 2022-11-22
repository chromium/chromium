/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/editing_utilities.h"

#include "third_party/blink/renderer/core/editing/editing_strategy.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"

namespace blink {

namespace {

template <typename Traversal>
int16_t SlowComparePositions(const Node* container_a,
                             int offset_a,
                             const Node* container_b,
                             int offset_b,
                             bool* disconnected) {
  DCHECK(container_a);
  DCHECK(container_b);

  if (disconnected)
    *disconnected = false;

  if (!container_a)
    return -1;
  if (!container_b)
    return 1;

  // see DOM2 traversal & range section 2.5

  // case 1: both points have the same container
  if (container_a == container_b) {
    if (offset_a == offset_b)
      return 0;  // A is equal to B
    if (offset_a < offset_b)
      return -1;  // A is before B
    return 1;     // A is after B
  }

  // case 2: node C (container B or an ancestor) is a child node of A
  const Node* c = container_b;
  while (c && Traversal::Parent(*c) != container_a)
    c = Traversal::Parent(*c);
  if (c) {
    int offset_c = 0;
    Node* n = Traversal::FirstChild(*container_a);
    while (n != c && offset_c < offset_a) {
      offset_c++;
      n = Traversal::NextSibling(*n);
    }

    if (offset_a <= offset_c)
      return -1;  // A is before B
    return 1;     // A is after B
  }

  // case 3: node C (container A or an ancestor) is a child node of B
  c = container_a;
  while (c && Traversal::Parent(*c) != container_b)
    c = Traversal::Parent(*c);
  if (c) {
    int offset_c = 0;
    Node* n = Traversal::FirstChild(*container_b);
    while (n != c && offset_c < offset_b) {
      offset_c++;
      n = Traversal::NextSibling(*n);
    }

    if (offset_c < offset_b)
      return -1;  // A is before B
    return 1;     // A is after B
  }

  // case 4: containers A & B are siblings, or children of siblings
  // ### we need to do a traversal here instead
  Node* common_ancestor = Traversal::CommonAncestor(*container_a, *container_b);
  if (!common_ancestor) {
    if (disconnected)
      *disconnected = true;
    return 0;
  }
  const Node* child_a = container_a;
  while (child_a && Traversal::Parent(*child_a) != common_ancestor)
    child_a = Traversal::Parent(*child_a);
  if (!child_a)
    child_a = common_ancestor;
  const Node* child_b = container_b;
  while (child_b && Traversal::Parent(*child_b) != common_ancestor)
    child_b = Traversal::Parent(*child_b);
  if (!child_b)
    child_b = common_ancestor;

  if (child_a == child_b)
    return 0;  // A is equal to B

  Node* n = Traversal::FirstChild(*common_ancestor);
  while (n) {
    if (n == child_a)
      return -1;  // A is before B
    if (n == child_b)
      return 1;  // A is after B
    n = Traversal::NextSibling(*n);
  }

  // Should never reach this point.
  NOTREACHED();
  return 0;
}

// The `Comparator` class implements `ComparePositions()` logic.
template <typename Traversal>
class Comparator {
  STATIC_ONLY(Comparator);

 public:
  // Returns
  //  -1 if `node_a` is before `node_b`
  //   0 if `node_a == node_b`
  //   1 if `node_a` is after `node_b`
  //    where
  //      * `node_a == Traversal::ChildAt(*container_a, offset_a)`
  //      * `node_b == Traversal::ChildAt(*container_a, offset_b)`
  // and set `disconnected` to true if `node_a` and `node_b` are in different
  // tree scopes.
  static int16_t ComparePositions(const Node* container_a,
                                  int offset_a,
                                  const Node* container_b,
                                  int offset_b,
                                  bool* disconnected) {
    DCHECK(container_a);
    DCHECK(container_b);

    if (disconnected)
      *disconnected = false;

    if (!container_a)
      return kAIsBeforeB;
    if (!container_b)
      return kAIsAfterB;

    // see DOM2 traversal & range section 2.5

    // Case 1: both points have the same container
    if (container_a == container_b) {
      if (offset_a == offset_b)
        return kAIsEqualToB;
      if (offset_a < offset_b)
        return kAIsBeforeB;
      return kAIsAfterB;
    }

    // Case 2: node C (container B or an ancestor) is a child node of A, e.g.
    //  * A < B
    //      `<a>...A...<c2>...<b>...B...</b>...</c2>...</a>`
    //  * A > B
    //      `<a>...<c2>...<b>...B...</b>...</c2>...A...</a>`
    //  * A == C2
    //             A
    //      `<a>...<c2>...<b>...B...</b>...</c2>...</a>`
    if (const Node* node_c2 = FindChildnInAncestors(*container_b, *container_a))
      return CompareNodesInSameParent(offset_a, node_c2, kAIsBeforeB);

    // Case 3: node C (container A or an ancestor) is a child node of B, e.g.
    //  * B < A
    //      `<b>...B....<c3>...<a>...A...</a>...</b>`
    //  * B > A
    //      `<b>...<c3>...<a>...A...</a>...</c3>...B...</b>`
    //  * B == C3
    //             B
    //      `<b>...<c3>...<a>...A...</a>...</b>`
    if (const Node* node_c3 = FindChildnInAncestors(*container_a, *container_b))
      return -CompareNodesInSameParent(offset_b, node_c3, kAIsBeforeB);

    // case 4: containers A & B are siblings, or children of siblings
    // ### we need to do a traversal here instead
    Node* const common_ancestor =
        Traversal::CommonAncestor(*container_a, *container_b);
    if (!common_ancestor) {
      if (disconnected)
        *disconnected = true;
      return kAIsEqualToB;
    }

    const Node* const child_a =
        FindChildnInAncestors(*container_a, *common_ancestor);
    const Node* const adjusted_child_a =
        child_a ? child_a : Traversal::LastChild(*common_ancestor);
    const Node* const child_b =
        FindChildnInAncestors(*container_b, *common_ancestor);
    const Node* const adjusted_child_b =
        child_b ? child_b : Traversal::LastChild(*common_ancestor);
    return CompareNodesInSameParent(adjusted_child_a, adjusted_child_b);
  }

 private:
  enum Result : int16_t {
    kAIsBeforeB = -1,
    kAIsEqualToB = 0,
    kAIsAfterB = 1,
  };

  // Returns where `offset_b =  Traversal::Index(*child_b)`:
  //  -1 if `offset_a < offset_b`
  //   0 if `offset_a == offset_b`
  //   1 if `offset_a > offset_b`
  // The number of iteration is `std::min(offset_a, offset_b)`.
  static Result CompareNodesInSameParent(
      int offset_a,
      const Node* child_b,
      Result result_of_a_is_equal_to_b = kAIsEqualToB) {
    DCHECK(child_b);
    int offset_b = 0;
    for (const Node& child_a :
         Traversal::ChildrenOf(*Traversal::Parent(*child_b))) {
      if (child_a == child_b || offset_a == offset_b) {
        const int diff = offset_a - offset_b;
        return !diff      ? result_of_a_is_equal_to_b
               : diff > 0 ? kAIsAfterB
                          : kAIsBeforeB;
      }
      ++offset_b;
    }
    NOTREACHED();
    return result_of_a_is_equal_to_b;
  }

  // Returns
  //  -1 if `Traversal::Index(*child_a) < Traversal::Index(*child_b)`
  //   0 if `Traversal::Index(*child_a) == Traversal::Index(*child_b)`
  //   1 if `Traversal::Index(*child_a) > Traversal::Index(*child_b)`
  // The number of iteration is `std::min(offset_a, offset_b)`.
  static Result CompareNodesInSameParent(
      const Node* child_a,
      const Node* child_b,
      Result result_of_a_is_equal_to_b = kAIsEqualToB) {
    DCHECK(child_a);
    DCHECK(child_b);
    if (child_a == child_b)
      return result_of_a_is_equal_to_b;
    DCHECK_EQ(Traversal::Parent(*child_a), Traversal::Parent(*child_b));
    for (const Node& child :
         Traversal::ChildrenOf(*Traversal::Parent(*child_a))) {
      if (child == child_a)
        return child == child_b ? result_of_a_is_equal_to_b : kAIsBeforeB;
      if (child == child_b)
        return kAIsAfterB;
    }
    NOTREACHED();
    return result_of_a_is_equal_to_b;
  }

  // Returns the child node in `parent` if `parent` is one of inclusive
  // ancestors of `node`, otherwise `nullptr`.
  // See https://dom.spec.whatwg.org/#boundary-points
  static const Node* FindChildnInAncestors(const Node& node,
                                           const Node& parent) {
    DCHECK_NE(node, parent);
    for (const Node& child : Traversal::InclusiveAncestorsOf(node)) {
      if (Traversal::Parent(child) == parent)
        return &child;
    }
    return nullptr;
  }
};

}  // namespace

int16_t ComparePositionsInDOMTree(const Node* container_a,
                                  int offset_a,
                                  const Node* container_b,
                                  int offset_b,
                                  bool* disconnected) {
  if (!RuntimeEnabledFeatures::FastComparePositionsEnabled()) {
    return SlowComparePositions<NodeTraversal>(
        container_a, offset_a, container_b, offset_b, disconnected);
  }

  return Comparator<NodeTraversal>::ComparePositions(
      container_a, offset_a, container_b, offset_b, disconnected);
}

int16_t ComparePositionsInFlatTree(const Node* container_a,
                                   int offset_a,
                                   const Node* container_b,
                                   int offset_b,
                                   bool* disconnected) {
  if (!RuntimeEnabledFeatures::FastComparePositionsEnabled()) {
    return SlowComparePositions<FlatTreeTraversal>(
        container_a, offset_a, container_b, offset_b, disconnected);
  }

  return Comparator<FlatTreeTraversal>::ComparePositions(
      container_a, offset_a, container_b, offset_b, disconnected);
}

// Compare two positions, taking into account the possibility that one or both
// could be inside a shadow tree. Only works for non-null values.
int16_t ComparePositions(const Position& a, const Position& b) {
  DCHECK(a.IsNotNull());
  DCHECK(b.IsNotNull());
  const TreeScope* common_scope = Position::CommonAncestorTreeScope(a, b);

  DCHECK(common_scope);
  if (!common_scope)
    return 0;

  Node* node_a = common_scope->AncestorInThisScope(a.ComputeContainerNode());
  DCHECK(node_a);
  bool has_descendent_a = node_a != a.ComputeContainerNode();
  int offset_a = has_descendent_a ? 0 : a.ComputeOffsetInContainerNode();

  Node* node_b = common_scope->AncestorInThisScope(b.ComputeContainerNode());
  DCHECK(node_b);
  bool has_descendent_b = node_b != b.ComputeContainerNode();
  int offset_b = has_descendent_b ? 0 : b.ComputeOffsetInContainerNode();

  int16_t bias = 0;
  if (node_a == node_b) {
    if (has_descendent_a)
      bias = 1;
    else if (has_descendent_b)
      bias = -1;
  }

  int16_t result =
      ComparePositionsInDOMTree(node_a, offset_a, node_b, offset_b);
  return result ? result : bias;
}

int16_t ComparePositions(const PositionWithAffinity& a,
                         const PositionWithAffinity& b) {
  return ComparePositions(a.GetPosition(), b.GetPosition());
}

int16_t ComparePositions(const VisiblePosition& a, const VisiblePosition& b) {
  return ComparePositions(a.DeepEquivalent(), b.DeepEquivalent());
}

int16_t ComparePositions(const PositionInFlatTree& position_a,
                         const PositionInFlatTree& position_b) {
  DCHECK(position_a.IsNotNull());
  DCHECK(position_b.IsNotNull());

  Node* container_a = position_a.ComputeContainerNode();
  Node* container_b = position_b.ComputeContainerNode();
  int offset_a = position_a.ComputeOffsetInContainerNode();
  int offset_b = position_b.ComputeOffsetInContainerNode();
  return ComparePositionsInFlatTree(container_a, offset_a, container_b,
                                    offset_b);
}

}  // namespace blink

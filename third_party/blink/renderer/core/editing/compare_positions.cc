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

constexpr int kInvalidOffset = -1;

// The `Comparator` class implements `ComparePositions()` logic.
template <typename Traversal>
class Comparator {
  STATIC_ONLY(Comparator);

 public:
  // Integer offset version of `ComparePositions()`.
  //
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
    return ComparePositionsInternal(container_a, IntAsOffset(offset_a),
                                    container_b, IntAsOffset(offset_b),
                                    disconnected);
  }

  // Integer/Node offset version of `ComparePositions()`.
  //
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
                                  const Node* child_a,
                                  const Node* container_b,
                                  int offset_b,
                                  const Node* child_b,
                                  bool* disconnected = nullptr) {
    if (offset_a == kInvalidOffset && offset_b == kInvalidOffset) {
      return ComparePositionsInternal(container_a, NodeAsOffset(child_a),
                                      container_b, NodeAsOffset(child_b),
                                      disconnected);
    }

    if (offset_a == kInvalidOffset) {
      return ComparePositionsInternal(container_a, NodeAsOffset(child_a),
                                      container_b, IntAsOffset(offset_b),
                                      disconnected);
    }

    if (offset_b == kInvalidOffset) {
      return ComparePositionsInternal(container_a, IntAsOffset(offset_a),
                                      container_b, NodeAsOffset(child_b),
                                      disconnected);
    }

    return ComparePositionsInternal(container_a, IntAsOffset(offset_a),
                                    container_b, IntAsOffset(offset_b),
                                    disconnected);
  }

 private:
  enum Result : int16_t {
    kAIsBeforeB = -1,
    kAIsEqualToB = 0,
    kAIsAfterB = 1,
  };

  // The wrapper class of `int` offset.
  class IntAsOffset {
    STACK_ALLOCATED();

   public:
    explicit IntAsOffset(int value) : value_(value) {}
    int Get() const { return value_; }

   private:
    int value_;
  };

  // The wrapper class of offset in `Node*` before position.
  class NodeAsOffset {
    STACK_ALLOCATED();

   public:
    explicit NodeAsOffset(const Node* value) : value_(value) {}
    const Node* Get() const { return value_; }

   private:
    const Node* value_;
  };

  // Returns
  //  -1 if `child_a` is before `child_b`
  //   0 if `child_a == child_b`
  //   1 if `child_a` is after `child_b`
  //    where
  //      * `child_a == Traversal::ChildAt(*container_a, offset_a)`
  //      * `child_b == Traversal::ChildAt(*container_a, offset_b)`
  // and set `disconnected` to true if `child_a` and `child_b` are in different
  // tree scopes.
  template <typename OffsetA, typename OffsetB>
  static int16_t ComparePositionsInternal(const Node* container_a,
                                          OffsetA offset_a,
                                          const Node* container_b,
                                          OffsetB offset_b,
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
    if (container_a == container_b)
      return CompareNodesInSameParent(offset_a.Get(), offset_b.Get());

    // Case 2: node C (container B or an ancestor) is a child node of A, e.g.
    //  * A < B
    //      `<a>...A...<c2>...<b>...B...</b>...</c2>...</a>`
    //  * A > B
    //      `<a>...<c2>...<b>...B...</b>...</c2>...A...</a>`
    //  * A == C2
    //             A
    //      `<a>...<c2>...<b>...B...</b>...</c2>...</a>`
    if (const Node* node_c2 =
            FindChildInAncestors(*container_b, *container_a)) {
      return CompareNodesInSameParent(
          offset_a.Get(), Traversal::PreviousSibling(*node_c2), kAIsBeforeB);
    }

    // Case 3: node C (container A or an ancestor) is a child node of B, e.g.
    //  * B < A
    //      `<b>...B....<c3>...<a>...A...</a>...</b>`
    //  * B > A
    //      `<b>...<c3>...<a>...A...</a>...</c3>...B...</b>`
    //  * B == C3
    //             B
    //      `<b>...<c3>...<a>...A...</a>...</b>`
    if (const Node* node_c3 =
            FindChildInAncestors(*container_a, *container_b)) {
      return -CompareNodesInSameParent(
          offset_b.Get(), Traversal::PreviousSibling(*node_c3), kAIsBeforeB);
    }

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
        FindChildInAncestors(*container_a, *common_ancestor);
    const Node* const adjusted_child_a =
        child_a ? child_a : Traversal::LastChild(*common_ancestor);
    const Node* const child_b =
        FindChildInAncestors(*container_b, *common_ancestor);
    const Node* const adjusted_child_b =
        child_b ? child_b : Traversal::LastChild(*common_ancestor);
    return CompareNodesInSameParent(adjusted_child_a, adjusted_child_b);
  }

  // Returns
  //  -1 if `offset_a < offset_b`
  //   0 if `offset_a == offset_b`
  //   1 if `offset_a > offset_b`
  //     where ```
  //        offset_b =  child_before_position_b
  //            ? Traversal::Index(*child_before_position_b) + 1
  //            : 0 ```
  // The number of iteration is `std::min(offset_a, offset_b)`.
  static Result CompareNodesInSameParent(
      int offset_a,
      const Node* child_before_position_b,
      Result result_of_a_is_equal_to_b = kAIsEqualToB) {
    if (!child_before_position_b)
      return !offset_a ? result_of_a_is_equal_to_b : kAIsAfterB;
    if (!offset_a)
      return kAIsBeforeB;
    // Starts from offset 1 and after `child_before_position_b`.
    const Node& child_b = *child_before_position_b;
    int offset = 1;
    for (const Node& child :
         Traversal::ChildrenOf(*Traversal::Parent(child_b))) {
      if (offset_a == offset)
        return child == child_b ? result_of_a_is_equal_to_b : kAIsBeforeB;
      if (child == child_b)
        return kAIsAfterB;
      ++offset;
    }
    NOTREACHED_IN_MIGRATION();
    return result_of_a_is_equal_to_b;
  }

  static int16_t CompareNodesInSameParent(
      int offset_a,
      int offset_b,
      Result result_of_a_is_equal_to_b = kAIsEqualToB) {
    if (offset_a == offset_b)
      return result_of_a_is_equal_to_b;
    return offset_a < offset_b ? kAIsBeforeB : kAIsAfterB;
  }

  static int16_t CompareNodesInSameParent(const Node* child_before_position_a,
                                          int offset_b) {
    return -CompareNodesInSameParent(offset_b, child_before_position_a);
  }

  // Returns
  //  -1 if `Traversal::Index(*child_a) < Traversal::Index(*child_b)`
  //   0 if `Traversal::Index(*child_a) == Traversal::Index(*child_b)`
  //   1 if `Traversal::Index(*child_a) > Traversal::Index(*child_b)`
  //  `child_a` and `child_b` should be in a same parent nod or `nullptr`.
  //
  //  When `child_a` < `child_b`. ```
  //                   child_a                           child_b
  ///   <-- backward_a --|-- forward_a --><-- backward_b --|-- forward_b -->
  //  |------------------+---------------------------------+----------------|
  //  ```
  //  When `child_a` > `child_b`. ```
  //                   child_b                           child_a
  ///   <-- backward_b --|-- forward_b --><-- backward_a --|-- forward_a -->
  //  |------------------+---------------------------------+----------------|
  //  ```
  //
  //  The number of iterations is: ```
  //    std::min(offset_a, offset_b,
  //             abs(offset_a - offset_b) / 2,
  //             number_of_children - offset_a,
  //             number_of_children - offset_b)
  //  where
  //    `offset_a` == `Traversal::Index(*child_a)`
  //    `offset_b` == `Traversal::Index(*child_b)`
  //
  //  ```
  // Note: this number can't exceed `number_of_children / 4`.
  //
  // Note: We call this function both "node before position" and "node after
  // position" cases. For "node after position" case, `child_a` and `child_b`
  // should not be `nullptr`.
  static int16_t CompareNodesInSameParent(
      const Node* child_a,
      const Node* child_b,
      Result result_of_a_is_equal_to_b = kAIsEqualToB) {
    if (child_a == child_b)
      return result_of_a_is_equal_to_b;
    if (!child_a)
      return kAIsBeforeB;
    if (!child_b)
      return kAIsAfterB;
    DCHECK_EQ(Traversal::Parent(*child_a), Traversal::Parent(*child_b));
    const Node* backward_a = child_a;
    const Node* forward_a = child_a;
    const Node* backward_b = child_b;
    const Node* forward_b = child_b;

    for (;;) {
      backward_a = Traversal::PreviousSibling(*backward_a);
      if (!backward_a)
        return kAIsBeforeB;
      if (backward_a == forward_b)
        return kAIsAfterB;

      forward_a = Traversal::NextSibling(*forward_a);
      if (!forward_a)
        return kAIsAfterB;
      if (forward_a == backward_b)
        return kAIsBeforeB;

      backward_b = Traversal::PreviousSibling(*backward_b);
      if (!backward_b)
        return kAIsAfterB;
      if (forward_a == backward_b)
        return kAIsBeforeB;

      forward_b = Traversal::NextSibling(*forward_b);
      if (!forward_b)
        return kAIsBeforeB;
      if (backward_a == forward_b)
        return kAIsAfterB;
    }

    NOTREACHED_IN_MIGRATION();
    return result_of_a_is_equal_to_b;
  }

  // Returns the child node in `parent` if `parent` is one of inclusive
  // ancestors of `node`, otherwise `nullptr`.
  // See https://dom.spec.whatwg.org/#boundary-points
  static const Node* FindChildInAncestors(const Node& node,
                                          const Node& parent) {
    DCHECK_NE(node, parent);
    const Node* candidate = &node;
    for (const Node& child : Traversal::AncestorsOf(node)) {
      if (child == parent)
        return candidate;
      candidate = &child;
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
  return Comparator<NodeTraversal>::ComparePositions(
      container_a, offset_a, container_b, offset_b, disconnected);
}

int16_t ComparePositionsInFlatTree(const Node* container_a,
                                   int offset_a,
                                   const Node* container_b,
                                   int offset_b,
                                   bool* disconnected) {
  if (container_a->IsShadowRoot()) {
    container_a = container_a->OwnerShadowHost();
  }
  if (container_b->IsShadowRoot()) {
    container_b = container_b->OwnerShadowHost();
  }
  return Comparator<FlatTreeTraversal>::ComparePositions(
      container_a, offset_a, container_b, offset_b, disconnected);
}

int16_t ComparePositions(const Position& position_a,
                         const Position& position_b) {
  DCHECK(position_a.IsNotNull());
  DCHECK(position_b.IsNotNull());

  const TreeScope* common_scope =
      Position::CommonAncestorTreeScope(position_a, position_b);

  DCHECK(common_scope);
  if (!common_scope)
    return 0;

  Node* const container_a = position_a.ComputeContainerNode();
  Node* const node_a = common_scope->AncestorInThisScope(container_a);
  DCHECK(node_a);
  const bool has_descendant_a = node_a != container_a;

  Node* const container_b = position_b.ComputeContainerNode();
  Node* const node_b = common_scope->AncestorInThisScope(container_b);
  DCHECK(node_b);
  const bool has_descendant_b = node_b != container_b;

  const int offset_a = position_a.IsOffsetInAnchor() && !has_descendant_a
                           ? position_a.OffsetInContainerNode()
                           : kInvalidOffset;

  const int offset_b = position_b.IsOffsetInAnchor() && !has_descendant_b
                           ? position_b.OffsetInContainerNode()
                           : kInvalidOffset;

  Node* const child_a = position_a.IsOffsetInAnchor() || has_descendant_a
                            ? nullptr
                            : position_a.ComputeNodeBeforePosition();

  Node* const child_b = position_b.IsOffsetInAnchor() || has_descendant_b
                            ? nullptr
                            : position_b.ComputeNodeBeforePosition();

  const int16_t bias = node_a != node_b   ? 0
                       : has_descendant_a ? 1
                       : has_descendant_b ? -1
                                          : 0;

  const int16_t result = Comparator<NodeTraversal>::ComparePositions(
      node_a, offset_a, child_a, node_b, offset_b, child_b);
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

  Node* const container_a = position_a.ComputeContainerNode();
  Node* const container_b = position_b.ComputeContainerNode();

  const int offset_a = position_a.IsOffsetInAnchor()
                           ? position_a.OffsetInContainerNode()
                           : kInvalidOffset;

  const int offset_b = position_b.IsOffsetInAnchor()
                           ? position_b.OffsetInContainerNode()
                           : kInvalidOffset;

  Node* const child_a = position_a.IsOffsetInAnchor()
                            ? nullptr
                            : position_a.ComputeNodeBeforePosition();

  Node* const child_b = position_b.IsOffsetInAnchor()
                            ? nullptr
                            : position_b.ComputeNodeBeforePosition();

  return Comparator<FlatTreeTraversal>::ComparePositions(
      container_a, offset_a, child_a, container_b, offset_b, child_b);
}

}  // namespace blink

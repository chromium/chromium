/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_ITERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editing_strategy.h"
#include "third_party/blink/renderer/core/editing/forward.h"

namespace blink {

// A Position iterator with nearly constant-time
// increment, decrement, and several predicates on the Position it is at.
// Conversion from Position is O(n) in the depth.
// Conversion to Position is O(1).
// PositionIteratorAlgorithm must be used without DOM tree change.
template <typename Strategy>
class PositionIteratorAlgorithm {
  STACK_ALLOCATED();

 public:
  explicit PositionIteratorAlgorithm(const PositionTemplate<Strategy>&);
  PositionIteratorAlgorithm();

  // Since |deprecatedComputePosition()| is slow, new code should use
  // |computePosition()| instead.
  PositionTemplate<Strategy> DeprecatedComputePosition() const;
  PositionTemplate<Strategy> ComputePosition() const;

  // increment() takes O(1) other than incrementing to a element that has
  // new parent.
  // In the later case, it takes time of O(<number of childlen>) but the case
  // happens at most depth-of-the-tree times over whole tree traversal.
  void Increment();
  // decrement() takes O(1) other than decrement into new node that has
  // childlen.
  // In the later case, it takes time of O(<number of childlen>).
  void Decrement();

  Node* GetNode() const { return anchor_node_; }
  int OffsetInLeafNode() const { return offset_in_anchor_; }

  bool AtStart() const;
  bool AtEnd() const;
  bool AtStartOfNode() const;
  bool AtEndOfNode() const;

 private:
  PositionIteratorAlgorithm(Node* anchor_node, int offset_in_anchoror_node);

  bool IsValid() const {
    return !anchor_node_ ||
           dom_tree_version_ == anchor_node_->GetDocument().DomTreeVersion();
  }

  Node* anchor_node_ = nullptr;
  // If this is non-null, Strategy::Parent(*node_after_position_in_anchor_) ==
  // anchor_node_;
  Node* node_after_position_in_anchor_ = nullptr;
  int offset_in_anchor_;
  wtf_size_t depth_to_anchor_node_;
  // If |node_after_position_in_anchor_| is not null,
  // offsets_in_anchor_node_[depth_to_anchor_node_] ==
  //    Strategy::Index(node_after_position_in_anchor_).
  Vector<int> offsets_in_anchor_node_;
  uint64_t dom_tree_version_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    PositionIteratorAlgorithm<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    PositionIteratorAlgorithm<EditingInFlatTreeStrategy>;

using PositionIterator = PositionIteratorAlgorithm<EditingStrategy>;
using PositionIteratorInFlatTree =
    PositionIteratorAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_POSITION_ITERATOR_H_

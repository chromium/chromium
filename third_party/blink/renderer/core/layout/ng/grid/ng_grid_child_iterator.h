// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_CHILD_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_CHILD_ITERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// A utility class which given the current grid node will iterate through its
// children.
//
// TODO(layout-dev): Once LayoutNG supports NG-fragmentation this will need
// to be updated to accept a break-token.
//
// This class does not handle modifications to its arguments after it has been
// constructed.
class CORE_EXPORT NGGridChildIterator {
  STACK_ALLOCATED();

 public:
  explicit NGGridChildIterator(const NGBlockNode node);

  // Returns the next block node which should be laid out.
  NGBlockNode NextChild() {
    DCHECK(position_ <= children_.size());
    if (position_ == children_.size())
      return nullptr;
    return children_[position_++].child;
  }

  struct ChildWithOrder {
    DISALLOW_NEW();

   public:
    ChildWithOrder(NGBlockNode child, int order) : child(child), order(order) {}
    void Trace(Visitor* visitor) const { visitor->Trace(child); }

    NGBlockNode child;
    int order;
  };

 protected:
  virtual void Setup(const NGBlockNode node);
  // |children_| cannot be modified after |Setup()|.
  HeapVector<ChildWithOrder, 4> children_;
  wtf_size_t position_ = 0;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(
    blink::NGGridChildIterator::ChildWithOrder)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_CHILD_ITERATOR_H_

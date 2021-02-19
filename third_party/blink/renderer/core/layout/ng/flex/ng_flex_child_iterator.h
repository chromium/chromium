// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_CHILD_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_CHILD_ITERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// A utility class for flex layout which given the current node will iterate
// through its children.
//
// TODO(layout-dev): Once flex layout supports NG-fragmentation this will need
// to be updated to accept a break-token.
//
// This class does not handle modifications to its arguments after it has been
// constructed.
class CORE_EXPORT NGFlexChildIterator {
  STACK_ALLOCATED();

 public:
  NGFlexChildIterator(const NGBlockNode node);

  // Returns the next block node which should be laid out.
  NGBlockNode NextChild() {
    if (iterator_ == children_.end())
      return nullptr;

    return (*iterator_++).child;
  }

  struct ChildWithOrder {
    DISALLOW_NEW();
    ChildWithOrder(NGBlockNode child, int order) : child(child), order(order) {}
    NGBlockNode child;
    int order;
  };

 private:
  Vector<ChildWithOrder, 4> children_;
  Vector<ChildWithOrder, 4>::const_iterator iterator_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(
    blink::NGFlexChildIterator::ChildWithOrder)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_CHILD_ITERATOR_H_

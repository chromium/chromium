// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"

namespace blink {

void PhysicalOofPositionedNode::Trace(Visitor* visitor) const {
  if (is_for_fragmentation) {
    static_cast<const PhysicalOofNodeForFragmentation*>(this)
        ->TraceAfterDispatch(visitor);
  } else {
    TraceAfterDispatch(visitor);
  }
}

void PhysicalOofPositionedNode::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(box);
  visitor->Trace(inline_container);
}

void LogicalOofPositionedNode::Trace(Visitor* visitor) const {
  if (is_for_fragmentation) {
    static_cast<const LogicalOofNodeForFragmentation*>(this)
        ->TraceAfterDispatch(visitor);
  } else {
    TraceAfterDispatch(visitor);
  }
}

void LogicalOofPositionedNode::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(box);
  visitor->Trace(inline_container);
}

void PhysicalOofNodeForFragmentation::TraceAfterDispatch(
    Visitor* visitor) const {
  PhysicalOofPositionedNode::TraceAfterDispatch(visitor);
  visitor->Trace(containing_block);
  visitor->Trace(fixedpos_containing_block);
  visitor->Trace(fixedpos_inline_container);
}

void LogicalOofNodeForFragmentation::TraceAfterDispatch(
    Visitor* visitor) const {
  LogicalOofPositionedNode::TraceAfterDispatch(visitor);
  visitor->Trace(containing_block);
  visitor->Trace(fixedpos_containing_block);
  visitor->Trace(fixedpos_inline_container);
}

}  // namespace blink

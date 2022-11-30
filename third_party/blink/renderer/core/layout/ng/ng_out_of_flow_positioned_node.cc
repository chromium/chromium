// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"

namespace blink {

void NGPhysicalOutOfFlowPositionedNode::Trace(Visitor* visitor) const {
  if (is_for_fragmentation) {
    static_cast<const NGPhysicalOOFNodeForFragmentation*>(this)
        ->TraceAfterDispatch(visitor);
  } else {
    TraceAfterDispatch(visitor);
  }
}

void NGPhysicalOutOfFlowPositionedNode::TraceAfterDispatch(
    Visitor* visitor) const {
  visitor->Trace(box);
  visitor->Trace(inline_container);
}

void NGLogicalOutOfFlowPositionedNode::Trace(Visitor* visitor) const {
  if (is_for_fragmentation) {
    static_cast<const NGLogicalOOFNodeForFragmentation*>(this)
        ->TraceAfterDispatch(visitor);
  } else {
    TraceAfterDispatch(visitor);
  }
}

void NGLogicalOutOfFlowPositionedNode::TraceAfterDispatch(
    Visitor* visitor) const {
  visitor->Trace(box);
  visitor->Trace(inline_container);
}

void NGPhysicalOOFNodeForFragmentation::TraceAfterDispatch(
    Visitor* visitor) const {
  NGPhysicalOutOfFlowPositionedNode::TraceAfterDispatch(visitor);
  visitor->Trace(containing_block);
  visitor->Trace(fixedpos_containing_block);
  visitor->Trace(fixedpos_inline_container);
}

void NGLogicalOOFNodeForFragmentation::TraceAfterDispatch(
    Visitor* visitor) const {
  NGLogicalOutOfFlowPositionedNode::TraceAfterDispatch(visitor);
  visitor->Trace(containing_block);
  visitor->Trace(fixedpos_containing_block);
  visitor->Trace(fixedpos_inline_container);
}

}  // namespace blink

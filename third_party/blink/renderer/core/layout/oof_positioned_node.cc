// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"

namespace blink {

template <>
void OofPositionedNode<PhysicalOffset, PhysicalStaticPosition>::Trace(
    Visitor* visitor) const {
  // Some poor-man's RTTI here, which will go away with FragmentedOofInCb, at
  // which point TraceAfterDispatch() can be merged into Trace().
  if (is_for_fragmentation_) {
    static_cast<const PhysicalOofNodeForFragmentation*>(this)
        ->TraceAfterDispatch(visitor);
  } else {
    TraceAfterDispatch(visitor);
  }
}

template <>
void OofPositionedNode<LogicalOffset, LogicalStaticPosition>::Trace(
    Visitor* visitor) const {
  // Some poor-man's RTTI here, which will go away with FragmentedOofInCb, at
  // which point TraceAfterDispatch() can be merged into Trace().
  if (is_for_fragmentation_) {
    static_cast<const LogicalOofNodeForFragmentation*>(this)
        ->TraceAfterDispatch(visitor);
  } else {
    TraceAfterDispatch(visitor);
  }
}

PhysicalOofPositionedNode LogicalOofPositionedNodeToPhysical(
    const LogicalOofPositionedNode& logical,
    const WritingModeConverter& converter) {
  OofInlineContainer<PhysicalOffset> inline_container(
      logical.InlineContainer(),
      converter.ToPhysical(logical.InlineContainerInfo().RelativeOffset(),
                           PhysicalSize()));
  return PhysicalOofPositionedNode(
      logical.Node(), logical.GetBreakToken(),
      logical.StaticPosition().ConvertToPhysical(converter),
      logical.RequiresContentBeforeBreaking(), inline_container);
}

LogicalOofPositionedNode PhysicalOofPositionedNodeToLogical(
    const PhysicalOofPositionedNode& physical,
    const WritingModeConverter& converter) {
  OofInlineContainer<LogicalOffset> inline_container(
      physical.InlineContainer(),
      converter.ToLogical(physical.InlineContainerInfo().RelativeOffset(),
                          PhysicalSize()));
  return LogicalOofPositionedNode(
      physical.Node(), physical.GetBreakToken(),
      physical.StaticPosition().ConvertToLogical(converter),
      physical.RequiresContentBeforeBreaking(), inline_container);
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

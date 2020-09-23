// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

namespace blink {

// A physical out-of-flow positioned-node is an element with the style
// "postion: absolute" or "position: fixed" which hasn't been bubbled up to its
// containing block yet, (e.g. an element with "position: relative"). As soon
// as a positioned-node reaches its containing block, it gets placed, and
// doesn't bubble further up the tree.
//
// However, when fragmentation comes into play, we no longer place a
// positioned-node as soon as it reaches its containing block. Instead, we
// continue to bubble the positioned node up until it reaches the
// fragmentation context root. There, it will get placed and properly
// fragmented.
//
// This needs its static position [1] to be placed correctly in its containing
// block. And in the case of fragmentation, this also needs the containing block
// fragment to be placed correctly within the fragmentation context root. In
// addition, the containing block offset is needed to compute the start offset
// and the initial fragmentainer of an out-of-flow positioned-node.
//
// This is struct is allowed to be stored/persisted.
//
// [1] https://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width
struct CORE_EXPORT NGPhysicalOutOfFlowPositionedNode {
  NGBlockNode node;
  NGPhysicalStaticPosition static_position;
  // Continuation root of the optional inline container.
  const LayoutInline* inline_container;
  PhysicalOffset containing_block_offset;
  scoped_refptr<const NGPhysicalContainerFragment> containing_block_fragment;

  NGPhysicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGPhysicalStaticPosition static_position,
      const LayoutInline* inline_container = nullptr,
      PhysicalOffset containing_block_offset = PhysicalOffset(),
      scoped_refptr<const NGPhysicalContainerFragment>
          containing_block_fragment = nullptr)
      : node(node),
        static_position(static_position),
        inline_container(inline_container),
        containing_block_offset(containing_block_offset),
        containing_block_fragment(std::move(containing_block_fragment)) {
    DCHECK(!inline_container ||
           inline_container == inline_container->ContinuationRoot());
  }
};

// The logical version of above. It is used within a an algorithm pass (within
// an |NGContainerFragmentBuilder|), and its logical coordinate system is wrt.
// the container builder's writing-mode.
//
// It is *only* used within an algorithm pass, (it is temporary, and should not
// be stored/persisted).
struct NGLogicalOutOfFlowPositionedNode {
  NGBlockNode node;
  NGLogicalStaticPosition static_position;
  // Continuation root of the optional inline container.
  const LayoutInline* inline_container;
  bool needs_block_offset_adjustment;
  LogicalOffset containing_block_offset;
  scoped_refptr<const NGPhysicalContainerFragment> containing_block_fragment;

  NGLogicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGLogicalStaticPosition static_position,
      const LayoutInline* inline_container = nullptr,
      bool needs_block_offset_adjustment = false,
      LogicalOffset containing_block_offset = LogicalOffset(),
      scoped_refptr<const NGPhysicalContainerFragment>
          containing_block_fragment = nullptr)
      : node(node),
        static_position(static_position),
        inline_container(inline_container),
        needs_block_offset_adjustment(needs_block_offset_adjustment),
        containing_block_offset(containing_block_offset),
        containing_block_fragment(std::move(containing_block_fragment)) {
    DCHECK(!inline_container ||
           inline_container == inline_container->ContinuationRoot());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

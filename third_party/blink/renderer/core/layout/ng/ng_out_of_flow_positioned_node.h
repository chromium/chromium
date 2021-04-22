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

// If an out-of-flow positioned element is inside a fragmentation context, it
// will be laid out once it reaches the fragmentation context root rather than
// once it reaches its containing block. A containing block holds the
// containing block information needed to place these OOF positioned nodes once
// they reach the fragmentation context root. See
// NGPhysicalOutOfFlowPositionedNode/NGLogicalOutOfFlowPositionedNode for more
// details.
template <typename OffsetType>
struct NGContainingBlock {
  DISALLOW_NEW();

 public:
  OffsetType offset;
  Member<const NGPhysicalFragment> fragment;

  NGContainingBlock() : fragment(nullptr) {}

  NGContainingBlock(OffsetType offset, const NGPhysicalFragment* fragment)
      : offset(offset), fragment(std::move(fragment)) {}

  void Trace(Visitor* visitor) const { visitor->Trace(fragment); }
};

// If an out-of-flow positioned element is inside a nested fragmentation
// context, it will be laid out once it reaches the outermost fragmentation
// context root. A multicol with pending OOFs is the inner multicol information
// needed to perform layout on the OOF descendants once they make their way to
// the outermost context.
template <typename OffsetType>
struct NGMulticolWithPendingOOFs
    : public GarbageCollected<NGMulticolWithPendingOOFs<OffsetType>> {
 public:
  // If no fixedpos containing block was found, |multicol_offset| will be
  // relative to the outer fragmentation context root. Otherwise, it will be
  // relative to the fixedpos containing block.
  OffsetType multicol_offset;
  // If an OOF node in a nested fragmentation context has fixedpos descendants,
  // those descendants will not find their containing block if the containing
  // block lives inside an outer fragmentation context. Thus, we also need to
  // store information on the containing block for any fixedpos descendants, if
  // one exists.
  NGContainingBlock<OffsetType> fixedpos_containing_block;

  NGMulticolWithPendingOOFs() = default;
  NGMulticolWithPendingOOFs(
      OffsetType multicol_offset,
      NGContainingBlock<OffsetType> fixedpos_containing_block)
      : multicol_offset(multicol_offset),
        fixedpos_containing_block(fixedpos_containing_block) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(fixedpos_containing_block);
  }
};

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
// If an OOF node in a fragmentation context has fixedpos descendants, those
// descendants will not find their containing block if the containing block
// lives inside the fragmentation context root. Thus, we also need to store
// information on the containing block for any fixedpos descendants, if one
// exists.
//
// This is struct is allowed to be stored/persisted.
//
// [1] https://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width
struct CORE_EXPORT NGPhysicalOutOfFlowPositionedNode final {
  DISALLOW_NEW();

 public:
  NGBlockNode node;
  NGPhysicalStaticPosition static_position;
  // Continuation root of the optional inline container.
  Member<const LayoutInline> inline_container;
  NGContainingBlock<PhysicalOffset> containing_block;
  NGContainingBlock<PhysicalOffset> fixedpos_containing_block;

  NGPhysicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGPhysicalStaticPosition static_position,
      const LayoutInline* inline_container = nullptr,
      NGContainingBlock<PhysicalOffset> containing_block =
          NGContainingBlock<PhysicalOffset>(),
      NGContainingBlock<PhysicalOffset> fixedpos_containing_block =
          NGContainingBlock<PhysicalOffset>())
      : node(node),
        static_position(static_position),
        inline_container(inline_container),
        containing_block(containing_block),
        fixedpos_containing_block(fixedpos_containing_block) {
    DCHECK(!inline_container ||
           inline_container == inline_container->ContinuationRoot());
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(node);
    visitor->Trace(inline_container);
    visitor->Trace(containing_block);
    visitor->Trace(fixedpos_containing_block);
  }
};

// The logical version of above. It is used within a an algorithm pass (within
// an |NGContainerFragmentBuilder|), and its logical coordinate system is wrt.
// the container builder's writing-mode.
//
// It is *only* used within an algorithm pass, (it is temporary, and should not
// be stored/persisted).
struct NGLogicalOutOfFlowPositionedNode final {
  DISALLOW_NEW();

 public:
  NGBlockNode node;
  NGLogicalStaticPosition static_position;
  // Continuation root of the optional inline container.
  Member<const LayoutInline> inline_container;
  bool needs_block_offset_adjustment;
  const LayoutUnit fragmentainer_consumed_block_size;
  NGContainingBlock<LogicalOffset> containing_block;
  NGContainingBlock<LogicalOffset> fixedpos_containing_block;
  base::Optional<LogicalRect> containing_block_rect;

  NGLogicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGLogicalStaticPosition static_position,
      const LayoutInline* inline_container = nullptr,
      bool needs_block_offset_adjustment = false,
      NGContainingBlock<LogicalOffset> containing_block =
          NGContainingBlock<LogicalOffset>(),
      NGContainingBlock<LogicalOffset> fixedpos_containing_block =
          NGContainingBlock<LogicalOffset>(),
      const base::Optional<LogicalRect> containing_block_rect = base::nullopt)
      : node(node),
        static_position(static_position),
        inline_container(inline_container),
        needs_block_offset_adjustment(needs_block_offset_adjustment),
        containing_block(containing_block),
        fixedpos_containing_block(fixedpos_containing_block),
        containing_block_rect(containing_block_rect) {
    DCHECK(!inline_container ||
           inline_container == inline_container->ContinuationRoot());
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(node);
    visitor->Trace(inline_container);
    visitor->Trace(containing_block);
    visitor->Trace(fixedpos_containing_block);
  }
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGPhysicalOutOfFlowPositionedNode)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGLogicalOutOfFlowPositionedNode)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

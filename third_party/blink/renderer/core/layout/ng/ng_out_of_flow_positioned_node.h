// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

namespace blink {

// If an out-of-flow positioned element is inside a fragmentation context, it
// will be laid out once it reaches the fragmentation context root rather than
// once it reaches its containing block. A containing block holds the
// containing block information needed to place these OOF positioned nodes once
// they reach the fragmentation context root. See
// NGPhysicalOOFNodeForFragmentation/NGLogicalOutOfFlowPositionedNode for more
// details.
template <typename OffsetType>
struct NGContainingBlock {
  DISALLOW_NEW();

 public:
  OffsetType offset;
  // The relative offset is stored separately to ensure that it is applied after
  // fragmentation: https://www.w3.org/TR/css-break-3/#transforms.
  OffsetType relative_offset;
  scoped_refptr<const NGPhysicalFragment> fragment;

  NGContainingBlock() : fragment(nullptr) {}

  NGContainingBlock(OffsetType offset,
                    OffsetType relative_offset,
                    scoped_refptr<const NGPhysicalFragment> fragment)
      : offset(offset),
        relative_offset(relative_offset),
        fragment(std::move(fragment)) {}
};

// This holds the containing block for an out-of-flow positioned element
// if the containing block is a non-atomic inline. It is the continuation
// root (i.e. the first LayoutInline in the continuation chain for the same
// node) if continuations are involved.
template <typename OffsetType>
struct NGInlineContainer {
  DISALLOW_NEW();

 public:
  const LayoutInline* container = nullptr;
  // Store the relative offset so that it can be applied after fragmentation,
  // if inside a fragmentation context.
  OffsetType relative_offset;

  NGInlineContainer() = default;
  NGInlineContainer(const LayoutInline* container, OffsetType relative_offset)
      : container(container), relative_offset(relative_offset) {}
};

// If an out-of-flow positioned element is inside a nested fragmentation
// context, it will be laid out once it reaches the outermost fragmentation
// context root. A multicol with pending OOFs is the inner multicol information
// needed to perform layout on the OOF descendants once they make their way to
// the outermost context.
template <typename OffsetType>
struct NGMulticolWithPendingOOFs {
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
};

// A physical out-of-flow positioned-node is an element with the style
// "postion: absolute" or "position: fixed" which hasn't been bubbled up to its
// containing block yet, (e.g. an element with "position: relative"). As soon
// as a positioned-node reaches its containing block, it gets placed, and
// doesn't bubble further up the tree.
//
// This needs its static position [1] to be placed correctly in its containing
// block.
//
// This is struct is allowed to be stored/persisted.
//
// [1] https://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width
struct CORE_EXPORT NGPhysicalOutOfFlowPositionedNode {
  DISALLOW_NEW();

  using HorizontalEdge = NGPhysicalStaticPosition::HorizontalEdge;
  using VerticalEdge = NGPhysicalStaticPosition::VerticalEdge;

 public:
  LayoutBox* box;
  // Unpacked NGPhysicalStaticPosition.
  PhysicalOffset static_position;
  unsigned static_position_horizontal_edge : 2;
  unsigned static_position_vertical_edge : 2;
  // Whether or not this is an NGPhysicalOOFNodeForFragmentation.
  unsigned is_for_fragmentation : 1;
  NGInlineContainer<PhysicalOffset> inline_container;

  NGPhysicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGPhysicalStaticPosition static_position,
      NGInlineContainer<PhysicalOffset> inline_container =
          NGInlineContainer<PhysicalOffset>())
      : box(node.GetLayoutBox()),
        static_position(static_position.offset),
        static_position_horizontal_edge(static_position.horizontal_edge),
        static_position_vertical_edge(static_position.vertical_edge),
        is_for_fragmentation(false),
        inline_container(inline_container) {
    DCHECK(!inline_container.container ||
           inline_container.container ==
               inline_container.container->ContinuationRoot());
    DCHECK(node.IsBlock());
  }

  NGBlockNode Node() const { return NGBlockNode(box); }
  HorizontalEdge GetStaticPositionHorizontalEdge() const {
    return static_cast<HorizontalEdge>(static_position_horizontal_edge);
  }
  VerticalEdge GetStaticPositionVerticalEdge() const {
    return static_cast<VerticalEdge>(static_position_vertical_edge);
  }
  NGPhysicalStaticPosition StaticPosition() const {
    return {static_position, GetStaticPositionHorizontalEdge(),
            GetStaticPositionVerticalEdge()};
  }
};

// When fragmentation comes into play, we no longer place a positioned-node as
// soon as it reaches its containing block. Instead, we continue to bubble the
// positioned node up until it reaches the fragmentation context root. There, it
// will get placed and properly fragmented.
//
// In addition to the static position, we also needs the containing block
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
struct CORE_EXPORT NGPhysicalOOFNodeForFragmentation final
    : public NGPhysicalOutOfFlowPositionedNode {
  DISALLOW_NEW();

 public:
  NGContainingBlock<PhysicalOffset> containing_block;
  NGContainingBlock<PhysicalOffset> fixedpos_containing_block;

  NGPhysicalOOFNodeForFragmentation(
      NGBlockNode node,
      NGPhysicalStaticPosition static_position,
      NGInlineContainer<PhysicalOffset> inline_container =
          NGInlineContainer<PhysicalOffset>(),
      NGContainingBlock<PhysicalOffset> containing_block =
          NGContainingBlock<PhysicalOffset>(),
      NGContainingBlock<PhysicalOffset> fixedpos_containing_block =
          NGContainingBlock<PhysicalOffset>())
      : NGPhysicalOutOfFlowPositionedNode(node,
                                          static_position,
                                          inline_container),
        containing_block(containing_block),
        fixedpos_containing_block(fixedpos_containing_block) {
    is_for_fragmentation = true;
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
  LayoutBox* box;
  NGLogicalStaticPosition static_position;
  NGInlineContainer<LogicalOffset> inline_container;
  bool needs_block_offset_adjustment;
  const LayoutUnit fragmentainer_consumed_block_size;
  NGContainingBlock<LogicalOffset> containing_block;
  NGContainingBlock<LogicalOffset> fixedpos_containing_block;

  NGLogicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGLogicalStaticPosition static_position,
      NGInlineContainer<LogicalOffset> inline_container =
          NGInlineContainer<LogicalOffset>(),
      bool needs_block_offset_adjustment = false,
      NGContainingBlock<LogicalOffset> containing_block =
          NGContainingBlock<LogicalOffset>(),
      NGContainingBlock<LogicalOffset> fixedpos_containing_block =
          NGContainingBlock<LogicalOffset>())
      : box(node.GetLayoutBox()),
        static_position(static_position),
        inline_container(inline_container),
        needs_block_offset_adjustment(needs_block_offset_adjustment),
        containing_block(containing_block),
        fixedpos_containing_block(fixedpos_containing_block) {
    DCHECK(!inline_container.container ||
           inline_container.container ==
               inline_container.container->ContinuationRoot());
    DCHECK(node.IsBlock());
  }

  NGBlockNode Node() const { return NGBlockNode(box); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

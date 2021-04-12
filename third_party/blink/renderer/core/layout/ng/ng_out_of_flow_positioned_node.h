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
// once it reaches its containing block. A physical containing block holds the
// containing block information needed to place these OOF positioned nodes once
// they reach the fragmentation context root. See
// NGPhysicalOutOfFlowPositionedNode for more details.
// TODO(almaher): Update NGPhysicalContainingBlock and NGLogicalContainingBlock
// to a single templated class if no other member variables are added.
struct NGPhysicalContainingBlock {
  DISALLOW_NEW();

 public:
  PhysicalOffset offset;
  Member<const NGPhysicalContainerFragment> fragment;

  NGPhysicalContainingBlock() : fragment(nullptr) {}

  NGPhysicalContainingBlock(PhysicalOffset offset,
                            const NGPhysicalContainerFragment* fragment)
      : offset(offset), fragment(std::move(fragment)) {}

  void Trace(Visitor* visitor) const { visitor->Trace(fragment); }
};

// The logical version of above. See NGLogicalOutOfFlowPositionedNode for more
// details.
struct NGLogicalContainingBlock {
  DISALLOW_NEW();

 public:
  LogicalOffset offset;
  Member<const NGPhysicalContainerFragment> fragment;

  NGLogicalContainingBlock() : fragment(nullptr) {}

  NGLogicalContainingBlock(LogicalOffset offset,
                           const NGPhysicalContainerFragment* fragment)
      : offset(offset), fragment(std::move(fragment)) {}

  void Trace(Visitor* visitor) const { visitor->Trace(fragment); }
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
  NGPhysicalContainingBlock containing_block;
  NGPhysicalContainingBlock fixedpos_containing_block;

  NGPhysicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGPhysicalStaticPosition static_position,
      const LayoutInline* inline_container = nullptr,
      NGPhysicalContainingBlock containing_block = NGPhysicalContainingBlock(),
      NGPhysicalContainingBlock fixedpos_containing_block =
          NGPhysicalContainingBlock())
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
  NGLogicalContainingBlock containing_block;
  NGLogicalContainingBlock fixedpos_containing_block;
  base::Optional<LogicalRect> containing_block_rect;

  NGLogicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGLogicalStaticPosition static_position,
      const LayoutInline* inline_container = nullptr,
      bool needs_block_offset_adjustment = false,
      NGLogicalContainingBlock containing_block = NGLogicalContainingBlock(),
      NGLogicalContainingBlock fixedpos_containing_block =
          NGLogicalContainingBlock(),
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
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGPhysicalContainingBlock)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGLogicalContainingBlock)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

// If an out-of-flow positioned element is inside a fragmentation context, it
// will be laid out once it reaches the fragmentation context root rather than
// once it reaches its containing block. A containing block holds the
// containing block information needed to place these OOF positioned nodes once
// they reach the fragmentation context root. See
// NGPhysicalOOFNodeForFragmentation/NGLogicalOOFNodeForFragmentation for more
// details.
template <typename OffsetType>
class NGContainingBlock {
  DISALLOW_NEW();

 public:
  NGContainingBlock() = default;

  NGContainingBlock(OffsetType offset,
                    OffsetType relative_offset,
                    const NGPhysicalFragment* fragment,
                    absl::optional<LayoutUnit> clipped_container_block_offset,
                    bool is_inside_column_spanner,
                    bool requires_content_before_breaking)
      : offset_(offset),
        relative_offset_(relative_offset),
        fragment_(std::move(fragment)),
        clipped_container_block_offset_(
            clipped_container_block_offset.value_or(LayoutUnit::Min())),
        is_inside_column_spanner_(is_inside_column_spanner),
        requires_content_before_breaking_(requires_content_before_breaking) {}

  OffsetType Offset() const { return offset_; }
  void IncreaseBlockOffset(LayoutUnit block_offset) {
    offset_.block_offset += block_offset;
  }
  OffsetType RelativeOffset() const { return relative_offset_; }
  const NGPhysicalFragment* Fragment() const { return fragment_; }
  absl::optional<LayoutUnit> ClippedContainerBlockOffset() const {
    if (clipped_container_block_offset_ == LayoutUnit::Min()) {
      return absl::nullopt;
    }
    return clipped_container_block_offset_;
  }
  bool IsInsideColumnSpanner() const { return is_inside_column_spanner_; }

  void SetRequiresContentBeforeBreaking(bool b) {
    requires_content_before_breaking_ = b;
  }
  bool RequiresContentBeforeBreaking() const {
    return requires_content_before_breaking_;
  }

  // True if the containing block of an OOF is inside a clipped container inside
  // a fragmentation context.
  // For example: <multicol><clipped-overflow-container><relpos><abspos>
  bool IsFragmentedInsideClippedContainer() const {
    return clipped_container_block_offset_ != LayoutUnit::Min();
  }

  void Trace(Visitor* visitor) const { visitor->Trace(fragment_); }

 private:
  OffsetType offset_;
  // The relative offset is stored separately to ensure that it is applied after
  // fragmentation: https://www.w3.org/TR/css-break-3/#transforms.
  OffsetType relative_offset_;
  Member<const NGPhysicalFragment> fragment_;
  // The distance to the innermost container that clips block overflow, if any.
  LayoutUnit clipped_container_block_offset_ = LayoutUnit::Min();
  // True if there is a column spanner between the containing block and the
  // multicol container (or if the containing block is a column spanner).
  bool is_inside_column_spanner_ = false;
  // True if we need to keep some child content in the current fragmentainer
  // before breaking (even that overflows the fragmentainer). See
  // NGBoxFragmentBuilder::SetRequiresContentBeforeBreaking() for more details.
  bool requires_content_before_breaking_ = false;
};

// This holds the containing block for an out-of-flow positioned element
// if the containing block is a non-atomic inline. It is the continuation
// root (i.e. the first LayoutInline in the continuation chain for the same
// node) if continuations are involved.
template <typename OffsetType>
struct NGInlineContainer {
  DISALLOW_NEW();

 public:
  Member<const LayoutInline> container;
  // Store the relative offset so that it can be applied after fragmentation,
  // if inside a fragmentation context.
  OffsetType relative_offset;

  NGInlineContainer() = default;
  NGInlineContainer(const LayoutInline* container, OffsetType relative_offset)
      : container(container), relative_offset(relative_offset) {}

  void Trace(Visitor* visitor) const { visitor->Trace(container); }
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
  // store information on the containing block and inline container for any
  // fixedpos descendants, if one exists.
  NGContainingBlock<OffsetType> fixedpos_containing_block;
  NGInlineContainer<OffsetType> fixedpos_inline_container;

  NGMulticolWithPendingOOFs() = default;
  NGMulticolWithPendingOOFs(
      OffsetType multicol_offset,
      NGContainingBlock<OffsetType> fixedpos_containing_block,
      NGInlineContainer<OffsetType> fixedpos_inline_container)
      : multicol_offset(multicol_offset),
        fixedpos_containing_block(fixedpos_containing_block),
        fixedpos_inline_container(fixedpos_inline_container) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(fixedpos_containing_block);
    visitor->Trace(fixedpos_inline_container);
  }
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
  Member<LayoutBox> box;
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

  void Trace(Visitor* visitor) const;
  void TraceAfterDispatch(Visitor*) const;
};

// The logical version of above. It is used within a an algorithm pass (within
// an |NGFragmentBuilder|), and its logical coordinate system is wrt.
// the container builder's writing-mode.
//
// It is *only* used within an algorithm pass, (it is temporary, and should not
// be stored/persisted).
struct CORE_EXPORT NGLogicalOutOfFlowPositionedNode {
  DISALLOW_NEW();

 public:
  Member<LayoutBox> box;
  NGLogicalStaticPosition static_position;
  NGInlineContainer<LogicalOffset> inline_container;
  // Whether or not this is an NGLogicalOOFNodeForFragmentation.
  unsigned is_for_fragmentation : 1;

  NGLogicalOutOfFlowPositionedNode(
      NGBlockNode node,
      NGLogicalStaticPosition static_position,
      NGInlineContainer<LogicalOffset> inline_container =
          NGInlineContainer<LogicalOffset>())
      : box(node.GetLayoutBox()),
        static_position(static_position),
        inline_container(inline_container),
        is_for_fragmentation(false) {
    DCHECK(node.IsBlock());
  }

  NGBlockNode Node() const { return NGBlockNode(box); }

  void Trace(Visitor* visitor) const;
  void TraceAfterDispatch(Visitor*) const;
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
// information on the containing block and inline container for any fixedpos
// descendants, if one exists.
//
// This is struct is allowed to be stored/persisted.
struct CORE_EXPORT NGPhysicalOOFNodeForFragmentation final
    : public NGPhysicalOutOfFlowPositionedNode {
  DISALLOW_NEW();

 public:
  NGContainingBlock<PhysicalOffset> containing_block;
  NGContainingBlock<PhysicalOffset> fixedpos_containing_block;
  NGInlineContainer<PhysicalOffset> fixedpos_inline_container;

  NGPhysicalOOFNodeForFragmentation(
      NGBlockNode node,
      NGPhysicalStaticPosition static_position,
      NGInlineContainer<PhysicalOffset> inline_container =
          NGInlineContainer<PhysicalOffset>(),
      NGContainingBlock<PhysicalOffset> containing_block =
          NGContainingBlock<PhysicalOffset>(),
      NGContainingBlock<PhysicalOffset> fixedpos_containing_block =
          NGContainingBlock<PhysicalOffset>(),
      NGInlineContainer<PhysicalOffset> fixedpos_inline_container =
          NGInlineContainer<PhysicalOffset>())
      : NGPhysicalOutOfFlowPositionedNode(node,
                                          static_position,
                                          inline_container),
        containing_block(containing_block),
        fixedpos_containing_block(fixedpos_containing_block),
        fixedpos_inline_container(fixedpos_inline_container) {
    is_for_fragmentation = true;
  }

  void TraceAfterDispatch(Visitor* visitor) const;
};

// The logical version of the above. It is used within a an algorithm pass
// (within an |NGFragmentBuilder|), and its logical coordinate system
// is wrt. the container builder's writing-mode.
//
// It is *only* used within an algorithm pass, (it is temporary, and should not
// be stored/persisted).
struct CORE_EXPORT NGLogicalOOFNodeForFragmentation final
    : public NGLogicalOutOfFlowPositionedNode {
  DISALLOW_NEW();

 public:
  NGContainingBlock<LogicalOffset> containing_block;
  NGContainingBlock<LogicalOffset> fixedpos_containing_block;
  NGInlineContainer<LogicalOffset> fixedpos_inline_container;

  NGLogicalOOFNodeForFragmentation(
      NGBlockNode node,
      NGLogicalStaticPosition static_position,
      NGInlineContainer<LogicalOffset> inline_container =
          NGInlineContainer<LogicalOffset>(),
      NGContainingBlock<LogicalOffset> containing_block =
          NGContainingBlock<LogicalOffset>(),
      NGContainingBlock<LogicalOffset> fixedpos_containing_block =
          NGContainingBlock<LogicalOffset>(),
      NGInlineContainer<LogicalOffset> fixedpos_inline_container =
          NGInlineContainer<LogicalOffset>())
      : NGLogicalOutOfFlowPositionedNode(node,
                                         static_position,
                                         inline_container),
        containing_block(containing_block),
        fixedpos_containing_block(fixedpos_containing_block),
        fixedpos_inline_container(fixedpos_inline_container) {
    is_for_fragmentation = true;
  }

  explicit NGLogicalOOFNodeForFragmentation(
      const NGLogicalOutOfFlowPositionedNode& oof_node)
      : NGLogicalOutOfFlowPositionedNode(oof_node.Node(),
                                         oof_node.static_position,
                                         oof_node.inline_container) {
    is_for_fragmentation = true;
  }

  const LayoutObject* CssContainingBlock() const { return box->Container(); }

  void TraceAfterDispatch(Visitor* visitor) const;
};

template <>
struct DowncastTraits<NGLogicalOOFNodeForFragmentation> {
  static bool AllowFrom(const NGLogicalOutOfFlowPositionedNode& oof_node) {
    return oof_node.is_for_fragmentation;
  }
};

// This is a sub class of |NGPhysicalFragment::OutOfFlowData| that can store OOF
// propagation data under the NG block fragmentation context.
//
// This class is defined here instead of |NGPhysicalFragment| because types
// needed for this class requires full definition of |NGPhysicalFragment|, and
// |NGPhysicalFragment| requires full definition of this class if this is put
// into |NGPhysicalFragment|.
struct NGFragmentedOutOfFlowData final : NGPhysicalFragment::OutOfFlowData {
  using MulticolCollection =
      HeapHashMap<Member<LayoutBox>,
                  Member<NGMulticolWithPendingOOFs<PhysicalOffset>>>;

  static bool HasOutOfFlowPositionedFragmentainerDescendants(
      const NGPhysicalFragment& fragment) {
    const NGFragmentedOutOfFlowData* oof_data =
        fragment.FragmentedOutOfFlowData();
    return oof_data &&
           !oof_data->oof_positioned_fragmentainer_descendants.empty();
  }

  bool NeedsOOFPositionedInfoPropagation() const {
    return !oof_positioned_fragmentainer_descendants.empty() ||
           !multicols_with_pending_oofs.empty();
  }

  static base::span<NGPhysicalOOFNodeForFragmentation>
  OutOfFlowPositionedFragmentainerDescendants(
      const NGPhysicalFragment& fragment) {
    const NGFragmentedOutOfFlowData* oof_data =
        fragment.FragmentedOutOfFlowData();
    if (!oof_data || oof_data->oof_positioned_fragmentainer_descendants.empty())
      return base::span<NGPhysicalOOFNodeForFragmentation>();
    HeapVector<NGPhysicalOOFNodeForFragmentation>& descendants =
        const_cast<HeapVector<NGPhysicalOOFNodeForFragmentation>&>(
            oof_data->oof_positioned_fragmentainer_descendants);
    return {descendants.data(), descendants.size()};
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(oof_positioned_fragmentainer_descendants);
    visitor->Trace(multicols_with_pending_oofs);
    NGPhysicalFragment::OutOfFlowData::Trace(visitor);
  }

  HeapVector<NGPhysicalOOFNodeForFragmentation>
      oof_positioned_fragmentainer_descendants;
  MulticolCollection multicols_with_pending_oofs;
};

inline PhysicalOffset RelativeInsetToPhysical(
    LogicalOffset relative_inset,
    WritingDirectionMode writing_direction) {
  return relative_inset.ConvertToPhysical(writing_direction, PhysicalSize(),
                                          PhysicalSize());
}

inline LogicalOffset RelativeInsetToLogical(
    PhysicalOffset relative_inset,
    WritingDirectionMode writing_direction) {
  return relative_inset.ConvertToLogical(writing_direction, PhysicalSize(),
                                         PhysicalSize());
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGPhysicalOutOfFlowPositionedNode)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGPhysicalOOFNodeForFragmentation)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGLogicalOutOfFlowPositionedNode)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGLogicalOOFNodeForFragmentation)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_POSITIONED_NODE_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OOF_POSITIONED_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OOF_POSITIONED_NODE_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/geometry/static_position.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class BlockBreakToken;

// If an out-of-flow positioned element is inside a fragmentation context, it
// will be laid out once it reaches the fragmentation context root rather than
// once it reaches its containing block. A containing block holds the
// containing block information needed to place these OOF positioned nodes once
// they reach the fragmentation context root. See
// PhysicalOofNodeForFragmentation/LogicalOofNodeForFragmentation for more
// details.
template <typename OffsetType>
class OofContainingBlock {
  DISALLOW_NEW();

 public:
  OofContainingBlock() = default;

  OofContainingBlock(OffsetType offset,
                     OffsetType relative_offset,
                     const PhysicalFragment* fragment,
                     std::optional<LayoutUnit> clipped_container_block_offset,
                     bool is_inside_column_spanner)
      : offset_(offset),
        relative_offset_(relative_offset),
        fragment_(std::move(fragment)),
        clipped_container_block_offset_(
            clipped_container_block_offset.value_or(LayoutUnit::Min())),
        is_inside_column_spanner_(is_inside_column_spanner) {}

  OffsetType Offset() const { return offset_; }
  void IncreaseBlockOffset(LayoutUnit block_offset) {
    offset_.block_offset += block_offset;
  }
  void IncreaseInlineOffset(LayoutUnit inline_offset) {
    offset_.inline_offset += inline_offset;
  }
  OffsetType RelativeOffset() const { return relative_offset_; }
  const PhysicalFragment* Fragment() const { return fragment_.Get(); }
  std::optional<LayoutUnit> ClippedContainerBlockOffset() const {
    if (clipped_container_block_offset_ == LayoutUnit::Min()) {
      return std::nullopt;
    }
    return clipped_container_block_offset_;
  }
  bool IsInsideColumnSpanner() const { return is_inside_column_spanner_; }

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
  Member<const PhysicalFragment> fragment_;
  // The distance to the innermost container that clips block overflow, if any.
  LayoutUnit clipped_container_block_offset_ = LayoutUnit::Min();
  // True if there is a column spanner between the containing block and the
  // multicol container (or if the containing block is a column spanner).
  bool is_inside_column_spanner_ = false;
};

// This holds the containing block for an out-of-flow positioned element
// if the containing block is a non-atomic inline. It is the continuation
// root (i.e. the first LayoutInline in the continuation chain for the same
// node) if continuations are involved.
template <typename OffsetType>
class OofInlineContainer {
  DISALLOW_NEW();

 public:
  OofInlineContainer() = default;
  OofInlineContainer(const LayoutInline* container, OffsetType relative_offset)
      : container_(container), relative_offset_(relative_offset) {}

  void Trace(Visitor* visitor) const { visitor->Trace(container_); }

  const LayoutInline* Container() const { return container_; }
  OffsetType RelativeOffset() const {
    // This field is only used by the old OOF fragmentation machinery.
    if (RuntimeEnabledFeatures::FragmentedOofInCbEnabled()) {
      return OffsetType();
    }
    return relative_offset_;
  }
  void IncreaseRelativeOffset(OffsetType increase) {
    DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
    relative_offset_ += increase;
  }

 private:
  Member<const LayoutInline> container_;

  // Store the relative offset so that it can be applied after fragmentation,
  // if inside a fragmentation context.
  OffsetType relative_offset_;
};

// If an out-of-flow positioned element is inside a nested fragmentation
// context, it will be laid out once it reaches the outermost fragmentation
// context root. A multicol with pending OOFs is the inner multicol information
// needed to perform layout on the OOF descendants once they make their way to
// the outermost context.
template <typename OffsetType>
struct MulticolWithPendingOofs
    : public GarbageCollected<MulticolWithPendingOofs<OffsetType>> {
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
  OofContainingBlock<OffsetType> fixedpos_containing_block;
  OofInlineContainer<OffsetType> fixedpos_inline_container;

  MulticolWithPendingOofs() = default;
  MulticolWithPendingOofs(
      OffsetType multicol_offset,
      OofContainingBlock<OffsetType> fixedpos_containing_block,
      OofInlineContainer<OffsetType> fixedpos_inline_container)
      : multicol_offset(multicol_offset),
        fixedpos_containing_block(fixedpos_containing_block),
        fixedpos_inline_container(fixedpos_inline_container) {
    DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(fixedpos_containing_block);
    visitor->Trace(fixedpos_inline_container);
  }
};

// An out-of-flow positioned node which hasn't bubbled up to the container where
// it is to be handled yet. This container is typically its containing block,
// but for block fragmentation it may have to bubble all the way to the
// outermost fragmentation context root.
//
// The hypothetical static position [1] is needed for it to be placed correctly
// in its containing block.
//
// It is templatized, as it needs both a logical (inside layout algorithms) and
// physical (stored/persisted in physical fragments for bubbling up the tree)
// representation.
//
// [1] https://www.w3.org/TR/CSS22/visudet.html#abs-non-replaced-width
template <typename OffsetType, typename StaticPositionType>
class CORE_EXPORT OofPositionedNode {
  DISALLOW_NEW();

 public:
  OofPositionedNode(const BlockNode& node,
                    const BlockBreakToken* break_token,
                    const StaticPositionType& static_position,
                    bool requires_content_before_breaking,
                    OofInlineContainer<OffsetType> inline_container = {})
      : box_(node.GetLayoutBox()),
        break_token_(break_token),
        static_position_(static_position),
        inline_container_(inline_container),
        requires_content_before_breaking_(requires_content_before_breaking) {
    DCHECK(node.IsBlock());
  }

  void Trace(Visitor* visitor) const;
  void TraceAfterDispatch(Visitor* visitor) const {
    visitor->Trace(box_);
    visitor->Trace(break_token_);
    visitor->Trace(inline_container_);
  }

  BlockNode Node() const { return BlockNode(box_); }
  const BlockBreakToken* GetBreakToken() const { return break_token_; }
  StaticPositionType StaticPosition() const { return static_position_; }
  const LayoutInline* InlineContainer() const {
    return inline_container_.Container();
  }
  OofInlineContainer<OffsetType>& InlineContainerInfo() {
    // Only needed by the old OOF fragmentation machinery, i.e. when
    // FragmentedOofInCb is off.
    return inline_container_;
  }
  const OofInlineContainer<OffsetType>& InlineContainerInfo() const {
    // Only needed by the old OOF fragmentation machinery, i.e. when
    // FragmentedOofInCb is off.
    return inline_container_;
  }
  bool IsForFragmentation() const {
    DCHECK(!is_for_fragmentation_ ||
           !RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
    return is_for_fragmentation_;
  }
  bool RequiresContentBeforeBreaking() const {
    return requires_content_before_breaking_;
  }

  void SetStaticPositionOffset(OffsetType offset) {
    static_position_.offset = offset;
  }
  void IncreaseStaticPositionOffset(OffsetType increase) {
    static_position_.offset += increase;
  }
  void SetInlineContainer(const LayoutInline* object) {
    inline_container_ = OofInlineContainer<OffsetType>(object, OffsetType());
  }

 protected:
  Member<LayoutBox> box_;
  Member<const BlockBreakToken> break_token_;
  StaticPositionType static_position_;
  OofInlineContainer<OffsetType> inline_container_;

  // Set if this is a PhysicalOofNodeForFragmentation or
  // LogicalOofNodeForFragmentation. This flag will go away with
  // FragmentedOofInCb.
  unsigned is_for_fragmentation_ : 1 = false;

  unsigned requires_content_before_breaking_ : 1;
};

using PhysicalOofPositionedNode =
    OofPositionedNode<PhysicalOffset, PhysicalStaticPosition>;
using LogicalOofPositionedNode =
    OofPositionedNode<LogicalOffset, LogicalStaticPosition>;

template <>
void OofPositionedNode<PhysicalOffset, PhysicalStaticPosition>::Trace(
    Visitor* visitor) const;
template <>
void OofPositionedNode<LogicalOffset, LogicalStaticPosition>::Trace(
    Visitor* visitor) const;

PhysicalOofPositionedNode LogicalOofPositionedNodeToPhysical(
    const LogicalOofPositionedNode&,
    const WritingModeConverter&);
LogicalOofPositionedNode PhysicalOofPositionedNodeToLogical(
    const PhysicalOofPositionedNode&,
    const WritingModeConverter&);

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
struct CORE_EXPORT PhysicalOofNodeForFragmentation final
    : public PhysicalOofPositionedNode {
  DISALLOW_NEW();

 public:
  OofContainingBlock<PhysicalOffset> containing_block;
  OofContainingBlock<PhysicalOffset> fixedpos_containing_block;
  OofInlineContainer<PhysicalOffset> fixedpos_inline_container;

  PhysicalOofNodeForFragmentation(
      BlockNode node,
      PhysicalStaticPosition static_position,
      bool requires_content_before_breaking,
      OofInlineContainer<PhysicalOffset> inline_container = {},
      OofContainingBlock<PhysicalOffset> containing_block = {},
      OofContainingBlock<PhysicalOffset> fixedpos_containing_block = {},
      OofInlineContainer<PhysicalOffset> fixedpos_inline_container = {})
      : PhysicalOofPositionedNode(node,
                                  /*break_token=*/nullptr,
                                  static_position,
                                  requires_content_before_breaking,
                                  inline_container),
        containing_block(containing_block),
        fixedpos_containing_block(fixedpos_containing_block),
        fixedpos_inline_container(fixedpos_inline_container) {
    DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
    is_for_fragmentation_ = true;
  }

  void TraceAfterDispatch(Visitor* visitor) const;
};

// The logical version of the above. It is used within a an algorithm pass
// (within an |FragmentBuilder|), and its logical coordinate system
// is wrt. the container builder's writing-mode.
//
// It is *only* used within an algorithm pass, (it is temporary, and should not
// be stored/persisted).
struct CORE_EXPORT LogicalOofNodeForFragmentation final
    : public LogicalOofPositionedNode {
  DISALLOW_NEW();

 public:
  OofContainingBlock<LogicalOffset> containing_block;
  OofContainingBlock<LogicalOffset> fixedpos_containing_block;
  OofInlineContainer<LogicalOffset> fixedpos_inline_container;

  LogicalOofNodeForFragmentation(
      BlockNode node,
      LogicalStaticPosition static_position,
      bool requires_content_before_breaking,
      OofInlineContainer<LogicalOffset> inline_container = {},
      OofContainingBlock<LogicalOffset> containing_block = {},
      OofContainingBlock<LogicalOffset> fixedpos_containing_block = {},
      OofInlineContainer<LogicalOffset> fixedpos_inline_container = {})
      : LogicalOofPositionedNode(node,
                                 /*break_token=*/nullptr,
                                 static_position,
                                 requires_content_before_breaking,
                                 inline_container),
        containing_block(containing_block),
        fixedpos_containing_block(fixedpos_containing_block),
        fixedpos_inline_container(fixedpos_inline_container) {
    DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
    is_for_fragmentation_ = true;
  }

  explicit LogicalOofNodeForFragmentation(
      const LogicalOofPositionedNode& oof_node)
      : LogicalOofPositionedNode(oof_node.Node(),
                                 /*break_token=*/nullptr,
                                 oof_node.StaticPosition(),
                                 oof_node.RequiresContentBeforeBreaking(),
                                 oof_node.InlineContainerInfo()) {
    DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
    is_for_fragmentation_ = true;
  }

  const LayoutObject* CssContainingBlock() const { return box_->Container(); }

  void TraceAfterDispatch(Visitor* visitor) const;
};

template <>
struct DowncastTraits<LogicalOofNodeForFragmentation> {
  static bool AllowFrom(const LogicalOofPositionedNode& oof_node) {
    return oof_node.IsForFragmentation();
  }
};

// This is a sub class of |PhysicalFragment::OofData| that can store OOF
// propagation data under the NG block fragmentation context.
//
// This class is defined here instead of |PhysicalFragment| because types
// needed for this class requires full definition of |PhysicalFragment|, and
// |PhysicalFragment| requires full definition of this class if this is put
// into |PhysicalFragment|.
struct FragmentedOofData final : PhysicalFragment::OofData {
  using MulticolCollection =
      HeapHashMap<Member<LayoutBox>,
                  Member<MulticolWithPendingOofs<PhysicalOffset>>>;

  FragmentedOofData() {
    DCHECK(!RuntimeEnabledFeatures::FragmentedOofInCbEnabled());
  }

  static bool HasOutOfFlowPositionedFragmentainerDescendants(
      const PhysicalFragment& fragment) {
    const auto* oof_data = fragment.GetFragmentedOofData();
    return oof_data &&
           !oof_data->oof_positioned_fragmentainer_descendants.empty();
  }

  bool NeedsOOFPositionedInfoPropagation() const {
    return !oof_positioned_fragmentainer_descendants.empty() ||
           !multicols_with_pending_oofs.empty();
  }

  static base::span<PhysicalOofNodeForFragmentation>
  OutOfFlowPositionedFragmentainerDescendants(
      const PhysicalFragment& fragment) {
    const auto* oof_data = fragment.GetFragmentedOofData();
    if (!oof_data || oof_data->oof_positioned_fragmentainer_descendants.empty())
      return base::span<PhysicalOofNodeForFragmentation>();
    HeapVector<PhysicalOofNodeForFragmentation>& descendants =
        const_cast<HeapVector<PhysicalOofNodeForFragmentation>&>(
            oof_data->oof_positioned_fragmentainer_descendants);
    return descendants;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(oof_positioned_fragmentainer_descendants);
    visitor->Trace(multicols_with_pending_oofs);
    PhysicalFragment::OofData::Trace(visitor);
  }

  HeapVector<PhysicalOofNodeForFragmentation>
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
  return WritingModeConverter(writing_direction, PhysicalSize())
      .ToLogical(relative_inset, PhysicalSize());
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::PhysicalOofPositionedNode)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::PhysicalOofNodeForFragmentation)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::LogicalOofPositionedNode)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::LogicalOofNodeForFragmentation)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OOF_POSITIONED_NODE_H_

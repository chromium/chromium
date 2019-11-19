// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_LAYOUT_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_LAYOUT_PART_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGBlockNode;
class NGBoxFragmentBuilder;
class NGConstraintSpace;
class NGLayoutResult;
struct NGLogicalOutOfFlowPositionedNode;

// Helper class for positioning of out-of-flow blocks.
// It should be used together with NGBoxFragmentBuilder.
// See NGBoxFragmentBuilder::AddOutOfFlowChildCandidate documentation
// for example of using these classes together.
class CORE_EXPORT NGOutOfFlowLayoutPart {
  STACK_ALLOCATED();

 public:
  NGOutOfFlowLayoutPart(const NGBlockNode& container_node,
                        const NGConstraintSpace& container_space,
                        const NGBoxStrut& border_scrollbar,
                        NGBoxFragmentBuilder* container_builder);

  // The |container_builder|, |border_scrollbar|, |container_space|, and
  // |container_style| parameters are all with respect to the containing block
  // of the relevant out-of-flow positioned descendants. If the CSS "containing
  // block" of such an out-of-flow positioned descendant isn't a true block
  // (e.g. a relatively positioned inline instead), the containing block here is
  // the containing block of said non-block.
  NGOutOfFlowLayoutPart(
      bool is_absolute_container,
      bool is_fixed_container,
      const ComputedStyle& container_style,
      const NGConstraintSpace& container_space,
      const NGBoxStrut& border_scrollbar,
      NGBoxFragmentBuilder* container_builder,
      base::Optional<LogicalSize> initial_containing_block_fixed_size =
          base::nullopt);

  // Normally this function lays out and positions all out-of-flow objects from
  // the container_builder and additional ones it discovers through laying out
  // those objects. However, if only_layout is specified, only that object will
  // get laid out; any additional ones will be stored as out-of-flow
  // descendants in the builder for use via
  // LayoutResult::OutOfFlowPositionedDescendants.
  void Run(const LayoutBox* only_layout = nullptr);

 private:
  // Information needed to position descendant within a containing block.
  // Geometry expressed here is complicated:
  // There are two types of containing blocks:
  // 1) Default containing block (DCB)
  //    Containing block passed in NGOutOfFlowLayoutPart constructor.
  //    It is the block element inside which this algorighm runs.
  //    All OOF descendants not in inline containing block are placed in DCB.
  // 2) Inline containing block
  //    OOF descendants might be positioned wrt inline containing block.
  //    Inline containing block is positioned wrt default containing block.
  struct ContainingBlockInfo {
    STACK_ALLOCATED();

   public:
    // The direction of the container.
    TextDirection direction;
    // Logical in containing block coordinates.
    LogicalSize content_size_for_absolute;
    // Content size for fixed is different for the ICB.
    LogicalSize content_size_for_fixed;

    // Offset of the container's padding-box.
    LogicalOffset container_offset;

    LogicalSize ContentSize(EPosition position) const {
      return position == EPosition::kAbsolute ? content_size_for_absolute
                                              : content_size_for_fixed;
    }
  };

  bool SweepLegacyCandidates(HashSet<const LayoutObject*>* placed_objects);

  const ContainingBlockInfo& GetContainingBlockInfo(
      const NGLogicalOutOfFlowPositionedNode&) const;

  void ComputeInlineContainingBlocks(
      const Vector<NGLogicalOutOfFlowPositionedNode>&);

  void LayoutCandidates(Vector<NGLogicalOutOfFlowPositionedNode>* candidates,
                        const LayoutBox* only_layout,
                        HashSet<const LayoutObject*>* placed_objects);

  scoped_refptr<const NGLayoutResult> LayoutCandidate(
      const NGLogicalOutOfFlowPositionedNode&,
      const LayoutBox* only_layout);

  scoped_refptr<const NGLayoutResult> Layout(NGBlockNode,
                                             const NGConstraintSpace&,
                                             const NGLogicalStaticPosition&,
                                             LogicalSize container_content_size,
                                             const ContainingBlockInfo&,
                                             const LayoutBox* only_layout);

  bool IsContainingBlockForCandidate(const NGLogicalOutOfFlowPositionedNode&);

  scoped_refptr<const NGLayoutResult> GenerateFragment(
      NGBlockNode node,
      const LogicalSize& container_content_size_in_child_writing_mode,
      const base::Optional<LayoutUnit>& block_estimate,
      const NGLogicalOutOfFlowPosition& node_position);

  const NGConstraintSpace& container_space_;
  NGBoxFragmentBuilder* container_builder_;
  ContainingBlockInfo default_containing_block_;
  HashMap<const LayoutObject*, ContainingBlockInfo> containing_blocks_map_;
  const WritingMode writing_mode_;
  bool is_absolute_container_;
  bool is_fixed_container_;
  bool allow_first_tier_oof_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_OUT_OF_FLOW_LAYOUT_PART_H_

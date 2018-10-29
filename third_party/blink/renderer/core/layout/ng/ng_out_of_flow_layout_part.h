// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOutOfFlowLayoutPart_h
#define NGOutOfFlowLayoutPart_h

#include "third_party/blink/renderer/core/core_export.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGBlockNode;
class NGBoxFragmentBuilder;
class NGConstraintSpace;
class NGLayoutResult;
struct NGOutOfFlowPositionedDescendant;

// Helper class for positioning of out-of-flow blocks.
// It should be used together with NGBoxFragmentBuilder.
// See NGBoxFragmentBuilder::AddOutOfFlowChildCandidate documentation
// for example of using these classes together.
class CORE_EXPORT NGOutOfFlowLayoutPart {
  STACK_ALLOCATED();

 public:
  // The container_builder, borders_and_scrollers, container_space and
  // container_style parameters are all with respect to the containing block of
  // the relevant out-of-flow positioned descendants. If the CSS "containing
  // block" of such an out-of-flow positioned descendant isn't a true block (but
  // e.g. a relatively positioned inline instead), the containing block here is
  // the containing block of said non-block.
  NGOutOfFlowLayoutPart(NGBoxFragmentBuilder* container_builder,
                        bool contains_absolute,
                        bool contains_fixed,
                        const NGBoxStrut& borders_and_scrollers,
                        const NGConstraintSpace& container_space,
                        const ComputedStyle& container_style);

  // Normally this function lays out and positions all out-of-flow objects
  // from the container_builder and additional ones it discovers through laying
  // out those objects. However, if only_layout is specified, only that object
  // will get laid out; any additional ones will be stored as out-of-flow
  // descendants in the builder for use via
  // LayoutResult::OutOfFlowPositionedDescendants.
  void Run(LayoutBox* only_layout = nullptr);

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
    // Containing block style.
    const ComputedStyle* style;
    // Logical in containing block coordinates.
    NGLogicalSize content_size;
    // Content offset wrt border box.
    NGLogicalOffset content_offset;
    // Physical content offset wrt border box.
    NGPhysicalOffset content_physical_offset;
    // Logical offset of container padding box
    // wrt default containing block padding box.
    NGLogicalOffset default_container_offset;
  };

  ContainingBlockInfo GetContainingBlockInfo(
      const NGOutOfFlowPositionedDescendant&) const;

  void ComputeInlineContainingBlocks(Vector<NGOutOfFlowPositionedDescendant>);

  scoped_refptr<NGLayoutResult> LayoutDescendant(
      const NGOutOfFlowPositionedDescendant&,
      NGLogicalOffset* offset);

  bool IsContainingBlockForDescendant(
      const NGOutOfFlowPositionedDescendant& descendant);

  scoped_refptr<NGLayoutResult> GenerateFragment(
      NGBlockNode node,
      const ContainingBlockInfo&,
      const base::Optional<LayoutUnit>& block_estimate,
      const NGAbsolutePhysicalPosition& node_position);

  NGBoxFragmentBuilder* container_builder_;
  bool contains_absolute_;
  bool contains_fixed_;
  NGPhysicalSize icb_size_;
  ContainingBlockInfo default_containing_block_;
  HashMap<const LayoutObject*, ContainingBlockInfo> containing_blocks_map_;
};

}  // namespace blink

#endif

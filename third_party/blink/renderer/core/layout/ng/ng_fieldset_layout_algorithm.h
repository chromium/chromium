// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

enum class NGBreakStatus;
class NGBlockBreakToken;
class NGConstraintSpace;

class CORE_EXPORT NGFieldsetLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGFieldsetLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(
      const MinMaxSizesFloatInput&) const override;

  static LayoutUnit ComputeLegendInlineOffset(
      const ComputedStyle& legend_style,
      LayoutUnit legend_border_box_inline_size,
      const NGBoxStrut& legend_margins,
      const ComputedStyle& fieldset_style,
      LayoutUnit fieldset_border_padding_inline_start,
      LayoutUnit fieldset_content_inline_size);

 private:
  NGBreakStatus LayoutChildren();
  void LayoutLegend(NGBlockNode& legend);
  NGBreakStatus LayoutFieldsetContent(
      NGBlockNode& fieldset_content,
      scoped_refptr<const NGBlockBreakToken> content_break_token,
      LogicalSize adjusted_padding_box_size,
      bool has_legend);

  const NGConstraintSpace CreateConstraintSpaceForLegend(
      NGBlockNode legend,
      LogicalSize available_size,
      LogicalSize percentage_size);
  const NGConstraintSpace CreateConstraintSpaceForFieldsetContent(
      NGBlockNode fieldset_content,
      LogicalSize padding_box_size,
      LayoutUnit block_offset,
      NGCacheSlot slot);
  bool IsFragmentainerOutOfSpace(LayoutUnit block_offset) const;

  const WritingDirectionMode writing_direction_;

  NGBoxStrut borders_;
  NGBoxStrut padding_;

  LayoutUnit intrinsic_block_size_;
  const LayoutUnit consumed_block_size_;
  LogicalSize border_box_size_;

  // The legend may eat from the available content box block size. This
  // represents the minimum block size needed by the border box to encompass
  // the legend.
  LayoutUnit minimum_border_box_block_size_;

  // If true, the legend is taller than the block-start border, so that it
  // sticks below it, allowing for a class C breakpoint [1] before any fieldset
  // content.
  //
  // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
  bool is_legend_past_border_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_LAYOUT_ALGORITHM_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"

#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

NGUnpositionedListMarker::NGUnpositionedListMarker(LayoutNGListMarker* marker)
    : marker_layout_object_(marker) {}

NGUnpositionedListMarker::NGUnpositionedListMarker(const NGBlockNode& node)
    : NGUnpositionedListMarker(ToLayoutNGListMarker(node.GetLayoutBox())) {}

// Returns true if this is an image marker.
bool NGUnpositionedListMarker::IsImage() const {
  DCHECK(marker_layout_object_);
  return marker_layout_object_->IsContentImage();
}

// Compute the inline offset of the marker, relative to the list item.
// The marker is relative to the border box of the list item and has nothing
// to do with the content offset.
// Open issue at https://github.com/w3c/csswg-drafts/issues/2361
LayoutUnit NGUnpositionedListMarker::InlineOffset(
    const LayoutUnit marker_inline_size) const {
  DCHECK(marker_layout_object_);
  auto margins = LayoutListMarker::InlineMarginsForOutside(
      marker_layout_object_->StyleRef(), IsImage(), marker_inline_size);
  return margins.first;
}

scoped_refptr<NGLayoutResult> NGUnpositionedListMarker::Layout(
    const NGConstraintSpace& space,
    FontBaseline baseline_type) const {
  DCHECK(marker_layout_object_);
  NGBlockNode marker_node(marker_layout_object_);
  scoped_refptr<NGLayoutResult> marker_layout_result =
      marker_node.LayoutAtomicInline(space, baseline_type,
                                     space.UseFirstLineStyle());
  DCHECK(marker_layout_result && marker_layout_result->PhysicalFragment());
  return marker_layout_result;
}

bool NGUnpositionedListMarker::AddToBox(
    const NGConstraintSpace& space,
    FontBaseline baseline_type,
    const NGPhysicalFragment& content,
    NGLogicalOffset* content_offset,
    NGBoxFragmentBuilder* container_builder,
    const NGBoxStrut& border_scrollbar_padding) const {
  // Baselines from two different writing-mode cannot be aligned.
  if (UNLIKELY(space.GetWritingMode() != content.Style().GetWritingMode()))
    return false;

  // Compute the baseline of the child content.
  NGLineHeightMetrics content_metrics;
  if (content.IsLineBox()) {
    const NGPhysicalLineBoxFragment& line_box =
        ToNGPhysicalLineBoxFragment(content);

    // If this child is an empty line-box, the list marker should be aligned
    // with the next non-empty line box produced. (This can occur with floats
    // producing empty line-boxes).
    if (line_box.Children().IsEmpty() && !line_box.BreakToken()->IsFinished())
      return false;

    content_metrics = line_box.Metrics();
  } else {
    NGBoxFragment content_fragment(space.GetWritingMode(), space.Direction(),
                                   ToNGPhysicalBoxFragment(content));
    content_metrics = content_fragment.BaselineMetricsWithoutSynthesize(
        {NGBaselineAlgorithmType::kFirstLine, baseline_type});

    // If this child content does not have any line boxes, the list marker
    // should be aligned to the first line box of next child.
    // https://github.com/w3c/csswg-drafts/issues/2417
    if (content_metrics.IsEmpty())
      return false;
  }

  // Layout the list marker.
  scoped_refptr<NGLayoutResult> marker_layout_result =
      Layout(space, baseline_type);
  DCHECK(marker_layout_result && marker_layout_result->PhysicalFragment());
  const NGPhysicalBoxFragment& marker_physical_fragment =
      ToNGPhysicalBoxFragment(*marker_layout_result->PhysicalFragment());

  // Compute the inline offset of the marker.
  NGBoxFragment marker_fragment(space.GetWritingMode(), space.Direction(),
                                marker_physical_fragment);
  NGLogicalOffset marker_offset(
      InlineOffset(marker_fragment.Size().inline_size),
      content_offset->block_offset);

  // Adjust the block offset to align baselines of the marker and the content.
  NGLineHeightMetrics marker_metrics = marker_fragment.BaselineMetrics(
      {NGBaselineAlgorithmType::kAtomicInline, baseline_type}, space);
  LayoutUnit baseline_adjust = content_metrics.ascent - marker_metrics.ascent;
  if (baseline_adjust >= 0) {
    marker_offset.block_offset += baseline_adjust;
  } else {
    // If the ascent of the marker is taller than the ascent of the content,
    // push the content down.
    content_offset->block_offset -= baseline_adjust;
  }
  marker_offset.inline_offset += ComputeIntrudedFloatOffset(
      space, container_builder, border_scrollbar_padding,
      marker_offset.block_offset);

  DCHECK(container_builder);
  container_builder->AddChild(*marker_layout_result, marker_offset);

  return true;
}

LayoutUnit NGUnpositionedListMarker::AddToBoxWithoutLineBoxes(
    const NGConstraintSpace& space,
    FontBaseline baseline_type,
    NGBoxFragmentBuilder* container_builder) const {
  // Layout the list marker.
  scoped_refptr<NGLayoutResult> marker_layout_result =
      Layout(space, baseline_type);
  DCHECK(marker_layout_result && marker_layout_result->PhysicalFragment());
  const NGPhysicalBoxFragment& marker_physical_fragment =
      ToNGPhysicalBoxFragment(*marker_layout_result->PhysicalFragment());

  // When there are no line boxes, marker is top-aligned to the list item.
  // https://github.com/w3c/csswg-drafts/issues/2417
  NGLogicalSize marker_size =
      marker_physical_fragment.Size().ConvertToLogical(space.GetWritingMode());
  NGLogicalOffset offset(InlineOffset(marker_size.inline_size), LayoutUnit());

  DCHECK(container_builder);
  container_builder->AddChild(*marker_layout_result, offset);

  return marker_size.block_size;
}

// Find the opportunity for marker, and compare it to ListItem, then compute the
// diff as intruded offset.
LayoutUnit NGUnpositionedListMarker::ComputeIntrudedFloatOffset(
    const NGConstraintSpace& space,
    const NGBoxFragmentBuilder* container_builder,
    const NGBoxStrut& border_scrollbar_padding,
    LayoutUnit marker_block_offset) const {
  DCHECK(container_builder);
  // Because opportunity.rect is in the content area of LI, so origin_offset
  // should plus border_scrollbar_padding.inline_start, and available_size
  // should minus border_scrollbar_padding.
  NGBfcOffset origin_offset = {
      container_builder->BfcLineOffset() +
          border_scrollbar_padding.inline_start,
      container_builder->BfcBlockOffset().value() + marker_block_offset};
  LayoutUnit available_size = container_builder->InlineSize() -
                              border_scrollbar_padding.inline_start -
                              border_scrollbar_padding.inline_end;
  NGLayoutOpportunity opportunity =
      space.ExclusionSpace().FindLayoutOpportunity(
          origin_offset, available_size, NGLogicalSize());
  DCHECK(marker_layout_object_);
  const TextDirection direction = marker_layout_object_->StyleRef().Direction();
  if (direction == TextDirection::kLtr) {
    // If Ltr, compare the left side.
    if (opportunity.rect.LineStartOffset() > origin_offset.line_offset)
      return opportunity.rect.LineStartOffset() - origin_offset.line_offset;
  } else if (opportunity.rect.LineEndOffset() <
             origin_offset.line_offset + available_size) {
    // If Rtl, Compare the right side.
    return origin_offset.line_offset + available_size -
           opportunity.rect.LineEndOffset();
  }
  return LayoutUnit(0);
}

}  // namespace blink

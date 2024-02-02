// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/list/unpositioned_list_marker.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/list/layout_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"

namespace blink {

UnpositionedListMarker::UnpositionedListMarker(LayoutOutsideListMarker* marker)
    : marker_layout_object_(marker) {}

UnpositionedListMarker::UnpositionedListMarker(const BlockNode& node)
    : UnpositionedListMarker(To<LayoutOutsideListMarker>(node.GetLayoutBox())) {
}

// Compute the inline offset of the marker, relative to the list item.
// The marker is relative to the border box of the list item and has nothing
// to do with the content offset.
// Open issue at https://github.com/w3c/csswg-drafts/issues/2361
LayoutUnit UnpositionedListMarker::InlineOffset(
    const LayoutUnit marker_inline_size) const {
  DCHECK(marker_layout_object_);
  LayoutObject* list_item =
      marker_layout_object_->Marker().ListItem(*marker_layout_object_);
  auto margins = ListMarker::InlineMarginsForOutside(
      list_item->GetDocument(), marker_layout_object_->StyleRef(),
      list_item->StyleRef(), marker_inline_size);
  return margins.first;
}

const LayoutResult* UnpositionedListMarker::Layout(
    const ConstraintSpace& parent_space,
    const ComputedStyle& parent_style,
    FontBaseline baseline_type) const {
  DCHECK(marker_layout_object_);
  BlockNode marker_node(marker_layout_object_);

  // We need the first-line baseline from the list-marker, instead of the
  // typical atomic-inline baseline.
  const LayoutResult* marker_layout_result = marker_node.LayoutAtomicInline(
      parent_space, parent_style, parent_space.UseFirstLineStyle(),
      BaselineAlgorithmType::kDefault);
  DCHECK(marker_layout_result);
  return marker_layout_result;
}

std::optional<LayoutUnit> UnpositionedListMarker::ContentAlignmentBaseline(
    const ConstraintSpace& space,
    FontBaseline baseline_type,
    const PhysicalFragment& content) const {
  // Compute the baseline of the child content.
  if (content.IsLineBox()) {
    const auto& line_box = To<PhysicalLineBoxFragment>(content);

    // If this child is an empty line-box, the list marker should be aligned
    // with the next non-empty line box produced. (This can occur with floats
    // producing empty line-boxes).
    if (line_box.IsEmptyLineBox() && line_box.GetBreakToken()) {
      return std::nullopt;
    }

    return line_box.Metrics().ascent;
  }

  // If this child content does not have any line boxes, the list marker
  // should be aligned to the first line box of next child.
  // https://github.com/w3c/csswg-drafts/issues/2417
  return LogicalBoxFragment(space.GetWritingDirection(),
                            To<PhysicalBoxFragment>(content))
      .FirstBaseline();
}

void UnpositionedListMarker::AddToBox(
    const ConstraintSpace& space,
    FontBaseline baseline_type,
    const PhysicalFragment& content,
    const BoxStrut& border_scrollbar_padding,
    const LayoutResult& marker_layout_result,
    LayoutUnit content_baseline,
    LayoutUnit* block_offset,
    BoxFragmentBuilder* container_builder) const {
  const auto& marker_physical_fragment =
      To<PhysicalBoxFragment>(marker_layout_result.GetPhysicalFragment());

  // Compute the inline offset of the marker.
  LogicalBoxFragment marker_fragment(space.GetWritingDirection(),
                                     marker_physical_fragment);
  LogicalOffset marker_offset(InlineOffset(marker_fragment.Size().inline_size),
                              *block_offset);

  // Adjust the block offset to align baselines of the marker and the content.
  FontHeight marker_metrics = marker_fragment.BaselineMetrics(
      /* margins */ LineBoxStrut(), baseline_type);
  LayoutUnit baseline_adjust = content_baseline - marker_metrics.ascent;
  if (baseline_adjust >= 0) {
    marker_offset.block_offset += baseline_adjust;
  } else {
    // If the ascent of the marker is taller than the ascent of the content,
    // push the content down.
    //
    // TODO(layout-dev): Adjusting block-offset "silently" without re-laying out
    // is bad for block fragmentation.
    *block_offset -= baseline_adjust;
  }
  marker_offset.inline_offset += ComputeIntrudedFloatOffset(
      space, container_builder, border_scrollbar_padding,
      marker_offset.block_offset);

  DCHECK(container_builder);
  if (FragmentItemsBuilder* items_builder = container_builder->ItemsBuilder()) {
    items_builder->AddListMarker(marker_physical_fragment, marker_offset);
    return;
  }
  container_builder->AddResult(marker_layout_result, marker_offset);
}

void UnpositionedListMarker::AddToBoxWithoutLineBoxes(
    const ConstraintSpace& space,
    FontBaseline baseline_type,
    const LayoutResult& marker_layout_result,
    BoxFragmentBuilder* container_builder,
    LayoutUnit* intrinsic_block_size) const {
  const auto& marker_physical_fragment =
      To<PhysicalBoxFragment>(marker_layout_result.GetPhysicalFragment());

  // When there are no line boxes, marker is top-aligned to the list item.
  // https://github.com/w3c/csswg-drafts/issues/2417
  LogicalSize marker_size =
      marker_physical_fragment.Size().ConvertToLogical(space.GetWritingMode());
  LogicalOffset offset(InlineOffset(marker_size.inline_size), LayoutUnit());

  DCHECK(container_builder);
  DCHECK(!container_builder->ItemsBuilder());
  container_builder->AddResult(marker_layout_result, offset);

  // Whether the list marker should affect the block size or not is not
  // well-defined, but 3 out of 4 impls do.
  // https://github.com/w3c/csswg-drafts/issues/2418
  //
  // The BFC block-offset has been resolved after layout marker. We'll always
  // include the marker into the block-size.
  if (container_builder->BfcBlockOffset()) {
    *intrinsic_block_size =
        std::max(marker_size.block_size, *intrinsic_block_size);
    container_builder->SetIntrinsicBlockSize(*intrinsic_block_size);
    container_builder->SetFragmentsTotalBlockSize(
        std::max(marker_size.block_size, container_builder->Size().block_size));
  }
}

// Find the opportunity for marker, and compare it to ListItem, then compute the
// diff as intruded offset.
LayoutUnit UnpositionedListMarker::ComputeIntrudedFloatOffset(
    const ConstraintSpace& space,
    const BoxFragmentBuilder* container_builder,
    const BoxStrut& border_scrollbar_padding,
    LayoutUnit marker_block_offset) const {
  DCHECK(container_builder);
  // If the BFC block-offset isn't resolved, the intruded offset isn't
  // available either.
  if (!container_builder->BfcBlockOffset())
    return LayoutUnit();
  // Because opportunity.rect is in the content area of LI, so origin_offset
  // should plus border_scrollbar_padding.inline_start, and available_size
  // should minus border_scrollbar_padding.
  BfcOffset origin_offset = {
      container_builder->BfcLineOffset() +
          border_scrollbar_padding.inline_start,
      *container_builder->BfcBlockOffset() + marker_block_offset};
  const LayoutUnit available_size =
      container_builder->ChildAvailableSize().inline_size;
  LayoutOpportunity opportunity =
      space.GetExclusionSpace().FindLayoutOpportunity(origin_offset,
                                                      available_size);
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
  return LayoutUnit();
}

#if DCHECK_IS_ON()
// TODO: Currently we haven't supported ::marker, so the margin-top of marker
// should always be zero. And this make us could resolve LI's BFC block-offset
// in BlockLayoutAlgorithm::PositionOrPropagateListMarker and
// BlockLayoutAlgorithm::PositionListMarkerWithoutLineBoxes without consider
// marker's margin-top.
void UnpositionedListMarker::CheckMargin() const {
  DCHECK(marker_layout_object_);
  DCHECK(marker_layout_object_->StyleRef().MarginBlockStart().IsZero());
}
#endif

void UnpositionedListMarker::Trace(Visitor* visitor) const {
  visitor->Trace(marker_layout_object_);
}

}  // namespace blink

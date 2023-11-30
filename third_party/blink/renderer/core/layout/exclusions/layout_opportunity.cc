// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/exclusions/layout_opportunity.h"

#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"

namespace blink {

namespace {

// Returns how far a line can "fit" into a given exclusion based on its shape
// area. If the exclusion does not obstruct the line, then the returned
// LineSegment will be "invalid".
LineSegment ExcludedSegment(const ExclusionArea& exclusion,
                            LayoutUnit bfc_block_offset,
                            LayoutUnit line_block_size) {
  DCHECK(exclusion.shape_data);
  const ExclusionShapeData& shape_data = *exclusion.shape_data;
  const Shape& shape =
      shape_data.layout_box->GetShapeOutsideInfo()->ComputedShape();

  // Determine the block offset (relative to the shape) at which we need to
  // test for.
  LayoutUnit shape_relative_block_offset =
      bfc_block_offset -
      (exclusion.rect.BlockStartOffset() + shape_data.margins.block_start +
       shape_data.shape_insets.block_start);

  // At the block-start/end of shapes it is possible for a line to just touch,
  // and GetExcludedInterval will return a valid segment.
  // This check skips the shape when this happens.
  if (!shape.LineOverlapsShapeMarginBounds(shape_relative_block_offset,
                                           line_block_size))
    return LineSegment();

  // Clamp the line size to the size of the shape.
  LayoutUnit clamped_line_block_size =
      std::min(line_block_size, exclusion.rect.BlockSize() -
                                    shape_data.shape_insets.BlockSum() -
                                    shape_data.margins.BlockSum());

  LineSegment segment = shape.GetExcludedInterval(shape_relative_block_offset,
                                                  clamped_line_block_size);

  // Adjust the segment offsets to be relative to the line-left margin edge.
  LayoutUnit margin_delta =
      shape_data.margins.LineLeft(TextDirection::kLtr) +
      shape_data.shape_insets.LineLeft(TextDirection::kLtr);
  segment.logical_left += margin_delta;
  segment.logical_right += margin_delta;

  // Clamp the segment offsets to the size of the exclusion.
  segment.logical_left = ClampTo<LayoutUnit>(segment.logical_left, LayoutUnit(),
                                             exclusion.rect.InlineSize());
  segment.logical_right = ClampTo<LayoutUnit>(
      segment.logical_right, LayoutUnit(), exclusion.rect.InlineSize());

  // Make the segment offsets relative to the BFC coordinate space.
  segment.logical_left += exclusion.rect.LineStartOffset();
  segment.logical_right += exclusion.rect.LineStartOffset();

  return segment;
}

// Returns if the given line block-size and offset intersects with the given
// exclusion.
bool IntersectsExclusion(const ExclusionArea& exclusion,
                         LayoutUnit bfc_block_offset,
                         LayoutUnit line_block_size) {
  return bfc_block_offset < exclusion.rect.BlockEndOffset() &&
         bfc_block_offset + line_block_size > exclusion.rect.BlockStartOffset();
}

}  // namespace

bool LayoutOpportunity::IsBlockDeltaBelowShapes(LayoutUnit block_delta) const {
  DCHECK(shape_exclusions);

  for (const auto& exclusion : shape_exclusions->line_left_shapes) {
    if (rect.BlockStartOffset() + block_delta <
        exclusion->rect.BlockEndOffset())
      return false;
  }

  for (const auto& exclusion : shape_exclusions->line_right_shapes) {
    if (rect.BlockStartOffset() + block_delta <
        exclusion->rect.BlockEndOffset())
      return false;
  }

  return true;
}

LayoutUnit LayoutOpportunity::ComputeLineLeftOffset(
    const ConstraintSpace& space,
    LayoutUnit line_block_size,
    LayoutUnit block_delta) const {
  if (!shape_exclusions || shape_exclusions->line_left_shapes.empty())
    return rect.LineStartOffset();

  LayoutUnit bfc_block_offset = rect.BlockStartOffset() + block_delta;

  // Step through each exclusion and re-build the line_left_offset. Without
  // shapes this would be the same as the opportunity offset.
  //
  // We rebuild this offset from the line-left end, checking each exclusion and
  // increasing the line_left when an exclusion intersects.
  LayoutUnit line_left = space.GetBfcOffset().line_offset;
  for (auto& exclusion : shape_exclusions->line_left_shapes) {
    if (!IntersectsExclusion(*exclusion, bfc_block_offset, line_block_size))
      continue;

    if (exclusion->shape_data) {
      LineSegment segment =
          ExcludedSegment(*exclusion, bfc_block_offset, line_block_size);
      if (segment.is_valid)
        line_left = std::max(line_left, segment.logical_right);
    } else {
      line_left = std::max(line_left, exclusion->rect.LineEndOffset());
    }
  }

  return std::min(line_left, rect.LineEndOffset());
}

LayoutUnit LayoutOpportunity::ComputeLineRightOffset(
    const ConstraintSpace& space,
    LayoutUnit line_block_size,
    LayoutUnit block_delta) const {
  if (!shape_exclusions || shape_exclusions->line_right_shapes.empty())
    return rect.LineEndOffset();

  LayoutUnit bfc_block_offset = rect.BlockStartOffset() + block_delta;

  LayoutUnit line_right =
      space.GetBfcOffset().line_offset + space.AvailableSize().inline_size;

  // Step through each exclusion and re-build the line_right_offset. Without
  // shapes this would be the same as the opportunity offset.
  //
  // We rebuild this offset from the line-right end, checking each exclusion and
  // reducing the line_right when an exclusion intersects.
  for (auto& exclusion : shape_exclusions->line_right_shapes) {
    if (!IntersectsExclusion(*exclusion, bfc_block_offset, line_block_size))
      continue;

    if (exclusion->shape_data) {
      LineSegment segment =
          ExcludedSegment(*exclusion, bfc_block_offset, line_block_size);
      if (segment.is_valid)
        line_right = std::min(line_right, segment.logical_left);
    } else {
      line_right = std::min(line_right, exclusion->rect.LineStartOffset());
    }
  }

  return std::max(line_right, rect.LineStartOffset());
}

bool LayoutOpportunity::operator==(const LayoutOpportunity& other) const {
  return rect == other.rect && shape_exclusions == other.shape_exclusions;
}

std::ostream& operator<<(std::ostream& ostream,
                         const LayoutOpportunity& opportunity) {
  if (opportunity.HasShapeExclusions())
    return ostream << "ShapeExclusion@";
  return ostream << opportunity.rect;
}

}  // namespace blink

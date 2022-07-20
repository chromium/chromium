// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_layout_algorithm.h"

#include <math.h>

#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_box.h"
#include "third_party/blink/renderer/core/layout/layout_vtt_cue.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

namespace blink {

VttCueLayoutAlgorithm::VttCueLayoutAlgorithm(VTTCueBox& cue)
    : cue_(cue), snap_to_lines_position_(cue.SnapToLinesPosition()) {}

void VttCueLayoutAlgorithm::Layout() {
  if (cue_.IsAdjusted() || !cue_.GetLayoutBox())
    return;

  // https://w3c.github.io/webvtt/#apply-webvtt-cue-settings
  // 10. Adjust the positions of boxes according to the appropriate steps
  // from the following list:
  if (isfinite(snap_to_lines_position_)) {
    // ↪ If cue’s WebVTT cue snap-to-lines flag is true
    AdjustPositionWithSnapToLines();
  } else {
    // ↪ If cue’s WebVTT cue snap-to-lines flag is false
    AdjustPositionWithoutSnapToLines();
  }
}

LayoutUnit VttCueLayoutAlgorithm::ComputeInitialPositionAdjustment(
    LayoutUnit max_dimension) {
  DCHECK(isfinite(snap_to_lines_position_));

  // 4. Let line be cue's computed line.
  // 5. Round line to an integer by adding 0.5 and then flooring it.
  float line = floorf(snap_to_lines_position_ + 0.5f);

  // 6. Vertical Growing Left: Add one to line then negate it.
  const auto& cue_box = *cue_.GetLayoutBox();
  if (cue_box.HasFlippedBlocksWritingMode())
    line = -(line + 1);

  // 7. Let position be the result of multiplying step and line offset.
  LayoutUnit position(step_ * line);

  // 8. Vertical Growing Left: Decrease position by the width of the
  // bounding box of the boxes in boxes, then increase position by step.
  if (cue_box.HasFlippedBlocksWritingMode()) {
    position -= cue_box.FrameRect().Width();
    position += step_;
  }

  // 9. If line is less than zero then increase position by max dimension,
  // and negate step.
  if (line < 0) {
    position += max_dimension;
    step_ = -step_;
  } else {
    // https://www.w3.org/TR/2017/WD-webvtt1-20170808/#apply-webvtt-cue-settings
    // 11.11. Otherwise, increase position by margin.
    position += margin_;
  }
  return position;
}

// CueBoundingBox() does not return the geometry from LayoutBox as is because
// the resultant rectangle should be:
// - Based on the latest adjusted position even before the box is laid out.
// - Based on the initial position without adjustment if the function is
//   called just after style-recalc.
//
// static
gfx::Rect VttCueLayoutAlgorithm::CueBoundingBox(const LayoutBox& cue_box) {
  const LayoutBlock* container = cue_box.ContainingBlock();
  PhysicalRect border_box =
      cue_box.LocalToAncestorRect(cue_box.PhysicalBorderBoxRect(), container);
  const LayoutSize size = container->Size();
  const auto* cue_dom = To<VTTCueBox>(cue_box.GetNode());
  if (cue_box.IsHorizontalWritingMode())
    border_box.SetY(cue_dom->AdjustedPosition(size.Height(), PassKey()));
  else
    border_box.SetX(cue_dom->AdjustedPosition(size.Width(), PassKey()));
  return ToEnclosingRect(border_box);
}

bool VttCueLayoutAlgorithm::IsOutside(const gfx::Rect& title_area) const {
  return !title_area.Contains(CueBoundingBox(*cue_.GetLayoutBox()));
}

bool VttCueLayoutAlgorithm::IsOverlapping(
    const gfx::Rect& controls_rect) const {
  gfx::Rect cue_box_rect = CueBoundingBox(*cue_.GetLayoutBox());
  for (LayoutBox* box = cue_.GetLayoutBox()->PreviousSiblingBox(); box;
       box = box->PreviousSiblingBox()) {
    if (IsA<VTTCueBox>(box->GetNode()) &&
        cue_box_rect.Intersects(CueBoundingBox(*box)))
      return true;
  }
  return cue_box_rect.Intersects(controls_rect);
}

bool VttCueLayoutAlgorithm::ShouldSwitchDirection(
    LayoutUnit cue_block_position,
    LayoutUnit cue_block_size,
    LayoutUnit full_dimension) const {
  // 14. Horizontal: If step is negative and the top of the first line box in
  // boxes is now above the top of the title area, or if step is positive and
  // the bottom of the first line box in boxes is now below the bottom of the
  // title area, jump to the step labeled switch direction.
  //     Vertical: If step is negative and the left edge of ...
  if (step_ < 0 && cue_block_position < margin_)
    return true;
  if (step_ > 0 &&
      cue_block_position + cue_block_size + margin_ > full_dimension)
    return true;
  return false;
}

void VttCueLayoutAlgorithm::AdjustPositionWithSnapToLines() {
  // 9. If there are no line boxes in boxes, skip the remainder of these
  // substeps for cue. The cue is ignored.
  const LayoutBox& cue_box = *cue_.GetLayoutBox();
  NGInlineCursor cursor(To<LayoutBlockFlow>(cue_box));
  cursor.MoveToFirstLine();
  if (cursor.IsNull()) {
    return;
  }
  // We refer to the block size of a kBox item for VTTCueBackgroundBox rather
  // than the block size of a line box. The kBox item is taller than the line
  // box due to paddings.
  cursor.MoveToNext();
  if (cursor.IsNull())
    return;
  const NGFragmentItem& first_item = *cursor.CurrentItem();
  DCHECK(first_item.GetLayoutObject());
  DCHECK(IsA<VTTCueBackgroundBox>(first_item.GetLayoutObject()->GetNode()));

  const bool is_horizontal = cue_box.IsHorizontalWritingMode();
  const LayoutBlock& container = *cue_box.ContainingBlock();

  // 1. Horizontal: Let full dimension be the height of video’s rendering area.
  //    Vertical: Let full dimension be the width of video’s rendering area.
  const LayoutUnit full_dimension =
      is_horizontal ? container.Size().Height() : container.Size().Width();

  // https://www.w3.org/TR/2017/WD-webvtt1-20170808/#apply-webvtt-cue-settings
  // 11.1. Horizontal: Let margin be a user-agent-defined vertical length which
  // will be used to define a margin at the top and bottom edges of the video
  // into which cues will not be placed. In situations with overscan, this
  // margin should be sufficient to place all cues within the title-safe area.
  // In the absence of overscan, this value should be picked for aesthetics (to
  // avoid text being aligned precisely on the bottom edge of the video, which
  // can be ugly).
  //       Vertical: Let margin be a user-agent-defined horizontal length ...
  //
  // 11.3. Let max dimension be full dimension - (2 × margin).
  //
  // TODO(crbug.com/1012242): Remove this. The latest specification does not
  // have these steps.
  const auto* settings = cue_.GetDocument().GetSettings();
  const double margin_ratio =
      settings ? settings->GetTextTrackMarginPercentage() / 100.0 : 0;
  margin_ = LayoutUnit(full_dimension * margin_ratio);
  const LayoutUnit max_dimension = full_dimension - 2 * margin_;

  // 2. Horizontal: Let step be the height of the first line box in boxes.
  //    Vertical: Let step be the width of the first line box in boxes.
  step_ = is_horizontal ? first_item.Size().height : first_item.Size().width;

  // 3. If step is zero, then jump to the step labeled done positioning below.
  if (step_ == LayoutUnit())
    return;

  // Step 4-9
  LayoutUnit position = ComputeInitialPositionAdjustment(max_dimension);

  // 10. Horizontal: Move all the boxes in boxes down by the distance given
  // by position.
  //     Vertical: Move all the boxes in boxes right ...
  LayoutUnit& adjusted_position = cue_.StartAdjustment(
      cue_.AdjustedPosition(full_dimension, PassKey()) + position);

  // 11. Remember the position of all the boxes in boxes as their specified
  // position.
  const LayoutUnit specified_position = adjusted_position;

  bool switched = false;

  // 12. Let title area be a box that covers all of the video’s rendering area.
  gfx::Rect title_area = ToEnclosingRect(container.PhysicalBorderBoxRect());
  // https://www.w3.org/TR/2017/WD-webvtt1-20170808/#apply-webvtt-cue-settings
  // 11.14. Horizontal: Let title area be a box that covers all of the video’s
  // rendering area except for a height of margin at the top of the rendering
  // area and a height of margin at the bottom of the rendering area.
  //        Vertical: Let title area be a box that covers all of the video’s
  // rendering area except for a width of margin at the left ...
  // TODO(crbug.com/1012242): Remove this. The latest specification does not
  // have margins.
  if (is_horizontal) {
    title_area.Inset(gfx::Insets::VH(margin_.ToInt(), 0));
  } else {
    title_area.Inset(gfx::Insets::VH(0, margin_.ToInt()));
  }

  // 13. Step loop: If none of the boxes in boxes would overlap any of the
  // boxes in output, and all of the boxes in boxes are entirely within the
  // title area box, then jump to the step labeled done positioning below.
  //
  // We also check overlapping with media controls.
  const gfx::Rect controls_rect = LayoutVTTCue::ComputeControlsRect(container);
  while (IsOutside(title_area) || IsOverlapping(controls_rect)) {
    // Step 14
    if (!ShouldSwitchDirection(adjusted_position, cue_box.LogicalHeight(),
                               full_dimension)) {
      // 15. Horizontal: Move all the boxes in boxes down by the distance
      // given by step.
      //     Vertical: Move all the boxes in boxes right ...
      adjusted_position += step_;

      // 16. Jump back to the step labeled step loop.
      continue;
    }

    // 17. Switch direction: If switched is true, then remove all the boxes in
    // boxes, and jump to the step labeled done positioning below.
    if (switched) {
      // Move the cue to the outside of the video rendering area instead of
      // removing the boxes. This makes "RevertAdjustment()" simpler.
      adjusted_position = full_dimension + 1;
      break;
    }

    // 18. Otherwise, move all the boxes in boxes back to their specified
    // position as determined in the earlier step.
    adjusted_position = specified_position;

    // 19. Negate step.
    step_ = -step_;

    // 20. Set switched to true.
    switched = true;

    // 21. Jump back to the step labeled step loop.
  }

  // Set adjusted_position to 'top' or 'left' property as a percentage value,
  // which will work well even if the video rendering area is resized.
  cue_.SetInlineStyleProperty(
      is_horizontal ? CSSPropertyID::kTop : CSSPropertyID::kLeft,
      (adjusted_position * 100 / full_dimension).ToDouble(),
      CSSPrimitiveValue::UnitType::kPercentage);
}

void VttCueLayoutAlgorithm::AdjustPositionWithoutSnapToLines() {
  // TODO(crbug.com/314037): Implement overlapping detection when snap-to-lines
  // is not set.
}

}  // namespace blink

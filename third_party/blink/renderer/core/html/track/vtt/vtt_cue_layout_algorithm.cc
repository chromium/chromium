// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_layout_algorithm.h"

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
  if (std::isfinite(snap_to_lines_position_)) {
    // ↪ If cue’s WebVTT cue snap-to-lines flag is true
    AdjustPositionWithSnapToLines();
  } else {
    // ↪ If cue’s WebVTT cue snap-to-lines flag is false
    AdjustPositionWithoutSnapToLines();
  }
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
  [[maybe_unused]] const LayoutUnit max_dimension =
      full_dimension - 2 * margin_;

  // 2. Horizontal: Let step be the height of the first line box in boxes.
  //    Vertical: Let step be the width of the first line box in boxes.
  step_ = is_horizontal ? first_item.Size().height : first_item.Size().width;

  // 3. If step is zero, then jump to the step labeled done positioning below.
  if (step_ == LayoutUnit())
    return;

  // TODO(crbug.com/1335309): Implement this.

  // This function will make cue_.adjusted_position_ a value other than
  // LayoutUnit::Min().
}

void VttCueLayoutAlgorithm::AdjustPositionWithoutSnapToLines() {
  // TODO(crbug.com/314037): Implement overlapping detection when snap-to-lines
  // is not set.
}

}  // namespace blink

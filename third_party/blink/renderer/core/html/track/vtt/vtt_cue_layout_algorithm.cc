// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_layout_algorithm.h"

#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_box.h"
#include "third_party/blink/renderer/core/layout/layout_vtt_cue.h"

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
  // TODO(crbug.com/1335309): Implement this.

  // This function will make cue_.adjusted_position_ a value other than
  // LayoutUnit::Min().
}

void VttCueLayoutAlgorithm::AdjustPositionWithoutSnapToLines() {
  // TODO(crbug.com/314037): Implement overlapping detection when snap-to-lines
  // is not set.
}

}  // namespace blink

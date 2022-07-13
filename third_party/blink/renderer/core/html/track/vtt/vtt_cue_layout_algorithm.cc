// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_layout_algorithm.h"

#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue_box.h"

namespace blink {

VttCueLayoutAlgorithm::VttCueLayoutAlgorithm(VTTCueBox& cue)
    : cue_(cue), snap_to_lines_position_(cue.SnapToLinesPosition()) {}

void VttCueLayoutAlgorithm::Layout() {
  if (!cue_.GetLayoutBox())
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

void VttCueLayoutAlgorithm::AdjustPositionWithSnapToLines() {
  // TODO(crbug.com/1335309): Implement this.
}

void VttCueLayoutAlgorithm::AdjustPositionWithoutSnapToLines() {
  // TODO(crbug.com/314037): Implement overlapping detection when snap-to-lines
  // is not set.
}

}  // namespace blink

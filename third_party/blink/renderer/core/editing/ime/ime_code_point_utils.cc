// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/ime_code_point_utils.h"

#include "third_party/blink/renderer/core/editing/state_machines/backward_code_point_state_machine.h"
#include "third_party/blink/renderer/core/editing/state_machines/forward_code_point_state_machine.h"

namespace blink {

std::optional<int> CalculateBeforeDeletionLengthsInCodePoints(
    const String& text,
    int before_length_in_code_points,
    int selection_start) {
  DCHECK_GE(before_length_in_code_points, 0);
  DCHECK_GE(selection_start, 0);
  DCHECK_LE(selection_start, static_cast<int>(text.length()));

  base::span<const UChar> u_text = text.Span16();
  BackwardCodePointStateMachine backward_machine;
  int counter = before_length_in_code_points;
  int deletion_start = selection_start;
  while (counter > 0 && deletion_start > 0) {
    const TextSegmentationMachineState state =
        backward_machine.FeedPrecedingCodeUnit(
            u_text[static_cast<size_t>(deletion_start - 1)]);
    // According to Android's InputConnection spec, we should do nothing if
    // |text| has invalid surrogate pair in the deletion range.
    if (state == TextSegmentationMachineState::kInvalid) {
      return std::nullopt;
    }

    if (backward_machine.AtCodePointBoundary()) {
      --counter;
    }
    --deletion_start;
  }
  if (!backward_machine.AtCodePointBoundary()) {
    return std::nullopt;
  }

  const int offset = backward_machine.GetBoundaryOffset();
  DCHECK_EQ(-offset, selection_start - deletion_start);
  return -offset;
}

std::optional<int> CalculateAfterDeletionLengthsInCodePoints(
    const String& text,
    int after_length_in_code_points,
    int selection_end) {
  DCHECK_GE(after_length_in_code_points, 0);
  const auto end = base::checked_cast<wtf_size_t>(selection_end);
  const wtf_size_t length = text.length();
  DCHECK_LE(end, length);

  base::span<const UChar> u_text = text.Span16();
  ForwardCodePointStateMachine forward_machine;
  int counter = after_length_in_code_points;
  wtf_size_t deletion_end = end;
  while (counter > 0 && deletion_end < length) {
    const TextSegmentationMachineState state =
        forward_machine.FeedFollowingCodeUnit(u_text[deletion_end]);
    // According to Android's InputConnection spec, we should do nothing if
    // |text| has invalid surrogate pair in the deletion range.
    if (state == TextSegmentationMachineState::kInvalid) {
      return std::nullopt;
    }

    if (forward_machine.AtCodePointBoundary()) {
      --counter;
    }
    ++deletion_end;
  }
  if (!forward_machine.AtCodePointBoundary()) {
    return std::nullopt;
  }

  const int offset = forward_machine.GetBoundaryOffset();
  DCHECK_EQ(static_cast<wtf_size_t>(offset), deletion_end - end);
  return offset;
}

}  // namespace blink

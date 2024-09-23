// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/backward_code_point_state_machine.h"

#include <unicode/utf16.h>

#include "base/notreached.h"

namespace blink {

enum class BackwardCodePointStateMachine::BackwardCodePointState {
  kNotSurrogate,
  kTrailSurrogate,
  kInvalid,
};

BackwardCodePointStateMachine::BackwardCodePointStateMachine()
    : state_(BackwardCodePointState::kNotSurrogate) {}

TextSegmentationMachineState
BackwardCodePointStateMachine::FeedPrecedingCodeUnit(UChar code_unit) {
  switch (state_) {
    case BackwardCodePointState::kNotSurrogate:
      if (U16_IS_LEAD(code_unit)) {
        code_units_to_be_deleted_ = 0;
        state_ = BackwardCodePointState::kInvalid;
        return TextSegmentationMachineState::kInvalid;
      }
      ++code_units_to_be_deleted_;
      if (U16_IS_TRAIL(code_unit)) {
        state_ = BackwardCodePointState::kTrailSurrogate;
        return TextSegmentationMachineState::kNeedMoreCodeUnit;
      }
      return TextSegmentationMachineState::kFinished;
    case BackwardCodePointState::kTrailSurrogate:
      if (U16_IS_LEAD(code_unit)) {
        ++code_units_to_be_deleted_;
        state_ = BackwardCodePointState::kNotSurrogate;
        return TextSegmentationMachineState::kFinished;
      }
      code_units_to_be_deleted_ = 0;
      state_ = BackwardCodePointState::kInvalid;
      return TextSegmentationMachineState::kInvalid;
    case BackwardCodePointState::kInvalid:
      code_units_to_be_deleted_ = 0;
      return TextSegmentationMachineState::kInvalid;
  }
  NOTREACHED_IN_MIGRATION();
  return TextSegmentationMachineState::kInvalid;
}

TextSegmentationMachineState
BackwardCodePointStateMachine::FeedFollowingCodeUnit(UChar code_unit) {
  NOTREACHED_IN_MIGRATION();
  return TextSegmentationMachineState::kInvalid;
}

bool BackwardCodePointStateMachine::AtCodePointBoundary() {
  return state_ == BackwardCodePointState::kNotSurrogate;
}

int BackwardCodePointStateMachine::GetBoundaryOffset() {
  return -code_units_to_be_deleted_;
}

void BackwardCodePointStateMachine::Reset() {
  code_units_to_be_deleted_ = 0;
  state_ = BackwardCodePointState::kNotSurrogate;
}

}  // namespace blink

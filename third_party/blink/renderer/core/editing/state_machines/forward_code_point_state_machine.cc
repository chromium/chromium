// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/forward_code_point_state_machine.h"

#include <unicode/utf16.h>

#include "base/notreached.h"

namespace blink {

enum class ForwardCodePointStateMachine::ForwardCodePointState {
  kNotSurrogate,
  kLeadSurrogate,
  kInvalid,
};

ForwardCodePointStateMachine::ForwardCodePointStateMachine()
    : state_(ForwardCodePointState::kNotSurrogate) {}

TextSegmentationMachineState
ForwardCodePointStateMachine::FeedFollowingCodeUnit(UChar code_unit) {
  switch (state_) {
    case ForwardCodePointState::kNotSurrogate:
      if (U16_IS_TRAIL(code_unit)) {
        code_units_to_be_deleted_ = 0;
        state_ = ForwardCodePointState::kInvalid;
        return TextSegmentationMachineState::kInvalid;
      }
      ++code_units_to_be_deleted_;
      if (U16_IS_LEAD(code_unit)) {
        state_ = ForwardCodePointState::kLeadSurrogate;
        return TextSegmentationMachineState::kNeedMoreCodeUnit;
      }
      return TextSegmentationMachineState::kFinished;
    case ForwardCodePointState::kLeadSurrogate:
      if (U16_IS_TRAIL(code_unit)) {
        ++code_units_to_be_deleted_;
        state_ = ForwardCodePointState::kNotSurrogate;
        return TextSegmentationMachineState::kFinished;
      }
      code_units_to_be_deleted_ = 0;
      state_ = ForwardCodePointState::kInvalid;
      return TextSegmentationMachineState::kInvalid;
    case ForwardCodePointState::kInvalid:
      code_units_to_be_deleted_ = 0;
      return TextSegmentationMachineState::kInvalid;
  }
  NOTREACHED_IN_MIGRATION();
  return TextSegmentationMachineState::kInvalid;
}

TextSegmentationMachineState
ForwardCodePointStateMachine::FeedPrecedingCodeUnit(UChar code_unit) {
  NOTREACHED_IN_MIGRATION();
  return TextSegmentationMachineState::kInvalid;
}

bool ForwardCodePointStateMachine::AtCodePointBoundary() {
  return state_ == ForwardCodePointState::kNotSurrogate;
}

int ForwardCodePointStateMachine::GetBoundaryOffset() {
  return code_units_to_be_deleted_;
}

void ForwardCodePointStateMachine::Reset() {
  code_units_to_be_deleted_ = 0;
  state_ = ForwardCodePointState::kNotSurrogate;
}

}  // namespace blink

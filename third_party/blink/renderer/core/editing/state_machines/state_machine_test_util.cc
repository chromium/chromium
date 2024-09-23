// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/state_machines/state_machine_test_util.h"

#include <algorithm>
#include "third_party/blink/renderer/core/editing/state_machines/backward_grapheme_boundary_state_machine.h"
#include "third_party/blink/renderer/core/editing/state_machines/forward_grapheme_boundary_state_machine.h"
#include "third_party/blink/renderer/core/editing/state_machines/text_segmentation_machine_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {
char MachineStateToChar(TextSegmentationMachineState state) {
  static const char kIndicators[] = {
      'I',  // Invalid
      'R',  // NeedMoreCodeUnit (Repeat)
      'S',  // NeedFollowingCodeUnit (Switch)
      'F',  // Finished
  };
  auto* const it = std::begin(kIndicators) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(kIndicators)) << "Unknown backspace value";
  DCHECK_LT(it, std::end(kIndicators)) << "Unknown backspace value";
  return *it;
}

Vector<UChar> CodePointsToCodeUnits(const Vector<UChar32>& code_points) {
  Vector<UChar> out;
  for (const auto& code_point : code_points) {
    if (U16_LENGTH(code_point) == 2) {
      out.push_back(U16_LEAD(code_point));
      out.push_back(U16_TRAIL(code_point));
    } else {
      out.push_back(static_cast<UChar>(code_point));
    }
  }
  return out;
}

template <typename StateMachine>
String ProcessSequence(StateMachine* machine,
                       const Vector<UChar32>& preceding,
                       const Vector<UChar32>& following) {
  machine->Reset();
  StringBuilder out;
  TextSegmentationMachineState state = TextSegmentationMachineState::kInvalid;
  Vector<UChar> preceding_code_units = CodePointsToCodeUnits(preceding);
  std::reverse(preceding_code_units.begin(), preceding_code_units.end());
  for (const auto& code_unit : preceding_code_units) {
    state = machine->FeedPrecedingCodeUnit(code_unit);
    out.Append(MachineStateToChar(state));
    switch (state) {
      case TextSegmentationMachineState::kInvalid:
      case TextSegmentationMachineState::kFinished:
        return out.ToString();
      case TextSegmentationMachineState::kNeedMoreCodeUnit:
        continue;
      case TextSegmentationMachineState::kNeedFollowingCodeUnit:
        break;
    }
  }
  if (preceding.empty() ||
      state == TextSegmentationMachineState::kNeedMoreCodeUnit) {
    state = machine->TellEndOfPrecedingText();
    out.Append(MachineStateToChar(state));
  }
  if (state == TextSegmentationMachineState::kFinished)
    return out.ToString();

  Vector<UChar> following_code_units = CodePointsToCodeUnits(following);
  for (const auto& code_unit : following_code_units) {
    state = machine->FeedFollowingCodeUnit(code_unit);
    out.Append(MachineStateToChar(state));
    switch (state) {
      case TextSegmentationMachineState::kInvalid:
      case TextSegmentationMachineState::kFinished:
        return out.ToString();
      case TextSegmentationMachineState::kNeedMoreCodeUnit:
        continue;
      case TextSegmentationMachineState::kNeedFollowingCodeUnit:
        break;
    }
  }
  return out.ToString();
}
}  // namespace

String GraphemeStateMachineTestBase::ProcessSequenceBackward(
    BackwardGraphemeBoundaryStateMachine* machine,
    const Vector<UChar32>& preceding) {
  const String& out = ProcessSequence(machine, preceding, Vector<UChar32>());
  if (machine->FinalizeAndGetBoundaryOffset() !=
      machine->FinalizeAndGetBoundaryOffset())
    return "State machine changes final offset after finished.";
  return out;
}

String GraphemeStateMachineTestBase::ProcessSequenceForward(
    ForwardGraphemeBoundaryStateMachine* machine,
    const Vector<UChar32>& preceding,
    const Vector<UChar32>& following) {
  const String& out = ProcessSequence(machine, preceding, following);
  if (machine->FinalizeAndGetBoundaryOffset() !=
      machine->FinalizeAndGetBoundaryOffset())
    return "State machine changes final offset after finished.";
  return out;
}

}  // namespace blink

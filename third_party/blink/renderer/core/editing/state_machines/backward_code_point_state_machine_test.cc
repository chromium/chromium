// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/backward_code_point_state_machine.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace backward_code_point_state_machine_test {

TEST(BackwardCodePointStateMachineTest, DoNothingCase) {
  BackwardCodePointStateMachine machine;
  EXPECT_EQ(0, machine.GetBoundaryOffset());
}

TEST(BackwardCodePointStateMachineTest, SingleCharacter) {
  BackwardCodePointStateMachine machine;
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.GetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedPrecedingCodeUnit('-'));
  EXPECT_EQ(-1, machine.GetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedPrecedingCodeUnit('\t'));
  EXPECT_EQ(-1, machine.GetBoundaryOffset());

  machine.Reset();
  // U+3042 HIRAGANA LETTER A.
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedPrecedingCodeUnit(0x3042));
  EXPECT_EQ(-1, machine.GetBoundaryOffset());
}

TEST(BackwardCodePointStateMachineTest, SurrogatePair) {
  BackwardCodePointStateMachine machine;

  // U+20BB7 is \uD83D\uDDFA in UTF-16.
  const UChar kLeadSurrogate = 0xD842;
  const UChar kTrailSurrogate = 0xDFB7;

  EXPECT_EQ(TextSegmentationMachineState::kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedPrecedingCodeUnit(kLeadSurrogate));
  EXPECT_EQ(-2, machine.GetBoundaryOffset());

  // Edge cases
  // Unpaired trailing surrogate. Nothing to delete.
  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(TextSegmentationMachineState::kInvalid,
            machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(0, machine.GetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(TextSegmentationMachineState::kInvalid,
            machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(0, machine.GetBoundaryOffset());

  // Unpaired leading surrogate. Nothing to delete.
  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kInvalid,
            machine.FeedPrecedingCodeUnit(kLeadSurrogate));
  EXPECT_EQ(0, machine.GetBoundaryOffset());
}

}  // namespace backward_code_point_state_machine_test

}  // namespace blink

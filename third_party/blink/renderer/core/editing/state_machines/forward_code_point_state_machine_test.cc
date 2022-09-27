// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/forward_code_point_state_machine.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ForwardCodePointStateMachineTest, DoNothingCase) {
  ForwardCodePointStateMachine machine;
  EXPECT_EQ(0, machine.GetBoundaryOffset());
}

TEST(ForwardCodePointStateMachineTest, SingleCharacter) {
  ForwardCodePointStateMachine machine;
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedFollowingCodeUnit('a'));
  EXPECT_EQ(1, machine.GetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedFollowingCodeUnit('-'));
  EXPECT_EQ(1, machine.GetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedFollowingCodeUnit('\t'));
  EXPECT_EQ(1, machine.GetBoundaryOffset());

  machine.Reset();
  // U+3042 HIRAGANA LETTER A.
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedFollowingCodeUnit(0x3042));
  EXPECT_EQ(1, machine.GetBoundaryOffset());
}

TEST(ForwardCodePointStateMachineTest, SurrogatePair) {
  ForwardCodePointStateMachine machine;

  // U+20BB7 is \uD83D\uDDFA in UTF-16.
  const UChar kLeadSurrogate = 0xD842;
  const UChar kTrailSurrogate = 0xDFB7;

  EXPECT_EQ(TextSegmentationMachineState::kNeedMoreCodeUnit,
            machine.FeedFollowingCodeUnit(kLeadSurrogate));
  EXPECT_EQ(TextSegmentationMachineState::kFinished,
            machine.FeedFollowingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(2, machine.GetBoundaryOffset());

  // Edge cases
  // Unpaired leading surrogate. Nothing to delete.
  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kNeedMoreCodeUnit,
            machine.FeedFollowingCodeUnit(kLeadSurrogate));
  EXPECT_EQ(TextSegmentationMachineState::kInvalid,
            machine.FeedFollowingCodeUnit('a'));
  EXPECT_EQ(0, machine.GetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kNeedMoreCodeUnit,
            machine.FeedFollowingCodeUnit(kLeadSurrogate));
  EXPECT_EQ(TextSegmentationMachineState::kInvalid,
            machine.FeedFollowingCodeUnit(kLeadSurrogate));
  EXPECT_EQ(0, machine.GetBoundaryOffset());

  // Unpaired trailing surrogate. Nothing to delete.
  machine.Reset();
  EXPECT_EQ(TextSegmentationMachineState::kInvalid,
            machine.FeedFollowingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(0, machine.GetBoundaryOffset());
}

}  // namespace blink

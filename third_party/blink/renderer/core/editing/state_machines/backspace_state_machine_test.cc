// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/backspace_state_machine.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace backspace_state_machine_test {

const TextSegmentationMachineState kNeedMoreCodeUnit =
    TextSegmentationMachineState::kNeedMoreCodeUnit;
const TextSegmentationMachineState kFinished =
    TextSegmentationMachineState::kFinished;

TEST(BackspaceStateMachineTest, DoNothingCase) {
  BackspaceStateMachine machine;
  EXPECT_EQ(0, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(0, machine.FinalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, SingleCharacter) {
  BackspaceStateMachine machine;
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('-'));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('\t'));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  machine.Reset();
  // U+3042 HIRAGANA LETTER A.
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(0x3042));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, SurrogatePair) {
  BackspaceStateMachine machine;

  // U+20BB7 is \uD83D\uDDFA in UTF-16.
  const UChar kLeadSurrogate = 0xD842;
  const UChar kTrailSurrogate = 0xDFB7;

  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kLeadSurrogate));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Edge cases
  // Unpaired trailing surrogate. Delete only broken trail surrogate.
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kTrailSurrogate));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Unpaired leading surrogate. Delete only broken lead surrogate.
  machine.Reset();
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kLeadSurrogate));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, CRLF) {
  BackspaceStateMachine machine;

  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('\r'));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit('\n'));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit('\n'));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(' '));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // CR LF should be deleted at the same time.
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit('\n'));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('\r'));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, KeyCap) {
  BackspaceStateMachine machine;

  const UChar kKeycap = 0x20E3;
  const UChar kVs16 = 0xFE0F;
  const UChar kNotKeycapBaseLead = 0xD83C;
  const UChar kNotKeycapBaseTrail = 0xDCCF;

  // keycapBase + keycap
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKeycap));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('0'));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // keycapBase + VS + keycap
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKeycap));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('0'));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // Followings are edge cases. Remove only keycap character.
  // Not keycapBase + keycap
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKeycap));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Not keycapBase + VS + keycap
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKeycap));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Not keycapBase(surrogate pair) + keycap
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKeycap));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kNotKeycapBaseTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kNotKeycapBaseLead));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Not keycapBase(surrogate pair) + VS + keycap
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKeycap));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kNotKeycapBaseTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kNotKeycapBaseLead));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Sot + keycap
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKeycap));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Sot + VS + keycap
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKeycap));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, EmojiModifier) {
  BackspaceStateMachine machine;

  const UChar kEmojiModifierLead = 0xD83C;
  const UChar kEmojiModifierTrail = 0xDFFB;
  const UChar kEmojiModifierBase = 0x261D;
  const UChar kEmojiModifierBaseLead = 0xD83D;
  const UChar kEmojiModifierBaseTrail = 0xDC66;
  const UChar kNotEmojiModifierBaseLead = 0xD83C;
  const UChar kNotEmojiModifierBaseTrail = 0xDCCF;
  const UChar kVs16 = 0xFE0F;
  const UChar kOther = 'a';

  // EMOJI_MODIFIER_BASE + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierBase));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // EMOJI_MODIFIER_BASE(surrogate pairs) + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierBaseTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
          machine.FeedPrecedingCodeUnit(kEmojiModifierBaseLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // EMOJI_MODIFIER_BASE + VS + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kEmojiModifierBase));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // EMOJI_MODIFIER_BASE(surrogate pairs) + VS + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierBaseTrail));
  EXPECT_EQ(kFinished,
            machine.FeedPrecedingCodeUnit(kEmojiModifierBaseLead));
  EXPECT_EQ(-5, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-5, machine.FinalizeAndGetBoundaryOffset());

  // Followings are edge cases. Remove only emoji modifier.
  // Not EMOJI_MODIFIER_BASE + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Not EMOJI_MODIFIER_BASE(surrogate pairs) + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kNotEmojiModifierBaseTrail));
  EXPECT_EQ(kFinished,
            machine.FeedPrecedingCodeUnit(kNotEmojiModifierBaseLead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Not EMOJI_MODIFIER_BASE + VS + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Not EMOJI_MODIFIER_BASE(surrogate pairs) + VS + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kNotEmojiModifierBaseTrail));
  EXPECT_EQ(kFinished,
            machine.FeedPrecedingCodeUnit(kNotEmojiModifierBaseLead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Sot + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Sot + VS + EMOJI_MODIFIER
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, RegionalIndicator) {
  BackspaceStateMachine machine;

  const UChar kRegionalIndicatorULead = 0xD83C;
  const UChar kRegionalIndicatorUTrail = 0xDDFA;
  const UChar kRegionalIndicatorSLead = 0xD83C;
  const UChar kRegionalIndicatorSTrail = 0xDDF8;
  const UChar kNotRegionalIndicatorLead = 0xD83C;
  const UChar kNotRegionalIndicatorTrail = 0xDCCF;

  // Not RI + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // Not RI(surrogate pairs) + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kNotRegionalIndicatorTrail));
  EXPECT_EQ(kFinished,
            machine.FeedPrecedingCodeUnit(kNotRegionalIndicatorLead));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // Sot + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // Not RI + RI + RI + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // Not RI(surrogate pairs) + RI + RI + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kNotRegionalIndicatorTrail));
  EXPECT_EQ(kFinished,
            machine.FeedPrecedingCodeUnit(kNotRegionalIndicatorLead));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // Sot + RI + RI + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // Followings are edge cases. Delete last regional indicator only.
  // Not RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Not RI(surrogate pairs) + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kNotRegionalIndicatorTrail));
  EXPECT_EQ(kFinished,
            machine.FeedPrecedingCodeUnit(kNotRegionalIndicatorLead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Sot + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Not RI + RI + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit('a'));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Not RI(surrogate pairs) + RI + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kNotRegionalIndicatorTrail));
  EXPECT_EQ(kFinished,
            machine.FeedPrecedingCodeUnit(kNotRegionalIndicatorLead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Sot + RI + RI + RI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kRegionalIndicatorULead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, VariationSequencec) {
  BackspaceStateMachine machine;

  UChar vs01 = 0xFE00;
  UChar vs01_base = 0xA85E;
  UChar vs01_base_lead = 0xD802;
  UChar vs01_base_trail = 0xDEC6;

  UChar vs17_lead = 0xDB40;
  UChar vs17_trail = 0xDD00;
  UChar vs17_base = 0x3402;
  UChar vs17_base_lead = 0xD841;
  UChar vs17_base_trail = 0xDC8C;

  UChar mongolian_vs = 0x180B;
  UChar mongolian_vs_base = 0x1820;
  // Variation selectors can't be a base of variation sequence.
  UChar notvs_base = 0xFE00;
  UChar notvs_base_lead = 0xDB40;
  UChar notvs_base_trail = 0xDD01;

  // VS_BASE + VS
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs01));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(vs01_base));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // VS_BASE + VS(surrogate pairs)
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_lead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(vs17_base));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // VS_BASE(surrogate pairs) + VS
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs01));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs01_base_trail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(vs01_base_lead));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // VS_BASE(surrogate pairs) + VS(surrogate pairs)
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_lead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_base_trail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(vs17_base_lead));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // mongolianVsBase + mongolianVs
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(mongolian_vs));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(mongolian_vs_base));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Followings are edge case. Delete only variation selector.
  // Not VS_BASE + VS
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs01));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(notvs_base));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Not VS_BASE + VS(surrogate pairs)
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_lead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(notvs_base));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Not VS_BASE(surrogate pairs) + VS
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs01));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(notvs_base_trail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(notvs_base_lead));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Not VS_BASE(surrogate pairs) + VS(surrogate pairs)
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_lead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(notvs_base_trail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(notvs_base_lead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Not VS_BASE + MONGOLIAN_VS
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(mongolian_vs));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(notvs_base));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Not VS_BASE(surrogate pairs) + MONGOLIAN_VS
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(mongolian_vs));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(notvs_base_trail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(notvs_base_lead));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Sot + VS
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs01));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Sot + VS(surrogate pair)
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(vs17_lead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Sot + MONGOLIAN_VS
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(mongolian_vs));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, ZWJSequence) {
  BackspaceStateMachine machine;

  const UChar kZwj = 0x200D;
  const UChar kEyeLead = 0xD83D;
  const UChar kEyeTrail = 0xDC41;
  const UChar kLeftSpeachBubbleLead = 0xD83D;
  const UChar kLeftSpeachBubbleTrail = 0xDDE8;
  const UChar kManLead = 0xD83D;
  const UChar kManTrail = 0xDC68;
  const UChar kBoyLead = 0xD83D;
  const UChar kBoyTrail = 0xDC66;
  const UChar kHeart = 0x2764;
  const UChar kKissLead = 0xD83D;
  const UChar kKissTrail = 0xDC8B;
  const UChar kVs16 = 0xFE0F;
  const UChar kLightSkinToneLead = 0xD83C;
  const UChar kLightSkinToneTrail = 0xDFFB;
  const UChar kDarkSkinToneLead = 0xD83C;
  const UChar kDarkSkinToneTrail = 0xDFFF;
  const UChar kOther = 'a';
  const UChar kOtherLead = 0xD83C;
  const UChar kOtherTrail = 0xDCCF;

  // Followings are chosen from valid zwj sequcne.
  // See http://www.unicode.org/Public/emoji/2.0//emoji-zwj-sequences.txt

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use EYE + ZWJ + LEFT_SPEACH_BUBBLE
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kLeftSpeachBubbleTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kLeftSpeachBubbleLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kEyeTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kEyeLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-5, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-5, machine.FinalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use EYE + ZWJ + LEFT_SPEACH_BUBBLE
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kLeftSpeachBubbleTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kLeftSpeachBubbleLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kEyeTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kEyeLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kOtherTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOtherLead));
  EXPECT_EQ(-5, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-5, machine.FinalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use EYE + ZWJ + LEFT_SPEACH_BUBBLE
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kLeftSpeachBubbleTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kLeftSpeachBubbleLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kEyeTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kEyeLead));
  EXPECT_EQ(-5, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-5, machine.FinalizeAndGetBoundaryOffset());

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-7, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-7, machine.FinalizeAndGetBoundaryOffset());

  // others + EMOJI_MODIFIER_BASE + EMOJI_MODIFIER + ZWJ
  // + EMOJI_MODIFIER_BASE + EMOJI_MODIFIER + ZWJ + ...
  // As an example, use MAN + LIGHT_SKIN_TONE + ZWJ + heart + vs16
  // + ZWJ + kiss + ZWJ + MAN + DARK_SKIN_TONE
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kDarkSkinToneTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kDarkSkinToneLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKissTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKissLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kLightSkinToneTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kLightSkinToneLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-15, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-15, machine.FinalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kOtherTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOtherLead));
  EXPECT_EQ(-7, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-7, machine.FinalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(-7, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-7, machine.FinalizeAndGetBoundaryOffset());

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + vs16 + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-8, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-8, machine.FinalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ +
  // ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + vs16 + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kOtherTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOtherLead));
  EXPECT_EQ(-8, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-8, machine.FinalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + vs16 + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(-8, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-8, machine.FinalizeAndGetBoundaryOffset());

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + MAN + ZWJ + boy + ZWJ + BOY
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI +
  // ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + MAN + ZWJ + boy + ZWJ + BOY
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kOtherTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOtherLead));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + MAN + ZWJ + boy + ZWJ + BOY
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBoyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ + ZWJ_EMOJI + ZWJ +
  // ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + VS + ZWJ + KISS + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKissTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKissLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ +
  // ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + VS + ZWJ + KISS + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKissTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKissLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kOtherTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOtherLead));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + VS + ZWJ + KISS + ZWJ + MAN
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKissTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kKissLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kVs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // Sot + EMOJI_MODIFIER_BASE + EMOJI_MODIFIER + ZWJ + ZWJ_EMOJI
  // As an example, use WOMAN + MODIFIER + ZWJ + BRIEFCASE
  const UChar kWomanLead = 0xD83D;
  const UChar kWomanTrail = 0xDC69;
  const UChar kEmojiModifierLead = 0xD83C;
  const UChar kEmojiModifierTrail = 0xDFFB;
  const UChar kBriefcaseLead = 0xD83D;
  const UChar kBriefcaseTrail = 0xDCBC;
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBriefcaseTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kBriefcaseLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.FeedPrecedingCodeUnit(kEmojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kWomanTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kWomanLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-7, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-7, machine.FinalizeAndGetBoundaryOffset());

  // Followings are not edge cases but good to check.
  // If leading character is not zwj, delete only ZWJ_EMOJI.
  // other + ZWJ_EMOJI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // other(surrogate pairs) + ZWJ_EMOJI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kOtherTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOtherLead));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kHeart));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // other + ZWJ_EMOJI(surrogate pairs)
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOther));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // other(surrogate pairs) + ZWJ_EMOJI(surrogate pairs)
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kOtherTrail));
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kOtherLead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI(surrogate pairs)
  machine.Reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.FeedPrecedingCodeUnit(kManLead));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Followings are edge case.
  // It is hard to list all edge case patterns. Check only over deleting by ZWJ.
  // any + ZWJ: should delete only last ZWJ.
  machine.Reset();
  EXPECT_EQ(kFinished, machine.FeedPrecedingCodeUnit(kZwj));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
}

}  // namespace backspace_state_machine_test

}  // namespace blink

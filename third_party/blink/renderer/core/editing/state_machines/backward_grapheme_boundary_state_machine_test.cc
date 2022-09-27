// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/backward_grapheme_boundary_state_machine.h"

#include "third_party/blink/renderer/core/editing/state_machines/state_machine_test_util.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace backward_grapheme_boundary_state_machine_test {

// Notations:
// SOT indicates start of text.
// [Lead] indicates broken lonely lead surrogate.
// [Trail] indicates broken lonely trail surrogate.
// [U] indicates regional indicator symbol U.
// [S] indicates regional indicator symbol S.

// kWatch kVS16, kEye kVS16 are valid standardized variants.
const UChar32 kWatch = 0x231A;
const UChar32 kEye = WTF::unicode::kEyeCharacter;
const UChar32 kVS16 = 0xFE0F;

// kHanBMP KVS17, kHanSIP kVS17 are valie IVD sequences.
const UChar32 kHanBMP = 0x845B;
const UChar32 kHanSIP = 0x20000;
const UChar32 kVS17 = 0xE0100;

// Following lead/trail values are used for invalid surrogate pairs.
const UChar kLead = 0xD83D;
const UChar kTrail = 0xDC66;

// U+1F1FA is REGIONAL INDICATOR SYMBOL LETTER U
// U+1F1F8 is REGIONAL INDICATOR SYMBOL LETTER S
const UChar32 kRisU = 0x1F1FA;
const UChar32 kRisS = 0x1F1F8;

class BackwardGraphemeBoundaryStatemachineTest
    : public GraphemeStateMachineTestBase {
 public:
  BackwardGraphemeBoundaryStatemachineTest(
      const BackwardGraphemeBoundaryStatemachineTest&) = delete;
  BackwardGraphemeBoundaryStatemachineTest& operator=(
      const BackwardGraphemeBoundaryStatemachineTest&) = delete;

 protected:
  BackwardGraphemeBoundaryStatemachineTest() = default;
  ~BackwardGraphemeBoundaryStatemachineTest() override = default;
};

TEST_F(BackwardGraphemeBoundaryStatemachineTest, DoNothingCase) {
  BackwardGraphemeBoundaryStateMachine machine;

  EXPECT_EQ(0, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(0, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, BrokenSurrogatePair) {
  BackwardGraphemeBoundaryStateMachine machine;

  // [Lead]
  EXPECT_EQ("F", ProcessSequenceBackward(&machine, AsCodePoints(kLead)));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail]
  EXPECT_EQ("RF", ProcessSequenceBackward(&machine, AsCodePoints('a', kTrail)));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail]
  EXPECT_EQ("RF",
            ProcessSequenceBackward(&machine, AsCodePoints(kTrail, kTrail)));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail]
  EXPECT_EQ("RF", ProcessSequenceBackward(&machine, AsCodePoints(kTrail)));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, BreakImmediately_BMP) {
  BackwardGraphemeBoundaryStateMachine machine;

  // U+0000 + U+0000
  EXPECT_EQ("RF", ProcessSequenceBackward(&machine, AsCodePoints(0, 0)));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + 'a'
  EXPECT_EQ("RF", ProcessSequenceBackward(&machine, AsCodePoints('a', 'a')));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + 'a'
  EXPECT_EQ("RRF", ProcessSequenceBackward(&machine, AsCodePoints(kEye, 'a')));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + 'a'
  EXPECT_EQ("RF", ProcessSequenceBackward(&machine, AsCodePoints('a')));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // Broken surrogates.
  // [Lead] + 'a'
  EXPECT_EQ("RF", ProcessSequenceBackward(&machine, AsCodePoints(kLead, 'a')));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + 'a'
  EXPECT_EQ("RRF",
            ProcessSequenceBackward(&machine, AsCodePoints('a', kTrail, 'a')));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + 'a'
  EXPECT_EQ("RRF", ProcessSequenceBackward(&machine,
                                           AsCodePoints(kTrail, kTrail, 'a')));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + 'a'
  EXPECT_EQ("RRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kTrail, 'a')));
  EXPECT_EQ(-1, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       BreakImmediately_SupplementaryPlane) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+1F441
  EXPECT_EQ("RRF", ProcessSequenceBackward(&machine, AsCodePoints('a', kEye)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + U+1F441
  EXPECT_EQ("RRRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kEye, kEye)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + U+1F441
  EXPECT_EQ("RRF", ProcessSequenceBackward(&machine, AsCodePoints(kEye)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // Broken surrogates.
  // [Lead] + U+1F441
  EXPECT_EQ("RRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kLead, kEye)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+1F441
  EXPECT_EQ("RRRF",
            ProcessSequenceBackward(&machine, AsCodePoints('a', kTrail, kEye)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+1F441
  EXPECT_EQ("RRRF", ProcessSequenceBackward(
                        &machine, AsCodePoints(kTrail, kTrail, kEye)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+1F441
  EXPECT_EQ("RRRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kTrail, kEye)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyBefore_BMP_BMP) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+231A + U+FE0F
  EXPECT_EQ("RRF", ProcessSequenceBackward(&machine,
                                           AsCodePoints('a', kWatch, kVS16)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + U+231A + U+FE0F
  EXPECT_EQ("RRRF", ProcessSequenceBackward(&machine,
                                            AsCodePoints(kEye, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + U+231A + U+FE0F
  EXPECT_EQ("RRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kWatch, kVS16)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + U+231A + U+FE0F
  EXPECT_EQ("RRF", ProcessSequenceBackward(&machine,
                                           AsCodePoints(kLead, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+231A + U+FE0F
  EXPECT_EQ("RRRF", ProcessSequenceBackward(
                        &machine, AsCodePoints('a', kTrail, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+231A + U+FE0F
  EXPECT_EQ("RRRF", ProcessSequenceBackward(
                        &machine, AsCodePoints(kTrail, kTrail, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+231A + U+FE0F
  EXPECT_EQ("RRRF", ProcessSequenceBackward(
                        &machine, AsCodePoints(kTrail, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyBefore_Supplementary_BMP) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+1F441 + U+FE0F
  EXPECT_EQ("RRRF",
            ProcessSequenceBackward(&machine, AsCodePoints('a', kEye, kVS16)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + U+1F441 + U+FE0F
  EXPECT_EQ("RRRRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kEye, kEye, kVS16)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + U+1F441 + U+FE0F
  EXPECT_EQ("RRRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kEye, kVS16)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + U+1F441 + U+FE0F
  EXPECT_EQ("RRRF", ProcessSequenceBackward(&machine,
                                            AsCodePoints(kLead, kEye, kVS16)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+1F441 + U+FE0F
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints('a', kTrail, kEye, kVS16)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+1F441 + U+FE0F
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints(kTrail, kTrail, kEye, kVS16)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+1F441 + U+FE0F
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints(kTrail, kEye, kVS16)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyBefore_BMP_Supplementary) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+845B + U+E0100
  EXPECT_EQ("RRRF", ProcessSequenceBackward(&machine,
                                            AsCodePoints('a', kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + U+845B + U+E0100
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints(kEye, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + U+845B + U+E0100
  EXPECT_EQ("RRRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + U+845B + U+E0100
  EXPECT_EQ("RRRF", ProcessSequenceBackward(
                        &machine, AsCodePoints(kLead, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+845B + U+E0100
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints('a', kTrail, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+845B + U+E0100
  EXPECT_EQ("RRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kTrail, kTrail, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+845B + U+E0100
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints(kTrail, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyBefore_Supplementary_Supplementary) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+20000 + U+E0100
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints('a', kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + U+20000 + U+E0100
  EXPECT_EQ("RRRRRF", ProcessSequenceBackward(
                          &machine, AsCodePoints(kEye, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + U+20000 + U+E0100
  EXPECT_EQ("RRRRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + U+20000 + U+E0100
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints(kLead, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+20000 + U+E0100
  EXPECT_EQ("RRRRRF", ProcessSequenceBackward(
                          &machine, AsCodePoints('a', kTrail, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+20000 + U+E0100
  EXPECT_EQ("RRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kTrail, kTrail, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+20000 + U+E0100
  EXPECT_EQ("RRRRRF", ProcessSequenceBackward(
                          &machine, AsCodePoints(kTrail, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, MuchLongerCase) {
  const UChar32 kMan = WTF::unicode::kManCharacter;
  const UChar32 kZwj = WTF::unicode::kZeroWidthJoinerCharacter;
  const UChar32 kHeart = WTF::unicode::kHeavyBlackHeartCharacter;
  const UChar32 kKiss = WTF::unicode::kKissMarkCharacter;

  BackwardGraphemeBoundaryStateMachine machine;

  // U+1F468 U+200D U+2764 U+FE0F U+200D U+1F48B U+200D U+1F468 is a valid ZWJ
  // emoji sequence.
  // 'a' + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints('a', kMan, kZwj, kHeart, kVS16, kZwj,
                                       kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kEye, kMan, kZwj, kHeart, kVS16, kZwj,
                                       kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + ZWJ Emoji Sequence
  EXPECT_EQ(
      "RRRRRRRRRRRF",
      ProcessSequenceBackward(&machine, AsCodePoints(kMan, kZwj, kHeart, kVS16,
                                                     kZwj, kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kLead, kMan, kZwj, kHeart, kVS16, kZwj,
                                       kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints('a', kTrail, kMan, kZwj, kHeart, kVS16,
                                       kZwj, kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kTrail, kTrail, kMan, kZwj, kHeart,
                                       kVS16, kZwj, kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kTrail, kMan, kZwj, kHeart, kVS16, kZwj,
                                       kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, Flags_singleFlag) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + [U] + [S]
  EXPECT_EQ("RRRRF",
            ProcessSequenceBackward(&machine, AsCodePoints('a', kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + [U] + [S]
  EXPECT_EQ("RRRRRF", ProcessSequenceBackward(
                          &machine, AsCodePoints(kEye, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [U] + [S]
  EXPECT_EQ("RRRRF",
            ProcessSequenceBackward(&machine, AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + [U] + [S]
  EXPECT_EQ("RRRRF", ProcessSequenceBackward(
                         &machine, AsCodePoints(kLead, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + [U] + [S]
  EXPECT_EQ("RRRRRF", ProcessSequenceBackward(
                          &machine, AsCodePoints('a', kTrail, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + [U] + [S]
  EXPECT_EQ("RRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kTrail, kTrail, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + [U] + [S]
  EXPECT_EQ("RRRRRF", ProcessSequenceBackward(
                          &machine, AsCodePoints(kTrail, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, Flags_twoFlags) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints('a', kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kEye, kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRF",
            ProcessSequenceBackward(&machine,
                                    AsCodePoints(kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kLead, kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRRF", ProcessSequenceBackward(
                              &machine, AsCodePoints('a', kTrail, kRisU, kRisS,
                                                     kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRRF", ProcessSequenceBackward(
                              &machine, AsCodePoints(kTrail, kTrail, kRisU,
                                                     kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kTrail, kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, Flags_oddNumberedRIS) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRF", ProcessSequenceBackward(
                           &machine, AsCodePoints('a', kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRRF", ProcessSequenceBackward(
                            &machine, AsCodePoints(kEye, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRF", ProcessSequenceBackward(
                           &machine, AsCodePoints(kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRF", ProcessSequenceBackward(
                           &machine, AsCodePoints(kLead, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints('a', kTrail, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRRF",
            ProcessSequenceBackward(
                &machine, AsCodePoints(kTrail, kTrail, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRRF",
            ProcessSequenceBackward(&machine,
                                    AsCodePoints(kTrail, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.FinalizeAndGetBoundaryOffset());
}

}  // namespace backward_grapheme_boundary_state_machine_test

}  // namespace blink

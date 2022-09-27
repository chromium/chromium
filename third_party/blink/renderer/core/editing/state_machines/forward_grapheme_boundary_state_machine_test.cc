// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/forward_grapheme_boundary_state_machine.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/state_machines/state_machine_test_util.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace forward_grapheme_boundary_state_machine_test {

// Notations:
// | indicates inidicates initial offset position.
// SOT indicates start of text.
// EOT indicates end of text.
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
const UChar32 kRisU = 0x1F1FA;
// U+1F1F8 is REGIONAL INDICATOR SYMBOL LETTER S
const UChar32 kRisS = 0x1F1F8;

class ForwardGraphemeBoundaryStatemachineTest
    : public GraphemeStateMachineTestBase {
 public:
  ForwardGraphemeBoundaryStatemachineTest(
      const ForwardGraphemeBoundaryStatemachineTest&) = delete;
  ForwardGraphemeBoundaryStatemachineTest& operator=(
      const ForwardGraphemeBoundaryStatemachineTest&) = delete;

 protected:
  ForwardGraphemeBoundaryStatemachineTest() = default;
  ~ForwardGraphemeBoundaryStatemachineTest() override = default;
};

TEST_F(ForwardGraphemeBoundaryStatemachineTest, DoNothingCase) {
  ForwardGraphemeBoundaryStateMachine machine;

  EXPECT_EQ(0, machine.FinalizeAndGetBoundaryOffset());
  EXPECT_EQ(0, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, PrecedingText) {
  ForwardGraphemeBoundaryStateMachine machine;
  // Preceding text should not affect the result except for flags.
  // SOT + | + 'a' + 'a'
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // SOT + [U] + | + 'a' + 'a'
  EXPECT_EQ("RRSRF", ProcessSequenceForward(&machine, AsCodePoints(kRisU),
                                            AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // SOT + [U] + [S] + | + 'a' + 'a'
  EXPECT_EQ("RRRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints(kRisU, kRisS),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // U+0000 + | + 'a' + 'a'
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints(0),
                                          AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // U+0000 + [U] + | + 'a' + 'a'
  EXPECT_EQ("RRSRF", ProcessSequenceForward(&machine, AsCodePoints(0, kRisU),
                                            AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // U+0000 + [U] + [S] + | + 'a' + 'a'
  EXPECT_EQ("RRRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints(0, kRisU, kRisS),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + | + 'a' + 'a'
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints('a'),
                                          AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // 'a' + [U] + | + 'a' + 'a'
  EXPECT_EQ("RRSRF", ProcessSequenceForward(&machine, AsCodePoints('a', kRisU),
                                            AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // 'a' + [U] + [S] + | + 'a' + 'a'
  EXPECT_EQ("RRRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kRisU, kRisS),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + | + 'a' + 'a'
  EXPECT_EQ("RSRF", ProcessSequenceForward(&machine, AsCodePoints(kEye),
                                           AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // U+1F441 + [U] + | + 'a' + 'a'
  EXPECT_EQ("RRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints(kEye, kRisU),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // U+1F441 + [U] + [S] + | + 'a' + 'a'
  EXPECT_EQ("RRRRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints(kEye, kRisU, kRisS),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // Broken surrogates in preceding text.

  // [Lead] + | + 'a' + 'a'
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints(kLead),
                                          AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // [Lead] + [U] + | + 'a' + 'a'
  EXPECT_EQ("RRSRF",
            ProcessSequenceForward(&machine, AsCodePoints(kLead, kRisU),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // [Lead] + [U] + [S] + | + 'a' + 'a'
  EXPECT_EQ("RRRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints(kLead, kRisU, kRisS),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + | + 'a' + 'a'
  EXPECT_EQ("RSRF", ProcessSequenceForward(&machine, AsCodePoints('a', kTrail),
                                           AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // 'a' + [Trail] + [U] + | + 'a' + 'a'
  EXPECT_EQ("RRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kTrail, kRisU),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // 'a' + [Trail] + [U] + [S] + | + 'a' + 'a'
  EXPECT_EQ("RRRRRSRF", ProcessSequenceForward(
                            &machine, AsCodePoints('a', kTrail, kRisU, kRisS),
                            AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + | + 'a' + 'a'
  EXPECT_EQ("RSRF",
            ProcessSequenceForward(&machine, AsCodePoints(kTrail, kTrail),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // [Trail] + [Trail] + [U] + | + 'a' + 'a'
  EXPECT_EQ("RRRSRF", ProcessSequenceForward(
                          &machine, AsCodePoints(kTrail, kTrail, kRisU),
                          AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // [Trail] + [Trail] + [U] + [S] + | + 'a' + 'a'
  EXPECT_EQ("RRRRRSRF",
            ProcessSequenceForward(&machine,
                                   AsCodePoints(kTrail, kTrail, kRisU, kRisS),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + | + 'a' + 'a'
  EXPECT_EQ("RSRF", ProcessSequenceForward(&machine, AsCodePoints(kTrail),
                                           AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // SOT + [Trail] + [U] + | + 'a' + 'a'
  EXPECT_EQ("RRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints(kTrail, kRisU),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // SOT + [Trail] + [U] + [S] + | + 'a' + 'a'
  EXPECT_EQ("RRRRRSRF",
            ProcessSequenceForward(&machine, AsCodePoints(kTrail, kRisU, kRisS),
                                   AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, BrokenSurrogatePair) {
  ForwardGraphemeBoundaryStateMachine machine;
  // SOT + | + [Trail]
  EXPECT_EQ("SF", ProcessSequenceForward(&machine, AsCodePoints(),
                                         AsCodePoints(kTrail)));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // SOT + | + [Lead] + 'a'
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints(kLead, 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // SOT + | + [Lead] + [Lead]
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints(kLead, kLead)));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
  // SOT + | + [Lead] + EOT
  EXPECT_EQ("SR", ProcessSequenceForward(&machine, AsCodePoints(),
                                         AsCodePoints(kLead)));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, BreakImmediately_BMP) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + | + U+0000 + U+0000
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints(0, 0)));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + 'a' + 'a'
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints('a', 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + 'a' + U+1F441
  EXPECT_EQ("SRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints('a', kEye)));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + 'a' + EOT
  EXPECT_EQ("SR", ProcessSequenceForward(&machine, AsCodePoints(),
                                         AsCodePoints('a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + 'a' + [Trail]
  EXPECT_EQ("SRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints('a', kTrail)));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + 'a' + [Lead] + 'a'
  EXPECT_EQ("SRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints('a', kLead, 'a')));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + 'a' + [Lead] + [Lead]
  EXPECT_EQ("SRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints('a', kLead, kLead)));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + 'a' + [Lead] + EOT
  EXPECT_EQ("SRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints('a', kLead)));
  EXPECT_EQ(1, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest,
       BreakImmediately_Supplementary) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + | + U+1F441 + 'a'
  EXPECT_EQ("SRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints(kEye, 'a')));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + U+1F441
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kEye, kEye)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + EOT
  EXPECT_EQ("SRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints(kEye)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + [Trail]
  EXPECT_EQ("SRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints(kEye, kTrail)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + [Lead] + 'a'
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kEye, kLead, 'a')));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + [Lead] + [Lead]
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kEye, kLead, kLead)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + [Lead] + EOT
  EXPECT_EQ("SRRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints(kEye, kLead)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyAfter_BMP_BMP) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + | + U+231A + U+FE0F + 'a'
  EXPECT_EQ("SRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints(kWatch, kVS16, 'a')));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+231A + U+FE0F + U+1F441
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kWatch, kVS16, kEye)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+231A + U+FE0F + EOT
  EXPECT_EQ("SRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                          AsCodePoints(kWatch, kVS16)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+231A + U+FE0F + [Trail]
  EXPECT_EQ("SRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kWatch, kVS16, kTrail)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+231A + U+FE0F + [Lead] + 'a'
  EXPECT_EQ("SRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kWatch, kVS16, kLead, 'a')));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+231A + U+FE0F + [Lead] + [Lead]
  EXPECT_EQ("SRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kWatch, kVS16, kLead, kLead)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+231A + U+FE0F + [Lead] + EOT
  EXPECT_EQ("SRRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints(kWatch, kVS16, kLead)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyAfter_Supplementary_BMP) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + | + U+1F441 + U+FE0F + 'a'
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kEye, kVS16, 'a')));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + U+FE0F + U+1F441
  EXPECT_EQ("SRRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                             AsCodePoints(kEye, kVS16, kEye)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + U+FE0F + EOT
  EXPECT_EQ("SRRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints(kEye, kVS16)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + U+FE0F + [Trail]
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kEye, kVS16, kTrail)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + U+FE0F + [Lead] + 'a'
  EXPECT_EQ("SRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kEye, kVS16, kLead, 'a')));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + U+FE0F + [Lead] + [Lead]
  EXPECT_EQ("SRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kEye, kVS16, kLead, kLead)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+1F441 + U+FE0F + [Lead] + EOT
  EXPECT_EQ("SRRRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kEye, kVS16, kLead)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyAfter_BMP_Supplementary) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + | + U+845B + U+E0100 + 'a'
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kHanBMP, kVS17, 'a')));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+845B + U+E0100 + U+1F441
  EXPECT_EQ("SRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanBMP, kVS17, kEye)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+845B + U+E0100 + EOT
  EXPECT_EQ("SRRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                           AsCodePoints(kHanBMP, kVS17)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+845B + U+E0100 + [Trail]
  EXPECT_EQ("SRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanBMP, kVS17, kTrail)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+845B + U+E0100 + [Lead] + 'a'
  EXPECT_EQ("SRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanBMP, kVS17, kLead, 'a')));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+845B + U+E0100 + [Lead] + [Lead]
  EXPECT_EQ("SRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanBMP, kVS17, kLead, kLead)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+845B + U+E0100 + [Lead] + EOT
  EXPECT_EQ("SRRRR",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanBMP, kVS17, kLead)));
  EXPECT_EQ(3, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyAfter_Supplementary_Supplementary) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + | + U+20000 + U+E0100 + 'a'
  EXPECT_EQ("SRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanSIP, kVS17, 'a')));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+20000 + U+E0100 + U+1F441
  EXPECT_EQ("SRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanSIP, kVS17, kEye)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+20000 + U+E0100 + EOT
  EXPECT_EQ("SRRRR", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kHanSIP, kVS17)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+20000 + U+E0100 + [Trail]
  EXPECT_EQ("SRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanSIP, kVS17, kTrail)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+20000 + U+E0100 + [Lead] + 'a'
  EXPECT_EQ("SRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanSIP, kVS17, kLead, 'a')));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+20000 + U+E0100 + [Lead] + [Lead]
  EXPECT_EQ("SRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanSIP, kVS17, kLead, kLead)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + U+20000 + U+E0100 + [Lead] + EOT
  EXPECT_EQ("SRRRRR",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kHanSIP, kVS17, kLead)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, MuchLongerCase) {
  ForwardGraphemeBoundaryStateMachine machine;

  const UChar32 kMan = WTF::unicode::kManCharacter;
  const UChar32 kZwj = WTF::unicode::kZeroWidthJoinerCharacter;
  const UChar32 kHeart = WTF::unicode::kHeavyBlackHeartCharacter;
  const UChar32 kKiss = WTF::unicode::kKissMarkCharacter;

  // U+1F468 U+200D U+2764 U+FE0F U+200D U+1F48B U+200D U+1F468 is a valid ZWJ
  // emoji sequence.
  // SOT + | + ZWJ Emoji Sequence + 'a'
  EXPECT_EQ("SRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + ZWJ Emoji Sequence + U+1F441
  EXPECT_EQ("SRRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, kEye)));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + ZWJ Emoji Sequence + EOT
  EXPECT_EQ("SRRRRRRRRRRR",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan)));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + ZWJ Emoji Sequence + [Trail]
  EXPECT_EQ("SRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, kTrail)));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + ZWJ Emoji Sequence + [Lead] + 'a'
  EXPECT_EQ("SRRRRRRRRRRRRF", ProcessSequenceForward(
                                  &machine, AsCodePoints(),
                                  AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                               kKiss, kZwj, kMan, kLead, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + ZWJ Emoji Sequence + [Lead] + [Lead]
  EXPECT_EQ(
      "SRRRRRRRRRRRRF",
      ProcessSequenceForward(&machine, AsCodePoints(),
                             AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                          kKiss, kZwj, kMan, kLead, kLead)));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("SRRRRRRRRRRRR",
            ProcessSequenceForward(&machine, AsCodePoints(),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, kLead)));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // Preceding text should not affect the result except for flags.
  // 'a' + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("SRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints('a'),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("RSRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kEye),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("SRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kLead),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("RSRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kTrail),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("RSRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kTrail, kTrail),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("RSRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kTrail),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [U] + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("RRSRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kRisU),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [U] + [S] + | + ZWJ Emoji Sequence + [Lead] + EOT
  EXPECT_EQ("RRRRSRRRRRRRRRRRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kRisU, kRisS),
                                   AsCodePoints(kMan, kZwj, kHeart, kVS16, kZwj,
                                                kKiss, kZwj, kMan, 'a')));
  EXPECT_EQ(11, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, singleFlags) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + | + [U] + [S]
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(),
                                            AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + | + [U] + [S]
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints('a'),
                                            AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + | + [U] + [S]
  EXPECT_EQ("RSRRRF", ProcessSequenceForward(&machine, AsCodePoints(kEye),
                                             AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + | + [U] + [S]
  EXPECT_EQ("SRRRF", ProcessSequenceForward(&machine, AsCodePoints(kLead),
                                            AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + | + [U] + [S]
  EXPECT_EQ("RSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kTrail),
                                   AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + | + [U] + [S]
  EXPECT_EQ("RSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kTrail, kTrail),
                                   AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + | + [U] + [S]
  EXPECT_EQ("RSRRRF", ProcessSequenceForward(&machine, AsCodePoints(kTrail),
                                             AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, twoFlags) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + [U] + [S] + | + [U] + [S]
  EXPECT_EQ("RRRRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kRisU, kRisS),
                                   AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [U] + [S] + | + [U] + [S]
  EXPECT_EQ("RRRRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kRisU, kRisS),
                                   AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + [U] + [S] + | + [U] + [S]
  EXPECT_EQ("RRRRRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kEye, kRisU, kRisS),
                                   AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + [U] + [S] + | + [U] + [S]
  EXPECT_EQ("RRRRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kLead, kRisU, kRisS),
                                   AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + [U] + [S] + | + [U] + [S]
  EXPECT_EQ("RRRRRSRRRF", ProcessSequenceForward(
                              &machine, AsCodePoints('a', kTrail, kRisU, kRisS),
                              AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + [U] + [S] + | + [U] + [S]
  EXPECT_EQ("RRRRRSRRRF",
            ProcessSequenceForward(&machine,
                                   AsCodePoints(kTrail, kTrail, kRisU, kRisS),
                                   AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + [U] + [S] + | + [U] + [S]
  EXPECT_EQ("RRRRRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kTrail, kRisU, kRisS),
                                   AsCodePoints(kRisU, kRisS)));
  EXPECT_EQ(4, machine.FinalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, oddNumberedFlags) {
  ForwardGraphemeBoundaryStateMachine machine;

  // SOT + [U] + | + [S] + [S]
  EXPECT_EQ("RRSRRRF", ProcessSequenceForward(&machine, AsCodePoints(kRisU),
                                              AsCodePoints(kRisS, kRisU)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [U] + | + [S] + [S]
  EXPECT_EQ("RRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kRisU),
                                   AsCodePoints(kRisS, kRisU)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // U+1F441 + [U] + | + [S] + [S]
  EXPECT_EQ("RRRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kEye, kRisU),
                                   AsCodePoints(kRisS, kRisU)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // [Lead] + [U] + | + [S] + [S]
  EXPECT_EQ("RRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kLead, kRisU),
                                   AsCodePoints(kRisS, kRisU)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + [U] + | + [S] + [S]
  EXPECT_EQ("RRRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints('a', kTrail, kRisU),
                                   AsCodePoints(kRisS, kRisU)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + [U] + | + [S] + [S]
  EXPECT_EQ("RRRSRRRF", ProcessSequenceForward(
                            &machine, AsCodePoints(kTrail, kTrail, kRisU),
                            AsCodePoints(kRisS, kRisU)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());

  // SOT + [Trail] + [U] + | + [S] + [S]
  EXPECT_EQ("RRRSRRRF",
            ProcessSequenceForward(&machine, AsCodePoints(kTrail, kRisU),
                                   AsCodePoints(kRisS, kRisU)));
  EXPECT_EQ(2, machine.FinalizeAndGetBoundaryOffset());
}

}  // namespace forward_grapheme_boundary_state_machine_test

}  // namespace blink

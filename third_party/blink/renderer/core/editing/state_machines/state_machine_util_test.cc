// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/state_machine_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

TEST(StateMachineUtilTest, IsGraphmeBreak_LineBreak) {
  // U+000AD (SOFT HYPHEN) has Control grapheme property.
  const UChar32 kControl = WTF::unicode::kSoftHyphenCharacter;

  // Grapheme Cluster Boundary Rule GB3: CR x LF
  EXPECT_FALSE(IsGraphemeBreak('\r', '\n'));
  EXPECT_TRUE(IsGraphemeBreak('\n', '\r'));

  // Grapheme Cluster Boundary Rule GB4: (Control | CR | LF) ÷
  EXPECT_TRUE(IsGraphemeBreak('\r', 'a'));
  EXPECT_TRUE(IsGraphemeBreak('\n', 'a'));
  EXPECT_TRUE(IsGraphemeBreak(kControl, 'a'));

  // Grapheme Cluster Boundary Rule GB5: ÷ (Control | CR | LF)
  EXPECT_TRUE(IsGraphemeBreak('a', '\r'));
  EXPECT_TRUE(IsGraphemeBreak('a', '\n'));
  EXPECT_TRUE(IsGraphemeBreak('a', kControl));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_Hangul) {
  // U+1100 (HANGUL CHOSEONG KIYEOK) has L grapheme property.
  const UChar32 kL = 0x1100;
  // U+1160 (HANGUL JUNGSEONG FILLER) has V grapheme property.
  const UChar32 kV = 0x1160;
  // U+AC00 (HANGUL SYLLABLE GA) has LV grapheme property.
  const UChar32 kLV = 0xAC00;
  // U+AC01 (HANGUL SYLLABLE GAG) has LVT grapheme property.
  const UChar32 kLVT = 0xAC01;
  // U+11A8 (HANGUL JONGSEONG KIYEOK) has T grapheme property.
  const UChar32 kT = 0x11A8;

  // Grapheme Cluster Boundary Rule GB6: L x (L | V | LV | LVT)
  EXPECT_FALSE(IsGraphemeBreak(kL, kL));
  EXPECT_FALSE(IsGraphemeBreak(kL, kV));
  EXPECT_FALSE(IsGraphemeBreak(kL, kLV));
  EXPECT_FALSE(IsGraphemeBreak(kL, kLVT));
  EXPECT_TRUE(IsGraphemeBreak(kL, kT));

  // Grapheme Cluster Boundary Rule GB7: (LV | V) x (V | T)
  EXPECT_TRUE(IsGraphemeBreak(kV, kL));
  EXPECT_FALSE(IsGraphemeBreak(kV, kV));
  EXPECT_TRUE(IsGraphemeBreak(kV, kLV));
  EXPECT_TRUE(IsGraphemeBreak(kV, kLVT));
  EXPECT_FALSE(IsGraphemeBreak(kV, kT));

  // Grapheme Cluster Boundary Rule GB7: (LV | V) x (V | T)
  EXPECT_TRUE(IsGraphemeBreak(kLV, kL));
  EXPECT_FALSE(IsGraphemeBreak(kLV, kV));
  EXPECT_TRUE(IsGraphemeBreak(kLV, kLV));
  EXPECT_TRUE(IsGraphemeBreak(kLV, kLVT));
  EXPECT_FALSE(IsGraphemeBreak(kLV, kT));

  // Grapheme Cluster Boundary Rule GB8: (LVT | T) x T
  EXPECT_TRUE(IsGraphemeBreak(kLVT, kL));
  EXPECT_TRUE(IsGraphemeBreak(kLVT, kV));
  EXPECT_TRUE(IsGraphemeBreak(kLVT, kLV));
  EXPECT_TRUE(IsGraphemeBreak(kLVT, kLVT));
  EXPECT_FALSE(IsGraphemeBreak(kLVT, kT));

  // Grapheme Cluster Boundary Rule GB8: (LVT | T) x T
  EXPECT_TRUE(IsGraphemeBreak(kT, kL));
  EXPECT_TRUE(IsGraphemeBreak(kT, kV));
  EXPECT_TRUE(IsGraphemeBreak(kT, kLV));
  EXPECT_TRUE(IsGraphemeBreak(kT, kLVT));
  EXPECT_FALSE(IsGraphemeBreak(kT, kT));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_Extend_or_ZWJ) {
  // U+0300 (COMBINING GRAVE ACCENT) has Extend grapheme property.
  const UChar32 kExtend = 0x0300;
  // Grapheme Cluster Boundary Rule GB9: x (Extend | ZWJ)
  EXPECT_FALSE(IsGraphemeBreak('a', kExtend));
  EXPECT_FALSE(IsGraphemeBreak('a', WTF::unicode::kZeroWidthJoinerCharacter));
  EXPECT_FALSE(IsGraphemeBreak(kExtend, kExtend));
  EXPECT_FALSE(IsGraphemeBreak(WTF::unicode::kZeroWidthJoinerCharacter,
                               WTF::unicode::kZeroWidthJoinerCharacter));
  EXPECT_FALSE(
      IsGraphemeBreak(kExtend, WTF::unicode::kZeroWidthJoinerCharacter));
  EXPECT_FALSE(
      IsGraphemeBreak(WTF::unicode::kZeroWidthJoinerCharacter, kExtend));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_SpacingMark) {
  // U+0903 (DEVANAGARI SIGN VISARGA) has SpacingMark grapheme property.
  const UChar32 kSpacingMark = 0x0903;

  // Grapheme Cluster Boundary Rule GB9a: x SpacingMark.
  EXPECT_FALSE(IsGraphemeBreak('a', kSpacingMark));
}

// TODO(nona): Introduce tests for GB9b rule once ICU grabs Unicod 9.0.
// There is no character having Prepend grapheme property in Unicode 8.0.

TEST(StateMachineUtilTest, IsGraphmeBreak_EmojiModifier) {
  // U+261D (WHITE UP POINTING INDEX) has E_Base grapheme property.
  const UChar32 kEBase = 0x261D;
  // U+1F466 (BOY) has E_Base_GAZ grapheme property.
  const UChar32 kEBaseGAZ = 0x1F466;
  // U+1F3FB (EMOJI MODIFIER FITZPATRICK TYPE-1-2) has E_Modifier grapheme
  // property.
  const UChar32 kEModifier = 0x1F3FB;

  // Grapheme Cluster Boundary Rule GB10: (E_Base, E_Base_GAZ) x E_Modifier
  EXPECT_FALSE(IsGraphemeBreak(kEBase, kEModifier));
  EXPECT_FALSE(IsGraphemeBreak(kEBaseGAZ, kEModifier));
  EXPECT_FALSE(IsGraphemeBreak(kEBase, kEModifier));

  EXPECT_TRUE(IsGraphemeBreak(kEBase, kEBase));
  EXPECT_TRUE(IsGraphemeBreak(kEBaseGAZ, kEBase));
  EXPECT_TRUE(IsGraphemeBreak(kEBase, kEBaseGAZ));
  EXPECT_TRUE(IsGraphemeBreak(kEBaseGAZ, kEBaseGAZ));
  // EModifier is absorbed into Extend and there is NO break
  // before Extend per GB 9.
  EXPECT_FALSE(IsGraphemeBreak(kEModifier, kEModifier));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_ZWJSequecne) {
  // U+2764 (HEAVY BLACK HEART) has Glue_After_Zwj grapheme property.
  const UChar32 kGlueAfterZwj = 0x2764;
  // U+1F466 (BOY) has E_Base_GAZ grapheme property.
  const UChar32 kEBaseGAZ = 0x1F466;
  // U+1F5FA (WORLD MAP) doesn'T have Glue_After_Zwj or E_Base_GAZ property
  // but has Emoji property.
  const UChar32 kEmoji = 0x1F5FA;

  // Grapheme Cluster Boundary Rule GB11: ZWJ x (Glue_After_Zwj | EBG)
  EXPECT_FALSE(
      IsGraphemeBreak(WTF::unicode::kZeroWidthJoinerCharacter, kGlueAfterZwj));
  EXPECT_FALSE(
      IsGraphemeBreak(WTF::unicode::kZeroWidthJoinerCharacter, kEBaseGAZ));
  EXPECT_FALSE(
      IsGraphemeBreak(WTF::unicode::kZeroWidthJoinerCharacter, kEmoji));

  EXPECT_TRUE(IsGraphemeBreak(kGlueAfterZwj, kEBaseGAZ));
  EXPECT_TRUE(IsGraphemeBreak(kGlueAfterZwj, kGlueAfterZwj));
  EXPECT_TRUE(IsGraphemeBreak(kEBaseGAZ, kGlueAfterZwj));

  EXPECT_TRUE(IsGraphemeBreak(WTF::unicode::kZeroWidthJoinerCharacter, 'a'));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_IndicSyllabicCategoryVirama) {
  // U+094D (DEVANAGARI SIGN VIRAMA) has Indic_Syllabic_Category=Virama
  // property.
  const UChar32 kVirama = 0x094D;

  // U+0915 (DEVANAGARI LETTER KA). Should not break after kVirama and before
  // this character.
  const UChar32 kDevangariKa = 0x0915;

  // Do not break after character having Indic_Syllabic_Category=Virama
  // property if following character has General_Category=C(Other) property.
  EXPECT_FALSE(IsGraphemeBreak(kVirama, kDevangariKa));

  // Tamil virama is an exception (crbug.com/693697).
  const UChar32 kTamilVirama = 0x0BCD;
  EXPECT_TRUE(IsGraphemeBreak(kTamilVirama, kDevangariKa));
}

}  // namespace blink

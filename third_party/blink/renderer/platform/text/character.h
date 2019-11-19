/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/character_property.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PLATFORM_EXPORT Character {
  STATIC_ONLY(Character);

 public:
  static inline bool IsInRange(UChar32 character,
                               UChar32 lower_bound,
                               UChar32 upper_bound) {
    return character >= lower_bound && character <= upper_bound;
  }

  static inline bool IsUnicodeVariationSelector(UChar32 character) {
    // http://www.unicode.org/Public/UCD/latest/ucd/StandardizedVariants.html
    return IsInRange(character, 0x180B,
                     0x180D)  // MONGOLIAN FREE VARIATION SELECTOR ONE to THREE
           ||
           IsInRange(character, 0xFE00, 0xFE0F)  // VARIATION SELECTOR-1 to 16
           || IsInRange(character, 0xE0100,
                        0xE01EF);  // VARIATION SELECTOR-17 to 256
  }

  static bool IsCJKIdeographOrSymbol(UChar32 c) {
    // Below U+02C7 is likely a common case.
    return c < 0x2C7 ? false : IsCJKIdeographOrSymbolSlow(c);
  }
  static bool IsCJKIdeographOrSymbolBase(UChar32 c) {
    return IsCJKIdeographOrSymbol(c) &&
           !(U_GET_GC_MASK(c) & (U_GC_M_MASK | U_GC_LM_MASK | U_GC_SK_MASK));
  }

  static bool IsHangul(UChar32 c) {
    // Below U+1100 is likely a common case.
    return c < 0x1100 ? false : IsHangulSlow(c);
  }

  static unsigned ExpansionOpportunityCount(base::span<const LChar>,
                                            TextDirection,
                                            bool& is_after_expansion,
                                            const TextJustify);
  static unsigned ExpansionOpportunityCount(base::span<const UChar>,
                                            TextDirection,
                                            bool& is_after_expansion,
                                            const TextJustify);
  static unsigned ExpansionOpportunityCount(const TextRun& run,
                                            bool& is_after_expansion) {
    if (run.Is8Bit())
      return ExpansionOpportunityCount(run.Span8(), run.Direction(),
                                       is_after_expansion,
                                       run.GetTextJustify());
    return ExpansionOpportunityCount(run.Span16(), run.Direction(),
                                     is_after_expansion, run.GetTextJustify());
  }

  static bool IsUprightInMixedVertical(UChar32 character);

  // https://html.spec.whatwg.org/C/#prod-potentialcustomelementname
  static bool IsPotentialCustomElementName8BitChar(LChar ch) {
    return IsASCIILower(ch) || IsASCIIDigit(ch) || ch == '-' || ch == '.' ||
           ch == '_' || ch == 0xb7 || (0xc0 <= ch && ch != 0xd7 && ch != 0xf7);
  }
  static bool IsPotentialCustomElementNameChar(UChar32 character);

  // http://unicode.org/reports/tr9/#Directional_Formatting_Characters
  static bool IsBidiControl(UChar32 character);

  static bool TreatAsSpace(UChar32 c) {
    return c == kSpaceCharacter || c == kTabulationCharacter ||
           c == kNewlineCharacter || c == kNoBreakSpaceCharacter;
  }
  static bool TreatAsZeroWidthSpace(UChar32 c) {
    return TreatAsZeroWidthSpaceInComplexScript(c) ||
           c == kZeroWidthNonJoinerCharacter || c == kZeroWidthJoinerCharacter;
  }
  static bool LegacyTreatAsZeroWidthSpaceInComplexScript(UChar32 c) {
    return c < 0x20  // ASCII Control Characters
           ||
           (c >= 0x7F && c < 0xA0)  // ASCII Delete .. No-break spaceCharacter
           || TreatAsZeroWidthSpaceInComplexScript(c);
  }
  static bool TreatAsZeroWidthSpaceInComplexScript(UChar32 c) {
    return c == kFormFeedCharacter || c == kCarriageReturnCharacter ||
           c == kSoftHyphenCharacter || c == kZeroWidthSpaceCharacter ||
           (c >= kLeftToRightMarkCharacter && c <= kRightToLeftMarkCharacter) ||
           (c >= kLeftToRightEmbedCharacter &&
            c <= kRightToLeftOverrideCharacter) ||
           c == kZeroWidthNoBreakSpaceCharacter ||
           c == kObjectReplacementCharacter;
  }
  static bool CanTextDecorationSkipInk(UChar32);
  static bool CanReceiveTextEmphasis(UChar32);

  static bool IsGraphemeExtended(UChar32 c) {
    // http://unicode.org/reports/tr29/#Extend
    return u_hasBinaryProperty(c, UCHAR_GRAPHEME_EXTEND);
  }

  // Returns true if the character has a Emoji property.
  // See http://www.unicode.org/Public/emoji/3.0/emoji-data.txt
  static bool IsEmoji(UChar32);
  // Default presentation style according to:
  // http://www.unicode.org/reports/tr51/#Presentation_Style
  static bool IsEmojiTextDefault(UChar32);
  static bool IsEmojiEmojiDefault(UChar32);
  static bool IsEmojiModifierBase(UChar32);
  static inline bool IsEmojiKeycapBase(UChar32 ch) {
    return (ch >= '0' && ch <= '9') || ch == '#' || ch == '*';
  }
  static bool IsRegionalIndicator(UChar32);
  static bool IsModifier(UChar32 c) { return c >= 0x1F3FB && c <= 0x1F3FF; }
  // http://www.unicode.org/reports/tr51/proposed.html#flag-emoji-tag-sequences
  static bool IsEmojiTagSequence(UChar32);

  static inline UChar NormalizeSpaces(UChar character) {
    if (TreatAsSpace(character))
      return kSpaceCharacter;

    if (TreatAsZeroWidthSpace(character))
      return kZeroWidthSpaceCharacter;

    return character;
  }

  static inline bool IsNormalizedCanvasSpaceCharacter(UChar32 c) {
    // According to specification all space characters should be replaced with
    // 0x0020 space character.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-canvas-element.html#text-preparation-algorithm
    // The space characters according to specification are : U+0020, U+0009,
    // U+000A, U+000C, and U+000D.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#space-character
    // This function returns true for 0x000B also, so that this is backward
    // compatible.  Otherwise, the test
    // web_tests/canvas/philip/tests/2d.text.draw.space.collapse.space.html
    // will fail
    return c == 0x0009 || (c >= 0x000A && c <= 0x000D);
  }

  static String NormalizeSpaces(const LChar*, unsigned length);
  static String NormalizeSpaces(const UChar*, unsigned length);

  static bool IsCommonOrInheritedScript(UChar32);
  static bool IsPrivateUse(UChar32);
  static bool IsNonCharacter(UChar32);

  // Returns whether a script code could be determined for the given character
  // and that script code is not USCRIPT_COMMON or USCRIPT_INHERITED.
  static bool HasDefiniteScript(UChar32);

  static bool IsModernGeorgianUppercase(UChar32 c) {
    return IsInRange(c, 0x1C90, 0x1CBF);
  }

  // Map modern secular Georgian uppercase letters added in Unicode
  // 11.0 to their corresponding lowercase letters.
  // https://www.unicode.org/charts/PDF/U10A0.pdf
  // https://www.unicode.org/charts/PDF/U1C90.pdf
  static UChar32 LowercaseModernGeorgianUppercase(UChar32 c) {
    return (c - (0x1C90 - 0x10D0));
  }

 private:
  static bool IsCJKIdeographOrSymbolSlow(UChar32);
  static bool IsHangulSlow(UChar32);
};

}  // namespace blink

#endif

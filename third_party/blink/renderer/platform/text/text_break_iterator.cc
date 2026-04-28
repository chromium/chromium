/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

#include <unicode/uchar.h>
#include <unicode/uvernum.h>

#include <array>

#include "third_party/blink/renderer/platform/text/break_iterator_data_inline_header.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

#define BA_LB_COUNT U_LB_COUNT
// Line breaking table for CSS word-break: break-all. This table differs from
// asciiLineBreakTable in:
// - Indices are Line Breaking Classes defined in UAX#14 Unicode Line Breaking
//   Algorithm: http://unicode.org/reports/tr14/#DescriptionOfProperties
// - 1 indicates additional break opportunities. 0 indicates to fallback to
//   normal line break, not "prohibit break."
// clang-format off
static constexpr std::array<std::array<unsigned char, BA_LB_COUNT / 8 + 1>,
                            BA_LB_COUNT>
    kBreakAllLineBreakClassTable = {{
  // XX AI AL B2 BA BB BK CB            SA SG SP SY ZW NL WJ H2             HH
  //            CL CM CR EX GL HY ID IN             H3 JL JT JV CP CJ HL RI
  //                        IS LF NS NU OP PO PR QU             EB EM ZWJ AK AP AS VF VI
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // XX
  { 0b01101000, 0b00000100, 0b00011010, 0b10000000, 0b00000010, 0b00000000, 0b00000000 }, // AI
  { 0b01101000, 0b00000100, 0b00011010, 0b10000000, 0b00000010, 0b00000000, 0b00000000 }, // AL
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // B2
  { 0b01101000, 0b00000100, 0b00011010, 0b10000000, 0b00000010, 0b00000000, 0b00000000 }, // BA
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // BB
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // BK
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // CB
  { 0b01101000, 0b00000100, 0b00010010, 0b00000000, 0b00000010, 0b00000000, 0b00000000 }, // CL
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // CM
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // CR
  { 0b01101000, 0b00000100, 0b00010110, 0b00000000, 0b00000010, 0b00000000, 0b00000000 }, // EX
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // GL
  { 0b00000000, 0b00000000, 0b00010000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // HY
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // ID
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // IN
  { 0b01101000, 0b00000100, 0b00010000, 0b00000000, 0b00000010, 0b00000000, 0b00000000 }, // IS
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // LF
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // NS
  { 0b01101000, 0b00000100, 0b00011010, 0b10000000, 0b00000010, 0b00000000, 0b00000000 }, // NUx
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // OP
  { 0b01101000, 0b00000100, 0b00010110, 0b00000000, 0b00000010, 0b00000000, 0b00000000 }, // PO
  { 0b00000000, 0b00000000, 0b00000100, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // PR
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // QU
  { 0b01101000, 0b00000100, 0b00011010, 0b10000000, 0b00000010, 0b00000000, 0b00000000 }, // SA
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // SG
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // SP
  { 0b01101000, 0b00000100, 0b00011010, 0b10000000, 0b00000010, 0b00000000, 0b00000000 }, // SY
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // ZW
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // NL
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // WJ
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // H2
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // H3
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // JL
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // JT
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // JV
  { 0b01101000, 0b00000100, 0b00010010, 0b10000000, 0b00000010, 0b00000000, 0b00000000 }, // CP
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // CJ
  { 0b01101000, 0b00000100, 0b00011010, 0b10000000, 0b00000010, 0b00000000, 0b00000000 }, // HL
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // RI
  // Added in ICU 58.
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // EB
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // EM
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // ZWJ
#if U_ICU_VERSION_MAJOR_NUM >= 74
  // Added in ICU 74. https://icu.unicode.org/download/74
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // AK
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // AP
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // AS
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // VF
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // VI
#endif  // U_ICU_VERSION_MAJOR_NUM >= 74
#if U_ICU_VERSION_MAJOR_NUM >= 78
  // Added in ICU 78, see uchar.h and https://www.unicode.org/reports/tr14/#HH
  { 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000 }, // HH
#endif
}};
// clang-format on

static_assert(std::size(kBreakAllLineBreakClassTable) == BA_LB_COUNT,
              "breakAllLineBreakClassTable should be consistent");

static inline ULineBreak LineBreakPropertyValue(UChar last_ch, UChar ch) {
  if (ch == '+')  // IE tailors '+' to AL-like class when break-all is enabled.
    return U_LB_ALPHABETIC;
  UChar32 ch32 = U16_IS_LEAD(last_ch) && U16_IS_TRAIL(ch)
                     ? U16_GET_SUPPLEMENTARY(last_ch, ch)
                     : ch;
  return static_cast<ULineBreak>(u_getIntPropertyValue(ch32, UCHAR_LINE_BREAK));
}

static inline bool ShouldBreakAfterBreakAll(ULineBreak last_line_break,
                                            ULineBreak line_break,
                                            UChar ch,
                                            LineBreakStrictness strictness) {
  if (line_break >= 0 && line_break < BA_LB_COUNT && last_line_break >= 0 &&
      last_line_break < BA_LB_COUNT) {
    const size_t last_line_break_index = last_line_break;
    const size_t line_break_index = line_break;
    if (!(kBreakAllLineBreakClassTable[last_line_break_index]
                                      [line_break_index / 8] &
          (0x80 >> (line_break_index % 8)))) {
      return false;
    }
    // LB21: Do not break before BA (Break After) class characters
    // (e.g., U+1361 Ethiopic Wordspace, U+007C Vertical Line),
    // except U+007C (Vertical Line) which is not relevant for this case.
    // Allow break only when line-break:loose relaxes LB21 for hyphens.
    if (line_break == U_LB_BREAK_AFTER && ch != 0x007C &&
        strictness != LineBreakStrictness::kLoose) {
      return false;
    }
    return true;
  }
  return false;
}

// Computes if 'word-break:keep-all' should prevent line break.
// https://drafts.csswg.org/css-text-3/#valdef-word-break-keep-all
// The spec is not very verbose on how this should work. This logic prevents L/M
// general categories and complex line breaking since the spec says "except some
// south east aisans".
// https://github.com/w3c/csswg-drafts/issues/1619
static inline bool ShouldKeepAfterKeepAll(UChar last_ch,
                                          UChar ch,
                                          UChar next_ch) {
  UChar pre_ch = U_MASK(u_charType(ch)) & U_GC_M_MASK ? last_ch : ch;
  return U_MASK(u_charType(pre_ch)) & (U_GC_L_MASK | U_GC_N_MASK) &&
         !unicode::HasLineBreakingPropertyComplexContext(pre_ch) &&
         U_MASK(u_charType(next_ch)) & (U_GC_L_MASK | U_GC_N_MASK) &&
         !unicode::HasLineBreakingPropertyComplexContext(next_ch);
}

enum class FastBreakResult : uint8_t { kNoBreak, kCanBreak, kUnknown };

template <typename CharacterType>
struct LazyLineBreakIterator::Context {
  STACK_ALLOCATED();

 public:
  struct ContextChar {
    STACK_ALLOCATED();

   public:
    ContextChar() = default;
    explicit ContextChar(UChar ch) : ch(ch), is_space(IsBreakableSpace(ch)) {}

    UChar ch = 0;
    bool is_space = false;
  };

  Context(const CharacterType* str,
          unsigned len,
          unsigned start_offset,
          unsigned index) {
    DCHECK_GE(index, start_offset);
    CHECK_LE(index, len);
    if (index > start_offset) {
      // SAFETY: `index <= len`, and `index > start_offset` guarantees
      // `index - 1` is a valid character within `str`.
      last = ContextChar(UNSAFE_BUFFERS(str[index - 1]));
      if (index > start_offset + 1) {
        last_last_ch = UNSAFE_BUFFERS(str[index - 2]);
      }
    }
  }

  bool Fetch(const CharacterType* str, unsigned len, unsigned index) {
    if (index >= len) [[unlikely]] {
      return false;
    }
    // SAFETY: `index < len` was checked above.
    current = ContextChar(UNSAFE_BUFFERS(str[index]));
    return true;
  }

  void Advance(unsigned& index) {
    ++index;
    last_last_ch = last.ch;
    last = current;
  }

  FastBreakResult ShouldBreakFast(bool disable_soft_hyphen) const {
    const UChar last_ch = last.ch;
    const UChar ch = current.ch;
    if (last_ch < kFastLineBreakMinChar || ch < kFastLineBreakMinChar)
        [[unlikely]] {
      return FastBreakResult::kNoBreak;
    }

    // U+002D HYPHEN-MINUS may depend on the context.
    static_assert('-' >= kFastLineBreakMinChar);
    if (last_ch == '-') [[unlikely]] {
      if (ch <= 0x7F) {
        // Up to U+007F is fast-breakable. See `LineBreakData::FillAscii()`.
        if (IsAsciiDigit(ch)) {
          // Don't allow line breaking between '-' and a digit if the '-' may
          // mean a minus sign in the context, while allow breaking in
          // 'ABCD-1234' and '1234-5678' which may be in long URLs.
          return IsAsciiAlphanumeric(last_last_ch) ? FastBreakResult::kCanBreak
                                                   : FastBreakResult::kNoBreak;
        }
      } else {
        // Defer to the Unicode algorithm to take more context into account.
        return FastBreakResult::kUnknown;
      }
    }

    // If both characters are in the fast line break table, use it for enhanced
    // speed. For ASCII characters, it is also for compatibility. The table is
    // generated at the build time, see the `LineBreakData` class.
    if (last_ch <= kFastLineBreakMaxChar && ch <= kFastLineBreakMaxChar) {
      if (!GetFastLineBreak(last_ch, ch)) {
        return FastBreakResult::kNoBreak;
      }
      static_assert(uchar::kSoftHyphen <= kFastLineBreakMaxChar);
      if (disable_soft_hyphen && last_ch == uchar::kSoftHyphen) [[unlikely]] {
        return FastBreakResult::kNoBreak;
      }
      return FastBreakResult::kCanBreak;
    }

    // Otherwise defer to the Unicode algorithm.
    static_assert(uchar::kNoBreakSpace <= kFastLineBreakMaxChar,
                  "Include NBSP for the performance.");
    return FastBreakResult::kUnknown;
  }

  ContextChar current;
  ContextChar last;
  CharacterType last_last_ch = 0;
};

template <typename CharacterType,
          LineBreakType line_break_type,
          BreakSpaceType break_space>
inline unsigned LazyLineBreakIterator::NextBreakablePosition(
    unsigned pos,
    base::span<const CharacterType> span) const {
  const CharacterType* str = span.data();
  unsigned len = span.size();
  Context<CharacterType> context(str, len, start_offset_, pos);
  unsigned next_break = 0;
  ULineBreak last_line_break;
  if constexpr (line_break_type == LineBreakType::kBreakAll) {
    last_line_break =
        LineBreakPropertyValue(context.last_last_ch, context.last.ch);
  }
  for (unsigned i = pos; context.Fetch(str, len, i); context.Advance(i)) {
    switch (break_space) {
      case BreakSpaceType::kAfterSpaceRun:
        if (context.current.is_space) {
          continue;
        }
        if (context.last.is_space) {
          return i;
        }
        break;
      case BreakSpaceType::kAfterEverySpace:
        if (context.last.is_space ||
            Character::IsOtherSpaceSeparator(context.last.ch)) {
          return i;
        }
        if ((context.current.is_space ||
             Character::IsOtherSpaceSeparator(context.current.ch)) &&
            i + 1 < len) {
          return i + 1;
        }
        break;
    }

    const FastBreakResult fast_break_result =
        context.ShouldBreakFast(disable_soft_hyphen_);
    if (fast_break_result == FastBreakResult::kCanBreak) {
      return i;
    }

    if constexpr (line_break_type == LineBreakType::kBreakAll) {
      if (!U16_IS_LEAD(context.current.ch)) {
        // https://drafts.csswg.org/css-text-4/#line-break-property
        // * The following breaks are allowed for 'loose' line breaking if the
        //   preceding character belongs to the Unicode line breaking class ID
        //   ...:
        //   breaks before hyphens:
        //   U+2010, U+2013
        if (strictness_ == LineBreakStrictness::kLoose &&
            (context.current.ch == uchar::kHyphen ||
             context.current.ch == uchar::kEnDash) &&
            (last_line_break == U_LB_NUMERIC ||
             last_line_break == U_LB_ALPHABETIC ||
             last_line_break == U_LB_COMPLEX_CONTEXT ||
             last_line_break == U_LB_IDEOGRAPHIC)) {
          return i;
        }
        ULineBreak line_break =
            LineBreakPropertyValue(context.last.ch, context.current.ch);
        if (ShouldBreakAfterBreakAll(last_line_break, line_break,
                                     context.current.ch, strictness_)) {
          return i > pos && U16_IS_TRAIL(context.current.ch) ? i - 1 : i;
        }
        if (line_break != U_LB_COMBINING_MARK) {
          last_line_break = line_break;
        }
      }
    } else if constexpr (line_break_type == LineBreakType::kKeepAll) {
      if (ShouldKeepAfterKeepAll(context.last_last_ch, context.last.ch,
                                 context.current.ch)) {
        // word-break:keep-all prevents breaks between East Asian ideographic.
        continue;
      }
    }

    if (fast_break_result == FastBreakResult::kNoBreak) {
      continue;
    }

    if (next_break < i || !next_break) {
      // Don't break if positioned at start of primary context.
      if (i <= start_offset_) [[unlikely]] {
        continue;
      }
      TextBreakIterator* break_iterator = GetIterator();
      if (!break_iterator) [[unlikely]] {
        continue;
      }
      next_break = i - 1;
      for (;;) {
        // Adjust the offset by |start_offset_| because |break_iterator|
        // has text after |start_offset_|.
        DCHECK_GE(next_break, start_offset_);
        const int32_t following = break_iterator->following(
            static_cast<int32_t>(next_break - start_offset_));
        if (following < 0) [[unlikely]] {
          DCHECK_EQ(following, icu::BreakIterator::DONE);
          next_break = len;
          break;
        }
        next_break = following + start_offset_;
        if (disable_soft_hyphen_ && next_break > 0 && next_break <= len &&
            // SAFETY: `next_break` is checked to be within `(0, len]`.
            UNSAFE_BUFFERS(str[next_break - 1]) == uchar::kSoftHyphen)
            [[unlikely]] {
          continue;
        }
        break;
      }
    }
    if (i == next_break && !context.last.is_space) {
      return i;
    }
  }

  return len;
}

template <typename CharacterType, LineBreakType lineBreakType>
inline unsigned LazyLineBreakIterator::NextBreakablePosition(
    unsigned pos,
    base::span<const CharacterType> span) const {
  switch (break_space_) {
    case BreakSpaceType::kAfterSpaceRun:
      return NextBreakablePosition<CharacterType, lineBreakType,
                                   BreakSpaceType::kAfterSpaceRun>(pos, span);
    case BreakSpaceType::kAfterEverySpace:
      return NextBreakablePosition<CharacterType, lineBreakType,
                                   BreakSpaceType::kAfterEverySpace>(pos, span);
  }
  NOTREACHED();
}

template <LineBreakType lineBreakType>
inline unsigned LazyLineBreakIterator::NextBreakablePosition(
    unsigned pos,
    unsigned len) const {
  if (string_.IsNull()) [[unlikely]] {
    return 0;
  }
  if (string_.Is8Bit()) {
    return NextBreakablePosition<LChar, lineBreakType>(
        pos, string_.Span8().first(len));
  }
  return NextBreakablePosition<UChar, lineBreakType>(
      pos, string_.Span16().first(len));
}

unsigned LazyLineBreakIterator::NextBreakablePositionBreakCharacter(
    unsigned pos) const {
  DCHECK_LE(start_offset_, string_.length());
  CharacterBreakIterator& iterator = GetCharacterBreakIterator();
  DCHECK_GE(pos, start_offset_);
  pos -= start_offset_;
  // `- 1` because the `Following()` returns the next opportunity after the
  // given `offset`.
  int32_t next =
      iterator.Following(static_cast<int32_t>(pos > 0 ? pos - 1 : 0));
  return next != kTextBreakDone ? next + start_offset_ : string_.length();
}

unsigned LazyLineBreakIterator::NextBreakablePosition(unsigned pos,
                                                      unsigned len) const {
  switch (break_type_) {
    case LineBreakType::kNormal:
    case LineBreakType::kPhrase:
      return NextBreakablePosition<LineBreakType::kNormal>(pos, len);
    case LineBreakType::kBreakAll:
      return NextBreakablePosition<LineBreakType::kBreakAll>(pos, len);
    case LineBreakType::kKeepAll:
      return NextBreakablePosition<LineBreakType::kKeepAll>(pos, len);
    case LineBreakType::kBreakCharacter:
      return NextBreakablePositionBreakCharacter(pos);
  }
  NOTREACHED();
}

unsigned LazyLineBreakIterator::NextBreakOpportunity(unsigned offset) const {
  DCHECK_LE(offset, string_.length());
  return NextBreakablePosition(offset, string_.length());
}

unsigned LazyLineBreakIterator::NextBreakOpportunity(unsigned offset,
                                                     unsigned len) const {
  DCHECK_LE(offset, len);
  DCHECK_LE(len, string_.length());
  return NextBreakablePosition(offset, len);
}

unsigned LazyLineBreakIterator::PreviousBreakOpportunity(unsigned offset,
                                                         unsigned min) const {
  unsigned pos = std::min(offset, string_.length());
  // +2 to ensure at least one code point is included.
  unsigned end = std::min(pos + 2, string_.length());
  const UChar* chars16 = string_.Is8Bit() ? nullptr : string_.Span16().data();
  while (pos > min) {
    unsigned next_break = NextBreakablePosition(pos, end);
    if (next_break == pos) {
      return next_break;
    }

    // There's no break opportunities at |pos| or after.
    end = pos;
    if (!chars16) {
      --pos;
    } else {
      // We don't use string_.Span16() here for performance reasons.
      // SAFETY: U16_BACK_1() accesses `pos - 1` and `pos - 2`, and `pos` is
      // <= string_.length().
      UNSAFE_BUFFERS(U16_BACK_1(chars16, 0, pos));
    }
  }
  return min;
}

std::ostream& operator<<(std::ostream& ostream, LineBreakType line_break_type) {
  switch (line_break_type) {
    case LineBreakType::kNormal:
      return ostream << "Normal";
    case LineBreakType::kBreakAll:
      return ostream << "BreakAll";
    case LineBreakType::kBreakCharacter:
      return ostream << "BreakCharacter";
    case LineBreakType::kKeepAll:
      return ostream << "KeepAll";
    case LineBreakType::kPhrase:
      return ostream << "Phrase";
  }
  NOTREACHED() << "LineBreakType::" << static_cast<int>(line_break_type);
}

std::ostream& operator<<(std::ostream& ostream, BreakSpaceType break_space) {
  switch (break_space) {
    case BreakSpaceType::kAfterSpaceRun:
      return ostream << "kAfterSpaceRun";
    case BreakSpaceType::kAfterEverySpace:
      return ostream << "kAfterEverySpace";
  }
  NOTREACHED() << "BreakSpaceType::" << static_cast<int>(break_space);
}

}  // namespace blink

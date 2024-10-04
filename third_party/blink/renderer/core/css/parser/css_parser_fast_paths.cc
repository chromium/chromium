// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"

#ifdef __SSE2__
#include <immintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include "build/build_config.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_revert_layer_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

static unsigned ParsePositiveDouble(const LChar* string,
                                    const LChar* end,
                                    double& value);

static bool ParseDoubleWithPrefix(const LChar* string,
                                  const LChar* end,
                                  double& value);

static inline bool IsSimpleLengthPropertyID(CSSPropertyID property_id,
                                            bool& accepts_negative_numbers) {
  static CSSBitset properties{{
      CSSPropertyID::kBlockSize,
      CSSPropertyID::kInlineSize,
      CSSPropertyID::kMinBlockSize,
      CSSPropertyID::kMinInlineSize,
      CSSPropertyID::kFontSize,
      CSSPropertyID::kHeight,
      CSSPropertyID::kWidth,
      CSSPropertyID::kMinHeight,
      CSSPropertyID::kMinWidth,
      CSSPropertyID::kPaddingBottom,
      CSSPropertyID::kPaddingLeft,
      CSSPropertyID::kPaddingRight,
      CSSPropertyID::kPaddingTop,
      CSSPropertyID::kScrollPaddingBlockEnd,
      CSSPropertyID::kScrollPaddingBlockStart,
      CSSPropertyID::kScrollPaddingBottom,
      CSSPropertyID::kScrollPaddingInlineEnd,
      CSSPropertyID::kScrollPaddingInlineStart,
      CSSPropertyID::kScrollPaddingLeft,
      CSSPropertyID::kScrollPaddingRight,
      CSSPropertyID::kScrollPaddingTop,
      CSSPropertyID::kPaddingBlockEnd,
      CSSPropertyID::kPaddingBlockStart,
      CSSPropertyID::kPaddingInlineEnd,
      CSSPropertyID::kPaddingInlineStart,
      CSSPropertyID::kShapeMargin,
      CSSPropertyID::kR,
      CSSPropertyID::kRx,
      CSSPropertyID::kRy,
      CSSPropertyID::kBottom,
      CSSPropertyID::kCx,
      CSSPropertyID::kCy,
      CSSPropertyID::kLeft,
      CSSPropertyID::kMarginBottom,
      CSSPropertyID::kMarginLeft,
      CSSPropertyID::kMarginRight,
      CSSPropertyID::kMarginTop,
      CSSPropertyID::kOffsetDistance,
      CSSPropertyID::kRight,
      CSSPropertyID::kTop,
      CSSPropertyID::kMarginBlockEnd,
      CSSPropertyID::kMarginBlockStart,
      CSSPropertyID::kMarginInlineEnd,
      CSSPropertyID::kMarginInlineStart,
      CSSPropertyID::kX,
      CSSPropertyID::kY,
  }};
  // A subset of the above.
  static CSSBitset accept_negative{
      {CSSPropertyID::kBottom, CSSPropertyID::kCx, CSSPropertyID::kCy,
       CSSPropertyID::kLeft, CSSPropertyID::kMarginBottom,
       CSSPropertyID::kMarginLeft, CSSPropertyID::kMarginRight,
       CSSPropertyID::kMarginTop, CSSPropertyID::kOffsetDistance,
       CSSPropertyID::kRight, CSSPropertyID::kTop,
       CSSPropertyID::kMarginBlockEnd, CSSPropertyID::kMarginBlockStart,
       CSSPropertyID::kMarginInlineEnd, CSSPropertyID::kMarginInlineStart,
       CSSPropertyID::kX, CSSPropertyID::kY}};

  accepts_negative_numbers = accept_negative.Has(property_id);
  if (accepts_negative_numbers) {
    DCHECK(properties.Has(property_id));
  }
  return properties.Has(property_id);
}

ALWAYS_INLINE static bool ParseSimpleLength(const LChar* characters,
                                            unsigned length,
                                            CSSPrimitiveValue::UnitType& unit,
                                            double& number) {
  if (length > 2 && (characters[length - 2] | 0x20) == 'p' &&
      (characters[length - 1] | 0x20) == 'x') {
    length -= 2;
    unit = CSSPrimitiveValue::UnitType::kPixels;
  } else if (length > 1 && characters[length - 1] == '%') {
    length -= 1;
    unit = CSSPrimitiveValue::UnitType::kPercentage;
  }

  // We rely on ParseDoubleWithPrefix() for validation as well. The function
  // will return a length different from “length” if the entire passed-in
  // character range does not represent a double.
  if (!ParseDoubleWithPrefix(characters, characters + length, number)) {
    return false;
  }
  number = ClampTo<double>(number, -std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());
  return true;
}

static CSSValue* ParseSimpleLengthValue(CSSPropertyID property_id,
                                        StringView string,
                                        CSSParserMode css_parser_mode) {
  DCHECK(!string.empty());
  bool accepts_negative_numbers = false;

  if (!IsSimpleLengthPropertyID(property_id, accepts_negative_numbers)) {
    return nullptr;
  }

  double number;
  CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::kNumber;

  const bool parsed_simple_length =
      ParseSimpleLength(string.Characters8(), string.length(), unit, number);
  if (!parsed_simple_length) {
    return nullptr;
  }

  if (unit == CSSPrimitiveValue::UnitType::kNumber) {
    if (css_parser_mode == kSVGAttributeMode) {
      unit = CSSPrimitiveValue::UnitType::kUserUnits;
    } else if (!number) {
      unit = CSSPrimitiveValue::UnitType::kPixels;
    } else {
      return nullptr;
    }
  }

  if (number < 0 && !accepts_negative_numbers) {
    return nullptr;
  }

  return CSSNumericLiteralValue::Create(number, unit);
}

// Returns the length of the angle, or 0 if the parse failed.
ALWAYS_INLINE static unsigned ParseSimpleAngle(
    const LChar* characters,
    unsigned length,
    CSSPrimitiveValue::UnitType& unit,
    double& number) {
  int number_length;
  if (length > 0 && *characters == '-') {
    number_length =
        ParsePositiveDouble(characters + 1, characters + length, number);
    if (number_length == 0) {
      return number_length;
    }
    ++number_length;
    number = -std::min<double>(number, std::numeric_limits<float>::max());
  } else {
    number_length =
        ParsePositiveDouble(characters, characters + length, number);
    if (number_length == 0) {
      return number_length;
    }
    number = std::min<double>(number, std::numeric_limits<float>::max());
  }

  characters += number_length;
  length -= number_length;

  if (length >= 3 && (characters[0] | 0x20) == 'd' &&
      (characters[1] | 0x20) == 'e' && (characters[2] | 0x20) == 'g') {
    unit = CSSPrimitiveValue::UnitType::kDegrees;
    return number_length + 3;
  } else if (length >= 4 && (characters[0] | 0x20) == 'g' &&
             (characters[1] | 0x20) == 'r' && (characters[2] | 0x20) == 'a' &&
             (characters[3] | 0x20) == 'd') {
    unit = CSSPrimitiveValue::UnitType::kGradians;
    return number_length + 4;
  } else if (length >= 3 && (characters[0] | 0x20) == 'r' &&
             (characters[1] | 0x20) == 'a' && (characters[2] | 0x20) == 'd') {
    unit = CSSPrimitiveValue::UnitType::kRadians;
    return number_length + 3;
  } else if (length >= 4 && (characters[0] | 0x20) == 't' &&
             (characters[1] | 0x20) == 'u' && (characters[2] | 0x20) == 'r' &&
             (characters[3] | 0x20) == 'n') {
    unit = CSSPrimitiveValue::UnitType::kTurns;
    return number_length + 4;
  } else {
    // For rotate: Only valid for zero (we'll check that in the caller).
    // For hsl(): To be treated as angles (also done in the caller).
    unit = CSSPrimitiveValue::UnitType::kNumber;
    return number_length;
  }
}

static inline bool IsColorPropertyID(CSSPropertyID property_id) {
  static CSSBitset properties{{
      CSSPropertyID::kCaretColor,
      CSSPropertyID::kColor,
      CSSPropertyID::kBackgroundColor,
      CSSPropertyID::kBorderBottomColor,
      CSSPropertyID::kBorderLeftColor,
      CSSPropertyID::kBorderRightColor,
      CSSPropertyID::kBorderTopColor,
      CSSPropertyID::kFill,
      CSSPropertyID::kFloodColor,
      CSSPropertyID::kLightingColor,
      CSSPropertyID::kOutlineColor,
      CSSPropertyID::kStopColor,
      CSSPropertyID::kStroke,
      CSSPropertyID::kBorderBlockEndColor,
      CSSPropertyID::kBorderBlockStartColor,
      CSSPropertyID::kBorderInlineEndColor,
      CSSPropertyID::kBorderInlineStartColor,
      CSSPropertyID::kColumnRuleColor,
      CSSPropertyID::kTextEmphasisColor,
      CSSPropertyID::kWebkitTextFillColor,
      CSSPropertyID::kWebkitTextStrokeColor,
      CSSPropertyID::kTextDecorationColor,

      // -internal-visited for all of the above that have them.
      CSSPropertyID::kInternalVisitedCaretColor,
      CSSPropertyID::kInternalVisitedColor,
      CSSPropertyID::kInternalVisitedBackgroundColor,
      CSSPropertyID::kInternalVisitedBorderBottomColor,
      CSSPropertyID::kInternalVisitedBorderLeftColor,
      CSSPropertyID::kInternalVisitedBorderRightColor,
      CSSPropertyID::kInternalVisitedBorderTopColor,
      CSSPropertyID::kInternalVisitedFill,
      CSSPropertyID::kInternalVisitedOutlineColor,
      CSSPropertyID::kInternalVisitedStroke,
      CSSPropertyID::kInternalVisitedBorderBlockEndColor,
      CSSPropertyID::kInternalVisitedBorderBlockStartColor,
      CSSPropertyID::kInternalVisitedBorderInlineEndColor,
      CSSPropertyID::kInternalVisitedBorderInlineStartColor,
      CSSPropertyID::kInternalVisitedColumnRuleColor,
      CSSPropertyID::kInternalVisitedTextEmphasisColor,
      CSSPropertyID::kInternalVisitedTextDecorationColor,
  }};
  return properties.Has(property_id);
}

// https://quirks.spec.whatwg.org/#the-hashless-hex-color-quirk
static inline bool ColorPropertyAllowsQuirkyColor(CSSPropertyID property_id) {
  static CSSBitset properties{{
      CSSPropertyID::kColor,
      CSSPropertyID::kBackgroundColor,
      CSSPropertyID::kBorderBottomColor,
      CSSPropertyID::kBorderLeftColor,
      CSSPropertyID::kBorderRightColor,
      CSSPropertyID::kBorderTopColor,
  }};
  return properties.Has(property_id);
}

// Returns the number of initial characters which form a valid double.
static unsigned FindLengthOfValidDouble(const LChar* string, const LChar* end) {
  int length = static_cast<int>(end - string);
  if (length < 1) {
    return 0;
  }

  bool decimal_mark_seen = false;
  int valid_length = 0;
#if defined(__SSE2__) || defined(__ARM_NEON__)
  if (length >= 16) {
    uint8_t b __attribute__((vector_size(16)));
    memcpy(&b, string, sizeof(b));
    auto is_decimal_mask = (b >= '0' && b <= '9');
    auto is_mark_mask = (b == '.');
#ifdef __SSE2__
    uint16_t is_decimal_bits =
        _mm_movemask_epi8(reinterpret_cast<__m128i>(is_decimal_mask));
    uint16_t is_mark_bits =
        _mm_movemask_epi8(reinterpret_cast<__m128i>(is_mark_mask));

    // Only count the first decimal mark.
    is_mark_bits &= -is_mark_bits;

    if ((is_decimal_bits | is_mark_bits) == 0xffff) {
      decimal_mark_seen = (is_mark_bits != 0);
      valid_length = 16;
      // Do the rest of the parsing using the scalar loop below.
      // It's unlikely that numbers will be much more than 16 bytes,
      // so we don't bother with a loop (which would also need logic
      // for checking for two decimal marks in separate 16-byte chunks).
    } else {
      // Get rid of any stray final period; i.e., one that is not
      // followed by a decimal.
      is_mark_bits &= (is_decimal_bits >> 1);
      uint16_t accept_bits = is_decimal_bits | is_mark_bits;
      return __builtin_ctz(~accept_bits);
    }
#else  // __ARM_NEON__

    // https://community.arm.com/arm-community-blogs/b/infrastructure-solutions-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
    uint64_t is_decimal_bits =
        vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(
                          vreinterpretq_u16_s8(is_decimal_mask), 4)),
                      0);
    uint64_t is_mark_bits = vget_lane_u64(
        vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_s8(is_mark_mask), 4)),
        0);

    // Only count the first decimal mark.
    is_mark_bits &= -is_mark_bits;
    is_mark_bits |= (is_mark_bits << 1);
    is_mark_bits |= (is_mark_bits << 2);

    if ((is_decimal_bits | is_mark_bits) == 0xffffffffffffffffULL) {
      decimal_mark_seen = (is_mark_bits != 0);
      valid_length = 16;
      // Do the rest of the parsing using the scalar loop below.
      // It's unlikely that numbers will be much more than 16 bytes,
      // so we don't bother with a loop (which would also need logic
      // for checking for two decimal marks in separate 16-byte chunks).
    } else {
      // Get rid of any stray final period; i.e., one that is not
      // followed by a decimal.
      is_mark_bits &= (is_decimal_bits >> 4);
      uint64_t accept_bits = is_decimal_bits | is_mark_bits;
      return __builtin_ctzll(~accept_bits) >> 2;
    }
#endif
  }
#endif  // defined(__SSE2__) || defined(__ARM_NEON__)

  for (; valid_length < length; ++valid_length) {
    if (!IsASCIIDigit(string[valid_length])) {
      if (!decimal_mark_seen && string[valid_length] == '.') {
        decimal_mark_seen = true;
      } else {
        break;
      }
    }
  }

  if (valid_length > 0 && string[valid_length - 1] == '.') {
    return 0;
  }

  return valid_length;
}

// If also_accept_whitespace is true: Checks whether string[pos] is the given
// character, _or_ an HTML space.
// Otherwise: Checks whether string[pos] is the given character.
// Returns false if pos is past the end of the string.
static bool ContainsCharAtPos(const LChar* string,
                              const LChar* end,
                              int pos,
                              char ch,
                              bool also_accept_whitespace) {
  DCHECK_GE(pos, 0);
  if (pos >= static_cast<int>(end - string)) {
    return false;
  }
  return string[pos] == ch ||
         (also_accept_whitespace && IsHTMLSpace(string[pos]));
}

// Like ParsePositiveDouble(), but also accepts initial whitespace and negative
// values. This is similar to CharactersToDouble(), but does not support
// trailing periods (e.g. “100.”), cf.
//
//   https://drafts.csswg.org/css-syntax/#consume-number
//   https://drafts.csswg.org/css-syntax/#number-token-diagram
//
// It also does not support exponential notation (e.g. “100e3”), which means
// that such cases go through the slow path.
static bool ParseDoubleWithPrefix(const LChar* string,
                                  const LChar* end,
                                  double& value) {
  while (string < end && IsHTMLSpace(*string)) {
    ++string;
  }
  if (string < end && *string == '-') {
    if (end - string == 1) {
      return false;
    }
    double v;
    if (ParsePositiveDouble(string + 1, end, v) !=
        static_cast<unsigned>(end - string - 1)) {
      return false;
    }
    value = -v;
    return true;
  } else if (string == end) {
    return false;
  } else {
    return ParsePositiveDouble(string, end, value) ==
           static_cast<unsigned>(end - string);
  }
}

// Returns the number of characters consumed for parsing a valid double,
// or 0 if the string did not start with a valid double.
//
// NOTE: Digits after the seventh decimal are ignored, potentially leading
// to accuracy issues. (All digits _before_ the decimal points are used.)
ALWAYS_INLINE static unsigned ParsePositiveDouble(const LChar* string,
                                                  const LChar* end,
                                                  double& value) {
  unsigned length = FindLengthOfValidDouble(string, end);
  if (length == 0) {
    return 0;
  }

  unsigned position = 0;
  double local_value = 0;

  // The consumed characters here are guaranteed to be
  // ASCII digits with or without a decimal mark
  for (; position < length; ++position) {
    if (string[position] == '.') {
      break;
    }
    local_value = local_value * 10 + (string[position] - '0');
  }

  if (++position >= length) {
    value = local_value;
    return length;
  }
  constexpr int kMaxDecimals = 7;
  int bytes_left = length - position;
  unsigned num_decimals = bytes_left > kMaxDecimals ? kMaxDecimals : bytes_left;

#ifdef __SSE2__
  // The closest double to 1e-7, rounded _up_ instead of to nearest.
  // We specifically don't want a value _smaller_ than 1e-7, because
  // we have specific midpoints (like 0.1) that we want specific values for
  // after rounding.
  static constexpr double kDiv1e7 = 0.000000100000000000000009;

  // If we have SSE2 and have a little bit of slop in our string,
  // we can parse all of our desired (up to) seven decimals
  // pretty much in one go. We subtract '0' from every digit,
  // widen to 16-bit, and then do multiplication with all the
  // digit weights in parallel. (This also blanks out characters
  // that are not digits.) Essentially what we want is
  //
  //   1000000 * d0 + 100000 * d1 + 10000 * d2 + ...
  //
  // Since we use PMADDWD (_mm_madd_epi16) for the multiplication,
  // we get pairwise addition of each of the products and automatic
  // widening to 32-bit for free, so that we do not get overflow
  // from the 16-bit values. Still, we need a little bit of care,
  // since we cannot store the largest weights directly; see below.
  if (end - (string + position) >= 7) {
    __m128i bytes = _mm_loadu_si64(string + position - 1);
    __m128i words = _mm_unpacklo_epi8(bytes, _mm_setzero_si128());
    words = _mm_sub_epi16(words, _mm_set1_epi16('0'));

    // NOTE: We cannot use _mm_setr_epi16(), as it is not constexpr.
    static constexpr __m128i kWeights[kMaxDecimals + 1] = {
        (__m128i)(__v8hi){0, 0, 0, 0, 0, 0, 0, 0},
        (__m128i)(__v8hi){0, 25000, 0, 0, 0, 0, 0, 0},
        (__m128i)(__v8hi){0, 25000, 2500, 0, 0, 0, 0, 0},
        (__m128i)(__v8hi){0, 25000, 2500, 250, 0, 0, 0, 0},
        (__m128i)(__v8hi){0, 25000, 2500, 250, 1000, 0, 0, 0},
        (__m128i)(__v8hi){0, 25000, 2500, 250, 1000, 100, 0, 0},
        (__m128i)(__v8hi){0, 25000, 2500, 250, 1000, 100, 10, 0},
        (__m128i)(__v8hi){0, 25000, 2500, 250, 1000, 100, 10, 1},
    };
    __m128i v = _mm_madd_epi16(words, kWeights[num_decimals]);

    // Now we have, ignoring scale factors:
    //
    //   {d0} {d1+d2} {d3+d4} {d5+d6}
    //
    // Do a standard SSE2 horizontal add of the neighboring pairs:
    v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));

    // Now we have:
    //
    //   {d0+d1+d2} {d0+d1+d2} {d3+d4+d5+d6} {d3+d4+d5+d6}
    //
    // We need to multiply the {d0+d1+d2} elements by 40 (we could not
    // fit 1000000 into a 16-bit int for kWeights[] above, and multiplication
    // with 40 can be done cheaply), before we do the final add,
    // conversion to float and scale.
    __v4si v_int = (__v4si)v;
    uint32_t fraction = v_int[0] * 40 + v_int[2];

    value = local_value + fraction * kDiv1e7;
    return length;
  }
#elif defined(__aarch64__) && defined(__ARM_NEON__)
  // See the SSE2 path.
  static constexpr double kDiv1e7 = 0.000000100000000000000009;

  // NEON is similar, but we don't have pairwise muladds, so we need to
  // structure with slightly more explicit widening, and an extra mul
  // by 10000. We can join the subtraction of '0' and the widening to
  // 16-bit into one operation, though, as NEON has widening subtraction.
  if (end - (string + position) >= 7) {
    uint8x8_t bytes = vld1_u8(string + position - 1);
    uint16x8_t words = vsubl_u8(bytes, vdup_n_u8('0'));
    static constexpr uint16x8_t kWeights[kMaxDecimals + 1] = {
        (uint16x8_t){0, 0, 0, 0, 0, 0, 0, 0},
        (uint16x8_t){0, 100, 0, 0, 0, 0, 0, 0},
        (uint16x8_t){0, 100, 10, 0, 0, 0, 0, 0},
        (uint16x8_t){0, 100, 10, 1, 0, 0, 0, 0},
        (uint16x8_t){0, 100, 10, 1, 1000, 0, 0, 0},
        (uint16x8_t){0, 100, 10, 1, 1000, 100, 0, 0},
        (uint16x8_t){0, 100, 10, 1, 1000, 100, 10, 0},
        (uint16x8_t){0, 100, 10, 1, 1000, 100, 10, 1},
    };
    uint32x4_t pairs = vpaddlq_u16(vmulq_u16(words, kWeights[num_decimals]));

    // Now we have:
    //
    //   {100*d0} {10*d1 + d2} {1000*d3 + 100*d4} + {10*d5 + d6}
    //
    // Multiply the first two lanes by 10000, and then sum all four
    // to get our final integer answer. (This final horizontal add
    // only exists on A64; thus the check for __aarch64__ and not
    // __ARM_NEON__.)
    static constexpr uint32x4_t kScaleFac{10000, 10000, 1, 1};
    uint32_t fraction = vaddvq_u32(vmulq_u32(pairs, kScaleFac));

    value = local_value + fraction * kDiv1e7;
    return length;
  }
#endif

  // OK, do it the slow, scalar way.
  double fraction = 0;
  double scale = 1;
  for (unsigned i = 0; i < num_decimals; ++i) {
    fraction = fraction * 10 + (string[position + i] - '0');
    scale *= 10;
  }

  value = local_value + fraction / scale;
  return length;
}

// Parse a float and clamp it upwards to max_value. Optimized for having
// no decimal part. Returns true if the parse was successful (though it
// may not consume the entire string; you'll need to check string != end
// yourself if that is the intention).
ALWAYS_INLINE static bool ParseFloatWithMaxValue(const LChar*& string,
                                                 const LChar* end,
                                                 int max_value,
                                                 double& value,
                                                 bool& negative) {
  value = 0.0;
  const LChar* current = string;
  while (current != end && IsHTMLSpace(*current)) {
    current++;
  }
  if (current != end && *current == '-') {
    negative = true;
    current++;
  } else {
    negative = false;
  }
  if (current == end || !IsASCIIDigit(*current)) {
    return false;
  }
  while (current != end && IsASCIIDigit(*current)) {
    double new_value = value * 10 + (*current++ - '0');
    if (new_value >= max_value) {
      // Clamp values at 255 or 100 (depending on the caller).
      value = max_value;
      while (current != end && IsASCIIDigit(*current)) {
        ++current;
      }
      break;
    }
    value = new_value;
  }

  if (current != end && *current == '.') {
    // We already parsed the integral part, try to parse
    // the fraction part.
    double fractional = 0;
    int num_characters_parsed = ParsePositiveDouble(current, end, fractional);
    if (num_characters_parsed == 0) {
      return false;
    }
    current += num_characters_parsed;
    value += fractional;
  }

  string = current;
  return true;
}

namespace {

enum TerminatorStatus {
  // List elements are delimited with whitespace,
  // e.g., rgb(10 20 30).
  kMustWhitespaceTerminate,

  // List elements are delimited with a given terminator,
  // and any whitespace before it should be skipped over,
  // e.g., rgb(10 , 20,30).
  kMustCharacterTerminate,

  // We are parsing the first element, so we could do either
  // variant -- and when it's an in/out argument, we set it
  // to one of the other values.
  kCouldWhitespaceTerminate,
};

}  // namespace

static bool SkipToTerminator(const LChar*& string,
                             const LChar* end,
                             const char terminator,
                             TerminatorStatus& terminator_status) {
  const LChar* current = string;

  while (current != end && IsHTMLSpace(*current)) {
    current++;
  }

  switch (terminator_status) {
    case kCouldWhitespaceTerminate:
      if (current != end && *current == terminator) {
        terminator_status = kMustCharacterTerminate;
        ++current;
        break;
      }
      terminator_status = kMustWhitespaceTerminate;
      [[fallthrough]];
    case kMustWhitespaceTerminate:
      // We must have skipped over at least one space before finding
      // something else (or the end).
      if (current == string) {
        return false;
      }
      break;
    case kMustCharacterTerminate:
      // We must have stopped at the given terminator character.
      if (current == end || *current != terminator) {
        return false;
      }
      ++current;  // Skip over the terminator.
      break;
  }

  string = current;
  return true;
}

static bool ParseColorNumberOrPercentage(const LChar*& string,
                                         const LChar* end,
                                         const char terminator,
                                         TerminatorStatus& terminator_status,
                                         CSSPrimitiveValue::UnitType& expect,
                                         int& value) {
  const LChar* current = string;
  double local_value;
  bool negative = false;
  if (!ParseFloatWithMaxValue(current, end, 255, local_value, negative)) {
    return false;
  }
  if (current == end) {
    return false;
  }

  if (expect == CSSPrimitiveValue::UnitType::kPercentage && *current != '%') {
    return false;
  }
  if (expect == CSSPrimitiveValue::UnitType::kNumber && *current == '%') {
    return false;
  }

  if (*current == '%') {
    expect = CSSPrimitiveValue::UnitType::kPercentage;
    local_value = local_value / 100.0 * 255.0;
    // Clamp values at 255 for percentages over 100%
    if (local_value > 255) {
      local_value = 255;
    }
    current++;
  } else {
    expect = CSSPrimitiveValue::UnitType::kNumber;
  }

  if (!SkipToTerminator(current, end, terminator, terminator_status)) {
    return false;
  }

  // Clamp negative values at zero.
  value = negative ? 0 : static_cast<int>(lround(local_value));
  string = current;
  return true;
}

// Parses a percentage (including the % sign), clamps it and converts it to
// 0.0..1.0.
ALWAYS_INLINE static bool ParsePercentage(const LChar*& string,
                                          const LChar* end,
                                          const char terminator,
                                          TerminatorStatus& terminator_status,
                                          double& value) {
  const LChar* current = string;
  bool negative = false;
  if (!ParseFloatWithMaxValue(current, end, 100, value, negative)) {
    return false;
  }

  if (current == end || *current != '%') {
    return false;
  }

  ++current;
  if (negative) {
    value = 0.0;
  } else {
    value = std::min(value * 0.01, 1.0);
  }

  if (!SkipToTerminator(current, end, terminator, terminator_status)) {
    return false;
  }

  string = current;
  return true;
}

static inline bool IsTenthAlpha(const LChar* string, const wtf_size_t length) {
  // "0.X"
  if (length == 3 && string[0] == '0' && string[1] == '.' &&
      IsASCIIDigit(string[2])) {
    return true;
  }

  // ".X"
  if (length == 2 && string[0] == '.' && IsASCIIDigit(string[1])) {
    return true;
  }

  return false;
}

ALWAYS_INLINE static bool ParseAlphaValue(const LChar*& string,
                                          const LChar* end,
                                          const char terminator,
                                          int& value) {
  while (string != end && IsHTMLSpace(*string)) {
    string++;
  }

  bool negative = false;

  if (string != end && *string == '-') {
    negative = true;
    string++;
  }

  value = 0;

  wtf_size_t length = static_cast<wtf_size_t>(end - string);
  if (length < 2) {
    return false;
  }

  if (string[length - 1] != terminator || !IsASCIIDigit(string[length - 2])) {
    return false;
  }

  if (string[0] != '0' && string[0] != '1' && string[0] != '.') {
    int double_length = FindLengthOfValidDouble(string, end);
    if (double_length > 0 &&
        ContainsCharAtPos(string, end, double_length, terminator,
                          /*also_accept_whitespace=*/false)) {
      value = negative ? 0 : 255;
      string = end;
      return true;
    }
    return false;
  }

  if (length == 2 && string[0] != '.') {
    value = !negative && string[0] == '1' ? 255 : 0;
    string = end;
    return true;
  }

  if (IsTenthAlpha(string, length - 1)) {
    // Fast conversions for 0.1 steps of alpha values between 0.0 and 0.9,
    // where 0.1 alpha is value 26 (25.5 rounded) and so on.
    static const int kTenthAlphaValues[] = {0,   26,  51,  77,  102,
                                            128, 153, 179, 204, 230};
    value = negative ? 0 : kTenthAlphaValues[string[length - 2] - '0'];
    string = end;
    return true;
  }

  double alpha = 0;
  int dbl_length = ParsePositiveDouble(string, end, alpha);
  if (dbl_length == 0 || !ContainsCharAtPos(string, end, dbl_length, terminator,
                                            /*also_accept_whitespace=*/false)) {
    return false;
  }
  value = negative ? 0 : static_cast<int>(lround(std::min(alpha, 1.0) * 255.0));
  string = end;
  return true;
}

// Fast for LChar, reasonable for UChar.
template <int N>
static inline bool MatchesLiteral(const LChar* a, const char (&b)[N]) {
  return memcmp(a, b, N - 1) == 0;
}

template <int N>
static inline bool MatchesLiteral(const UChar* a, const char (&b)[N]) {
  for (int i = 0; i < N - 1; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

// Right-hand side must already be lowercase.
static inline bool MatchesCaseInsensitiveLiteral4(const LChar* a,
                                                  const char (&b)[5]) {
  uint32_t av, bv;
  memcpy(&av, a, sizeof(av));
  memcpy(&bv, b, sizeof(bv));

  uint32_t mask = 0;
  if ((bv & 0xff) >= 'a' && (bv & 0xff) <= 'z') {
    mask |= 0x20;
  }
  if (((bv >> 8) & 0xff) >= 'a' && ((bv >> 8) & 0xff) <= 'z') {
    mask |= 0x2000;
  }
  if (((bv >> 16) & 0xff) >= 'a' && ((bv >> 16) & 0xff) <= 'z') {
    mask |= 0x200000;
  }
  if ((bv >> 24) >= 'a' && (bv >> 24) <= 'z') {
    mask |= 0x20000000;
  }

  return (av | mask) == bv;
}

static inline bool MatchesCaseInsensitiveLiteral2(const LChar* a,
                                                  const char (&b)[3]) {
  uint16_t av, bv;
  memcpy(&av, a, sizeof(av));
  memcpy(&bv, b, sizeof(bv));

  uint16_t mask = 0;
  if ((bv & 0xff) >= 'a' && (bv & 0xff) <= 'z') {
    mask |= 0x20;
  }
  if ((bv >> 8) >= 'a' && (bv >> 8) <= 'z') {
    mask |= 0x2000;
  }

  return (av | mask) == bv;
}

static inline bool MightBeRGBOrRGBA(const LChar* characters, unsigned length) {
  if (length < 5) {
    return false;
  }
  return MatchesLiteral(characters, "rgb") &&
         (characters[3] == '(' ||
          (characters[3] == 'a' && characters[4] == '('));
}

static inline bool MightBeHSLOrHSLA(const LChar* characters, unsigned length) {
  if (length < 5) {
    return false;
  }
  return MatchesLiteral(characters, "hsl") &&
         (characters[3] == '(' ||
          (characters[3] == 'a' && characters[4] == '('));
}

static bool FastParseColorInternal(Color& color,
                                   const LChar* characters,
                                   unsigned length,
                                   bool quirks_mode) {
  if (length >= 4 && characters[0] == '#') {
    return Color::ParseHexColor(characters + 1, length - 1, color);
  }

  if (quirks_mode && (length == 3 || length == 6)) {
    if (Color::ParseHexColor(characters, length, color)) {
      return true;
    }
  }

  // rgb() and rgba() have the same syntax.
  if (MightBeRGBOrRGBA(characters, length)) {
    int length_to_add = (characters[3] == 'a') ? 5 : 4;
    const LChar* current = characters + length_to_add;
    const LChar* end = characters + length;
    int red;
    int green;
    int blue;
    int alpha;
    bool should_have_alpha = false;

    TerminatorStatus terminator_status = kCouldWhitespaceTerminate;
    CSSPrimitiveValue::UnitType expect = CSSPrimitiveValue::UnitType::kUnknown;
    if (!ParseColorNumberOrPercentage(current, end, ',', terminator_status,
                                      expect, red)) {
      return false;
    }
    if (!ParseColorNumberOrPercentage(current, end, ',', terminator_status,
                                      expect, green)) {
      return false;
    }

    TerminatorStatus no_whitespace_check = kMustCharacterTerminate;
    if (!ParseColorNumberOrPercentage(current, end, ',', no_whitespace_check,
                                      expect, blue)) {
      // Might have slash as separator.
      if (ParseColorNumberOrPercentage(current, end, '/', no_whitespace_check,
                                       expect, blue)) {
        if (terminator_status != kMustWhitespaceTerminate) {
          return false;
        }
        should_have_alpha = true;
      }
      // Might not have alpha.
      else if (!ParseColorNumberOrPercentage(
                   current, end, ')', no_whitespace_check, expect, blue)) {
        return false;
      }
    } else {
      if (terminator_status != kMustCharacterTerminate) {
        return false;
      }
      should_have_alpha = true;
    }

    if (should_have_alpha) {
      if (!ParseAlphaValue(current, end, ')', alpha)) {
        return false;
      }
      color = Color::FromRGBA(red, green, blue, alpha);
    } else {
      if (current != end) {
        return false;
      }
      color = Color::FromRGB(red, green, blue);
    }
    return true;
  }

  // hsl() and hsla() also have the same syntax:
  // https://www.w3.org/TR/css-color-4/#the-hsl-notation
  // Also for legacy reasons, an hsla() function also exists, with an identical
  // grammar and behavior to hsl().

  if (MightBeHSLOrHSLA(characters, length)) {
    int length_to_add = (characters[3] == 'a') ? 5 : 4;
    const LChar* current = characters + length_to_add;
    const LChar* end = characters + length;
    bool should_have_alpha = false;

    // Skip any whitespace before the hue.
    while (current != end && IsHTMLSpace(*current)) {
      current++;
    }

    CSSPrimitiveValue::UnitType hue_unit = CSSPrimitiveValue::UnitType::kNumber;
    double hue;
    unsigned hue_length = ParseSimpleAngle(
        current, static_cast<unsigned>(end - current), hue_unit, hue);
    if (hue_length == 0) {
      return false;
    }

    switch (hue_unit) {
      case CSSPrimitiveValue::UnitType::kNumber:
      case CSSPrimitiveValue::UnitType::kDegrees:
        // Unitless numbers are to be treated as degrees.
        break;
      case CSSPrimitiveValue::UnitType::kRadians:
        hue = Rad2deg(hue);
        break;
      case CSSPrimitiveValue::UnitType::kGradians:
        hue = Grad2deg(hue);
        break;
      case CSSPrimitiveValue::UnitType::kTurns:
        hue *= 360.0;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        return false;
    }

    // Deal with wraparound so that we end up in [0, 360],
    // roughly analogous to the code in ParseHSLParameters().
    // Taking these branches should be rare.
    if (hue < 0.0) {
      hue = fmod(hue, 360.0) + 360.0;
    } else if (hue > 360.0) {
      hue = fmod(hue, 360.0);
    }

    current += hue_length;

    TerminatorStatus terminator_status = kCouldWhitespaceTerminate;
    if (!SkipToTerminator(current, end, ',', terminator_status)) {
      return false;
    }

    // Saturation and lightness must always be percentages.
    double saturation;
    if (!ParsePercentage(current, end, ',', terminator_status, saturation)) {
      return false;
    }

    TerminatorStatus no_whitespace_check = kMustCharacterTerminate;

    double lightness;
    if (!ParsePercentage(current, end, ',', no_whitespace_check, lightness)) {
      // Might have slash as separator.
      if (ParsePercentage(current, end, '/', no_whitespace_check, lightness)) {
        if (terminator_status != kMustWhitespaceTerminate) {
          return false;
        }
        should_have_alpha = true;
      }
      // Might not have alpha.
      else if (!ParsePercentage(current, end, ')', no_whitespace_check,
                                lightness)) {
        return false;
      }
    } else {
      if (terminator_status != kMustCharacterTerminate) {
        return false;
      }
      should_have_alpha = true;
    }

    if (should_have_alpha) {
      int alpha;
      if (!ParseAlphaValue(current, end, ')', alpha)) {
        return false;
      }
      if (current != end) {
        return false;
      }
      color =
          Color::FromHSLA(hue, saturation, lightness, alpha * (1.0f / 255.0f));
    } else {
      if (current != end) {
        return false;
      }
      color = Color::FromHSLA(hue, saturation, lightness, 1.0f);
    }
    return true;
  }

  return false;
}

// If the string identifies a color keyword, `out_color_keyword` is set and
// `kKeyword` is returned. If the string identifies a color, then `out_color`
// is set and `kColor` is returned.
static ParseColorResult ParseColor(CSSPropertyID property_id,
                                   StringView string,
                                   CSSParserMode parser_mode,
                                   Color& out_color,
                                   CSSValueID& out_color_keyword) {
  DCHECK(!string.empty());
  DCHECK(IsColorPropertyID(property_id));
  CSSValueID value_id = CssValueKeywordID(string);
  if ((value_id == CSSValueID::kAccentcolor ||
       value_id == CSSValueID::kAccentcolortext) &&
      !RuntimeEnabledFeatures::CSSSystemAccentColorEnabled()) {
    return ParseColorResult::kFailure;
  }
  if (StyleColor::IsColorKeyword(value_id)) {
    if (!isValueAllowedInMode(value_id, parser_mode)) {
      return ParseColorResult::kFailure;
    }
    out_color_keyword = value_id;
    return ParseColorResult::kKeyword;
  }

  bool quirks_mode = IsQuirksModeBehavior(parser_mode) &&
                     ColorPropertyAllowsQuirkyColor(property_id);

  // Fast path for hex colors and rgb()/rgba()/hsl()/hsla() colors.
  // Note that ParseColor may be called from external contexts,
  // i.e., when parsing style sheets, so we need the Unicode path here.
  const bool parsed = FastParseColorInternal(out_color, string.Characters8(),
                                             string.length(), quirks_mode);
  return parsed ? ParseColorResult::kColor : ParseColorResult::kFailure;
}

ParseColorResult CSSParserFastPaths::ParseColor(const String& string,
                                                CSSParserMode parser_mode,
                                                Color& color) {
  if (!string.Is8Bit()) {
    // See comment on MaybeParseValue().
    return ParseColorResult::kFailure;
  }
  CSSValueID color_id;
  return blink::ParseColor(CSSPropertyID::kColor, string, parser_mode, color,
                           color_id);
}

bool CSSParserFastPaths::IsValidKeywordPropertyAndValue(
    CSSPropertyID property_id,
    CSSValueID value_id,
    CSSParserMode parser_mode) {
  if (!IsValidCSSValueID(value_id) ||
      !isValueAllowedInMode(value_id, parser_mode)) {
    return false;
  }

  // For range checks, enum ordering is defined by CSSValueKeywords.in.
  switch (property_id) {
    case CSSPropertyID::kAlignmentBaseline:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kAlphabetic ||
             value_id == CSSValueID::kBaseline ||
             value_id == CSSValueID::kMiddle ||
             value_id == CSSValueID::kHanging ||
             (value_id >= CSSValueID::kBeforeEdge &&
              value_id <= CSSValueID::kMathematical);
    case CSSPropertyID::kAll:
      return false;  // Only accepts css-wide keywords
    case CSSPropertyID::kBaselineSource:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kFirst ||
             value_id == CSSValueID::kLast;
    case CSSPropertyID::kBorderCollapse:
      return value_id == CSSValueID::kCollapse ||
             value_id == CSSValueID::kSeparate;
    case CSSPropertyID::kBorderTopStyle:
    case CSSPropertyID::kBorderRightStyle:
    case CSSPropertyID::kBorderBottomStyle:
    case CSSPropertyID::kBorderLeftStyle:
    case CSSPropertyID::kBorderBlockEndStyle:
    case CSSPropertyID::kBorderBlockStartStyle:
    case CSSPropertyID::kBorderInlineEndStyle:
    case CSSPropertyID::kBorderInlineStartStyle:
    case CSSPropertyID::kColumnRuleStyle:
      return value_id >= CSSValueID::kNone && value_id <= CSSValueID::kDouble;
    case CSSPropertyID::kBoxSizing:
      return value_id == CSSValueID::kBorderBox ||
             value_id == CSSValueID::kContentBox;
    case CSSPropertyID::kBufferedRendering:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kDynamic ||
             value_id == CSSValueID::kStatic;
    case CSSPropertyID::kCaptionSide:
      return value_id == CSSValueID::kTop || value_id == CSSValueID::kBottom;
    case CSSPropertyID::kCaretAnimation:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kManual;
    case CSSPropertyID::kClear:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kLeft ||
             value_id == CSSValueID::kRight || value_id == CSSValueID::kBoth ||
             value_id == CSSValueID::kInlineStart ||
             value_id == CSSValueID::kInlineEnd;
    case CSSPropertyID::kClipRule:
    case CSSPropertyID::kFillRule:
      return value_id == CSSValueID::kNonzero ||
             value_id == CSSValueID::kEvenodd;
    case CSSPropertyID::kColorInterpolation:
    case CSSPropertyID::kColorInterpolationFilters:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kSRGB ||
             value_id == CSSValueID::kLinearrgb;
    case CSSPropertyID::kColorRendering:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kOptimizespeed ||
             value_id == CSSValueID::kOptimizequality;
    case CSSPropertyID::kDirection:
      return value_id == CSSValueID::kLtr || value_id == CSSValueID::kRtl;
    case CSSPropertyID::kDominantBaseline:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kAlphabetic ||
             value_id == CSSValueID::kMiddle ||
             value_id == CSSValueID::kHanging ||
             (value_id >= CSSValueID::kUseScript &&
              value_id <= CSSValueID::kResetSize) ||
             (value_id >= CSSValueID::kCentral &&
              value_id <= CSSValueID::kMathematical);
    case CSSPropertyID::kEmptyCells:
      return value_id == CSSValueID::kShow || value_id == CSSValueID::kHide;
    case CSSPropertyID::kFloat:
      return value_id == CSSValueID::kLeft || value_id == CSSValueID::kRight ||
             value_id == CSSValueID::kInlineStart ||
             value_id == CSSValueID::kInlineEnd ||
             value_id == CSSValueID::kNone;
    case CSSPropertyID::kForcedColorAdjust:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kAuto ||
             (value_id == CSSValueID::kPreserveParentColor &&
              (RuntimeEnabledFeatures::
                   ForcedColorsPreserveParentColorEnabled() ||
               parser_mode == kUASheetMode));
    case CSSPropertyID::kImageRendering:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kWebkitOptimizeContrast ||
             value_id == CSSValueID::kPixelated;
    case CSSPropertyID::kInterpolateSize:
      return value_id == CSSValueID::kNumericOnly ||
             value_id == CSSValueID::kAllowKeywords;
    case CSSPropertyID::kIsolation:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kIsolate;
    case CSSPropertyID::kListStylePosition:
      return value_id == CSSValueID::kInside ||
             value_id == CSSValueID::kOutside;
    case CSSPropertyID::kMaskType:
      return value_id == CSSValueID::kLuminance ||
             value_id == CSSValueID::kAlpha;
    case CSSPropertyID::kMathShift:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kCompact;
    case CSSPropertyID::kMathStyle:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kCompact;
    case CSSPropertyID::kObjectFit:
      return value_id == CSSValueID::kFill ||
             value_id == CSSValueID::kContain ||
             value_id == CSSValueID::kCover || value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kScaleDown;
    case CSSPropertyID::kOutlineStyle:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone ||
             (value_id >= CSSValueID::kInset &&
              value_id <= CSSValueID::kDouble);
    case CSSPropertyID::kOverflowAnchor:
      return value_id == CSSValueID::kVisible ||
             value_id == CSSValueID::kNone || value_id == CSSValueID::kAuto;
    case CSSPropertyID::kOverflowWrap:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kBreakWord ||
             value_id == CSSValueID::kAnywhere;
    case CSSPropertyID::kInternalOverflowBlock:
    case CSSPropertyID::kInternalOverflowInline:
    case CSSPropertyID::kOverflowBlock:
    case CSSPropertyID::kOverflowInline:
    case CSSPropertyID::kOverflowX:
    case CSSPropertyID::kOverflowY:
      return value_id == CSSValueID::kVisible ||
             value_id == CSSValueID::kHidden ||
             value_id == CSSValueID::kScroll || value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kOverlay || value_id == CSSValueID::kClip;
    case CSSPropertyID::kBreakAfter:
    case CSSPropertyID::kBreakBefore:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kAvoid ||
             value_id == CSSValueID::kAvoidPage ||
             value_id == CSSValueID::kPage || value_id == CSSValueID::kLeft ||
             value_id == CSSValueID::kRight || value_id == CSSValueID::kRecto ||
             value_id == CSSValueID::kVerso ||
             value_id == CSSValueID::kAvoidColumn ||
             value_id == CSSValueID::kColumn;
    case CSSPropertyID::kBreakInside:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kAvoid ||
             value_id == CSSValueID::kAvoidPage ||
             value_id == CSSValueID::kAvoidColumn;
    case CSSPropertyID::kPageOrientation:
      return value_id == CSSValueID::kUpright ||
             value_id == CSSValueID::kRotateLeft ||
             value_id == CSSValueID::kRotateRight;
    case CSSPropertyID::kPointerEvents:
      return value_id == CSSValueID::kVisible ||
             value_id == CSSValueID::kNone || value_id == CSSValueID::kAll ||
             value_id == CSSValueID::kAuto ||
             (value_id >= CSSValueID::kVisiblepainted &&
              value_id <= CSSValueID::kBoundingBox);
    case CSSPropertyID::kPosition:
      return value_id == CSSValueID::kStatic ||
             value_id == CSSValueID::kRelative ||
             value_id == CSSValueID::kAbsolute ||
             value_id == CSSValueID::kFixed || value_id == CSSValueID::kSticky;
    case CSSPropertyID::kPositionTryOrder:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kMostWidth ||
             value_id == CSSValueID::kMostHeight ||
             value_id == CSSValueID::kMostBlockSize ||
             value_id == CSSValueID::kMostInlineSize;
    case CSSPropertyID::kReadingFlow:
      DCHECK(RuntimeEnabledFeatures::CSSReadingFlowEnabled());
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kFlexVisual ||
             value_id == CSSValueID::kFlexFlow ||
             value_id == CSSValueID::kGridRows ||
             value_id == CSSValueID::kGridColumns ||
             value_id == CSSValueID::kGridOrder;
    case CSSPropertyID::kResize:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kBoth ||
             value_id == CSSValueID::kHorizontal ||
             value_id == CSSValueID::kVertical ||
             value_id == CSSValueID::kBlock ||
             value_id == CSSValueID::kInline ||
             value_id == CSSValueID::kInternalTextareaAuto ||
             (RuntimeEnabledFeatures::CSSResizeAutoEnabled() &&
              value_id == CSSValueID::kAuto);
    case CSSPropertyID::kScrollMarkerGroup:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kAfter ||
             value_id == CSSValueID::kBefore;
    case CSSPropertyID::kScrollBehavior:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kSmooth;
    case CSSPropertyID::kScrollStartTarget:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone;
    case CSSPropertyID::kShapeRendering:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kOptimizespeed ||
             value_id == CSSValueID::kCrispedges ||
             value_id == CSSValueID::kGeometricprecision;
    case CSSPropertyID::kSpeak:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kSpellOut ||
             value_id == CSSValueID::kDigits ||
             value_id == CSSValueID::kLiteralPunctuation ||
             value_id == CSSValueID::kNoPunctuation;
    case CSSPropertyID::kStrokeLinejoin:
      return value_id == CSSValueID::kMiter || value_id == CSSValueID::kRound ||
             value_id == CSSValueID::kBevel;
    case CSSPropertyID::kStrokeLinecap:
      return value_id == CSSValueID::kButt || value_id == CSSValueID::kRound ||
             value_id == CSSValueID::kSquare;
    case CSSPropertyID::kTableLayout:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kFixed;
    case CSSPropertyID::kTextAlign:
      return (value_id >= CSSValueID::kWebkitAuto &&
              value_id <= CSSValueID::kInternalCenter) ||
             value_id == CSSValueID::kStart || value_id == CSSValueID::kEnd;
    case CSSPropertyID::kTextAlignLast:
      return (value_id >= CSSValueID::kLeft &&
              value_id <= CSSValueID::kJustify) ||
             value_id == CSSValueID::kStart || value_id == CSSValueID::kEnd ||
             value_id == CSSValueID::kAuto;
    case CSSPropertyID::kTextAnchor:
      return value_id == CSSValueID::kStart ||
             value_id == CSSValueID::kMiddle || value_id == CSSValueID::kEnd;
    case CSSPropertyID::kTextCombineUpright:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kAll;
    case CSSPropertyID::kTextDecorationStyle:
      return value_id == CSSValueID::kSolid ||
             value_id == CSSValueID::kDouble ||
             value_id == CSSValueID::kDotted ||
             value_id == CSSValueID::kDashed || value_id == CSSValueID::kWavy;
    case CSSPropertyID::kTextDecorationSkipInk:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone;
    case CSSPropertyID::kTextOrientation:
      return value_id == CSSValueID::kMixed ||
             value_id == CSSValueID::kUpright ||
             value_id == CSSValueID::kSideways ||
             value_id == CSSValueID::kSidewaysRight;
    case CSSPropertyID::kWebkitTextOrientation:
      return value_id == CSSValueID::kSideways ||
             value_id == CSSValueID::kSidewaysRight ||
             value_id == CSSValueID::kVerticalRight ||
             value_id == CSSValueID::kUpright;
    case CSSPropertyID::kTextOverflow:
      return value_id == CSSValueID::kClip || value_id == CSSValueID::kEllipsis;
    case CSSPropertyID::kOverlay:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kAuto;
    case CSSPropertyID::kTextRendering:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kOptimizespeed ||
             value_id == CSSValueID::kOptimizelegibility ||
             value_id == CSSValueID::kGeometricprecision;
    case CSSPropertyID::kTextTransform:
      return (value_id >= CSSValueID::kCapitalize &&
              value_id <= CSSValueID::kMathAuto) ||
             value_id == CSSValueID::kNone;
    case CSSPropertyID::kUnicodeBidi:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kEmbed ||
             value_id == CSSValueID::kBidiOverride ||
             value_id == CSSValueID::kWebkitIsolate ||
             value_id == CSSValueID::kWebkitIsolateOverride ||
             value_id == CSSValueID::kWebkitPlaintext ||
             value_id == CSSValueID::kIsolate ||
             value_id == CSSValueID::kIsolateOverride ||
             value_id == CSSValueID::kPlaintext;
    case CSSPropertyID::kVectorEffect:
      return value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kNonScalingStroke;
    case CSSPropertyID::kVisibility:
      return value_id == CSSValueID::kVisible ||
             value_id == CSSValueID::kHidden ||
             value_id == CSSValueID::kCollapse ||
             (RuntimeEnabledFeatures::CSSVisibilityInertEnabled() &&
              value_id == CSSValueID::kInert);
    case CSSPropertyID::kAppRegion:
      return (value_id >= CSSValueID::kDrag &&
              value_id <= CSSValueID::kNoDrag) ||
             value_id == CSSValueID::kNone;
    case CSSPropertyID::kAppearance:
      return (value_id == CSSValueID::kCheckbox ||
              value_id == CSSValueID::kRadio ||
              value_id == CSSValueID::kButton ||
              value_id == CSSValueID::kListbox ||
              value_id == CSSValueID::kInternalMediaControl ||
              value_id == CSSValueID::kMenulist ||
              value_id == CSSValueID::kMenulistButton ||
              value_id == CSSValueID::kMeter ||
              value_id == CSSValueID::kProgressBar ||
              value_id == CSSValueID::kSearchfield ||
              value_id == CSSValueID::kTextfield ||
              value_id == CSSValueID::kTextarea) ||
             (RuntimeEnabledFeatures::CustomizableSelectEnabled() &&
              value_id == CSSValueID::kBaseSelect) ||
             (RuntimeEnabledFeatures::
                  NonStandardAppearanceValueSliderVerticalEnabled() &&
              value_id == CSSValueID::kSliderVertical) ||
             value_id == CSSValueID::kNone || value_id == CSSValueID::kAuto;
    case CSSPropertyID::kBackfaceVisibility:
      return value_id == CSSValueID::kVisible ||
             value_id == CSSValueID::kHidden;
    case CSSPropertyID::kMixBlendMode:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kMultiply ||
             value_id == CSSValueID::kScreen ||
             value_id == CSSValueID::kOverlay ||
             value_id == CSSValueID::kDarken ||
             value_id == CSSValueID::kLighten ||
             value_id == CSSValueID::kColorDodge ||
             value_id == CSSValueID::kColorBurn ||
             value_id == CSSValueID::kHardLight ||
             value_id == CSSValueID::kSoftLight ||
             value_id == CSSValueID::kDifference ||
             value_id == CSSValueID::kExclusion ||
             value_id == CSSValueID::kHue ||
             value_id == CSSValueID::kSaturation ||
             value_id == CSSValueID::kColor ||
             value_id == CSSValueID::kLuminosity ||
             value_id == CSSValueID::kPlusLighter;
    case CSSPropertyID::kWebkitBoxAlign:
      return value_id == CSSValueID::kStretch ||
             value_id == CSSValueID::kStart || value_id == CSSValueID::kEnd ||
             value_id == CSSValueID::kCenter ||
             value_id == CSSValueID::kBaseline;
    case CSSPropertyID::kBoxDecorationBreak:
      if (!RuntimeEnabledFeatures::BoxDecorationBreakEnabled()) {
        return false;
      }
      [[fallthrough]];
    case CSSPropertyID::kWebkitBoxDecorationBreak:
      return value_id == CSSValueID::kClone || value_id == CSSValueID::kSlice;
    case CSSPropertyID::kWebkitBoxDirection:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kReverse;
    case CSSPropertyID::kWebkitBoxOrient:
      return value_id == CSSValueID::kHorizontal ||
             value_id == CSSValueID::kVertical ||
             value_id == CSSValueID::kInlineAxis ||
             value_id == CSSValueID::kBlockAxis;
    case CSSPropertyID::kWebkitBoxPack:
      return value_id == CSSValueID::kStart || value_id == CSSValueID::kEnd ||
             value_id == CSSValueID::kCenter ||
             value_id == CSSValueID::kJustify;
    case CSSPropertyID::kColumnFill:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kBalance;
    case CSSPropertyID::kAlignContent:
      // FIXME: Per CSS alignment, this property should accept an optional
      // <overflow-position>. We should share this parsing code with
      // 'justify-self'.
      return value_id == CSSValueID::kFlexStart ||
             value_id == CSSValueID::kFlexEnd ||
             value_id == CSSValueID::kCenter ||
             value_id == CSSValueID::kSpaceBetween ||
             value_id == CSSValueID::kSpaceAround ||
             value_id == CSSValueID::kStretch;
    case CSSPropertyID::kAlignItems:
      // FIXME: Per CSS alignment, this property should accept the same
      // arguments as 'justify-self' so we should share its parsing code.
      return value_id == CSSValueID::kFlexStart ||
             value_id == CSSValueID::kFlexEnd ||
             value_id == CSSValueID::kCenter ||
             value_id == CSSValueID::kBaseline ||
             value_id == CSSValueID::kStretch;
    case CSSPropertyID::kAlignSelf:
      // FIXME: Per CSS alignment, this property should accept the same
      // arguments as 'justify-self' so we should share its parsing code.
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kFlexStart ||
             value_id == CSSValueID::kFlexEnd ||
             value_id == CSSValueID::kCenter ||
             value_id == CSSValueID::kBaseline ||
             value_id == CSSValueID::kStretch;
    case CSSPropertyID::kFlexDirection:
      return value_id == CSSValueID::kRow ||
             value_id == CSSValueID::kRowReverse ||
             value_id == CSSValueID::kColumn ||
             value_id == CSSValueID::kColumnReverse;
    case CSSPropertyID::kFlexWrap:
      return value_id == CSSValueID::kNowrap || value_id == CSSValueID::kWrap ||
             value_id == CSSValueID::kWrapReverse;
    case CSSPropertyID::kFieldSizing:
      return value_id == CSSValueID::kFixed || value_id == CSSValueID::kContent;
    case CSSPropertyID::kHyphens:
#if BUILDFLAG(USE_MINIKIN_HYPHENATION) || BUILDFLAG(IS_APPLE)
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kManual;
#else
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kManual;
#endif
    case CSSPropertyID::kJustifyContent:
      // FIXME: Per CSS alignment, this property should accept an optional
      // <overflow-position>. We should share this parsing code with
      // 'justify-self'.
      return value_id == CSSValueID::kFlexStart ||
             value_id == CSSValueID::kFlexEnd ||
             value_id == CSSValueID::kCenter ||
             value_id == CSSValueID::kSpaceBetween ||
             value_id == CSSValueID::kSpaceAround;
    case CSSPropertyID::kFontKerning:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kNone;
    case CSSPropertyID::kFontOpticalSizing:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone;
    case CSSPropertyID::kFontSynthesisWeight:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone;
    case CSSPropertyID::kFontSynthesisStyle:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone;
    case CSSPropertyID::kFontSynthesisSmallCaps:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone;
    case CSSPropertyID::kWebkitFontSmoothing:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kAntialiased ||
             value_id == CSSValueID::kSubpixelAntialiased;
    case CSSPropertyID::kFontVariantPosition:
      return value_id == CSSValueID::kNormal || value_id == CSSValueID::kSub ||
             value_id == CSSValueID::kSuper;
    case CSSPropertyID::kFontVariantEmoji:
      DCHECK(RuntimeEnabledFeatures::FontVariantEmojiEnabled());
      return value_id == CSSValueID::kNormal || value_id == CSSValueID::kText ||
             value_id == CSSValueID::kEmoji || value_id == CSSValueID::kUnicode;
    case CSSPropertyID::kLineBreak:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kLoose ||
             value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kStrict ||
             value_id == CSSValueID::kAnywhere;
    case CSSPropertyID::kWebkitLineBreak:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kLoose ||
             value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kStrict ||
             value_id == CSSValueID::kAfterWhiteSpace;
    case CSSPropertyID::kWebkitPrintColorAdjust:
      return value_id == CSSValueID::kExact || value_id == CSSValueID::kEconomy;
    case CSSPropertyID::kWebkitRtlOrdering:
      return value_id == CSSValueID::kLogical ||
             value_id == CSSValueID::kVisual;
    case CSSPropertyID::kRubyAlign:
      return value_id == CSSValueID::kSpaceAround ||
             value_id == CSSValueID::kStart ||
             value_id == CSSValueID::kCenter ||
             value_id == CSSValueID::kSpaceBetween;
    case CSSPropertyID::kWebkitRubyPosition:
      return value_id == CSSValueID::kBefore || value_id == CSSValueID::kAfter;
    case CSSPropertyID::kRubyPosition:
      return value_id == CSSValueID::kOver || value_id == CSSValueID::kUnder;
    case CSSPropertyID::kTextAutospace:
      DCHECK(RuntimeEnabledFeatures::CSSTextAutoSpaceEnabled());
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kNoAutospace;
    case CSSPropertyID::kTextSpacingTrim:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kTrimStart ||
             value_id == CSSValueID::kSpaceAll ||
             value_id == CSSValueID::kSpaceFirst;
    case CSSPropertyID::kWebkitTextCombine:
      return value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kHorizontal;
    case CSSPropertyID::kWebkitTextSecurity:
      return value_id == CSSValueID::kDisc || value_id == CSSValueID::kCircle ||
             value_id == CSSValueID::kSquare || value_id == CSSValueID::kNone;
    case CSSPropertyID::kTextWrapMode:
      return value_id == CSSValueID::kWrap || value_id == CSSValueID::kNowrap;
    case CSSPropertyID::kTextWrapStyle:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kBalance ||
             value_id == CSSValueID::kPretty || value_id == CSSValueID::kStable;
    case CSSPropertyID::kTransformBox:
      return value_id == CSSValueID::kContentBox ||
             value_id == CSSValueID::kBorderBox ||
             value_id == CSSValueID::kStrokeBox ||
             value_id == CSSValueID::kFillBox ||
             value_id == CSSValueID::kViewBox;
    case CSSPropertyID::kTransformStyle:
      return value_id == CSSValueID::kFlat ||
             value_id == CSSValueID::kPreserve3d;
    case CSSPropertyID::kWebkitUserDrag:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kElement;
    case CSSPropertyID::kWebkitUserModify:
      return value_id == CSSValueID::kReadOnly ||
             value_id == CSSValueID::kReadWrite ||
             value_id == CSSValueID::kReadWritePlaintextOnly;
    case CSSPropertyID::kUserSelect:
      if (!RuntimeEnabledFeatures::CSSUserSelectContainEnabled()) {
        return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone ||
               value_id == CSSValueID::kText || value_id == CSSValueID::kAll;
      }
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kText || value_id == CSSValueID::kAll ||
             value_id == CSSValueID::kContain;
    case CSSPropertyID::kWebkitWritingMode:
      return value_id >= CSSValueID::kHorizontalTb &&
             value_id <= CSSValueID::kVerticalLr;
    case CSSPropertyID::kWritingMode:
      if (RuntimeEnabledFeatures::SidewaysWritingModesEnabled()) {
        if (value_id == CSSValueID::kSidewaysRl ||
            value_id == CSSValueID::kSidewaysLr) {
          return true;
        }
      }
      return value_id == CSSValueID::kHorizontalTb ||
             value_id == CSSValueID::kVerticalRl ||
             value_id == CSSValueID::kVerticalLr ||
             value_id == CSSValueID::kLrTb || value_id == CSSValueID::kRlTb ||
             value_id == CSSValueID::kTbRl || value_id == CSSValueID::kLr ||
             value_id == CSSValueID::kRl || value_id == CSSValueID::kTb;
    case CSSPropertyID::kWhiteSpaceCollapse:
      return value_id == CSSValueID::kCollapse ||
             value_id == CSSValueID::kPreserve ||
             value_id == CSSValueID::kPreserveBreaks ||
             value_id == CSSValueID::kBreakSpaces;
    case CSSPropertyID::kWordBreak:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kBreakAll ||
             value_id == CSSValueID::kKeepAll ||
             value_id == CSSValueID::kBreakWord ||
             value_id == CSSValueID::kAutoPhrase;
    case CSSPropertyID::kScrollbarWidth:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kThin ||
             value_id == CSSValueID::kNone;
    case CSSPropertyID::kScrollSnapStop:
      return value_id == CSSValueID::kNormal || value_id == CSSValueID::kAlways;
    case CSSPropertyID::kOverscrollBehaviorInline:
    case CSSPropertyID::kOverscrollBehaviorBlock:
    case CSSPropertyID::kOverscrollBehaviorX:
    case CSSPropertyID::kOverscrollBehaviorY:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kContain || value_id == CSSValueID::kNone;
    case CSSPropertyID::kOriginTrialTestProperty:
      return value_id == CSSValueID::kNormal || value_id == CSSValueID::kNone;
    case CSSPropertyID::kTextBoxTrim:
      DCHECK(RuntimeEnabledFeatures::CSSTextBoxTrimEnabled());
      return value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kTrimStart ||
             value_id == CSSValueID::kTrimEnd ||
             value_id == CSSValueID::kTrimBoth;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

// NOTE: This list must match exactly those properties handled by
// IsValidKeywordPropertyAndValue().
CSSBitset CSSParserFastPaths::handled_by_keyword_fast_paths_properties_{{
    CSSPropertyID::kAlignmentBaseline,
    CSSPropertyID::kAll,
    CSSPropertyID::kAppearance,
    CSSPropertyID::kMixBlendMode,
    CSSPropertyID::kIsolation,
    CSSPropertyID::kBaselineSource,
    CSSPropertyID::kBorderBottomStyle,
    CSSPropertyID::kBorderCollapse,
    CSSPropertyID::kBorderLeftStyle,
    CSSPropertyID::kBorderRightStyle,
    CSSPropertyID::kBorderTopStyle,
    CSSPropertyID::kBoxDecorationBreak,
    CSSPropertyID::kBoxSizing,
    CSSPropertyID::kBufferedRendering,
    CSSPropertyID::kCaptionSide,
    CSSPropertyID::kCaretAnimation,
    CSSPropertyID::kClear,
    CSSPropertyID::kClipRule,
    CSSPropertyID::kColorInterpolation,
    CSSPropertyID::kColorInterpolationFilters,
    CSSPropertyID::kColorRendering,
    CSSPropertyID::kDirection,
    CSSPropertyID::kDominantBaseline,
    CSSPropertyID::kEmptyCells,
    CSSPropertyID::kFillRule,
    CSSPropertyID::kFloat,
    CSSPropertyID::kFieldSizing,
    CSSPropertyID::kForcedColorAdjust,
    CSSPropertyID::kHyphens,
    CSSPropertyID::kImageRendering,
    CSSPropertyID::kInternalOverflowBlock,
    CSSPropertyID::kInternalOverflowInline,
    CSSPropertyID::kInterpolateSize,
    CSSPropertyID::kListStylePosition,
    CSSPropertyID::kMaskType,
    CSSPropertyID::kMathShift,
    CSSPropertyID::kMathStyle,
    CSSPropertyID::kObjectFit,
    CSSPropertyID::kOutlineStyle,
    CSSPropertyID::kOverflowAnchor,
    CSSPropertyID::kOverflowBlock,
    CSSPropertyID::kOverflowInline,
    CSSPropertyID::kOverflowWrap,
    CSSPropertyID::kOverflowX,
    CSSPropertyID::kOverflowY,
    CSSPropertyID::kBreakAfter,
    CSSPropertyID::kBreakBefore,
    CSSPropertyID::kBreakInside,
    CSSPropertyID::kPageOrientation,
    CSSPropertyID::kPointerEvents,
    CSSPropertyID::kPosition,
    CSSPropertyID::kPositionTryOrder,
    CSSPropertyID::kReadingFlow,
    CSSPropertyID::kResize,
    CSSPropertyID::kScrollMarkerGroup,
    CSSPropertyID::kScrollBehavior,
    CSSPropertyID::kOverscrollBehaviorInline,
    CSSPropertyID::kOverscrollBehaviorBlock,
    CSSPropertyID::kOverscrollBehaviorX,
    CSSPropertyID::kOverscrollBehaviorY,
    CSSPropertyID::kRubyAlign,
    CSSPropertyID::kShapeRendering,
    CSSPropertyID::kSpeak,
    CSSPropertyID::kStrokeLinecap,
    CSSPropertyID::kStrokeLinejoin,
    CSSPropertyID::kTableLayout,
    CSSPropertyID::kTextAlign,
    CSSPropertyID::kTextAlignLast,
    CSSPropertyID::kTextAnchor,
    CSSPropertyID::kTextAutospace,
    CSSPropertyID::kTextCombineUpright,
    CSSPropertyID::kTextDecorationStyle,
    CSSPropertyID::kTextDecorationSkipInk,
    CSSPropertyID::kTextOrientation,
    CSSPropertyID::kWebkitTextOrientation,
    CSSPropertyID::kTextOverflow,
    CSSPropertyID::kTextRendering,
    CSSPropertyID::kTextSpacingTrim,
    CSSPropertyID::kTextTransform,
    CSSPropertyID::kUnicodeBidi,
    CSSPropertyID::kVectorEffect,
    CSSPropertyID::kVisibility,
    CSSPropertyID::kAppRegion,
    CSSPropertyID::kBackfaceVisibility,
    CSSPropertyID::kBorderBlockEndStyle,
    CSSPropertyID::kBorderBlockStartStyle,
    CSSPropertyID::kBorderInlineEndStyle,
    CSSPropertyID::kBorderInlineStartStyle,
    CSSPropertyID::kWebkitBoxAlign,
    CSSPropertyID::kWebkitBoxDecorationBreak,
    CSSPropertyID::kWebkitBoxDirection,
    CSSPropertyID::kWebkitBoxOrient,
    CSSPropertyID::kWebkitBoxPack,
    CSSPropertyID::kColumnFill,
    CSSPropertyID::kColumnRuleStyle,
    CSSPropertyID::kFlexDirection,
    CSSPropertyID::kFlexWrap,
    CSSPropertyID::kFontKerning,
    CSSPropertyID::kFontOpticalSizing,
    CSSPropertyID::kFontSynthesisWeight,
    CSSPropertyID::kFontSynthesisStyle,
    CSSPropertyID::kFontSynthesisSmallCaps,
    CSSPropertyID::kFontVariantEmoji,
    CSSPropertyID::kFontVariantPosition,
    CSSPropertyID::kWebkitFontSmoothing,
    CSSPropertyID::kLineBreak,
    CSSPropertyID::kWebkitLineBreak,
    CSSPropertyID::kWebkitPrintColorAdjust,
    CSSPropertyID::kWebkitRtlOrdering,
    CSSPropertyID::kWebkitRubyPosition,
    CSSPropertyID::kWebkitTextCombine,
    CSSPropertyID::kWebkitTextSecurity,
    CSSPropertyID::kTextWrapMode,
    CSSPropertyID::kTextWrapStyle,
    CSSPropertyID::kTransformBox,
    CSSPropertyID::kTransformStyle,
    CSSPropertyID::kWebkitUserDrag,
    CSSPropertyID::kWebkitUserModify,
    CSSPropertyID::kUserSelect,
    CSSPropertyID::kWebkitWritingMode,
    CSSPropertyID::kWhiteSpaceCollapse,
    CSSPropertyID::kWordBreak,
    CSSPropertyID::kWritingMode,
    CSSPropertyID::kScrollbarWidth,
    CSSPropertyID::kScrollSnapStop,
    CSSPropertyID::kOriginTrialTestProperty,
    CSSPropertyID::kOverlay,
    CSSPropertyID::kTextBoxTrim,
    CSSPropertyID::kScrollStartTarget,
}};

bool CSSParserFastPaths::IsValidSystemFont(CSSValueID value_id) {
  return value_id >= CSSValueID::kCaption && value_id <= CSSValueID::kStatusBar;
}

static inline CSSValue* ParseCSSWideKeywordValue(const LChar* ptr,
                                                 unsigned length) {
  if (length == 7 && MatchesCaseInsensitiveLiteral4(ptr, "init") &&
      MatchesCaseInsensitiveLiteral4(ptr + 3, "tial")) {
    return CSSInitialValue::Create();
  }
  if (length == 7 && MatchesCaseInsensitiveLiteral4(ptr, "inhe") &&
      MatchesCaseInsensitiveLiteral4(ptr + 3, "erit")) {
    return CSSInheritedValue::Create();
  }
  if (length == 5 && MatchesCaseInsensitiveLiteral4(ptr, "unse") &&
      IsASCIIAlphaCaselessEqual(ptr[4], 't')) {
    return cssvalue::CSSUnsetValue::Create();
  }
  if (length == 6 && MatchesCaseInsensitiveLiteral4(ptr, "reve") &&
      MatchesCaseInsensitiveLiteral2(ptr + 4, "rt")) {
    return cssvalue::CSSRevertValue::Create();
  }
  if (length == 12 && MatchesCaseInsensitiveLiteral4(ptr, "reve") &&
      MatchesCaseInsensitiveLiteral4(ptr + 4, "rt-l") &&
      MatchesCaseInsensitiveLiteral4(ptr + 8, "ayer")) {
    return cssvalue::CSSRevertLayerValue::Create();
  }
  return nullptr;
}

static CSSValue* ParseKeywordValue(CSSPropertyID property_id,
                                   StringView string,
                                   const CSSParserContext* context) {
  DCHECK(!string.empty());

  CSSValue* css_wide_keyword =
      ParseCSSWideKeywordValue(string.Characters8(), string.length());

  if (!CSSParserFastPaths::IsHandledByKeywordFastPath(property_id)) {
    // This isn't a property we have a fast path for, but even
    // so, it will generally accept a CSS-wide keyword.
    // So check if we're in that situation, in which case we
    // can run through the fast path anyway (if not, we'll return
    // nullptr, letting us fall back to the slow path).

    if (css_wide_keyword == nullptr) {
      return nullptr;
    }

    if (shorthandForProperty(property_id).length()) {
      // CSS-wide keyword shorthands must be parsed using the CSSPropertyParser.
      return nullptr;
    }

    if (!CSSProperty::Get(property_id).IsProperty()) {
      // Descriptors do not support CSS-wide keywords.
      return nullptr;
    }

    // Fall through.
  }

  if (css_wide_keyword != nullptr) {
    return css_wide_keyword;
  }

  CSSValueID value_id = CssValueKeywordID(string);

  if (!IsValidCSSValueID(value_id)) {
    return nullptr;
  }

  DCHECK_NE(value_id, CSSValueID::kInherit);
  DCHECK_NE(value_id, CSSValueID::kInitial);
  DCHECK_NE(value_id, CSSValueID::kUnset);
  DCHECK_NE(value_id, CSSValueID::kRevert);
  DCHECK_NE(value_id, CSSValueID::kRevertLayer);

  if (CSSParserFastPaths::IsValidKeywordPropertyAndValue(property_id, value_id,
                                                         context->Mode())) {
    css_parsing_utils::CountKeywordOnlyPropertyUsage(property_id, *context,
                                                     value_id);
    return CSSIdentifierValue::Create(value_id);
  }
  css_parsing_utils::WarnInvalidKeywordPropertyUsage(property_id, *context,
                                                     value_id);
  return nullptr;
}

static bool ParseTransformTranslateArguments(
    const LChar*& pos,
    const LChar* end,
    unsigned expected_count,
    CSSFunctionValue* transform_value) {
  while (expected_count) {
    wtf_size_t delimiter = WTF::Find(pos, static_cast<wtf_size_t>(end - pos),
                                     expected_count == 1 ? ')' : ',');
    if (delimiter == kNotFound) {
      return false;
    }
    unsigned argument_length = static_cast<unsigned>(delimiter);
    CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::kNumber;
    double number;
    if (!ParseSimpleLength(pos, argument_length, unit, number)) {
      return false;
    }
    if (unit != CSSPrimitiveValue::UnitType::kPixels &&
        (number || unit != CSSPrimitiveValue::UnitType::kNumber)) {
      return false;
    }
    transform_value->Append(*CSSNumericLiteralValue::Create(
        number, CSSPrimitiveValue::UnitType::kPixels));
    pos += argument_length + 1;
    --expected_count;
  }
  return true;
}

static bool ParseTransformRotateArgument(const LChar*& pos,
                                         const LChar* end,
                                         CSSFunctionValue* transform_value) {
  wtf_size_t delimiter =
      WTF::Find(pos, static_cast<wtf_size_t>(end - pos), ')');
  if (delimiter == kNotFound) {
    return false;
  }
  unsigned argument_length = static_cast<unsigned>(delimiter);
  CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::kNumber;
  double number;
  if (ParseSimpleAngle(pos, argument_length, unit, number) != argument_length) {
    return false;
  }
  if (unit == CSSPrimitiveValue::UnitType::kNumber) {
    if (number != 0.0) {
      return false;
    } else {
      // Matches ConsumeNumericLiteralAngle().
      unit = CSSPrimitiveValue::UnitType::kDegrees;
    }
  }
  transform_value->Append(*CSSNumericLiteralValue::Create(number, unit));
  pos += argument_length + 1;
  return true;
}

static bool ParseTransformNumberArguments(const LChar*& pos,
                                          const LChar* end,
                                          unsigned expected_count,
                                          CSSFunctionValue* transform_value) {
  while (expected_count) {
    wtf_size_t delimiter = WTF::Find(pos, static_cast<wtf_size_t>(end - pos),
                                     expected_count == 1 ? ')' : ',');
    if (delimiter == kNotFound) {
      return false;
    }
    unsigned argument_length = static_cast<unsigned>(delimiter);
    double number;
    if (!ParseDoubleWithPrefix(pos, pos + argument_length, number)) {
      return false;
    }
    transform_value->Append(*CSSNumericLiteralValue::Create(
        number, CSSPrimitiveValue::UnitType::kNumber));
    pos += argument_length + 1;
    --expected_count;
  }
  return true;
}

static const int kShortestValidTransformStringLength = 12;

static CSSFunctionValue* ParseSimpleTransformValue(const LChar*& pos,
                                                   const LChar* end) {
  if (end - pos < kShortestValidTransformStringLength) {
    return nullptr;
  }

  const bool is_translate = MatchesLiteral(pos, "translate");

  if (is_translate) {
    CSSValueID transform_type;
    unsigned expected_argument_count = 1;
    unsigned argument_start = 11;
    if (IsASCIIAlphaCaselessEqual(pos[9], 'x') && pos[10] == '(') {
      transform_type = CSSValueID::kTranslateX;
    } else if (IsASCIIAlphaCaselessEqual(pos[9], 'y') && pos[10] == '(') {
      transform_type = CSSValueID::kTranslateY;
    } else if (IsASCIIAlphaCaselessEqual(pos[9], 'z') && pos[10] == '(') {
      transform_type = CSSValueID::kTranslateZ;
    } else if (pos[9] == '(') {
      transform_type = CSSValueID::kTranslate;
      expected_argument_count = 2;
      argument_start = 10;
    } else if (pos[9] == '3' && pos[10] == 'd' && pos[11] == '(') {
      transform_type = CSSValueID::kTranslate3d;
      expected_argument_count = 3;
      argument_start = 12;
    } else {
      return nullptr;
    }
    pos += argument_start;
    CSSFunctionValue* transform_value =
        MakeGarbageCollected<CSSFunctionValue>(transform_type);
    if (!ParseTransformTranslateArguments(pos, end, expected_argument_count,
                                          transform_value)) {
      return nullptr;
    }
    return transform_value;
  }

  const bool is_matrix3d = MatchesLiteral(pos, "matrix3d(");

  if (is_matrix3d) {
    pos += 9;
    CSSFunctionValue* transform_value =
        MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMatrix3d);
    if (!ParseTransformNumberArguments(pos, end, 16, transform_value)) {
      return nullptr;
    }
    return transform_value;
  }

  const bool is_scale3d = MatchesLiteral(pos, "scale3d(");

  if (is_scale3d) {
    pos += 8;
    CSSFunctionValue* transform_value =
        MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kScale3d);
    if (!ParseTransformNumberArguments(pos, end, 3, transform_value)) {
      return nullptr;
    }
    return transform_value;
  }

  const bool is_rotate = MatchesLiteral(pos, "rotate");

  if (is_rotate) {
    CSSValueID rotate_value_id = CSSValueID::kInvalid;
    if (pos[6] == '(') {
      pos += 7;
      rotate_value_id = CSSValueID::kRotate;
    } else if (IsASCIIAlphaCaselessEqual(pos[6], 'z') && pos[7] == '(') {
      pos += 8;
      rotate_value_id = CSSValueID::kRotateZ;
    } else {
      return nullptr;
    }
    CSSFunctionValue* transform_value =
        MakeGarbageCollected<CSSFunctionValue>(rotate_value_id);
    if (!ParseTransformRotateArgument(pos, end, transform_value)) {
      return nullptr;
    }
    return transform_value;
  }

  return nullptr;
}

static bool TransformCanLikelyUseFastPath(const LChar* chars, unsigned length) {
  // Very fast scan that attempts to reject most transforms that couldn't
  // take the fast path. This avoids doing the malloc and string->double
  // conversions in parseSimpleTransformValue only to discard them when we
  // run into a transform component we don't understand.
  unsigned i = 0;
  while (i < length) {
    if (chars[i] == ' ') {
      ++i;
      continue;
    }
    if (length - i < kShortestValidTransformStringLength) {
      return false;
    }
    switch ((chars[i])) {
      case 't':
        // translate, translateX, translateY, translateZ, translate3d.
        if (chars[i + 8] != 'e') {
          return false;
        }
        i += 9;
        break;
      case 'm':
        // matrix3d.
        if (chars[i + 7] != 'd') {
          return false;
        }
        i += 8;
        break;
      case 's':
        // scale3d.
        if (chars[i + 6] != 'd') {
          return false;
        }
        i += 7;
        break;
      case 'r':
        // rotate.
        if (chars[i + 5] != 'e') {
          return false;
        }
        i += 6;
        break;
      default:
        // All other things, ex. skew.
        return false;
    }
    wtf_size_t arguments_end = WTF::Find(chars, length, ')', i);
    if (arguments_end == kNotFound) {
      return false;
    }
    // Advance to the end of the arguments.
    i = arguments_end + 1;
  }
  return i == length;
}

static CSSValue* ParseSimpleTransform(CSSPropertyID property_id,
                                      StringView string) {
  DCHECK(!string.empty());

  if (property_id != CSSPropertyID::kTransform) {
    return nullptr;
  }

  const LChar* pos = string.Characters8();
  unsigned length = string.length();
  if (!TransformCanLikelyUseFastPath(pos, length)) {
    return nullptr;
  }
  const auto* end = pos + length;
  CSSValueList* transform_list = nullptr;
  while (pos < end) {
    while (pos < end && *pos == ' ') {
      ++pos;
    }
    if (pos >= end) {
      break;
    }
    auto* transform_value = ParseSimpleTransformValue(pos, end);
    if (!transform_value) {
      return nullptr;
    }
    if (!transform_list) {
      transform_list = CSSValueList::CreateSpaceSeparated();
    }
    transform_list->Append(*transform_value);
  }
  return transform_list;
}

CSSValue* CSSParserFastPaths::MaybeParseValue(CSSPropertyID property_id,
                                              StringView string,
                                              const CSSParserContext* context) {
  if (!string.Is8Bit()) {
    // If we have non-ASCII characters, we can never match any of the
    // fast paths that we support, so we can just as well return early.
    // (We could be UChar due to unrelated comments, but we don't
    // support comments in these paths anyway.)
    return nullptr;
  }
  if (CSSValue* length =
          ParseSimpleLengthValue(property_id, string, context->Mode())) {
    return length;
  }
  if (IsColorPropertyID(property_id)) {
    Color color;
    CSSValueID color_id;
    switch (blink::ParseColor(property_id, string, context->Mode(), color,
                              color_id)) {
      case ParseColorResult::kFailure:
        break;
      case ParseColorResult::kKeyword:
        return CSSIdentifierValue::Create(color_id);
      case ParseColorResult::kColor:
        return cssvalue::CSSColor::Create(color);
    }
  }
  if (CSSValue* keyword = ParseKeywordValue(property_id, string, context)) {
    return keyword;
  }
  if (CSSValue* transform = ParseSimpleTransform(property_id, string)) {
    return transform;
  }
  return nullptr;
}

}  // namespace blink

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"

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
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

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

static inline bool ParseSimpleLength(const LChar* characters,
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

  // We rely on charactersToDouble for validation as well. The function
  // will set "ok" to "false" if the entire passed-in character range does
  // not represent a double.
  bool ok;
  number = CharactersToDouble(characters, length, &ok);
  if (!ok) {
    return false;
  }
  number = ClampTo<double>(number, -std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());
  return true;
}

static CSSValue* ParseSimpleLengthValue(CSSPropertyID property_id,
                                        const String& string,
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

template <typename CharacterType>
static inline bool ParseSimpleAngle(const CharacterType* characters,
                                    unsigned length,
                                    CSSPrimitiveValue::UnitType& unit,
                                    double& number) {
  if (length > 3 && (characters[length - 3] | 0x20) == 'd' &&
      (characters[length - 2] | 0x20) == 'e' &&
      (characters[length - 1] | 0x20) == 'g') {
    length -= 3;
    unit = CSSPrimitiveValue::UnitType::kDegrees;
  } else if (length > 4 && (characters[length - 4] | 0x20) == 'g' &&
             (characters[length - 3] | 0x20) == 'r' &&
             (characters[length - 2] | 0x20) == 'a' &&
             (characters[length - 1] | 0x20) ==
                 'd') {  // Note: 'grad' must be checked before 'rad'.
    length -= 4;
    unit = CSSPrimitiveValue::UnitType::kGradians;
  } else if (length > 3 && (characters[length - 3] | 0x20) == 'r' &&
             (characters[length - 2] | 0x20) == 'a' &&
             (characters[length - 1] | 0x20) == 'd') {
    length -= 3;
    unit = CSSPrimitiveValue::UnitType::kRadians;
  } else if (length > 4 && (characters[length - 4] | 0x20) == 't' &&
             (characters[length - 3] | 0x20) == 'u' &&
             (characters[length - 2] | 0x20) == 'r' &&
             (characters[length - 1] | 0x20) == 'n') {
    length -= 4;
    unit = CSSPrimitiveValue::UnitType::kTurns;
  } else {
    // For rotate: Only valid for zero (we'll check that in the caller).
    // For hsl(): To be treated as angles (also done in the caller).
    unit = CSSPrimitiveValue::UnitType::kNumber;
  }

  // We rely on charactersToDouble for validation as well. The function
  // will set "ok" to "false" if the entire passed-in character range does
  // not represent a double.
  bool ok;
  number = CharactersToDouble(characters, length, &ok);
  if (!ok) {
    return false;
  }
  number = ClampTo<double>(number, -std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());
  return true;
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
template <typename CharacterType>
static int FindLengthOfValidDouble(const CharacterType* string,
                                   const CharacterType* end) {
  int length = static_cast<int>(end - string);
  if (length < 1) {
    return 0;
  }

  bool decimal_mark_seen = false;
  int processed_length = 0;

  for (int i = 0; i < length; ++i, ++processed_length) {
    if (!IsASCIIDigit(string[i])) {
      if (!decimal_mark_seen && string[i] == '.') {
        decimal_mark_seen = true;
      } else {
        break;
      }
    }
  }

  if (decimal_mark_seen && processed_length == 1) {
    return 0;
  }

  return processed_length;
}

// If also_accept_whitespace is true: Checks whether string[pos] is the given
// character, _or_ an HTML space.
// Otherwise: Checks whether string[pos] is the given character.
// Returns false if pos is past the end of the string.
template <typename CharacterType>
static bool ContainsCharAtPos(const CharacterType* string,
                              const CharacterType* end,
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

// Returns the number of characters consumed for parsing a valid double,
// or 0 if the string did not start with a valid double.
template <typename CharacterType>
static int ParseDouble(const CharacterType* string,
                       const CharacterType* end,
                       double& value) {
  int length = FindLengthOfValidDouble(string, end);
  if (length == 0) {
    return 0;
  }

  int position = 0;
  double local_value = 0;

  // The consumed characters here are guaranteed to be
  // ASCII digits with or without a decimal mark
  for (; position < length; ++position) {
    if (string[position] == '.') {
      break;
    }
    local_value = local_value * 10 + string[position] - '0';
  }

  if (++position == length) {
    value = local_value;
    return length;
  }

  double fraction = 0;
  double scale = 1;

  const double kMaxScale = 1000000;
  while (position < length && scale < kMaxScale) {
    fraction = fraction * 10 + string[position++] - '0';
    scale *= 10;
  }

  value = local_value + fraction / scale;
  return length;
}

// Parse a float and clamp it upwards to max_value. Optimized for having
// no decimal part.
template <typename CharacterType>
static bool ParseFloatWithMaxValue(const CharacterType*& string,
                                   const CharacterType* end,
                                   int max_value,
                                   double& value,
                                   bool& negative) {
  value = 0.0;
  const CharacterType* current = string;
  while (current != end && IsHTMLSpace<CharacterType>(*current)) {
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
    double new_value = value * 10 + *current++ - '0';
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

  if (current == end) {
    return false;
  }

  if (*current == '.') {
    // We already parsed the integral part, try to parse
    // the fraction part.
    double fractional = 0;
    int num_characters_parsed = ParseDouble(current, end, fractional);
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

template <typename CharacterType>
static bool SkipToTerminator(const CharacterType*& string,
                             const CharacterType* end,
                             const char terminator,
                             TerminatorStatus& terminator_status) {
  const CharacterType* current = string;

  while (current != end && IsHTMLSpace<CharacterType>(*current)) {
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

template <typename CharacterType>
static bool ParseColorNumberOrPercentage(const CharacterType*& string,
                                         const CharacterType* end,
                                         const char terminator,
                                         TerminatorStatus& terminator_status,
                                         CSSPrimitiveValue::UnitType& expect,
                                         int& value) {
  const CharacterType* current = string;
  double local_value;
  bool negative = false;
  if (!ParseFloatWithMaxValue<CharacterType>(current, end, 255, local_value,
                                             negative)) {
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
  value = negative ? 0 : static_cast<int>(round(local_value));
  string = current;
  return true;
}

// Parses a percentage (including the % sign), clamps it and converts it to
// 0.0..1.0.
template <typename CharacterType>
static bool ParsePercentage(const CharacterType*& string,
                            const CharacterType* end,
                            const char terminator,
                            TerminatorStatus& terminator_status,
                            double& value) {
  const CharacterType* current = string;
  bool negative = false;
  if (!ParseFloatWithMaxValue<CharacterType>(current, end, 100, value,
                                             negative)) {
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

template <typename CharacterType>
static inline bool IsTenthAlpha(const CharacterType* string,
                                const wtf_size_t length) {
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

template <typename CharacterType>
static inline bool ParseAlphaValue(const CharacterType*& string,
                                   const CharacterType* end,
                                   const char terminator,
                                   int& value) {
  while (string != end && IsHTMLSpace<CharacterType>(*string)) {
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
  int dbl_length = ParseDouble(string, end, alpha);
  if (dbl_length == 0 || !ContainsCharAtPos(string, end, dbl_length, terminator,
                                            /*also_accept_whitespace=*/false)) {
    return false;
  }
  value = negative ? 0 : static_cast<int>(round(std::min(alpha, 1.0) * 255.0));
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

template <typename CharacterType>
static inline bool MightBeRGBOrRGBA(const CharacterType* characters,
                                    unsigned length) {
  if (length < 5) {
    return false;
  }
  return MatchesLiteral(characters, "rgb") &&
         (characters[3] == '(' ||
          (characters[3] == 'a' && characters[4] == '('));
}

template <typename CharacterType>
static inline bool MightBeHSLOrHSLA(const CharacterType* characters,
                                    unsigned length) {
  if (length < 5) {
    return false;
  }
  return MatchesLiteral(characters, "hsl") &&
         (characters[3] == '(' ||
          (characters[3] == 'a' && characters[4] == '('));
}

template <typename CharacterType>
static bool FastParseColorInternal(Color& color,
                                   const CharacterType* characters,
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
    const CharacterType* current = characters + length_to_add;
    const CharacterType* end = characters + length;
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
    const CharacterType* current = characters + length_to_add;
    const CharacterType* end = characters + length;
    bool should_have_alpha = false;

    // Skip any whitespace before the hue.
    while (current != end && IsHTMLSpace(*current)) {
      current++;
    }

    // Find the end of the hue. This isn't optimal, but allows us to reuse
    // ParseAngle() cleanly.
    const CharacterType* hue_end = current;
    while (hue_end != end && !IsHTMLSpace(*hue_end) && *hue_end != ',') {
      hue_end++;
    }

    CSSPrimitiveValue::UnitType hue_unit = CSSPrimitiveValue::UnitType::kNumber;
    double hue;
    if (!ParseSimpleAngle(current, static_cast<unsigned>(hue_end - current),
                          hue_unit, hue)) {
      return false;
    }

    // We need to convert the hue to the 0..6 scale that FromHSLA() expects.
    switch (hue_unit) {
      case CSSPrimitiveValue::UnitType::kNumber:
      case CSSPrimitiveValue::UnitType::kDegrees:
        // Unitless numbers are to be treated as degrees.
        hue *= (6.0 / 360.0);
        break;
      case CSSPrimitiveValue::UnitType::kRadians:
        hue = Rad2deg(hue) * (6.0 / 360.0);
        break;
      case CSSPrimitiveValue::UnitType::kGradians:
        hue = Grad2deg(hue) * (6.0 / 360.0);
        break;
      case CSSPrimitiveValue::UnitType::kTurns:
        hue *= 6.0;
        break;
      default:
        NOTREACHED();
        return false;
    }

    // Deal with wraparound so that we end up in 0..6,
    // roughly analogous to the code in ParseHSLParameters().
    // Taking these branches should be rare.
    if (hue < 0.0) {
      hue = fmod(hue, 6.0) + 6.0;
    } else if (hue > 6.0) {
      hue = fmod(hue, 6.0);
    }

    current = hue_end;

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
                                   const String& string,
                                   CSSParserMode parser_mode,
                                   Color& out_color,
                                   CSSValueID& out_color_keyword) {
  DCHECK(!string.empty());
  DCHECK(IsColorPropertyID(property_id));
  CSSValueID value_id = CssValueKeywordID(string);
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
  const bool parsed =
      WTF::VisitCharacters(string, [&](const auto* chars, unsigned length) {
        return FastParseColorInternal(out_color, chars, length, quirks_mode);
      });
  return parsed ? ParseColorResult::kColor : ParseColorResult::kFailure;
}

ParseColorResult CSSParserFastPaths::ParseColor(const String& string,
                                                CSSParserMode parser_mode,
                                                Color& color) {
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
    case CSSPropertyID::kBackgroundRepeatX:
    case CSSPropertyID::kBackgroundRepeatY:
      return value_id == CSSValueID::kRepeat ||
             value_id == CSSValueID::kNoRepeat;
    case CSSPropertyID::kBaselineSource:
      DCHECK(RuntimeEnabledFeatures::CSSBaselineSourceEnabled());
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
    case CSSPropertyID::kClear:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kLeft ||
             value_id == CSSValueID::kRight || value_id == CSSValueID::kBoth ||
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueID::kInlineStart ||
               value_id == CSSValueID::kInlineEnd));
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
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueID::kInlineStart ||
               value_id == CSSValueID::kInlineEnd)) ||
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
    case CSSPropertyID::kIsolation:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kIsolate;
    case CSSPropertyID::kListStylePosition:
      return value_id == CSSValueID::kInside ||
             value_id == CSSValueID::kOutside;
    case CSSPropertyID::kMaskType:
      return value_id == CSSValueID::kLuminance ||
             value_id == CSSValueID::kAlpha;
    case CSSPropertyID::kMathShift:
      DCHECK(RuntimeEnabledFeatures::CSSMathShiftEnabled());
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kCompact;
    case CSSPropertyID::kMathStyle:
      DCHECK(RuntimeEnabledFeatures::CSSMathStyleEnabled());
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
    case CSSPropertyID::kResize:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kBoth ||
             value_id == CSSValueID::kHorizontal ||
             value_id == CSSValueID::kVertical ||
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueID::kBlock ||
               value_id == CSSValueID::kInline)) ||
             value_id == CSSValueID::kAuto;
    case CSSPropertyID::kScrollBehavior:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kSmooth;
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
    case CSSPropertyID::kTopLayer:
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kBrowser;
    case CSSPropertyID::kTextRendering:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kOptimizespeed ||
             value_id == CSSValueID::kOptimizelegibility ||
             value_id == CSSValueID::kGeometricprecision;
    case CSSPropertyID::kTextTransform:
      return (value_id >= CSSValueID::kCapitalize &&
              value_id <= CSSValueID::kLowercase) ||
             value_id == CSSValueID::kNone ||
             (RuntimeEnabledFeatures::CSSMathVariantEnabled() &&
              value_id == CSSValueID::kMathAuto);
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
             value_id == CSSValueID::kCollapse;
    case CSSPropertyID::kAppRegion:
      return (value_id >= CSSValueID::kDrag &&
              value_id <= CSSValueID::kNoDrag) ||
             value_id == CSSValueID::kNone;
    case CSSPropertyID::kAppearance:
      return (value_id >= CSSValueID::kCheckbox &&
              value_id <= CSSValueID::kTextarea) ||
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
             (RuntimeEnabledFeatures::CSSMixBlendModePlusLighterEnabled() &&
              value_id == CSSValueID::kPlusLighter);
    case CSSPropertyID::kWebkitBoxAlign:
      return value_id == CSSValueID::kStretch ||
             value_id == CSSValueID::kStart || value_id == CSSValueID::kEnd ||
             value_id == CSSValueID::kCenter ||
             value_id == CSSValueID::kBaseline;
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
      DCHECK(RuntimeEnabledFeatures::FontVariantPositionEnabled());
      return value_id == CSSValueID::kNormal || value_id == CSSValueID::kSub ||
             value_id == CSSValueID::kSuper;
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
    case CSSPropertyID::kWebkitRubyPosition:
      return value_id == CSSValueID::kBefore || value_id == CSSValueID::kAfter;
    case CSSPropertyID::kRubyPosition:
      return value_id == CSSValueID::kOver || value_id == CSSValueID::kUnder;
    case CSSPropertyID::kWebkitTextCombine:
      return value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kHorizontal;
    case CSSPropertyID::kWebkitTextSecurity:
      return value_id == CSSValueID::kDisc || value_id == CSSValueID::kCircle ||
             value_id == CSSValueID::kSquare || value_id == CSSValueID::kNone;
    case CSSPropertyID::kTextWrap:
      DCHECK(RuntimeEnabledFeatures::CSSTextWrapEnabled());
      return value_id == CSSValueID::kWrap || value_id == CSSValueID::kBalance;
    case CSSPropertyID::kTransformBox:
      return value_id == CSSValueID::kFillBox ||
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
      return value_id == CSSValueID::kHorizontalTb ||
             value_id == CSSValueID::kVerticalRl ||
             value_id == CSSValueID::kVerticalLr ||
             value_id == CSSValueID::kLrTb || value_id == CSSValueID::kRlTb ||
             value_id == CSSValueID::kTbRl || value_id == CSSValueID::kLr ||
             value_id == CSSValueID::kRl || value_id == CSSValueID::kTb;
    case CSSPropertyID::kWhiteSpace:
      return value_id == CSSValueID::kNormal || value_id == CSSValueID::kPre ||
             value_id == CSSValueID::kPreWrap ||
             value_id == CSSValueID::kPreLine ||
             value_id == CSSValueID::kNowrap ||
             value_id == CSSValueID::kBreakSpaces;
    case CSSPropertyID::kWordBreak:
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kBreakAll ||
             value_id == CSSValueID::kKeepAll ||
             value_id == CSSValueID::kBreakWord;
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
      return value_id == CSSValueID::kNormal ||
             value_id == CSSValueID::kStart || value_id == CSSValueID::kEnd ||
             value_id == CSSValueID::kBoth;
    default:
      NOTREACHED();
      return false;
  }
}

// NOTE: This list must match exactly those properties handled by
// IsValidKeywordPropertyAndValue().
CSSBitset CSSParserFastPaths::handled_by_keyword_fast_paths_properties_{{
    CSSPropertyID::kAlignmentBaseline,
    CSSPropertyID::kAll,
    CSSPropertyID::kMixBlendMode,
    CSSPropertyID::kIsolation,
    CSSPropertyID::kBackgroundRepeatX,
    CSSPropertyID::kBackgroundRepeatY,
    CSSPropertyID::kBaselineSource,
    CSSPropertyID::kBorderBottomStyle,
    CSSPropertyID::kBorderCollapse,
    CSSPropertyID::kBorderLeftStyle,
    CSSPropertyID::kBorderRightStyle,
    CSSPropertyID::kBorderTopStyle,
    CSSPropertyID::kBoxSizing,
    CSSPropertyID::kBufferedRendering,
    CSSPropertyID::kCaptionSide,
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
    CSSPropertyID::kForcedColorAdjust,
    CSSPropertyID::kHyphens,
    CSSPropertyID::kImageRendering,
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
    CSSPropertyID::kResize,
    CSSPropertyID::kScrollBehavior,
    CSSPropertyID::kOverscrollBehaviorInline,
    CSSPropertyID::kOverscrollBehaviorBlock,
    CSSPropertyID::kOverscrollBehaviorX,
    CSSPropertyID::kOverscrollBehaviorY,
    CSSPropertyID::kRubyPosition,
    CSSPropertyID::kShapeRendering,
    CSSPropertyID::kSpeak,
    CSSPropertyID::kStrokeLinecap,
    CSSPropertyID::kStrokeLinejoin,
    CSSPropertyID::kTableLayout,
    CSSPropertyID::kTextAlign,
    CSSPropertyID::kTextAlignLast,
    CSSPropertyID::kTextAnchor,
    CSSPropertyID::kTextCombineUpright,
    CSSPropertyID::kTextDecorationStyle,
    CSSPropertyID::kTextDecorationSkipInk,
    CSSPropertyID::kTextOrientation,
    CSSPropertyID::kWebkitTextOrientation,
    CSSPropertyID::kTextOverflow,
    CSSPropertyID::kTextRendering,
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
    CSSPropertyID::kFontVariantPosition,
    CSSPropertyID::kWebkitFontSmoothing,
    CSSPropertyID::kLineBreak,
    CSSPropertyID::kWebkitLineBreak,
    CSSPropertyID::kWebkitPrintColorAdjust,
    CSSPropertyID::kWebkitRtlOrdering,
    CSSPropertyID::kWebkitRubyPosition,
    CSSPropertyID::kWebkitTextCombine,
    CSSPropertyID::kWebkitTextSecurity,
    CSSPropertyID::kTextWrap,
    CSSPropertyID::kTransformBox,
    CSSPropertyID::kTransformStyle,
    CSSPropertyID::kWebkitUserDrag,
    CSSPropertyID::kWebkitUserModify,
    CSSPropertyID::kUserSelect,
    CSSPropertyID::kWebkitWritingMode,
    CSSPropertyID::kWhiteSpace,
    CSSPropertyID::kWordBreak,
    CSSPropertyID::kWritingMode,
    CSSPropertyID::kScrollbarWidth,
    CSSPropertyID::kScrollSnapStop,
    CSSPropertyID::kOriginTrialTestProperty,
    CSSPropertyID::kTopLayer,
    CSSPropertyID::kTextBoxTrim,
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
                                   const String& string,
                                   CSSParserMode parser_mode) {
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
                                                         parser_mode)) {
    return CSSIdentifierValue::Create(value_id);
  }
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
  if (!ParseSimpleAngle(pos, argument_length, unit, number)) {
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
    bool ok;
    double number = CSSValueClampingUtils::ClampDouble(
        CharactersToDouble(pos, argument_length, &ok));
    if (!ok) {
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

  // TODO(crbug.com/841960): Many of these use CharactersToDouble(),
  // which accepts numbers in scientific notation that do not end
  // in a digit; e.g., 1.e10px. (1.0e10px is allowed.) This means that
  // the fast path accepts some invalid lengths that the regular path
  // does not.

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

template <typename CharType>
static bool TransformCanLikelyUseFastPath(const CharType* chars,
                                          unsigned length) {
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
                                      const String& string) {
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
                                              const String& string,
                                              CSSParserMode parser_mode) {
  if (!string.Is8Bit()) {
    // If we have non-ASCII characters, we can never match any of the
    // fast paths that we support, so we can just as well return early.
    // (We could be UChar due to unrelated comments, but we don't
    // support comments in these paths anyway.)
    return nullptr;
  }
  if (CSSValue* length =
          ParseSimpleLengthValue(property_id, string, parser_mode)) {
    return length;
  }
  if (IsColorPropertyID(property_id)) {
    Color color;
    CSSValueID color_id;
    switch (
        blink::ParseColor(property_id, string, parser_mode, color, color_id)) {
      case ParseColorResult::kFailure:
        break;
      case ParseColorResult::kKeyword:
        return CSSIdentifierValue::Create(color_id);
      case ParseColorResult::kColor:
        return cssvalue::CSSColor::Create(color);
    }
  }
  if (CSSValue* keyword = ParseKeywordValue(property_id, string, parser_mode)) {
    return keyword;
  }
  if (CSSValue* transform = ParseSimpleTransform(property_id, string)) {
    return transform;
  }
  return nullptr;
}

}  // namespace blink

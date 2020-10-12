// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"

#include "build/build_config.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

static inline bool IsSimpleLengthPropertyID(CSSPropertyID property_id,
                                            bool& accepts_negative_numbers) {
  switch (property_id) {
    case CSSPropertyID::kBlockSize:
    case CSSPropertyID::kInlineSize:
    case CSSPropertyID::kMinBlockSize:
    case CSSPropertyID::kMinInlineSize:
    case CSSPropertyID::kFontSize:
    case CSSPropertyID::kHeight:
    case CSSPropertyID::kWidth:
    case CSSPropertyID::kMinHeight:
    case CSSPropertyID::kMinWidth:
    case CSSPropertyID::kPaddingBottom:
    case CSSPropertyID::kPaddingLeft:
    case CSSPropertyID::kPaddingRight:
    case CSSPropertyID::kPaddingTop:
    case CSSPropertyID::kScrollPaddingBlockEnd:
    case CSSPropertyID::kScrollPaddingBlockStart:
    case CSSPropertyID::kScrollPaddingBottom:
    case CSSPropertyID::kScrollPaddingInlineEnd:
    case CSSPropertyID::kScrollPaddingInlineStart:
    case CSSPropertyID::kScrollPaddingLeft:
    case CSSPropertyID::kScrollPaddingRight:
    case CSSPropertyID::kScrollPaddingTop:
    case CSSPropertyID::kPaddingBlockEnd:
    case CSSPropertyID::kPaddingBlockStart:
    case CSSPropertyID::kPaddingInlineEnd:
    case CSSPropertyID::kPaddingInlineStart:
    case CSSPropertyID::kShapeMargin:
    case CSSPropertyID::kR:
    case CSSPropertyID::kRx:
    case CSSPropertyID::kRy:
      accepts_negative_numbers = false;
      return true;
    case CSSPropertyID::kBottom:
    case CSSPropertyID::kCx:
    case CSSPropertyID::kCy:
    case CSSPropertyID::kLeft:
    case CSSPropertyID::kMarginBottom:
    case CSSPropertyID::kMarginLeft:
    case CSSPropertyID::kMarginRight:
    case CSSPropertyID::kMarginTop:
    case CSSPropertyID::kOffsetDistance:
    case CSSPropertyID::kRight:
    case CSSPropertyID::kTop:
    case CSSPropertyID::kMarginBlockEnd:
    case CSSPropertyID::kMarginBlockStart:
    case CSSPropertyID::kMarginInlineEnd:
    case CSSPropertyID::kMarginInlineStart:
    case CSSPropertyID::kX:
    case CSSPropertyID::kY:
      accepts_negative_numbers = true;
      return true;
    default:
      return false;
  }
}

template <typename CharacterType>
static inline bool ParseSimpleLength(const CharacterType* characters,
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
  if (!ok)
    return false;
  number = clampTo<double>(number, -std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());
  return true;
}

static CSSValue* ParseSimpleLengthValue(CSSPropertyID property_id,
                                        const String& string,
                                        CSSParserMode css_parser_mode) {
  DCHECK(!string.IsEmpty());
  bool accepts_negative_numbers = false;

  // In @viewport, width and height are shorthands, not simple length values.
  if (IsCSSViewportParsingEnabledForMode(css_parser_mode) ||
      !IsSimpleLengthPropertyID(property_id, accepts_negative_numbers))
    return nullptr;

  double number;
  CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::kNumber;

  const bool parsed_simple_length =
      WTF::VisitCharacters(string, [&](const auto* chars, unsigned length) {
        return ParseSimpleLength(chars, length, unit, number);
      });
  if (!parsed_simple_length)
    return nullptr;

  if (unit == CSSPrimitiveValue::UnitType::kNumber) {
    if (css_parser_mode == kSVGAttributeMode)
      unit = CSSPrimitiveValue::UnitType::kUserUnits;
    else if (!number)
      unit = CSSPrimitiveValue::UnitType::kPixels;
    else
      return nullptr;
  }

  if (number < 0 && !accepts_negative_numbers)
    return nullptr;

  return CSSNumericLiteralValue::Create(number, unit);
}

static inline bool IsColorPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyID::kCaretColor:
    case CSSPropertyID::kColor:
    case CSSPropertyID::kBackgroundColor:
    case CSSPropertyID::kBorderBottomColor:
    case CSSPropertyID::kBorderLeftColor:
    case CSSPropertyID::kBorderRightColor:
    case CSSPropertyID::kBorderTopColor:
    case CSSPropertyID::kFill:
    case CSSPropertyID::kFloodColor:
    case CSSPropertyID::kLightingColor:
    case CSSPropertyID::kOutlineColor:
    case CSSPropertyID::kStopColor:
    case CSSPropertyID::kStroke:
    case CSSPropertyID::kBorderBlockEndColor:
    case CSSPropertyID::kBorderBlockStartColor:
    case CSSPropertyID::kBorderInlineEndColor:
    case CSSPropertyID::kBorderInlineStartColor:
    case CSSPropertyID::kColumnRuleColor:
    case CSSPropertyID::kWebkitTextEmphasisColor:
    case CSSPropertyID::kWebkitTextFillColor:
    case CSSPropertyID::kWebkitTextStrokeColor:
    case CSSPropertyID::kTextDecorationColor:
      return true;
    default:
      return false;
  }
}

// https://quirks.spec.whatwg.org/#the-hashless-hex-color-quirk
static inline bool ColorPropertyAllowsQuirkyColor(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyID::kColor:
    case CSSPropertyID::kBackgroundColor:
    case CSSPropertyID::kBorderBottomColor:
    case CSSPropertyID::kBorderLeftColor:
    case CSSPropertyID::kBorderRightColor:
    case CSSPropertyID::kBorderTopColor:
      return true;
    default:
      DCHECK(IsColorPropertyID(property_id));
      return false;
  }
}

// Returns the number of characters which form a valid double
// and are terminated by the given terminator character
template <typename CharacterType>
static int CheckForValidDouble(const CharacterType* string,
                               const CharacterType* end,
                               const bool terminated_by_space,
                               const char terminator) {
  int length = static_cast<int>(end - string);
  if (length < 1)
    return 0;

  bool decimal_mark_seen = false;
  int processed_length = 0;

  for (int i = 0; i < length; ++i) {
    if (string[i] == terminator ||
        (terminated_by_space && IsHTMLSpace<CharacterType>(string[i]))) {
      processed_length = i;
      break;
    }
    if (!IsASCIIDigit(string[i])) {
      if (!decimal_mark_seen && string[i] == '.')
        decimal_mark_seen = true;
      else
        return 0;
    }
  }

  if (decimal_mark_seen && processed_length == 1)
    return 0;

  return processed_length;
}

// Returns the number of characters consumed for parsing a valid double
// terminated by the given terminator character
template <typename CharacterType>
static int ParseDouble(const CharacterType* string,
                       const CharacterType* end,
                       const char terminator,
                       const bool terminated_by_space,
                       double& value) {
  int length =
      CheckForValidDouble(string, end, terminated_by_space, terminator);
  if (!length)
    return 0;

  int position = 0;
  double local_value = 0;

  // The consumed characters here are guaranteed to be
  // ASCII digits with or without a decimal mark
  for (; position < length; ++position) {
    if (string[position] == '.')
      break;
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

template <typename CharacterType>
static bool ParseColorNumberOrPercentage(const CharacterType*& string,
                                         const CharacterType* end,
                                         const char terminator,
                                         bool& should_whitespace_terminate,
                                         bool is_first_value,
                                         CSSPrimitiveValue::UnitType& expect,
                                         int& value) {
  const CharacterType* current = string;
  double local_value = 0;
  bool negative = false;
  while (current != end && IsHTMLSpace<CharacterType>(*current))
    current++;
  if (current != end && *current == '-') {
    negative = true;
    current++;
  }
  if (current == end || !IsASCIIDigit(*current))
    return false;
  while (current != end && IsASCIIDigit(*current)) {
    double new_value = local_value * 10 + *current++ - '0';
    if (new_value >= 255) {
      // Clamp values at 255.
      local_value = 255;
      while (current != end && IsASCIIDigit(*current))
        ++current;
      break;
    }
    local_value = new_value;
  }

  if (current == end)
    return false;

  if (expect == CSSPrimitiveValue::UnitType::kNumber && *current == '%')
    return false;

  if (*current == '.') {
    // We already parsed the integral part, try to parse
    // the fraction part.
    double fractional = 0;
    int num_characters_parsed =
        ParseDouble(current, end, '%', false, fractional);
    if (num_characters_parsed) {
      // Number is a percent.
      current += num_characters_parsed;
      if (*current != '%')
        return false;
    } else {
      // Number is a decimal.
      num_characters_parsed =
          ParseDouble(current, end, terminator, true, fractional);
      if (!num_characters_parsed)
        return false;
      current += num_characters_parsed;
    }
    local_value += fractional;
  }

  if (expect == CSSPrimitiveValue::UnitType::kPercentage && *current != '%')
    return false;

  if (*current == '%') {
    expect = CSSPrimitiveValue::UnitType::kPercentage;
    local_value = local_value / 100.0 * 255.0;
    // Clamp values at 255 for percentages over 100%
    if (local_value > 255)
      local_value = 255;
    current++;
  } else {
    expect = CSSPrimitiveValue::UnitType::kNumber;
  }

  while (current != end && IsHTMLSpace<CharacterType>(*current))
    current++;

  if (current == end || *current != terminator) {
    if (!should_whitespace_terminate ||
        !IsHTMLSpace<CharacterType>(*(current - 1))) {
      return false;
    }
  } else if (should_whitespace_terminate && is_first_value) {
    should_whitespace_terminate = false;
  } else if (should_whitespace_terminate) {
    return false;
  }

  if (!should_whitespace_terminate)
    current++;

  // Clamp negative values at zero.
  value = negative ? 0 : static_cast<int>(round(local_value));
  string = current;
  return true;
}

template <typename CharacterType>
static inline bool IsTenthAlpha(const CharacterType* string, const int length) {
  // "0.X"
  if (length == 3 && string[0] == '0' && string[1] == '.' &&
      IsASCIIDigit(string[2]))
    return true;

  // ".X"
  if (length == 2 && string[0] == '.' && IsASCIIDigit(string[1]))
    return true;

  return false;
}

template <typename CharacterType>
static inline bool ParseAlphaValue(const CharacterType*& string,
                                   const CharacterType* end,
                                   const char terminator,
                                   int& value) {
  while (string != end && IsHTMLSpace<CharacterType>(*string))
    string++;

  bool negative = false;

  if (string != end && *string == '-') {
    negative = true;
    string++;
  }

  value = 0;

  size_t length = end - string;
  if (length < 2)
    return false;

  if (string[length - 1] != terminator || !IsASCIIDigit(string[length - 2]))
    return false;

  if (string[0] != '0' && string[0] != '1' && string[0] != '.') {
    if (CheckForValidDouble(string, end, false, terminator)) {
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
  if (!ParseDouble(string, end, terminator, false, alpha))
    return false;
  value = negative ? 0 : static_cast<int>(round(std::min(alpha, 1.0) * 255.0));
  string = end;
  return true;
}

template <typename CharacterType>
static inline bool MightBeRGBOrRGBA(const CharacterType* characters,
                                    unsigned length) {
  if (length < 5)
    return false;
  return IsASCIIAlphaCaselessEqual(characters[0], 'r') &&
         IsASCIIAlphaCaselessEqual(characters[1], 'g') &&
         IsASCIIAlphaCaselessEqual(characters[2], 'b') &&
         (characters[3] == '(' ||
          (IsASCIIAlphaCaselessEqual(characters[3], 'a') &&
           characters[4] == '('));
}

template <typename CharacterType>
static bool FastParseColorInternal(RGBA32& rgb,
                                   const CharacterType* characters,
                                   unsigned length,
                                   bool quirks_mode) {
  CSSPrimitiveValue::UnitType expect = CSSPrimitiveValue::UnitType::kUnknown;

  if (length >= 4 && characters[0] == '#')
    return Color::ParseHexColor(characters + 1, length - 1, rgb);

  if (quirks_mode && (length == 3 || length == 6)) {
    if (Color::ParseHexColor(characters, length, rgb))
      return true;
  }

  // rgb() and rgba() have the same syntax
  if (MightBeRGBOrRGBA(characters, length)) {
    int length_to_add = IsASCIIAlphaCaselessEqual(characters[3], 'a') ? 5 : 4;
    const CharacterType* current = characters + length_to_add;
    const CharacterType* end = characters + length;
    int red;
    int green;
    int blue;
    int alpha;
    bool should_have_alpha = false;
    bool should_whitespace_terminate = true;
    bool no_whitespace_check = false;

    if (!ParseColorNumberOrPercentage(current, end, ',',
                                      should_whitespace_terminate,
                                      true /* is_first_value */, expect, red))
      return false;
    if (!ParseColorNumberOrPercentage(
            current, end, ',', should_whitespace_terminate,
            false /* is_first_value */, expect, green))
      return false;
    if (!ParseColorNumberOrPercentage(current, end, ',', no_whitespace_check,
                                      false /* is_first_value */, expect,
                                      blue)) {
      // Might have slash as separator
      if (ParseColorNumberOrPercentage(current, end, '/', no_whitespace_check,
                                       false /* is_first_value */, expect,
                                       blue)) {
        if (!should_whitespace_terminate)
          return false;
        should_have_alpha = true;
      }
      // Might not have alpha
      else if (!ParseColorNumberOrPercentage(
                   current, end, ')', no_whitespace_check,
                   false /* is_first_value */, expect, blue))
        return false;
    } else {
      if (should_whitespace_terminate)
        return false;
      should_have_alpha = true;
    }

    if (should_have_alpha) {
      if (!ParseAlphaValue(current, end, ')', alpha))
        return false;
      if (current != end)
        return false;
      rgb = MakeRGBA(red, green, blue, alpha);
    } else {
      if (current != end)
        return false;
      rgb = MakeRGB(red, green, blue);
    }
    return true;
  }

  return false;
}

static CSSValue* ParseColor(CSSPropertyID property_id,
                            const String& string,
                            CSSParserMode parser_mode) {
  if (!IsColorPropertyID(property_id))
    return nullptr;

  DCHECK(!string.IsEmpty());
  CSSValueID value_id = CssValueKeywordID(string);
  if (StyleColor::IsColorKeyword(value_id)) {
    if (!isValueAllowedInMode(value_id, parser_mode))
      return nullptr;
    return CSSIdentifierValue::Create(value_id);
  }

  RGBA32 color;
  bool quirks_mode = IsQuirksModeBehavior(parser_mode) &&
                     ColorPropertyAllowsQuirkyColor(property_id);

  // Fast path for hex colors and rgb()/rgba() colors
  bool parse_result =
      WTF::VisitCharacters(string, [&](const auto* chars, unsigned length) {
        return FastParseColorInternal(color, chars, length, quirks_mode);
      });
  if (!parse_result)
    return nullptr;
  return cssvalue::CSSColorValue::Create(color);
}

CSSValue* CSSParserFastPaths::ParseColor(const String& string,
                                         CSSParserMode parser_mode) {
  return blink::ParseColor(CSSPropertyID::kColor, string, parser_mode);
}

bool CSSParserFastPaths::IsValidKeywordPropertyAndValue(
    CSSPropertyID property_id,
    CSSValueID value_id,
    CSSParserMode parser_mode) {
  if (!IsValidCSSValueID(value_id) ||
      !isValueAllowedInMode(value_id, parser_mode))
    return false;

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
      return value_id == CSSValueID::kNone || value_id == CSSValueID::kAuto;
    case CSSPropertyID::kImageRendering:
      return value_id == CSSValueID::kAuto ||
             value_id == CSSValueID::kWebkitOptimizeContrast ||
             value_id == CSSValueID::kPixelated;
    case CSSPropertyID::kIsolation:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kIsolate;
    case CSSPropertyID::kListStylePosition:
      return value_id == CSSValueID::kInside ||
             value_id == CSSValueID::kOutside;
    case CSSPropertyID::kListStyleType:
      return (value_id >= CSSValueID::kDisc &&
              value_id <= CSSValueID::kKatakanaIroha) ||
             value_id == CSSValueID::kNone;
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
             value_id == CSSValueID::kOverlay ||
             (RuntimeEnabledFeatures::OverflowClipEnabled() &&
              value_id == CSSValueID::kClip);
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
    case CSSPropertyID::kTextJustify:
      DCHECK(RuntimeEnabledFeatures::CSS3TextEnabled());
      return value_id == CSSValueID::kInterWord ||
             value_id == CSSValueID::kDistribute ||
             value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone;
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
    case CSSPropertyID::kWebkitAppRegion:
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
             value_id == CSSValueID::kLuminosity;
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
#if BUILDFLAG(USE_MINIKIN_HYPHENATION) || defined(OS_MAC)
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
    case CSSPropertyID::kWebkitFontSmoothing:
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kAntialiased ||
             value_id == CSSValueID::kSubpixelAntialiased;
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
      return value_id == CSSValueID::kAuto || value_id == CSSValueID::kNone ||
             value_id == CSSValueID::kText || value_id == CSSValueID::kAll;
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
    default:
      NOTREACHED();
      return false;
  }
}

bool CSSParserFastPaths::IsKeywordPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyID::kAlignmentBaseline:
    case CSSPropertyID::kAll:
    case CSSPropertyID::kMixBlendMode:
    case CSSPropertyID::kIsolation:
    case CSSPropertyID::kBackgroundRepeatX:
    case CSSPropertyID::kBackgroundRepeatY:
    case CSSPropertyID::kBorderBottomStyle:
    case CSSPropertyID::kBorderCollapse:
    case CSSPropertyID::kBorderLeftStyle:
    case CSSPropertyID::kBorderRightStyle:
    case CSSPropertyID::kBorderTopStyle:
    case CSSPropertyID::kBoxSizing:
    case CSSPropertyID::kBufferedRendering:
    case CSSPropertyID::kCaptionSide:
    case CSSPropertyID::kClear:
    case CSSPropertyID::kClipRule:
    case CSSPropertyID::kColorInterpolation:
    case CSSPropertyID::kColorInterpolationFilters:
    case CSSPropertyID::kColorRendering:
    case CSSPropertyID::kDirection:
    case CSSPropertyID::kDominantBaseline:
    case CSSPropertyID::kEmptyCells:
    case CSSPropertyID::kFillRule:
    case CSSPropertyID::kFloat:
    case CSSPropertyID::kForcedColorAdjust:
    case CSSPropertyID::kHyphens:
    case CSSPropertyID::kImageRendering:
    case CSSPropertyID::kListStylePosition:
    case CSSPropertyID::kListStyleType:
    case CSSPropertyID::kMaskType:
    case CSSPropertyID::kMathShift:
    case CSSPropertyID::kMathStyle:
    case CSSPropertyID::kObjectFit:
    case CSSPropertyID::kOutlineStyle:
    case CSSPropertyID::kOverflowAnchor:
    case CSSPropertyID::kOverflowBlock:
    case CSSPropertyID::kOverflowInline:
    case CSSPropertyID::kOverflowWrap:
    case CSSPropertyID::kOverflowX:
    case CSSPropertyID::kOverflowY:
    case CSSPropertyID::kBreakAfter:
    case CSSPropertyID::kBreakBefore:
    case CSSPropertyID::kBreakInside:
    case CSSPropertyID::kPageOrientation:
    case CSSPropertyID::kPointerEvents:
    case CSSPropertyID::kPosition:
    case CSSPropertyID::kResize:
    case CSSPropertyID::kScrollBehavior:
    case CSSPropertyID::kOverscrollBehaviorInline:
    case CSSPropertyID::kOverscrollBehaviorBlock:
    case CSSPropertyID::kOverscrollBehaviorX:
    case CSSPropertyID::kOverscrollBehaviorY:
    case CSSPropertyID::kRubyPosition:
    case CSSPropertyID::kShapeRendering:
    case CSSPropertyID::kSpeak:
    case CSSPropertyID::kStrokeLinecap:
    case CSSPropertyID::kStrokeLinejoin:
    case CSSPropertyID::kTableLayout:
    case CSSPropertyID::kTextAlign:
    case CSSPropertyID::kTextAlignLast:
    case CSSPropertyID::kTextAnchor:
    case CSSPropertyID::kTextCombineUpright:
    case CSSPropertyID::kTextDecorationStyle:
    case CSSPropertyID::kTextDecorationSkipInk:
    case CSSPropertyID::kTextJustify:
    case CSSPropertyID::kTextOrientation:
    case CSSPropertyID::kWebkitTextOrientation:
    case CSSPropertyID::kTextOverflow:
    case CSSPropertyID::kTextRendering:
    case CSSPropertyID::kTextTransform:
    case CSSPropertyID::kUnicodeBidi:
    case CSSPropertyID::kVectorEffect:
    case CSSPropertyID::kVisibility:
    case CSSPropertyID::kWebkitAppRegion:
    case CSSPropertyID::kBackfaceVisibility:
    case CSSPropertyID::kBorderBlockEndStyle:
    case CSSPropertyID::kBorderBlockStartStyle:
    case CSSPropertyID::kBorderInlineEndStyle:
    case CSSPropertyID::kBorderInlineStartStyle:
    case CSSPropertyID::kWebkitBoxAlign:
    case CSSPropertyID::kWebkitBoxDecorationBreak:
    case CSSPropertyID::kWebkitBoxDirection:
    case CSSPropertyID::kWebkitBoxOrient:
    case CSSPropertyID::kWebkitBoxPack:
    case CSSPropertyID::kColumnFill:
    case CSSPropertyID::kColumnRuleStyle:
    case CSSPropertyID::kFlexDirection:
    case CSSPropertyID::kFlexWrap:
    case CSSPropertyID::kFontKerning:
    case CSSPropertyID::kFontOpticalSizing:
    case CSSPropertyID::kWebkitFontSmoothing:
    case CSSPropertyID::kLineBreak:
    case CSSPropertyID::kWebkitLineBreak:
    case CSSPropertyID::kWebkitPrintColorAdjust:
    case CSSPropertyID::kWebkitRtlOrdering:
    case CSSPropertyID::kWebkitRubyPosition:
    case CSSPropertyID::kWebkitTextCombine:
    case CSSPropertyID::kWebkitTextSecurity:
    case CSSPropertyID::kTransformBox:
    case CSSPropertyID::kTransformStyle:
    case CSSPropertyID::kWebkitUserDrag:
    case CSSPropertyID::kWebkitUserModify:
    case CSSPropertyID::kUserSelect:
    case CSSPropertyID::kWebkitWritingMode:
    case CSSPropertyID::kWhiteSpace:
    case CSSPropertyID::kWordBreak:
    case CSSPropertyID::kWritingMode:
    case CSSPropertyID::kScrollSnapStop:
    case CSSPropertyID::kOriginTrialTestProperty:
      return true;
    default:
      return false;
  }
}

bool CSSParserFastPaths::IsPartialKeywordPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyID::kListStyleType:
      return true;
    default:
      return false;
  }
}

static CSSValue* ParseKeywordValue(CSSPropertyID property_id,
                                   const String& string,
                                   CSSParserMode parser_mode) {
  DCHECK(!string.IsEmpty());

  if (!CSSParserFastPaths::IsKeywordPropertyID(property_id)) {
    // All properties accept CSS-wide keywords.
    if (!EqualIgnoringASCIICase(string, "initial") &&
        !EqualIgnoringASCIICase(string, "inherit") &&
        !EqualIgnoringASCIICase(string, "unset") &&
        (!RuntimeEnabledFeatures::CSSRevertEnabled() ||
         !EqualIgnoringASCIICase(string, "revert")))
      return nullptr;

    // Parse CSS-wide keyword shorthands using the CSSPropertyParser.
    if (shorthandForProperty(property_id).length())
      return nullptr;

    // Descriptors do not support css wide keywords.
    if (!CSSProperty::Get(property_id).IsProperty())
      return nullptr;
  }

  CSSValueID value_id = CssValueKeywordID(string);

  if (!IsValidCSSValueID(value_id))
    return nullptr;

  if (value_id == CSSValueID::kInherit)
    return CSSInheritedValue::Create();
  if (value_id == CSSValueID::kInitial)
    return CSSInitialValue::Create();
  if (value_id == CSSValueID::kUnset)
    return cssvalue::CSSUnsetValue::Create();
  if (RuntimeEnabledFeatures::CSSRevertEnabled() &&
      value_id == CSSValueID::kRevert) {
    return cssvalue::CSSRevertValue::Create();
  }
  if (CSSParserFastPaths::IsValidKeywordPropertyAndValue(property_id, value_id,
                                                         parser_mode))
    return CSSIdentifierValue::Create(value_id);
  return nullptr;
}

template <typename CharType>
static bool ParseTransformTranslateArguments(
    CharType*& pos,
    CharType* end,
    unsigned expected_count,
    CSSFunctionValue* transform_value) {
  while (expected_count) {
    wtf_size_t delimiter = WTF::Find(pos, static_cast<wtf_size_t>(end - pos),
                                     expected_count == 1 ? ')' : ',');
    if (delimiter == kNotFound)
      return false;
    unsigned argument_length = static_cast<unsigned>(delimiter);
    CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::kNumber;
    double number;
    if (!ParseSimpleLength(pos, argument_length, unit, number))
      return false;
    if (unit != CSSPrimitiveValue::UnitType::kPixels &&
        (number || unit != CSSPrimitiveValue::UnitType::kNumber))
      return false;
    transform_value->Append(*CSSNumericLiteralValue::Create(
        number, CSSPrimitiveValue::UnitType::kPixels));
    pos += argument_length + 1;
    --expected_count;
  }
  return true;
}

template <typename CharType>
static bool ParseTransformNumberArguments(CharType*& pos,
                                          CharType* end,
                                          unsigned expected_count,
                                          CSSFunctionValue* transform_value) {
  while (expected_count) {
    wtf_size_t delimiter = WTF::Find(pos, static_cast<wtf_size_t>(end - pos),
                                     expected_count == 1 ? ')' : ',');
    if (delimiter == kNotFound)
      return false;
    unsigned argument_length = static_cast<unsigned>(delimiter);
    bool ok;
    double number = CharactersToDouble(pos, argument_length, &ok);
    if (!ok)
      return false;
    transform_value->Append(*CSSNumericLiteralValue::Create(
        number, CSSPrimitiveValue::UnitType::kNumber));
    pos += argument_length + 1;
    --expected_count;
  }
  return true;
}

static const int kShortestValidTransformStringLength = 12;

template <typename CharType>
static CSSFunctionValue* ParseSimpleTransformValue(CharType*& pos,
                                                   CharType* end) {
  if (end - pos < kShortestValidTransformStringLength)
    return nullptr;

  const bool is_translate =
      ToASCIILower(pos[0]) == 't' && ToASCIILower(pos[1]) == 'r' &&
      ToASCIILower(pos[2]) == 'a' && ToASCIILower(pos[3]) == 'n' &&
      ToASCIILower(pos[4]) == 's' && ToASCIILower(pos[5]) == 'l' &&
      ToASCIILower(pos[6]) == 'a' && ToASCIILower(pos[7]) == 't' &&
      ToASCIILower(pos[8]) == 'e';

  if (is_translate) {
    CSSValueID transform_type;
    unsigned expected_argument_count = 1;
    unsigned argument_start = 11;
    CharType c9 = ToASCIILower(pos[9]);
    if (c9 == 'x' && pos[10] == '(') {
      transform_type = CSSValueID::kTranslateX;
    } else if (c9 == 'y' && pos[10] == '(') {
      transform_type = CSSValueID::kTranslateY;
    } else if (c9 == 'z' && pos[10] == '(') {
      transform_type = CSSValueID::kTranslateZ;
    } else if (c9 == '(') {
      transform_type = CSSValueID::kTranslate;
      expected_argument_count = 2;
      argument_start = 10;
    } else if (c9 == '3' && ToASCIILower(pos[10]) == 'd' && pos[11] == '(') {
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
                                          transform_value))
      return nullptr;
    return transform_value;
  }

  const bool is_matrix3d =
      ToASCIILower(pos[0]) == 'm' && ToASCIILower(pos[1]) == 'a' &&
      ToASCIILower(pos[2]) == 't' && ToASCIILower(pos[3]) == 'r' &&
      ToASCIILower(pos[4]) == 'i' && ToASCIILower(pos[5]) == 'x' &&
      pos[6] == '3' && ToASCIILower(pos[7]) == 'd' && pos[8] == '(';

  if (is_matrix3d) {
    pos += 9;
    CSSFunctionValue* transform_value =
        MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kMatrix3d);
    if (!ParseTransformNumberArguments(pos, end, 16, transform_value))
      return nullptr;
    return transform_value;
  }

  const bool is_scale3d =
      ToASCIILower(pos[0]) == 's' && ToASCIILower(pos[1]) == 'c' &&
      ToASCIILower(pos[2]) == 'a' && ToASCIILower(pos[3]) == 'l' &&
      ToASCIILower(pos[4]) == 'e' && pos[5] == '3' &&
      ToASCIILower(pos[6]) == 'd' && pos[7] == '(';

  if (is_scale3d) {
    pos += 8;
    CSSFunctionValue* transform_value =
        MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kScale3d);
    if (!ParseTransformNumberArguments(pos, end, 3, transform_value))
      return nullptr;
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
    if (IsCSSSpace(chars[i])) {
      ++i;
      continue;
    }
    if (length - i < kShortestValidTransformStringLength)
      return false;
    switch (ToASCIILower(chars[i])) {
      case 't':
        // translate, translateX, translateY, translateZ, translate3d.
        if (ToASCIILower(chars[i + 8]) != 'e')
          return false;
        i += 9;
        break;
      case 'm':
        // matrix3d.
        if (ToASCIILower(chars[i + 7]) != 'd')
          return false;
        i += 8;
        break;
      case 's':
        // scale3d.
        if (ToASCIILower(chars[i + 6]) != 'd')
          return false;
        i += 7;
        break;
      default:
        // All other things, ex. rotate.
        return false;
    }
    wtf_size_t arguments_end = WTF::Find(chars, length, ')', i);
    if (arguments_end == kNotFound)
      return false;
    // Advance to the end of the arguments.
    i = arguments_end + 1;
  }
  return i == length;
}

static CSSValue* ParseSimpleTransform(CSSPropertyID property_id,
                                      const String& string) {
  DCHECK(!string.IsEmpty());

  if (property_id != CSSPropertyID::kTransform)
    return nullptr;

  return WTF::VisitCharacters(
      string, [&](const auto* pos, unsigned length) -> CSSValueList* {
        if (!TransformCanLikelyUseFastPath(pos, length))
          return nullptr;
        const auto* end = pos + length;
        CSSValueList* transform_list = nullptr;
        while (pos < end) {
          while (pos < end && IsCSSSpace(*pos))
            ++pos;
          if (pos >= end)
            break;
          auto* transform_value = ParseSimpleTransformValue(pos, end);
          if (!transform_value)
            return nullptr;
          if (!transform_list)
            transform_list = CSSValueList::CreateSpaceSeparated();
          transform_list->Append(*transform_value);
        }
        return transform_list;
      });
}

CSSValue* CSSParserFastPaths::MaybeParseValue(CSSPropertyID property_id,
                                              const String& string,
                                              CSSParserMode parser_mode) {
  if (CSSValue* length =
          ParseSimpleLengthValue(property_id, string, parser_mode))
    return length;
  if (CSSValue* color = blink::ParseColor(property_id, string, parser_mode))
    return color;
  if (CSSValue* keyword = ParseKeywordValue(property_id, string, parser_mode))
    return keyword;
  if (CSSValue* transform = ParseSimpleTransform(property_id, string))
    return transform;
  return nullptr;
}

}  // namespace blink

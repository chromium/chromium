// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

using namespace cssvalue;

static inline bool IsSimpleLengthPropertyID(CSSPropertyID property_id,
                                            bool& accepts_negative_numbers) {
  switch (property_id) {
    case CSSPropertyBlockSize:
    case CSSPropertyInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyFontSize:
    case CSSPropertyHeight:
    case CSSPropertyWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyScrollMarginBlockEnd:
    case CSSPropertyScrollMarginBlockStart:
    case CSSPropertyScrollMarginBottom:
    case CSSPropertyScrollMarginInlineEnd:
    case CSSPropertyScrollMarginInlineStart:
    case CSSPropertyScrollMarginLeft:
    case CSSPropertyScrollMarginRight:
    case CSSPropertyScrollMarginTop:
    case CSSPropertyScrollPaddingBlockEnd:
    case CSSPropertyScrollPaddingBlockStart:
    case CSSPropertyScrollPaddingBottom:
    case CSSPropertyScrollPaddingInlineEnd:
    case CSSPropertyScrollPaddingInlineStart:
    case CSSPropertyScrollPaddingLeft:
    case CSSPropertyScrollPaddingRight:
    case CSSPropertyScrollPaddingTop:
    case CSSPropertyPaddingBlockEnd:
    case CSSPropertyPaddingBlockStart:
    case CSSPropertyPaddingInlineEnd:
    case CSSPropertyPaddingInlineStart:
    case CSSPropertyShapeMargin:
    case CSSPropertyR:
    case CSSPropertyRx:
    case CSSPropertyRy:
      accepts_negative_numbers = false;
      return true;
    case CSSPropertyBottom:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyLeft:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyOffsetDistance:
    case CSSPropertyRight:
    case CSSPropertyTop:
    case CSSPropertyMarginBlockEnd:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginInlineEnd:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyX:
    case CSSPropertyY:
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

  unsigned length = string.length();
  double number;
  CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::kNumber;

  if (string.Is8Bit()) {
    if (!ParseSimpleLength(string.Characters8(), length, unit, number))
      return nullptr;
  } else {
    if (!ParseSimpleLength(string.Characters16(), length, unit, number))
      return nullptr;
  }

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

  return CSSPrimitiveValue::Create(number, unit);
}

static inline bool IsColorPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyCaretColor:
    case CSSPropertyColor:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyFill:
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyStopColor:
    case CSSPropertyStroke:
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineStartColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyTextDecorationColor:
      return true;
    default:
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

CSSValue* CSSParserFastPaths::ParseColor(const String& string,
                                         CSSParserMode parser_mode) {
  DCHECK(!string.IsEmpty());
  CSSValueID value_id = CssValueKeywordID(string);
  if (StyleColor::IsColorKeyword(value_id)) {
    if (!isValueAllowedInMode(value_id, parser_mode))
      return nullptr;
    return CSSIdentifierValue::Create(value_id);
  }

  RGBA32 color;
  bool quirks_mode = IsQuirksModeBehavior(parser_mode);

  // Fast path for hex colors and rgb()/rgba() colors
  bool parse_result;
  if (string.Is8Bit())
    parse_result = FastParseColorInternal(color, string.Characters8(),
                                          string.length(), quirks_mode);
  else
    parse_result = FastParseColorInternal(color, string.Characters16(),
                                          string.length(), quirks_mode);
  if (!parse_result)
    return nullptr;
  return CSSColorValue::Create(color);
}

bool CSSParserFastPaths::IsValidKeywordPropertyAndValue(
    CSSPropertyID property_id,
    CSSValueID value_id,
    CSSParserMode parser_mode) {
  if (value_id == CSSValueInvalid ||
      !isValueAllowedInMode(value_id, parser_mode))
    return false;

  // For range checks, enum ordering is defined by CSSValueKeywords.in.
  switch (property_id) {
    case CSSPropertyAlignmentBaseline:
      return value_id == CSSValueAuto || value_id == CSSValueAlphabetic ||
             value_id == CSSValueBaseline || value_id == CSSValueMiddle ||
             (value_id >= CSSValueBeforeEdge &&
              value_id <= CSSValueMathematical);
    case CSSPropertyAll:
      return false;  // Only accepts css-wide keywords
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
      return value_id == CSSValueRepeat || value_id == CSSValueNoRepeat;
    case CSSPropertyBorderCollapse:
      return value_id == CSSValueCollapse || value_id == CSSValueSeparate;
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyColumnRuleStyle:
      return value_id >= CSSValueNone && value_id <= CSSValueDouble;
    case CSSPropertyBoxSizing:
      return value_id == CSSValueBorderBox || value_id == CSSValueContentBox;
    case CSSPropertyBufferedRendering:
      return value_id == CSSValueAuto || value_id == CSSValueDynamic ||
             value_id == CSSValueStatic;
    case CSSPropertyCaptionSide:
      return value_id == CSSValueTop || value_id == CSSValueBottom;
    case CSSPropertyClear:
      return value_id == CSSValueNone || value_id == CSSValueLeft ||
             value_id == CSSValueRight || value_id == CSSValueBoth ||
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueInlineStart ||
               value_id == CSSValueInlineEnd));
    case CSSPropertyClipRule:
    case CSSPropertyFillRule:
      return value_id == CSSValueNonzero || value_id == CSSValueEvenodd;
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
      return value_id == CSSValueAuto || value_id == CSSValueSRGB ||
             value_id == CSSValueLinearrgb;
    case CSSPropertyColorRendering:
      return value_id == CSSValueAuto || value_id == CSSValueOptimizespeed ||
             value_id == CSSValueOptimizequality;
    case CSSPropertyDirection:
      return value_id == CSSValueLtr || value_id == CSSValueRtl;
    case CSSPropertyDisplay:
      return (value_id >= CSSValueInline && value_id <= CSSValueInlineFlex) ||
             value_id == CSSValueWebkitFlex ||
             value_id == CSSValueWebkitInlineFlex || value_id == CSSValueNone ||
             value_id == CSSValueGrid || value_id == CSSValueInlineGrid ||
             value_id == CSSValueContents;
    case CSSPropertyDominantBaseline:
      return value_id == CSSValueAuto || value_id == CSSValueAlphabetic ||
             value_id == CSSValueMiddle ||
             (value_id >= CSSValueUseScript && value_id <= CSSValueResetSize) ||
             (value_id >= CSSValueCentral && value_id <= CSSValueMathematical);
    case CSSPropertyEmptyCells:
      return value_id == CSSValueShow || value_id == CSSValueHide;
    case CSSPropertyFloat:
      return value_id == CSSValueLeft || value_id == CSSValueRight ||
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueInlineStart ||
               value_id == CSSValueInlineEnd)) ||
             value_id == CSSValueNone;
    case CSSPropertyImageRendering:
      return value_id == CSSValueAuto ||
             value_id == CSSValueWebkitOptimizeContrast ||
             value_id == CSSValuePixelated;
    case CSSPropertyIsolation:
      return value_id == CSSValueAuto || value_id == CSSValueIsolate;
    case CSSPropertyListStylePosition:
      return value_id == CSSValueInside || value_id == CSSValueOutside;
    case CSSPropertyListStyleType:
      return (value_id >= CSSValueDisc && value_id <= CSSValueKatakanaIroha) ||
             value_id == CSSValueNone;
    case CSSPropertyMaskType:
      return value_id == CSSValueLuminance || value_id == CSSValueAlpha;
    case CSSPropertyObjectFit:
      return value_id == CSSValueFill || value_id == CSSValueContain ||
             value_id == CSSValueCover || value_id == CSSValueNone ||
             value_id == CSSValueScaleDown;
    case CSSPropertyOutlineStyle:
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             (value_id >= CSSValueInset && value_id <= CSSValueDouble);
    case CSSPropertyOverflowAnchor:
      return value_id == CSSValueVisible || value_id == CSSValueNone ||
             value_id == CSSValueAuto;
    case CSSPropertyOverflowWrap:
      return value_id == CSSValueNormal || value_id == CSSValueBreakWord;
    case CSSPropertyOverflowX:
      return value_id == CSSValueVisible || value_id == CSSValueHidden ||
             value_id == CSSValueScroll || value_id == CSSValueAuto ||
             value_id == CSSValueOverlay;
    case CSSPropertyOverflowY:
      return value_id == CSSValueVisible || value_id == CSSValueHidden ||
             value_id == CSSValueScroll || value_id == CSSValueAuto ||
             value_id == CSSValueOverlay || value_id == CSSValueWebkitPagedX ||
             value_id == CSSValueWebkitPagedY;
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
      return value_id == CSSValueAuto || value_id == CSSValueAvoid ||
             value_id == CSSValueAvoidPage || value_id == CSSValuePage ||
             value_id == CSSValueLeft || value_id == CSSValueRight ||
             value_id == CSSValueRecto || value_id == CSSValueVerso ||
             value_id == CSSValueAvoidColumn || value_id == CSSValueColumn;
    case CSSPropertyBreakInside:
      return value_id == CSSValueAuto || value_id == CSSValueAvoid ||
             value_id == CSSValueAvoidPage || value_id == CSSValueAvoidColumn;
    case CSSPropertyPointerEvents:
      return value_id == CSSValueVisible || value_id == CSSValueNone ||
             value_id == CSSValueAll || value_id == CSSValueAuto ||
             (value_id >= CSSValueVisiblePainted &&
              value_id <= CSSValueBoundingBox);
    case CSSPropertyPosition:
      return value_id == CSSValueStatic || value_id == CSSValueRelative ||
             value_id == CSSValueAbsolute || value_id == CSSValueFixed ||
             value_id == CSSValueSticky;
    case CSSPropertyResize:
      return value_id == CSSValueNone || value_id == CSSValueBoth ||
             value_id == CSSValueHorizontal || value_id == CSSValueVertical ||
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueBlock || value_id == CSSValueInline)) ||
             value_id == CSSValueAuto;
    case CSSPropertyScrollBehavior:
      DCHECK(RuntimeEnabledFeatures::CSSOMSmoothScrollEnabled());
      return value_id == CSSValueAuto || value_id == CSSValueSmooth;
    case CSSPropertyShapeRendering:
      return value_id == CSSValueAuto || value_id == CSSValueOptimizespeed ||
             value_id == CSSValueCrispedges ||
             value_id == CSSValueGeometricprecision;
    case CSSPropertySpeak:
      return value_id == CSSValueNone || value_id == CSSValueNormal ||
             value_id == CSSValueSpellOut || value_id == CSSValueDigits ||
             value_id == CSSValueLiteralPunctuation ||
             value_id == CSSValueNoPunctuation;
    case CSSPropertyStrokeLinejoin:
      return value_id == CSSValueMiter || value_id == CSSValueRound ||
             value_id == CSSValueBevel;
    case CSSPropertyStrokeLinecap:
      return value_id == CSSValueButt || value_id == CSSValueRound ||
             value_id == CSSValueSquare;
    case CSSPropertyTableLayout:
      return value_id == CSSValueAuto || value_id == CSSValueFixed;
    case CSSPropertyTextAlign:
      return (value_id >= CSSValueWebkitAuto &&
              value_id <= CSSValueInternalCenter) ||
             value_id == CSSValueStart || value_id == CSSValueEnd;
    case CSSPropertyTextAlignLast:
      return (value_id >= CSSValueLeft && value_id <= CSSValueJustify) ||
             value_id == CSSValueStart || value_id == CSSValueEnd ||
             value_id == CSSValueAuto;
    case CSSPropertyTextAnchor:
      return value_id == CSSValueStart || value_id == CSSValueMiddle ||
             value_id == CSSValueEnd;
    case CSSPropertyTextCombineUpright:
      return value_id == CSSValueNone || value_id == CSSValueAll;
    case CSSPropertyTextDecorationStyle:
      return value_id == CSSValueSolid || value_id == CSSValueDouble ||
             value_id == CSSValueDotted || value_id == CSSValueDashed ||
             value_id == CSSValueWavy;
    case CSSPropertyTextDecorationSkipInk:
      return value_id == CSSValueAuto || value_id == CSSValueNone;
    case CSSPropertyTextJustify:
      DCHECK(RuntimeEnabledFeatures::CSS3TextEnabled());
      return value_id == CSSValueInterWord || value_id == CSSValueDistribute ||
             value_id == CSSValueAuto || value_id == CSSValueNone;
    case CSSPropertyTextOrientation:
      return value_id == CSSValueMixed || value_id == CSSValueUpright ||
             value_id == CSSValueSideways || value_id == CSSValueSidewaysRight;
    case CSSPropertyWebkitTextOrientation:
      return value_id == CSSValueSideways ||
             value_id == CSSValueSidewaysRight ||
             value_id == CSSValueVerticalRight || value_id == CSSValueUpright;
    case CSSPropertyTextOverflow:
      return value_id == CSSValueClip || value_id == CSSValueEllipsis;
    case CSSPropertyTextRendering:
      return value_id == CSSValueAuto || value_id == CSSValueOptimizespeed ||
             value_id == CSSValueOptimizelegibility ||
             value_id == CSSValueGeometricprecision;
    case CSSPropertyTextTransform:  // capitalize | uppercase | lowercase | none
      return (value_id >= CSSValueCapitalize &&
              value_id <= CSSValueLowercase) ||
             value_id == CSSValueNone;
    case CSSPropertyUnicodeBidi:
      return value_id == CSSValueNormal || value_id == CSSValueEmbed ||
             value_id == CSSValueBidiOverride ||
             value_id == CSSValueWebkitIsolate ||
             value_id == CSSValueWebkitIsolateOverride ||
             value_id == CSSValueWebkitPlaintext ||
             value_id == CSSValueIsolate ||
             value_id == CSSValueIsolateOverride ||
             value_id == CSSValuePlaintext;
    case CSSPropertyVectorEffect:
      return value_id == CSSValueNone || value_id == CSSValueNonScalingStroke;
    case CSSPropertyVisibility:
      return value_id == CSSValueVisible || value_id == CSSValueHidden ||
             value_id == CSSValueCollapse;
    case CSSPropertyWebkitAppRegion:
      return (value_id >= CSSValueDrag && value_id <= CSSValueNoDrag) ||
             value_id == CSSValueNone;
    case CSSPropertyWebkitAppearance:
      return (value_id >= CSSValueCheckbox && value_id <= CSSValueTextarea) ||
             value_id == CSSValueNone;
    case CSSPropertyBackfaceVisibility:
      return value_id == CSSValueVisible || value_id == CSSValueHidden;
    case CSSPropertyMixBlendMode:
      return value_id == CSSValueNormal || value_id == CSSValueMultiply ||
             value_id == CSSValueScreen || value_id == CSSValueOverlay ||
             value_id == CSSValueDarken || value_id == CSSValueLighten ||
             value_id == CSSValueColorDodge || value_id == CSSValueColorBurn ||
             value_id == CSSValueHardLight || value_id == CSSValueSoftLight ||
             value_id == CSSValueDifference || value_id == CSSValueExclusion ||
             value_id == CSSValueHue || value_id == CSSValueSaturation ||
             value_id == CSSValueColor || value_id == CSSValueLuminosity;
    case CSSPropertyWebkitBoxAlign:
      return value_id == CSSValueStretch || value_id == CSSValueStart ||
             value_id == CSSValueEnd || value_id == CSSValueCenter ||
             value_id == CSSValueBaseline;
    case CSSPropertyWebkitBoxDecorationBreak:
      return value_id == CSSValueClone || value_id == CSSValueSlice;
    case CSSPropertyWebkitBoxDirection:
      return value_id == CSSValueNormal || value_id == CSSValueReverse;
    case CSSPropertyWebkitBoxOrient:
      return value_id == CSSValueHorizontal || value_id == CSSValueVertical ||
             value_id == CSSValueInlineAxis || value_id == CSSValueBlockAxis;
    case CSSPropertyWebkitBoxPack:
      return value_id == CSSValueStart || value_id == CSSValueEnd ||
             value_id == CSSValueCenter || value_id == CSSValueJustify;
    case CSSPropertyColumnFill:
      return value_id == CSSValueAuto || value_id == CSSValueBalance;
    case CSSPropertyAlignContent:
      // FIXME: Per CSS alignment, this property should accept an optional
      // <overflow-position>. We should share this parsing code with
      // 'justify-self'.
      return value_id == CSSValueFlexStart || value_id == CSSValueFlexEnd ||
             value_id == CSSValueCenter || value_id == CSSValueSpaceBetween ||
             value_id == CSSValueSpaceAround || value_id == CSSValueStretch;
    case CSSPropertyAlignItems:
      // FIXME: Per CSS alignment, this property should accept the same
      // arguments as 'justify-self' so we should share its parsing code.
      return value_id == CSSValueFlexStart || value_id == CSSValueFlexEnd ||
             value_id == CSSValueCenter || value_id == CSSValueBaseline ||
             value_id == CSSValueStretch;
    case CSSPropertyAlignSelf:
      // FIXME: Per CSS alignment, this property should accept the same
      // arguments as 'justify-self' so we should share its parsing code.
      return value_id == CSSValueAuto || value_id == CSSValueFlexStart ||
             value_id == CSSValueFlexEnd || value_id == CSSValueCenter ||
             value_id == CSSValueBaseline || value_id == CSSValueStretch;
    case CSSPropertyFlexDirection:
      return value_id == CSSValueRow || value_id == CSSValueRowReverse ||
             value_id == CSSValueColumn || value_id == CSSValueColumnReverse;
    case CSSPropertyFlexWrap:
      return value_id == CSSValueNowrap || value_id == CSSValueWrap ||
             value_id == CSSValueWrapReverse;
    case CSSPropertyHyphens:
#if defined(OS_ANDROID) || defined(OS_MACOSX)
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             value_id == CSSValueManual;
#else
      return value_id == CSSValueNone || value_id == CSSValueManual;
#endif
    case CSSPropertyJustifyContent:
      // FIXME: Per CSS alignment, this property should accept an optional
      // <overflow-position>. We should share this parsing code with
      // 'justify-self'.
      return value_id == CSSValueFlexStart || value_id == CSSValueFlexEnd ||
             value_id == CSSValueCenter || value_id == CSSValueSpaceBetween ||
             value_id == CSSValueSpaceAround;
    case CSSPropertyFontKerning:
      return value_id == CSSValueAuto || value_id == CSSValueNormal ||
             value_id == CSSValueNone;
    case CSSPropertyWebkitFontSmoothing:
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             value_id == CSSValueAntialiased ||
             value_id == CSSValueSubpixelAntialiased;
    case CSSPropertyLineBreak:
      return value_id == CSSValueAuto || value_id == CSSValueLoose ||
             value_id == CSSValueNormal || value_id == CSSValueStrict;
    case CSSPropertyWebkitLineBreak:
      return value_id == CSSValueAuto || value_id == CSSValueLoose ||
             value_id == CSSValueNormal || value_id == CSSValueStrict ||
             value_id == CSSValueAfterWhiteSpace;
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
      return value_id == CSSValueCollapse || value_id == CSSValueSeparate ||
             value_id == CSSValueDiscard;
    case CSSPropertyWebkitPrintColorAdjust:
      return value_id == CSSValueExact || value_id == CSSValueEconomy;
    case CSSPropertyWebkitRtlOrdering:
      return value_id == CSSValueLogical || value_id == CSSValueVisual;
    case CSSPropertyWebkitRubyPosition:
      return value_id == CSSValueBefore || value_id == CSSValueAfter;
    case CSSPropertyWebkitTextCombine:
      return value_id == CSSValueNone || value_id == CSSValueHorizontal;
    case CSSPropertyWebkitTextSecurity:
      return value_id == CSSValueDisc || value_id == CSSValueCircle ||
             value_id == CSSValueSquare || value_id == CSSValueNone;
    case CSSPropertyTransformBox:
      return value_id == CSSValueFillBox || value_id == CSSValueViewBox;
    case CSSPropertyTransformStyle:
      return value_id == CSSValueFlat || value_id == CSSValuePreserve3d;
    case CSSPropertyWebkitUserDrag:
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             value_id == CSSValueElement;
    case CSSPropertyWebkitUserModify:
      return value_id == CSSValueReadOnly || value_id == CSSValueReadWrite ||
             value_id == CSSValueReadWritePlaintextOnly;
    case CSSPropertyUserSelect:
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             value_id == CSSValueText || value_id == CSSValueAll;
    case CSSPropertyWebkitWritingMode:
      return value_id >= CSSValueHorizontalTb && value_id <= CSSValueVerticalLr;
    case CSSPropertyWritingMode:
      return value_id == CSSValueHorizontalTb ||
             value_id == CSSValueVerticalRl || value_id == CSSValueVerticalLr ||
             value_id == CSSValueLrTb || value_id == CSSValueRlTb ||
             value_id == CSSValueTbRl || value_id == CSSValueLr ||
             value_id == CSSValueRl || value_id == CSSValueTb;
    case CSSPropertyWhiteSpace:
      return value_id == CSSValueNormal || value_id == CSSValuePre ||
             value_id == CSSValuePreWrap || value_id == CSSValuePreLine ||
             value_id == CSSValueNowrap;
    case CSSPropertyWordBreak:
      return value_id == CSSValueNormal || value_id == CSSValueBreakAll ||
             value_id == CSSValueKeepAll || value_id == CSSValueBreakWord;
    case CSSPropertyScrollSnapStop:
      DCHECK(RuntimeEnabledFeatures::CSSScrollSnapPointsEnabled());
      return value_id == CSSValueNormal || value_id == CSSValueAlways;
    case CSSPropertyOverscrollBehaviorX:
      return value_id == CSSValueAuto || value_id == CSSValueContain ||
             value_id == CSSValueNone;
    case CSSPropertyOverscrollBehaviorY:
      return value_id == CSSValueAuto || value_id == CSSValueContain ||
             value_id == CSSValueNone;
    default:
      NOTREACHED();
      return false;
  }
}

bool CSSParserFastPaths::IsKeywordPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyAll:
    case CSSPropertyMixBlendMode:
    case CSSPropertyIsolation:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBoxSizing:
    case CSSPropertyBufferedRendering:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyClipRule:
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
    case CSSPropertyColorRendering:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyDominantBaseline:
    case CSSPropertyEmptyCells:
    case CSSPropertyFillRule:
    case CSSPropertyFloat:
    case CSSPropertyHyphens:
    case CSSPropertyImageRendering:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyMaskType:
    case CSSPropertyObjectFit:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOverflowAnchor:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
    case CSSPropertyBreakInside:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertyScrollBehavior:
    case CSSPropertyOverscrollBehaviorX:
    case CSSPropertyOverscrollBehaviorY:
    case CSSPropertyShapeRendering:
    case CSSPropertySpeak:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlign:
    case CSSPropertyTextAlignLast:
    case CSSPropertyTextAnchor:
    case CSSPropertyTextCombineUpright:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationSkipInk:
    case CSSPropertyTextJustify:
    case CSSPropertyTextOrientation:
    case CSSPropertyWebkitTextOrientation:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyVectorEffect:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAppRegion:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyBackfaceVisibility:
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDecorationBreak:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyColumnFill:
    case CSSPropertyColumnRuleStyle:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexWrap:
    case CSSPropertyFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyLineBreak:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTextCombine:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyTransformBox:
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyUserSelect:
    case CSSPropertyWebkitWritingMode:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWordBreak:
    case CSSPropertyWritingMode:
    case CSSPropertyScrollSnapStop:
      return true;
    default:
      return false;
  }
}

bool CSSParserFastPaths::IsPartialKeywordPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyDisplay:
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
    // All properties accept the values of "initial," "inherit" and "unset".
    if (!EqualIgnoringASCIICase(string, "initial") &&
        !EqualIgnoringASCIICase(string, "inherit") &&
        !EqualIgnoringASCIICase(string, "unset"))
      return nullptr;

    // Parse initial/inherit/unset shorthands using the CSSPropertyParser.
    if (shorthandForProperty(property_id).length())
      return nullptr;

    // Descriptors do not support css wide keywords.
    if (!CSSProperty::Get(property_id).IsProperty())
      return nullptr;
  }

  CSSValueID value_id = CssValueKeywordID(string);

  if (!value_id)
    return nullptr;

  if (value_id == CSSValueInherit)
    return CSSInheritedValue::Create();
  if (value_id == CSSValueInitial)
    return CSSInitialValue::Create();
  if (value_id == CSSValueUnset)
    return cssvalue::CSSUnsetValue::Create();
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
    transform_value->Append(*CSSPrimitiveValue::Create(
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
    transform_value->Append(*CSSPrimitiveValue::Create(
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
      transform_type = CSSValueTranslateX;
    } else if (c9 == 'y' && pos[10] == '(') {
      transform_type = CSSValueTranslateY;
    } else if (c9 == 'z' && pos[10] == '(') {
      transform_type = CSSValueTranslateZ;
    } else if (c9 == '(') {
      transform_type = CSSValueTranslate;
      expected_argument_count = 2;
      argument_start = 10;
    } else if (c9 == '3' && ToASCIILower(pos[10]) == 'd' && pos[11] == '(') {
      transform_type = CSSValueTranslate3d;
      expected_argument_count = 3;
      argument_start = 12;
    } else {
      return nullptr;
    }
    pos += argument_start;
    CSSFunctionValue* transform_value =
        CSSFunctionValue::Create(transform_type);
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
        CSSFunctionValue::Create(CSSValueMatrix3d);
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
        CSSFunctionValue::Create(CSSValueScale3d);
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

template <typename CharType>
static CSSValueList* ParseSimpleTransformList(const CharType* chars,
                                              unsigned length) {
  if (!TransformCanLikelyUseFastPath(chars, length))
    return nullptr;
  const CharType*& pos = chars;
  const CharType* end = chars + length;
  CSSValueList* transform_list = nullptr;
  while (pos < end) {
    while (pos < end && IsCSSSpace(*pos))
      ++pos;
    if (pos >= end)
      break;
    CSSFunctionValue* transform_value = ParseSimpleTransformValue(pos, end);
    if (!transform_value)
      return nullptr;
    if (!transform_list)
      transform_list = CSSValueList::CreateSpaceSeparated();
    transform_list->Append(*transform_value);
  }
  return transform_list;
}

static CSSValue* ParseSimpleTransform(CSSPropertyID property_id,
                                      const String& string) {
  DCHECK(!string.IsEmpty());

  if (property_id != CSSPropertyTransform)
    return nullptr;
  if (string.Is8Bit())
    return ParseSimpleTransformList(string.Characters8(), string.length());
  return ParseSimpleTransformList(string.Characters16(), string.length());
}

CSSValue* CSSParserFastPaths::MaybeParseValue(CSSPropertyID property_id,
                                              const String& string,
                                              CSSParserMode parser_mode) {
  if (CSSValue* length =
          ParseSimpleLengthValue(property_id, string, parser_mode))
    return length;
  if (IsColorPropertyID(property_id))
    return ParseColor(string, parser_mode);
  if (CSSValue* keyword = ParseKeywordValue(property_id, string, parser_mode))
    return keyword;
  if (CSSValue* transform = ParseSimpleTransform(property_id, string))
    return transform;
  return nullptr;
}

}  // namespace blink

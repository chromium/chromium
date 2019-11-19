/*
 * CSS Media Query
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/media_query_exp.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using media_feature_names::kMaxDeviceAspectRatioMediaFeature;
using media_feature_names::kMaxDevicePixelRatioMediaFeature;
using media_feature_names::kMinDeviceAspectRatioMediaFeature;

static inline bool FeatureWithValidIdent(const String& media_feature,
                                         CSSValueID ident) {
  if (media_feature == media_feature_names::kDisplayModeMediaFeature) {
    return ident == CSSValueID::kFullscreen ||
           ident == CSSValueID::kStandalone ||
           ident == CSSValueID::kMinimalUi || ident == CSSValueID::kBrowser;
  }

  if (media_feature == media_feature_names::kOrientationMediaFeature)
    return ident == CSSValueID::kPortrait || ident == CSSValueID::kLandscape;

  if (media_feature == media_feature_names::kPointerMediaFeature ||
      media_feature == media_feature_names::kAnyPointerMediaFeature) {
    return ident == CSSValueID::kNone || ident == CSSValueID::kCoarse ||
           ident == CSSValueID::kFine;
  }

  if (media_feature == media_feature_names::kHoverMediaFeature ||
      media_feature == media_feature_names::kAnyHoverMediaFeature)
    return ident == CSSValueID::kNone || ident == CSSValueID::kHover;

  if (media_feature == media_feature_names::kScanMediaFeature)
    return ident == CSSValueID::kInterlace || ident == CSSValueID::kProgressive;

  if (RuntimeEnabledFeatures::MediaQueryShapeEnabled()) {
    if (media_feature == media_feature_names::kShapeMediaFeature)
      return ident == CSSValueID::kRect || ident == CSSValueID::kRound;
  }

  if (media_feature == media_feature_names::kColorGamutMediaFeature) {
    return ident == CSSValueID::kSRGB || ident == CSSValueID::kP3 ||
           ident == CSSValueID::kRec2020;
  }

  if (RuntimeEnabledFeatures::MediaQueryPrefersColorSchemeEnabled()) {
    if (media_feature == media_feature_names::kPrefersColorSchemeMediaFeature) {
      return ident == CSSValueID::kNoPreference || ident == CSSValueID::kDark ||
             ident == CSSValueID::kLight;
    }
  }

  if (media_feature == media_feature_names::kPrefersReducedMotionMediaFeature)
    return ident == CSSValueID::kNoPreference || ident == CSSValueID::kReduce;

  if (RuntimeEnabledFeatures::ForcedColorsEnabled()) {
    if (media_feature == media_feature_names::kForcedColorsMediaFeature) {
      return ident == CSSValueID::kNone || ident == CSSValueID::kActive;
    }
  }

  if (RuntimeEnabledFeatures::MediaQueryNavigationControlsEnabled()) {
    if (media_feature == media_feature_names::kNavigationControlsMediaFeature) {
      return ident == CSSValueID::kNone || ident == CSSValueID::kBackButton;
    }
  }

  return false;
}

static inline bool FeatureWithValidPositiveLength(
    const String& media_feature,
    const CSSPrimitiveValue* value) {
  if (!(value->IsLength() ||
        (value->IsNumber() && value->GetDoubleValue() == 0)))
    return false;

  return media_feature == media_feature_names::kHeightMediaFeature ||
         media_feature == media_feature_names::kMaxHeightMediaFeature ||
         media_feature == media_feature_names::kMinHeightMediaFeature ||
         media_feature == media_feature_names::kWidthMediaFeature ||
         media_feature == media_feature_names::kMaxWidthMediaFeature ||
         media_feature == media_feature_names::kMinWidthMediaFeature ||
         media_feature == media_feature_names::kDeviceHeightMediaFeature ||
         media_feature == media_feature_names::kMaxDeviceHeightMediaFeature ||
         media_feature == media_feature_names::kMinDeviceHeightMediaFeature ||
         media_feature == media_feature_names::kDeviceWidthMediaFeature ||
         media_feature == media_feature_names::kMinDeviceWidthMediaFeature ||
         media_feature == media_feature_names::kMaxDeviceWidthMediaFeature;
}

static inline bool FeatureWithValidDensity(const String& media_feature,
                                           const CSSPrimitiveValue* value) {
  if (!value->IsResolution() || value->GetDoubleValue() <= 0)
    return false;

  return media_feature == media_feature_names::kResolutionMediaFeature ||
         media_feature == media_feature_names::kMinResolutionMediaFeature ||
         media_feature == media_feature_names::kMaxResolutionMediaFeature;
}

static inline bool FeatureExpectingPositiveInteger(
    const String& media_feature) {
  return media_feature == media_feature_names::kColorMediaFeature ||
         media_feature == media_feature_names::kMaxColorMediaFeature ||
         media_feature == media_feature_names::kMinColorMediaFeature ||
         media_feature == media_feature_names::kColorIndexMediaFeature ||
         media_feature == media_feature_names::kMaxColorIndexMediaFeature ||
         media_feature == media_feature_names::kMinColorIndexMediaFeature ||
         media_feature == media_feature_names::kMonochromeMediaFeature ||
         media_feature == media_feature_names::kMaxMonochromeMediaFeature ||
         media_feature == media_feature_names::kMinMonochromeMediaFeature ||
         media_feature == media_feature_names::kImmersiveMediaFeature;
}

static inline bool FeatureWithPositiveInteger(const String& media_feature,
                                              const CSSPrimitiveValue* value) {
  if (!value->IsInteger())
    return false;
  return FeatureExpectingPositiveInteger(media_feature);
}

static inline bool FeatureWithPositiveNumber(const String& media_feature,
                                             const CSSPrimitiveValue* value) {
  if (!value->IsNumber())
    return false;

  return media_feature == media_feature_names::kTransform3dMediaFeature ||
         media_feature == media_feature_names::kDevicePixelRatioMediaFeature ||
         media_feature == kMaxDevicePixelRatioMediaFeature ||
         media_feature == media_feature_names::kMinDevicePixelRatioMediaFeature;
}

static inline bool FeatureWithZeroOrOne(const String& media_feature,
                                        const CSSPrimitiveValue* value) {
  if (!value->IsInteger() ||
      !(value->GetDoubleValue() == 1 || !value->GetDoubleValue()))
    return false;

  return media_feature == media_feature_names::kGridMediaFeature;
}

static inline bool FeatureWithAspectRatio(const String& media_feature) {
  return media_feature == media_feature_names::kAspectRatioMediaFeature ||
         media_feature == media_feature_names::kDeviceAspectRatioMediaFeature ||
         media_feature == media_feature_names::kMinAspectRatioMediaFeature ||
         media_feature == media_feature_names::kMaxAspectRatioMediaFeature ||
         media_feature == kMinDeviceAspectRatioMediaFeature ||
         media_feature == kMaxDeviceAspectRatioMediaFeature;
}

static inline bool FeatureWithoutValue(const String& media_feature) {
  // Media features that are prefixed by min/max cannot be used without a value.
  return media_feature == media_feature_names::kMonochromeMediaFeature ||
         media_feature == media_feature_names::kColorMediaFeature ||
         media_feature == media_feature_names::kColorIndexMediaFeature ||
         media_feature == media_feature_names::kGridMediaFeature ||
         media_feature == media_feature_names::kHeightMediaFeature ||
         media_feature == media_feature_names::kWidthMediaFeature ||
         media_feature == media_feature_names::kDeviceHeightMediaFeature ||
         media_feature == media_feature_names::kDeviceWidthMediaFeature ||
         media_feature == media_feature_names::kOrientationMediaFeature ||
         media_feature == media_feature_names::kAspectRatioMediaFeature ||
         media_feature == media_feature_names::kDeviceAspectRatioMediaFeature ||
         media_feature == media_feature_names::kHoverMediaFeature ||
         media_feature == media_feature_names::kAnyHoverMediaFeature ||
         media_feature == media_feature_names::kTransform3dMediaFeature ||
         media_feature == media_feature_names::kPointerMediaFeature ||
         media_feature == media_feature_names::kAnyPointerMediaFeature ||
         media_feature == media_feature_names::kDevicePixelRatioMediaFeature ||
         media_feature == media_feature_names::kResolutionMediaFeature ||
         media_feature == media_feature_names::kDisplayModeMediaFeature ||
         media_feature == media_feature_names::kScanMediaFeature ||
         media_feature == media_feature_names::kShapeMediaFeature ||
         media_feature == media_feature_names::kColorGamutMediaFeature ||
         media_feature == media_feature_names::kImmersiveMediaFeature ||
         media_feature ==
             media_feature_names::kPrefersColorSchemeMediaFeature ||
         media_feature ==
             media_feature_names::kPrefersReducedMotionMediaFeature ||
         media_feature == media_feature_names::kForcedColorsMediaFeature ||
         media_feature == media_feature_names::kNavigationControlsMediaFeature;
}

bool MediaQueryExp::IsViewportDependent() const {
  return media_feature_ == media_feature_names::kWidthMediaFeature ||
         media_feature_ == media_feature_names::kHeightMediaFeature ||
         media_feature_ == media_feature_names::kMinWidthMediaFeature ||
         media_feature_ == media_feature_names::kMinHeightMediaFeature ||
         media_feature_ == media_feature_names::kMaxWidthMediaFeature ||
         media_feature_ == media_feature_names::kMaxHeightMediaFeature ||
         media_feature_ == media_feature_names::kOrientationMediaFeature ||
         media_feature_ == media_feature_names::kAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMinAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kDevicePixelRatioMediaFeature ||
         media_feature_ == media_feature_names::kResolutionMediaFeature ||
         media_feature_ == media_feature_names::kMaxAspectRatioMediaFeature ||
         media_feature_ == kMaxDevicePixelRatioMediaFeature ||
         media_feature_ ==
             media_feature_names::kMinDevicePixelRatioMediaFeature;
}

bool MediaQueryExp::IsDeviceDependent() const {
  return media_feature_ ==
             media_feature_names::kDeviceAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kDeviceWidthMediaFeature ||
         media_feature_ == media_feature_names::kDeviceHeightMediaFeature ||
         media_feature_ == kMinDeviceAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMinDeviceWidthMediaFeature ||
         media_feature_ == media_feature_names::kMinDeviceHeightMediaFeature ||
         media_feature_ == kMaxDeviceAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMaxDeviceWidthMediaFeature ||
         media_feature_ == media_feature_names::kMaxDeviceHeightMediaFeature ||
         media_feature_ == media_feature_names::kShapeMediaFeature;
}

MediaQueryExp::MediaQueryExp(const MediaQueryExp& other)
    : media_feature_(other.MediaFeature()), exp_value_(other.ExpValue()) {}

MediaQueryExp::MediaQueryExp(const String& media_feature,
                             const MediaQueryExpValue& exp_value)
    : media_feature_(media_feature), exp_value_(exp_value) {}

MediaQueryExp MediaQueryExp::Create(const String& media_feature,
                                    CSSParserTokenRange& range) {
  DCHECK(!media_feature.IsNull());

  MediaQueryExpValue exp_value;
  String lower_media_feature =
      AttemptStaticStringCreation(media_feature.LowerASCII());

  CSSPrimitiveValue* value =
      css_property_parser_helpers::ConsumeInteger(range, 0);
  if (!value && !FeatureExpectingPositiveInteger(lower_media_feature) &&
      !FeatureWithAspectRatio(lower_media_feature)) {
    value = css_property_parser_helpers::ConsumeNumber(range,
                                                       kValueRangeNonNegative);
  }
  if (!value) {
    value = css_property_parser_helpers::ConsumeLength(range, kHTMLStandardMode,
                                                       kValueRangeNonNegative);
  }
  if (!value)
    value = css_property_parser_helpers::ConsumeResolution(range);

  if (!value) {
    if (CSSIdentifierValue* ident =
            css_property_parser_helpers::ConsumeIdent(range)) {
      CSSValueID ident_id = ident->GetValueID();
      if (!FeatureWithValidIdent(lower_media_feature, ident_id))
        return Invalid();
      exp_value.id = ident_id;
      exp_value.is_id = true;
      return MediaQueryExp(lower_media_feature, exp_value);
    }
    if (FeatureWithoutValue(lower_media_feature)) {
      // Valid, creates a MediaQueryExp with an 'invalid' MediaQueryExpValue
      return MediaQueryExp(lower_media_feature, exp_value);
    }
    return Invalid();
  }

  // Now we have |value| as a number, length or resolution
  // Create value for media query expression that must have 1 or more values.
  if (FeatureWithAspectRatio(lower_media_feature)) {
    if (!value->IsInteger() || value->GetDoubleValue() == 0)
      return Invalid();
    if (!css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range))
      return Invalid();
    CSSPrimitiveValue* denominator =
        css_property_parser_helpers::ConsumePositiveInteger(range);
    if (!denominator)
      return Invalid();

    exp_value.numerator = clampTo<unsigned>(value->GetDoubleValue());
    exp_value.denominator = clampTo<unsigned>(denominator->GetDoubleValue());
    exp_value.is_ratio = true;
    return MediaQueryExp(lower_media_feature, exp_value);
  }

  if (FeatureWithValidDensity(lower_media_feature, value)) {
    // TODO(crbug.com/983613): Support resolution in math functions.
    DCHECK(value->IsNumericLiteralValue());
    const auto* numeric_literal = To<CSSNumericLiteralValue>(value);
    exp_value.value = numeric_literal->DoubleValue();
    exp_value.unit = numeric_literal->GetType();
    exp_value.is_value = true;
    return MediaQueryExp(lower_media_feature, exp_value);
  }

  if (FeatureWithPositiveInteger(lower_media_feature, value) ||
      FeatureWithPositiveNumber(lower_media_feature, value) ||
      FeatureWithZeroOrOne(lower_media_feature, value)) {
    exp_value.value = value->GetDoubleValue();
    exp_value.unit = CSSPrimitiveValue::UnitType::kNumber;
    exp_value.is_value = true;
    return MediaQueryExp(lower_media_feature, exp_value);
  }

  if (FeatureWithValidPositiveLength(lower_media_feature, value)) {
    if (value->IsNumber()) {
      exp_value.value = value->GetDoubleValue();
      exp_value.unit = CSSPrimitiveValue::UnitType::kNumber;
      exp_value.is_value = true;
      return MediaQueryExp(lower_media_feature, exp_value);
    }

    DCHECK(value->IsLength());
    if (const auto* numeric_literal =
            DynamicTo<CSSNumericLiteralValue>(value)) {
      exp_value.value = numeric_literal->GetDoubleValue();
      exp_value.unit = numeric_literal->GetType();
      exp_value.is_value = true;
      return MediaQueryExp(lower_media_feature, exp_value);
    }

    const auto* math_value = To<CSSMathFunctionValue>(value);
    CSSPrimitiveValue::UnitType expression_unit =
        math_value->ExpressionNode()->ResolvedUnitType();
    if (expression_unit == CSSPrimitiveValue::UnitType::kUnknown) {
      // TODO(crbug.com/982542): Support math expressions involving type
      // conversions properly. For example, calc(10px + 1em).
      return Invalid();
    }
    exp_value.value = math_value->DoubleValue();
    exp_value.unit = expression_unit;
    exp_value.is_value = true;
    return MediaQueryExp(lower_media_feature, exp_value);
  }

  return Invalid();
}

MediaQueryExp::~MediaQueryExp() = default;

bool MediaQueryExp::operator==(const MediaQueryExp& other) const {
  return (other.media_feature_ == media_feature_) &&
         ((!other.exp_value_.IsValid() && !exp_value_.IsValid()) ||
          (other.exp_value_.IsValid() && exp_value_.IsValid() &&
           other.exp_value_.Equals(exp_value_)));
}

String MediaQueryExp::Serialize() const {
  StringBuilder result;
  result.Append('(');
  result.Append(media_feature_.LowerASCII());
  if (exp_value_.IsValid()) {
    result.Append(": ");
    result.Append(exp_value_.CssText());
  }
  result.Append(')');

  return result.ToString();
}

static inline String PrintNumber(double number) {
  return Decimal::FromDouble(number).ToString();
}

String MediaQueryExpValue::CssText() const {
  StringBuilder output;
  if (is_value) {
    output.Append(PrintNumber(value));
    output.Append(CSSPrimitiveValue::UnitTypeToString(unit));
  } else if (is_ratio) {
    output.Append(PrintNumber(numerator));
    output.Append('/');
    output.Append(PrintNumber(denominator));
  } else if (is_id) {
    output.Append(getValueName(id));
  }

  return output.ToString();
}

}  // namespace blink

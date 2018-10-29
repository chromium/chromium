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

#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using namespace media_feature_names;

static inline bool FeatureWithValidIdent(const String& media_feature,
                                         CSSValueID ident) {
  if (media_feature == kDisplayModeMediaFeature)
    return ident == CSSValueFullscreen || ident == CSSValueStandalone ||
           ident == CSSValueMinimalUi || ident == CSSValueBrowser;

  if (media_feature == kOrientationMediaFeature)
    return ident == CSSValuePortrait || ident == CSSValueLandscape;

  if (media_feature == kPointerMediaFeature ||
      media_feature == kAnyPointerMediaFeature)
    return ident == CSSValueNone || ident == CSSValueCoarse ||
           ident == CSSValueFine;

  if (media_feature == kHoverMediaFeature ||
      media_feature == kAnyHoverMediaFeature)
    return ident == CSSValueNone || ident == CSSValueHover;

  if (media_feature == kScanMediaFeature)
    return ident == CSSValueInterlace || ident == CSSValueProgressive;

  if (RuntimeEnabledFeatures::MediaQueryShapeEnabled()) {
    if (media_feature == kShapeMediaFeature)
      return ident == CSSValueRect || ident == CSSValueRound;
  }

  if (media_feature == kColorGamutMediaFeature) {
    return ident == CSSValueSRGB || ident == CSSValueP3 ||
           ident == CSSValueRec2020;
  }

  return false;
}

static inline bool FeatureWithValidPositiveLength(
    const String& media_feature,
    const CSSPrimitiveValue* value) {
  if (!(value->IsLength() ||
        (value->IsNumber() && value->GetDoubleValue() == 0)))
    return false;

  return media_feature == kHeightMediaFeature ||
         media_feature == kMaxHeightMediaFeature ||
         media_feature == kMinHeightMediaFeature ||
         media_feature == kWidthMediaFeature ||
         media_feature == kMaxWidthMediaFeature ||
         media_feature == kMinWidthMediaFeature ||
         media_feature == kDeviceHeightMediaFeature ||
         media_feature == kMaxDeviceHeightMediaFeature ||
         media_feature == kMinDeviceHeightMediaFeature ||
         media_feature == kDeviceWidthMediaFeature ||
         media_feature == kMinDeviceWidthMediaFeature ||
         media_feature == kMaxDeviceWidthMediaFeature;
}

static inline bool FeatureWithValidDensity(const String& media_feature,
                                           const CSSPrimitiveValue* value) {
  if ((value->TypeWithCalcResolved() !=
           CSSPrimitiveValue::UnitType::kDotsPerPixel &&
       value->TypeWithCalcResolved() !=
           CSSPrimitiveValue::UnitType::kDotsPerInch &&
       value->TypeWithCalcResolved() !=
           CSSPrimitiveValue::UnitType::kDotsPerCentimeter) ||
      value->GetDoubleValue() <= 0)
    return false;

  return media_feature == kResolutionMediaFeature ||
         media_feature == kMinResolutionMediaFeature ||
         media_feature == kMaxResolutionMediaFeature;
}

static inline bool FeatureExpectingPositiveInteger(
    const String& media_feature) {
  return media_feature == kColorMediaFeature ||
         media_feature == kMaxColorMediaFeature ||
         media_feature == kMinColorMediaFeature ||
         media_feature == kColorIndexMediaFeature ||
         media_feature == kMaxColorIndexMediaFeature ||
         media_feature == kMinColorIndexMediaFeature ||
         media_feature == kMonochromeMediaFeature ||
         media_feature == kMaxMonochromeMediaFeature ||
         media_feature == kMinMonochromeMediaFeature ||
         media_feature == kImmersiveMediaFeature;
}

static inline bool FeatureWithPositiveInteger(const String& media_feature,
                                              const CSSPrimitiveValue* value) {
  if (value->TypeWithCalcResolved() != CSSPrimitiveValue::UnitType::kInteger)
    return false;
  return FeatureExpectingPositiveInteger(media_feature);
}

static inline bool FeatureWithPositiveNumber(const String& media_feature,
                                             const CSSPrimitiveValue* value) {
  if (!value->IsNumber())
    return false;

  return media_feature == kTransform3dMediaFeature ||
         media_feature == kDevicePixelRatioMediaFeature ||
         media_feature == kMaxDevicePixelRatioMediaFeature ||
         media_feature == kMinDevicePixelRatioMediaFeature;
}

static inline bool FeatureWithZeroOrOne(const String& media_feature,
                                        const CSSPrimitiveValue* value) {
  if (value->TypeWithCalcResolved() != CSSPrimitiveValue::UnitType::kInteger ||
      !(value->GetDoubleValue() == 1 || !value->GetDoubleValue()))
    return false;

  return media_feature == kGridMediaFeature;
}

static inline bool FeatureWithAspectRatio(const String& media_feature) {
  return media_feature == kAspectRatioMediaFeature ||
         media_feature == kDeviceAspectRatioMediaFeature ||
         media_feature == kMinAspectRatioMediaFeature ||
         media_feature == kMaxAspectRatioMediaFeature ||
         media_feature == kMinDeviceAspectRatioMediaFeature ||
         media_feature == kMaxDeviceAspectRatioMediaFeature;
}

static inline bool FeatureWithoutValue(const String& media_feature) {
  // Media features that are prefixed by min/max cannot be used without a value.
  return media_feature == kMonochromeMediaFeature ||
         media_feature == kColorMediaFeature ||
         media_feature == kColorIndexMediaFeature ||
         media_feature == kGridMediaFeature ||
         media_feature == kHeightMediaFeature ||
         media_feature == kWidthMediaFeature ||
         media_feature == kDeviceHeightMediaFeature ||
         media_feature == kDeviceWidthMediaFeature ||
         media_feature == kOrientationMediaFeature ||
         media_feature == kAspectRatioMediaFeature ||
         media_feature == kDeviceAspectRatioMediaFeature ||
         media_feature == kHoverMediaFeature ||
         media_feature == kAnyHoverMediaFeature ||
         media_feature == kTransform3dMediaFeature ||
         media_feature == kPointerMediaFeature ||
         media_feature == kAnyPointerMediaFeature ||
         media_feature == kDevicePixelRatioMediaFeature ||
         media_feature == kResolutionMediaFeature ||
         media_feature == kDisplayModeMediaFeature ||
         media_feature == kScanMediaFeature ||
         media_feature == kShapeMediaFeature ||
         media_feature == kColorGamutMediaFeature ||
         media_feature == kImmersiveMediaFeature;
}

bool MediaQueryExp::IsViewportDependent() const {
  return media_feature_ == kWidthMediaFeature ||
         media_feature_ == kHeightMediaFeature ||
         media_feature_ == kMinWidthMediaFeature ||
         media_feature_ == kMinHeightMediaFeature ||
         media_feature_ == kMaxWidthMediaFeature ||
         media_feature_ == kMaxHeightMediaFeature ||
         media_feature_ == kOrientationMediaFeature ||
         media_feature_ == kAspectRatioMediaFeature ||
         media_feature_ == kMinAspectRatioMediaFeature ||
         media_feature_ == kDevicePixelRatioMediaFeature ||
         media_feature_ == kResolutionMediaFeature ||
         media_feature_ == kMaxAspectRatioMediaFeature ||
         media_feature_ == kMaxDevicePixelRatioMediaFeature ||
         media_feature_ == kMinDevicePixelRatioMediaFeature;
}

bool MediaQueryExp::IsDeviceDependent() const {
  return media_feature_ == kDeviceAspectRatioMediaFeature ||
         media_feature_ == kDeviceWidthMediaFeature ||
         media_feature_ == kDeviceHeightMediaFeature ||
         media_feature_ == kMinDeviceAspectRatioMediaFeature ||
         media_feature_ == kMinDeviceWidthMediaFeature ||
         media_feature_ == kMinDeviceHeightMediaFeature ||
         media_feature_ == kMaxDeviceAspectRatioMediaFeature ||
         media_feature_ == kMaxDeviceWidthMediaFeature ||
         media_feature_ == kMaxDeviceHeightMediaFeature ||
         media_feature_ == kShapeMediaFeature;
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

  CSSPrimitiveValue* value = CSSPropertyParserHelpers::ConsumeInteger(range, 0);
  if (!value && !FeatureExpectingPositiveInteger(lower_media_feature) &&
      !FeatureWithAspectRatio(lower_media_feature))
    value =
        CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (!value)
    value = CSSPropertyParserHelpers::ConsumeLength(range, kHTMLStandardMode,
                                                    kValueRangeNonNegative);
  if (!value)
    value = CSSPropertyParserHelpers::ConsumeResolution(range);
  // Create value for media query expression that must have 1 or more values.
  if (value) {
    if (FeatureWithAspectRatio(lower_media_feature)) {
      if (value->TypeWithCalcResolved() !=
              CSSPrimitiveValue::UnitType::kInteger ||
          value->GetDoubleValue() == 0)
        return Invalid();
      if (!CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range))
        return Invalid();
      CSSPrimitiveValue* denominator =
          CSSPropertyParserHelpers::ConsumePositiveInteger(range);
      if (!denominator)
        return Invalid();

      exp_value.numerator = clampTo<unsigned>(value->GetDoubleValue());
      exp_value.denominator = clampTo<unsigned>(denominator->GetDoubleValue());
      exp_value.is_ratio = true;
    } else if (FeatureWithValidDensity(lower_media_feature, value) ||
               FeatureWithValidPositiveLength(lower_media_feature, value) ||
               FeatureWithPositiveInteger(lower_media_feature, value) ||
               FeatureWithPositiveNumber(lower_media_feature, value) ||
               FeatureWithZeroOrOne(lower_media_feature, value)) {
      exp_value.value = value->GetDoubleValue();
      if (value->IsNumber())
        exp_value.unit = CSSPrimitiveValue::UnitType::kNumber;
      else
        exp_value.unit = value->TypeWithCalcResolved();
      exp_value.is_value = true;
    } else {
      return Invalid();
    }
  } else if (CSSIdentifierValue* ident = CSSPropertyParserHelpers::ConsumeIdent(range)) {
    CSSValueID ident_id = ident->GetValueID();
    if (!FeatureWithValidIdent(lower_media_feature, ident_id))
      return Invalid();
    exp_value.id = ident_id;
    exp_value.is_id = true;
  } else if (FeatureWithoutValue(lower_media_feature)) {
    // Valid, creates a MediaQueryExp with an 'invalid' MediaQueryExpValue
  } else {
    return Invalid();
  }

  return MediaQueryExp(lower_media_feature, exp_value);
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

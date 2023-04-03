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

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenized_value.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
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
           ident == CSSValueID::kBorderless ||
           ident == CSSValueID::kStandalone ||
           ident == CSSValueID::kMinimalUi ||
           ident == CSSValueID::kWindowControlsOverlay ||
           ident == CSSValueID::kBrowser || ident == CSSValueID::kTabbed;
  }

  if (media_feature == media_feature_names::kOrientationMediaFeature) {
    return ident == CSSValueID::kPortrait || ident == CSSValueID::kLandscape;
  }

  if (media_feature == media_feature_names::kPointerMediaFeature ||
      media_feature == media_feature_names::kAnyPointerMediaFeature) {
    return ident == CSSValueID::kNone || ident == CSSValueID::kCoarse ||
           ident == CSSValueID::kFine;
  }

  if (media_feature == media_feature_names::kHoverMediaFeature ||
      media_feature == media_feature_names::kAnyHoverMediaFeature) {
    return ident == CSSValueID::kNone || ident == CSSValueID::kHover;
  }

  if (media_feature == media_feature_names::kScanMediaFeature) {
    return ident == CSSValueID::kInterlace || ident == CSSValueID::kProgressive;
  }

  if (media_feature == media_feature_names::kColorGamutMediaFeature) {
    return ident == CSSValueID::kSRGB || ident == CSSValueID::kP3 ||
           ident == CSSValueID::kRec2020;
  }

  if (media_feature == media_feature_names::kPrefersColorSchemeMediaFeature) {
    return ident == CSSValueID::kDark || ident == CSSValueID::kLight;
  }

  if (media_feature == media_feature_names::kPrefersContrastMediaFeature) {
    return ident == CSSValueID::kNoPreference || ident == CSSValueID::kMore ||
           ident == CSSValueID::kLess || ident == CSSValueID::kCustom;
  }

  if (media_feature == media_feature_names::kPrefersReducedMotionMediaFeature) {
    return ident == CSSValueID::kNoPreference || ident == CSSValueID::kReduce;
  }

  if (media_feature == media_feature_names::kDynamicRangeMediaFeature) {
    return ident == CSSValueID::kStandard || ident == CSSValueID::kHigh;
  }

  if (RuntimeEnabledFeatures::CSSVideoDynamicRangeMediaQueriesEnabled()) {
    if (media_feature == media_feature_names::kVideoDynamicRangeMediaFeature) {
      return ident == CSSValueID::kStandard || ident == CSSValueID::kHigh;
    }
  }

  if (RuntimeEnabledFeatures::PrefersReducedDataEnabled() &&
      media_feature == media_feature_names::kPrefersReducedDataMediaFeature) {
    return ident == CSSValueID::kNoPreference || ident == CSSValueID::kReduce;
  }

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

  if (RuntimeEnabledFeatures::DevicePostureEnabled()) {
    if (media_feature == media_feature_names::kDevicePostureMediaFeature) {
      return ident == CSSValueID::kContinuous || ident == CSSValueID::kFolded;
    }
  }

  if (RuntimeEnabledFeatures::CSSOverflowMediaFeaturesEnabled()) {
    if (media_feature == media_feature_names::kOverflowInlineMediaFeature) {
      return ident == CSSValueID::kNone || ident == CSSValueID::kScroll;
    }
  }

  if (RuntimeEnabledFeatures::CSSOverflowMediaFeaturesEnabled()) {
    if (media_feature == media_feature_names::kOverflowBlockMediaFeature) {
      return ident == CSSValueID::kNone || ident == CSSValueID::kScroll ||
             ident == CSSValueID::kPaged;
    }
  }

  if (RuntimeEnabledFeatures::CSSUpdateMediaFeatureEnabled()) {
    if (media_feature == media_feature_names::kUpdateMediaFeature) {
      return ident == CSSValueID::kNone || ident == CSSValueID::kFast ||
             ident == CSSValueID::kSlow;
    }
  }

  return false;
}

static inline bool FeatureWithValidLength(const String& media_feature,
                                          const CSSPrimitiveValue* value) {
  if (!(value->IsLength() ||
        (value->IsNumber() && value->GetDoubleValue() == 0))) {
    return false;
  }

  return media_feature == media_feature_names::kHeightMediaFeature ||
         media_feature == media_feature_names::kMaxHeightMediaFeature ||
         media_feature == media_feature_names::kMinHeightMediaFeature ||
         media_feature == media_feature_names::kWidthMediaFeature ||
         media_feature == media_feature_names::kMaxWidthMediaFeature ||
         media_feature == media_feature_names::kMinWidthMediaFeature ||
         media_feature == media_feature_names::kBlockSizeMediaFeature ||
         media_feature == media_feature_names::kMaxBlockSizeMediaFeature ||
         media_feature == media_feature_names::kMinBlockSizeMediaFeature ||
         media_feature == media_feature_names::kInlineSizeMediaFeature ||
         media_feature == media_feature_names::kMaxInlineSizeMediaFeature ||
         media_feature == media_feature_names::kMinInlineSizeMediaFeature ||
         media_feature == media_feature_names::kDeviceHeightMediaFeature ||
         media_feature == media_feature_names::kMaxDeviceHeightMediaFeature ||
         media_feature == media_feature_names::kMinDeviceHeightMediaFeature ||
         media_feature == media_feature_names::kDeviceWidthMediaFeature ||
         media_feature == media_feature_names::kMinDeviceWidthMediaFeature ||
         media_feature == media_feature_names::kMaxDeviceWidthMediaFeature;
}

static inline bool FeatureWithValidDensity(const String& media_feature,
                                           const CSSPrimitiveValue* value) {
  if (!value->IsResolution()) {
    return false;
  }

  return media_feature == media_feature_names::kResolutionMediaFeature ||
         media_feature == media_feature_names::kMinResolutionMediaFeature ||
         media_feature == media_feature_names::kMaxResolutionMediaFeature;
}

static inline bool FeatureExpectingInteger(const String& media_feature) {
  if (media_feature == media_feature_names::kColorMediaFeature ||
      media_feature == media_feature_names::kMaxColorMediaFeature ||
      media_feature == media_feature_names::kMinColorMediaFeature ||
      media_feature == media_feature_names::kColorIndexMediaFeature ||
      media_feature == media_feature_names::kMaxColorIndexMediaFeature ||
      media_feature == media_feature_names::kMinColorIndexMediaFeature ||
      media_feature == media_feature_names::kMonochromeMediaFeature ||
      media_feature == media_feature_names::kMaxMonochromeMediaFeature ||
      media_feature == media_feature_names::kMinMonochromeMediaFeature) {
    return true;
  }

  if (RuntimeEnabledFeatures::CSSFoldablesEnabled()) {
    if (media_feature ==
            media_feature_names::kHorizontalViewportSegmentsMediaFeature ||
        media_feature ==
            media_feature_names::kVerticalViewportSegmentsMediaFeature) {
      return true;
    }
  }

  return false;
}

static inline bool FeatureWithInteger(const String& media_feature,
                                      const CSSPrimitiveValue* value) {
  if (!value->IsInteger()) {
    return false;
  }
  return FeatureExpectingInteger(media_feature);
}

static inline bool FeatureWithNumber(const String& media_feature,
                                     const CSSPrimitiveValue* value) {
  if (!value->IsNumber()) {
    return false;
  }

  return media_feature == media_feature_names::kTransform3dMediaFeature ||
         media_feature == media_feature_names::kDevicePixelRatioMediaFeature ||
         media_feature == kMaxDevicePixelRatioMediaFeature ||
         media_feature == media_feature_names::kMinDevicePixelRatioMediaFeature;
}

static inline bool FeatureWithZeroOrOne(const String& media_feature,
                                        const CSSPrimitiveValue* value) {
  if (!value->IsInteger() ||
      !(value->GetDoubleValue() == 1 || !value->GetDoubleValue())) {
    return false;
  }

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
         media_feature_ == media_feature_names::kDynamicRangeMediaFeature ||
         media_feature_ == media_feature_names::kVideoDynamicRangeMediaFeature;
}

bool MediaQueryExp::IsWidthDependent() const {
  return media_feature_ == media_feature_names::kWidthMediaFeature ||
         media_feature_ == media_feature_names::kMinWidthMediaFeature ||
         media_feature_ == media_feature_names::kMaxWidthMediaFeature ||
         media_feature_ == media_feature_names::kAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMinAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMaxAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kOrientationMediaFeature;
}

bool MediaQueryExp::IsHeightDependent() const {
  return media_feature_ == media_feature_names::kHeightMediaFeature ||
         media_feature_ == media_feature_names::kMinHeightMediaFeature ||
         media_feature_ == media_feature_names::kMaxHeightMediaFeature ||
         media_feature_ == media_feature_names::kAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMinAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMaxAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kOrientationMediaFeature;
}

bool MediaQueryExp::IsInlineSizeDependent() const {
  return media_feature_ == media_feature_names::kInlineSizeMediaFeature ||
         media_feature_ == media_feature_names::kMinInlineSizeMediaFeature ||
         media_feature_ == media_feature_names::kMaxInlineSizeMediaFeature;
}

bool MediaQueryExp::IsBlockSizeDependent() const {
  return media_feature_ == media_feature_names::kBlockSizeMediaFeature ||
         media_feature_ == media_feature_names::kMinBlockSizeMediaFeature ||
         media_feature_ == media_feature_names::kMaxBlockSizeMediaFeature;
}

MediaQueryExp::MediaQueryExp(const MediaQueryExp& other)
    : media_feature_(other.MediaFeature()), bounds_(other.bounds_) {}

MediaQueryExp::MediaQueryExp(const String& media_feature,
                             const MediaQueryExpValue& value)
    : MediaQueryExp(media_feature,
                    MediaQueryExpBounds(MediaQueryExpComparison(value))) {}

MediaQueryExp::MediaQueryExp(const String& media_feature,
                             const MediaQueryExpBounds& bounds)
    : media_feature_(media_feature), bounds_(bounds) {}

MediaQueryExp MediaQueryExp::Create(const String& media_feature,
                                    CSSParserTokenRange& range,
                                    const CSSParserTokenOffsets& offsets,
                                    const CSSParserContext& context) {
  String feature = AttemptStaticStringCreation(media_feature);
  if (auto value =
          MediaQueryExpValue::Consume(feature, range, offsets, context)) {
    return MediaQueryExp(feature, *value);
  }
  return Invalid();
}

bool MediaQueryExpValue::IsResolution() const {
  switch (type_) {
    case Type::kNumeric:
      return CSSPrimitiveValue::IsResolution(Unit());
    case Type::kCSSValue:
      if (const auto* math_function =
              DynamicTo<CSSMathFunctionValue>(css_value_.Get())) {
        return math_function->IsResolution();
      }
      return false;
    default:
      return false;
  }
}

double MediaQueryExpValue::Value() const {
  if (const auto* math_function =
          DynamicTo<CSSMathFunctionValue>(css_value_.Get())) {
    if (math_function->IsResolution()) {
      return math_function->ComputeDotsPerPixel();
    }
  }

  DCHECK(IsNumeric());
  return numeric_.value;
}

CSSPrimitiveValue::UnitType MediaQueryExpValue::Unit() const {
  if (const auto* math_function =
          DynamicTo<CSSMathFunctionValue>(css_value_.Get())) {
    if (math_function->IsResolution()) {
      return CSSPrimitiveValue::UnitType::kDotsPerPixel;
    }
  }

  DCHECK(IsNumeric());
  return numeric_.unit;
}

absl::optional<MediaQueryExpValue> MediaQueryExpValue::Consume(
    const String& media_feature,
    CSSParserTokenRange& range,
    const CSSParserTokenOffsets& offsets,
    const CSSParserContext& context) {
  CSSParserContext::ParserModeOverridingScope scope(context, kHTMLStandardMode);

  if (CSSVariableParser::IsValidVariableName(media_feature)) {
    base::span span = range.RemainingSpan();
    StringView original_string =
        offsets.StringForTokens(span.data(), span.data() + span.size());
    CSSTokenizedValue tokenized_value{range, original_string};
    CSSParserImpl::RemoveImportantAnnotationIfPresent(tokenized_value);
    if (const CSSValue* value =
            CSSVariableParser::ParseDeclarationIncludingCSSWide(
                tokenized_value, false, context)) {
      while (!range.AtEnd()) {
        range.Consume();
      }
      return MediaQueryExpValue(*value);
    }
    return absl::nullopt;
  }

  DCHECK_EQ(media_feature, media_feature.LowerASCII())
      << "Under the assumption that custom properties in style() container "
         "queries are currently the only case sensitive features";

  CSSPrimitiveValue* value = css_parsing_utils::ConsumeInteger(
      range, context, -std::numeric_limits<double>::max() /* minimum_value */);
  if (!value && !FeatureExpectingInteger(media_feature)) {
    value = css_parsing_utils::ConsumeNumber(
        range, context, CSSPrimitiveValue::ValueRange::kAll);
  }
  if (!value) {
    value = css_parsing_utils::ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kAll);
  }
  if (!value) {
    value = css_parsing_utils::ConsumeResolution(range, context);
  }

  if (!value) {
    if (CSSIdentifierValue* ident = css_parsing_utils::ConsumeIdent(range)) {
      CSSValueID ident_id = ident->GetValueID();
      if (!FeatureWithValidIdent(media_feature, ident_id)) {
        return absl::nullopt;
      }
      return MediaQueryExpValue(ident_id);
    }
    return absl::nullopt;
  }

  // Now we have |value| as a number, length or resolution
  // Create value for media query expression that must have 1 or more values.
  if (FeatureWithAspectRatio(media_feature)) {
    if (value->GetDoubleValue() < 0) {
      return absl::nullopt;
    }
    if (!css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
      return MediaQueryExpValue(value->GetDoubleValue(), 1);
    }
    CSSPrimitiveValue* denominator = css_parsing_utils::ConsumeNumber(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!denominator) {
      return absl::nullopt;
    }
    if (value->GetDoubleValue() == 0 && denominator->GetDoubleValue() == 0) {
      return MediaQueryExpValue(1, 0);
    }
    return MediaQueryExpValue(value->GetDoubleValue(),
                              denominator->GetDoubleValue());
  }

  if (FeatureWithValidDensity(media_feature, value)) {
    DCHECK(value->IsResolution());

    if (const auto* math_function = DynamicTo<CSSMathFunctionValue>(value)) {
      return MediaQueryExpValue(*math_function);
    }

    const auto* numeric_literal = To<CSSNumericLiteralValue>(value);
    return MediaQueryExpValue(numeric_literal->DoubleValue(),
                              numeric_literal->GetType());
  }

  if (FeatureWithInteger(media_feature, value) ||
      FeatureWithNumber(media_feature, value) ||
      FeatureWithZeroOrOne(media_feature, value)) {
    return MediaQueryExpValue(value->GetDoubleValue(),
                              CSSPrimitiveValue::UnitType::kNumber);
  }

  if (FeatureWithValidLength(media_feature, value)) {
    if (value->IsNumber()) {
      return MediaQueryExpValue(value->GetDoubleValue(),
                                CSSPrimitiveValue::UnitType::kNumber);
    }

    DCHECK(value->IsLength());
    if (const auto* numeric_literal =
            DynamicTo<CSSNumericLiteralValue>(value)) {
      return MediaQueryExpValue(numeric_literal->GetDoubleValue(),
                                numeric_literal->GetType());
    }

    return MediaQueryExpValue(*value);
  }

  return absl::nullopt;
}

namespace {

const char* MediaQueryOperatorToString(MediaQueryOperator op) {
  switch (op) {
    case MediaQueryOperator::kNone:
      return "";
    case MediaQueryOperator::kEq:
      return "=";
    case MediaQueryOperator::kLt:
      return "<";
    case MediaQueryOperator::kLe:
      return "<=";
    case MediaQueryOperator::kGt:
      return ">";
    case MediaQueryOperator::kGe:
      return ">=";
  }

  NOTREACHED();
  return "";
}

}  // namespace

MediaQueryExp MediaQueryExp::Create(const String& media_feature,
                                    const MediaQueryExpBounds& bounds) {
  return MediaQueryExp(media_feature, bounds);
}

MediaQueryExp::~MediaQueryExp() = default;

void MediaQueryExp::Trace(Visitor* visitor) const {
  visitor->Trace(bounds_);
}

bool MediaQueryExp::operator==(const MediaQueryExp& other) const {
  return (other.media_feature_ == media_feature_) && (bounds_ == other.bounds_);
}

String MediaQueryExp::Serialize() const {
  StringBuilder result;
  // <mf-boolean> e.g. (color)
  // <mf-plain>  e.g. (width: 100px)
  if (!bounds_.IsRange()) {
    result.Append(media_feature_);
    if (bounds_.right.IsValid()) {
      result.Append(": ");
      result.Append(bounds_.right.value.CssText());
    }
  } else {
    if (bounds_.left.IsValid()) {
      result.Append(bounds_.left.value.CssText());
      result.Append(" ");
      result.Append(MediaQueryOperatorToString(bounds_.left.op));
      result.Append(" ");
    }
    result.Append(media_feature_);
    if (bounds_.right.IsValid()) {
      result.Append(" ");
      result.Append(MediaQueryOperatorToString(bounds_.right.op));
      result.Append(" ");
      result.Append(bounds_.right.value.CssText());
    }
  }

  return result.ReleaseString();
}

unsigned MediaQueryExp::GetUnitFlags() const {
  unsigned unit_flags = 0;
  if (Bounds().left.IsValid()) {
    unit_flags |= Bounds().left.value.GetUnitFlags();
  }
  if (Bounds().right.IsValid()) {
    unit_flags |= Bounds().right.value.GetUnitFlags();
  }
  return unit_flags;
}

static inline String PrintNumber(double number) {
  return Decimal::FromDouble(number).ToString();
}

String MediaQueryExpValue::CssText() const {
  StringBuilder output;
  switch (type_) {
    case Type::kInvalid:
      break;
    case Type::kNumeric:
      output.Append(PrintNumber(Value()));
      output.Append(CSSPrimitiveValue::UnitTypeToString(Unit()));
      break;
    case Type::kRatio:
      output.Append(PrintNumber(Numerator()));
      output.Append(" / ");
      output.Append(PrintNumber(Denominator()));
      break;
    case Type::kId:
      output.Append(getValueName(Id()));
      break;
    case Type::kCSSValue:
      output.Append(GetCSSValue().CssText());
      break;
  }

  return output.ReleaseString();
}

unsigned MediaQueryExpValue::GetUnitFlags() const {
  CSSPrimitiveValue::LengthTypeFlags length_type_flags;

  if (IsCSSValue()) {
    if (auto* primitive = DynamicTo<CSSPrimitiveValue>(GetCSSValue())) {
      primitive->AccumulateLengthUnitTypes(length_type_flags);
    }
  }
  if (IsNumeric() && CSSPrimitiveValue::IsLength(Unit())) {
    CSSPrimitiveValue::LengthUnitType length_unit_type;
    bool ok =
        CSSPrimitiveValue::UnitTypeToLengthUnitType(Unit(), length_unit_type);
    DCHECK(ok);
    length_type_flags.set(length_unit_type);
  }

  unsigned unit_flags = 0;

  if (length_type_flags.test(CSSPrimitiveValue::kUnitTypeFontSize) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeFontXSize) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeZeroCharacterWidth) ||
      length_type_flags.test(
          CSSPrimitiveValue::kUnitTypeIdeographicFullWidth) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeLineHeight)) {
    unit_flags |= UnitFlags::kFontRelative;
  }

  if (length_type_flags.test(CSSPrimitiveValue::kUnitTypeRootFontSize) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeRootFontXSize) ||
      length_type_flags.test(
          CSSPrimitiveValue::kUnitTypeRootFontZeroCharacterWidth) ||
      length_type_flags.test(
          CSSPrimitiveValue::kUnitTypeRootFontIdeographicFullWidth) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeRootLineHeight)) {
    unit_flags |= UnitFlags::kRootFontRelative;
  }

  if (CSSPrimitiveValue::HasDynamicViewportUnits(length_type_flags)) {
    unit_flags |= UnitFlags::kDynamicViewport;
  }

  if (CSSPrimitiveValue::HasStaticViewportUnits(length_type_flags)) {
    unit_flags |= UnitFlags::kStaticViewport;
  }

  if (length_type_flags.test(CSSPrimitiveValue::kUnitTypeContainerWidth) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeContainerHeight) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeContainerInlineSize) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeContainerBlockSize) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeContainerMin) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeContainerMax)) {
    unit_flags |= UnitFlags::kContainer;
  }

  return unit_flags;
}

String MediaQueryExpNode::Serialize() const {
  StringBuilder builder;
  SerializeTo(builder);
  return builder.ReleaseString();
}

const MediaQueryExpNode* MediaQueryExpNode::Not(
    const MediaQueryExpNode* operand) {
  if (!operand) {
    return nullptr;
  }
  return MakeGarbageCollected<MediaQueryNotExpNode>(operand);
}

const MediaQueryExpNode* MediaQueryExpNode::Nested(
    const MediaQueryExpNode* operand) {
  if (!operand) {
    return nullptr;
  }
  return MakeGarbageCollected<MediaQueryNestedExpNode>(operand);
}

const MediaQueryExpNode* MediaQueryExpNode::Function(
    const MediaQueryExpNode* operand,
    const AtomicString& name) {
  if (!operand) {
    return nullptr;
  }
  return MakeGarbageCollected<MediaQueryFunctionExpNode>(operand, name);
}

const MediaQueryExpNode* MediaQueryExpNode::And(
    const MediaQueryExpNode* left,
    const MediaQueryExpNode* right) {
  if (!left || !right) {
    return nullptr;
  }
  return MakeGarbageCollected<MediaQueryAndExpNode>(left, right);
}

const MediaQueryExpNode* MediaQueryExpNode::Or(const MediaQueryExpNode* left,
                                               const MediaQueryExpNode* right) {
  if (!left || !right) {
    return nullptr;
  }
  return MakeGarbageCollected<MediaQueryOrExpNode>(left, right);
}

bool MediaQueryFeatureExpNode::IsViewportDependent() const {
  return exp_.IsViewportDependent();
}

bool MediaQueryFeatureExpNode::IsDeviceDependent() const {
  return exp_.IsDeviceDependent();
}

unsigned MediaQueryFeatureExpNode::GetUnitFlags() const {
  return exp_.GetUnitFlags();
}

bool MediaQueryFeatureExpNode::IsWidthDependent() const {
  return exp_.IsWidthDependent();
}

bool MediaQueryFeatureExpNode::IsHeightDependent() const {
  return exp_.IsHeightDependent();
}

bool MediaQueryFeatureExpNode::IsInlineSizeDependent() const {
  return exp_.IsInlineSizeDependent();
}

bool MediaQueryFeatureExpNode::IsBlockSizeDependent() const {
  return exp_.IsBlockSizeDependent();
}

void MediaQueryFeatureExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append(exp_.Serialize());
}

void MediaQueryFeatureExpNode::CollectExpressions(
    HeapVector<MediaQueryExp>& result) const {
  result.push_back(exp_);
}

MediaQueryExpNode::FeatureFlags MediaQueryFeatureExpNode::CollectFeatureFlags()
    const {
  FeatureFlags flags = 0;

  if (exp_.IsWidthDependent()) {
    flags |= kFeatureWidth;
  }
  if (exp_.IsHeightDependent()) {
    flags |= kFeatureHeight;
  }
  if (exp_.IsInlineSizeDependent()) {
    flags |= kFeatureInlineSize;
  }
  if (exp_.IsBlockSizeDependent()) {
    flags |= kFeatureBlockSize;
  }

  return flags;
}

void MediaQueryFeatureExpNode::Trace(Visitor* visitor) const {
  visitor->Trace(exp_);
  MediaQueryExpNode::Trace(visitor);
}

void MediaQueryUnaryExpNode::Trace(Visitor* visitor) const {
  visitor->Trace(operand_);
  MediaQueryExpNode::Trace(visitor);
}

void MediaQueryUnaryExpNode::CollectExpressions(
    HeapVector<MediaQueryExp>& result) const {
  operand_->CollectExpressions(result);
}

MediaQueryExpNode::FeatureFlags MediaQueryUnaryExpNode::CollectFeatureFlags()
    const {
  return operand_->CollectFeatureFlags();
}

void MediaQueryNestedExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append("(");
  Operand().SerializeTo(builder);
  builder.Append(")");
}

void MediaQueryFunctionExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append(name_);
  builder.Append("(");
  Operand().SerializeTo(builder);
  builder.Append(")");
}

MediaQueryExpNode::FeatureFlags MediaQueryFunctionExpNode::CollectFeatureFlags()
    const {
  FeatureFlags flags = MediaQueryUnaryExpNode::CollectFeatureFlags();
  if (name_ == AtomicString("style")) {
    flags |= kFeatureStyle;
  }
  return flags;
}

void MediaQueryNotExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append("not ");
  Operand().SerializeTo(builder);
}

void MediaQueryCompoundExpNode::Trace(Visitor* visitor) const {
  visitor->Trace(left_);
  visitor->Trace(right_);
  MediaQueryExpNode::Trace(visitor);
}

void MediaQueryCompoundExpNode::CollectExpressions(
    HeapVector<MediaQueryExp>& result) const {
  left_->CollectExpressions(result);
  right_->CollectExpressions(result);
}

MediaQueryExpNode::FeatureFlags MediaQueryCompoundExpNode::CollectFeatureFlags()
    const {
  return left_->CollectFeatureFlags() | right_->CollectFeatureFlags();
}

void MediaQueryAndExpNode::SerializeTo(StringBuilder& builder) const {
  Left().SerializeTo(builder);
  builder.Append(" and ");
  Right().SerializeTo(builder);
}

void MediaQueryOrExpNode::SerializeTo(StringBuilder& builder) const {
  Left().SerializeTo(builder);
  builder.Append(" or ");
  Right().SerializeTo(builder);
}

void MediaQueryUnknownExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append(string_);
}

void MediaQueryUnknownExpNode::CollectExpressions(
    HeapVector<MediaQueryExp>&) const {}

MediaQueryExpNode::FeatureFlags MediaQueryUnknownExpNode::CollectFeatureFlags()
    const {
  return kFeatureUnknown;
}

}  // namespace blink

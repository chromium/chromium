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
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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

  if (media_feature == media_feature_names::kColorGamutMediaFeature) {
    return ident == CSSValueID::kSRGB || ident == CSSValueID::kP3 ||
           ident == CSSValueID::kRec2020;
  }

  if (media_feature == media_feature_names::kPrefersColorSchemeMediaFeature)
    return ident == CSSValueID::kDark || ident == CSSValueID::kLight;

  if (RuntimeEnabledFeatures::PrefersContrastEnabled()) {
    if (media_feature == media_feature_names::kPrefersContrastMediaFeature) {
      return ident == CSSValueID::kNoPreference || ident == CSSValueID::kMore ||
             ident == CSSValueID::kLess || ident == CSSValueID::kCustom;
    }
  }

  if (media_feature == media_feature_names::kPrefersReducedMotionMediaFeature)
    return ident == CSSValueID::kNoPreference || ident == CSSValueID::kReduce;

  if (RuntimeEnabledFeatures::CSSDynamicRangeMediaQueriesEnabled()) {
    if (media_feature == media_feature_names::kDynamicRangeMediaFeature)
      return ident == CSSValueID::kStandard || ident == CSSValueID::kHigh;
  }

  if (RuntimeEnabledFeatures::CSSVideoDynamicRangeMediaQueriesEnabled()) {
    if (media_feature == media_feature_names::kVideoDynamicRangeMediaFeature)
      return ident == CSSValueID::kStandard || ident == CSSValueID::kHigh;
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
      return ident == CSSValueID::kContinuous || ident == CSSValueID::kFolded ||
             ident == CSSValueID::kFoldedOver;
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
  if (!value->IsResolution() || value->GetDoubleValue() <= 0)
    return false;

  return media_feature == media_feature_names::kResolutionMediaFeature ||
         media_feature == media_feature_names::kMinResolutionMediaFeature ||
         media_feature == media_feature_names::kMaxResolutionMediaFeature;
}

static inline bool FeatureExpectingPositiveInteger(
    const String& media_feature) {
  if (media_feature == media_feature_names::kColorMediaFeature ||
      media_feature == media_feature_names::kMaxColorMediaFeature ||
      media_feature == media_feature_names::kMinColorMediaFeature ||
      media_feature == media_feature_names::kColorIndexMediaFeature ||
      media_feature == media_feature_names::kMaxColorIndexMediaFeature ||
      media_feature == media_feature_names::kMinColorIndexMediaFeature ||
      media_feature == media_feature_names::kMonochromeMediaFeature ||
      media_feature == media_feature_names::kMaxMonochromeMediaFeature ||
      media_feature == media_feature_names::kMinMonochromeMediaFeature ||
      media_feature == media_feature_names::kImmersiveMediaFeature) {
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

static inline bool FeatureWithoutValue(
    const String& media_feature,
    const ExecutionContext* execution_context) {
  // Media features that are prefixed by min/max cannot be used without a value.
  return media_feature == media_feature_names::kMonochromeMediaFeature ||
         media_feature == media_feature_names::kColorMediaFeature ||
         media_feature == media_feature_names::kColorIndexMediaFeature ||
         media_feature == media_feature_names::kGridMediaFeature ||
         media_feature == media_feature_names::kHeightMediaFeature ||
         media_feature == media_feature_names::kWidthMediaFeature ||
         media_feature == media_feature_names::kBlockSizeMediaFeature ||
         media_feature == media_feature_names::kInlineSizeMediaFeature ||
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
         media_feature == media_feature_names::kColorGamutMediaFeature ||
         media_feature == media_feature_names::kImmersiveMediaFeature ||
         media_feature ==
             media_feature_names::kPrefersColorSchemeMediaFeature ||
         (media_feature == media_feature_names::kPrefersContrastMediaFeature &&
          RuntimeEnabledFeatures::PrefersContrastEnabled()) ||
         media_feature ==
             media_feature_names::kPrefersReducedMotionMediaFeature ||
         (media_feature ==
              media_feature_names::kPrefersReducedDataMediaFeature &&
          RuntimeEnabledFeatures::PrefersReducedDataEnabled()) ||
         (media_feature == media_feature_names::kForcedColorsMediaFeature &&
          RuntimeEnabledFeatures::ForcedColorsEnabled()) ||
         (media_feature ==
              media_feature_names::kNavigationControlsMediaFeature &&
          RuntimeEnabledFeatures::MediaQueryNavigationControlsEnabled()) ||
         (media_feature == media_feature_names::kOriginTrialTestMediaFeature &&
          RuntimeEnabledFeatures::OriginTrialsSampleAPIEnabled(
              execution_context)) ||
         (media_feature ==
              media_feature_names::kHorizontalViewportSegmentsMediaFeature &&
          RuntimeEnabledFeatures::CSSFoldablesEnabled()) ||
         (media_feature ==
              media_feature_names::kVerticalViewportSegmentsMediaFeature &&
          RuntimeEnabledFeatures::CSSFoldablesEnabled()) ||
         (media_feature == media_feature_names::kDevicePostureMediaFeature &&
          RuntimeEnabledFeatures::DevicePostureEnabled());
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
                                    const CSSParserContext& context,
                                    const ExecutionContext* execution_context) {
  String lower_media_feature =
      AttemptStaticStringCreation(media_feature.LowerASCII());
  if (auto value = MediaQueryExpValue::Consume(lower_media_feature, range,
                                               context, execution_context)) {
    return MediaQueryExp(lower_media_feature, *value);
  }
  return Invalid();
}

absl::optional<MediaQueryExpValue> MediaQueryExpValue::Consume(
    const String& lower_media_feature,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const ExecutionContext* execution_context) {
  DCHECK_EQ(lower_media_feature, lower_media_feature.LowerASCII());

  CSSParserContext::ParserModeOverridingScope scope(context, kHTMLStandardMode);

  CSSPrimitiveValue* value =
      css_parsing_utils::ConsumeInteger(range, context, 0);
  if (!value && !FeatureExpectingPositiveInteger(lower_media_feature) &&
      !FeatureWithAspectRatio(lower_media_feature)) {
    value = css_parsing_utils::ConsumeNumber(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  }
  if (!value) {
    value = css_parsing_utils::ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
  }
  if (!value)
    value = css_parsing_utils::ConsumeResolution(range);

  if (!value) {
    if (CSSIdentifierValue* ident = css_parsing_utils::ConsumeIdent(range)) {
      CSSValueID ident_id = ident->GetValueID();
      if (!FeatureWithValidIdent(lower_media_feature, ident_id))
        return absl::nullopt;
      return MediaQueryExpValue(ident_id);
    }
    if (FeatureWithoutValue(lower_media_feature, execution_context)) {
      // Valid, creates a MediaQueryExp with an 'invalid' MediaQueryExpValue
      return MediaQueryExpValue();
    }
    return absl::nullopt;
  }

  // Now we have |value| as a number, length or resolution
  // Create value for media query expression that must have 1 or more values.
  if (FeatureWithAspectRatio(lower_media_feature)) {
    if (!value->IsInteger() || value->GetDoubleValue() == 0)
      return absl::nullopt;
    if (!css_parsing_utils::ConsumeSlashIncludingWhitespace(range))
      return absl::nullopt;
    CSSPrimitiveValue* denominator =
        css_parsing_utils::ConsumePositiveInteger(range, context);
    if (!denominator)
      return absl::nullopt;

    return MediaQueryExpValue(ClampTo<unsigned>(value->GetDoubleValue()),
                              ClampTo<unsigned>(denominator->GetDoubleValue()));
  }

  if (FeatureWithValidDensity(lower_media_feature, value)) {
    // TODO(crbug.com/983613): Support resolution in math functions.
    DCHECK(value->IsNumericLiteralValue());
    const auto* numeric_literal = To<CSSNumericLiteralValue>(value);
    return MediaQueryExpValue(numeric_literal->DoubleValue(),
                              numeric_literal->GetType());
  }

  if (FeatureWithPositiveInteger(lower_media_feature, value) ||
      FeatureWithPositiveNumber(lower_media_feature, value) ||
      FeatureWithZeroOrOne(lower_media_feature, value)) {
    return MediaQueryExpValue(value->GetDoubleValue(),
                              CSSPrimitiveValue::UnitType::kNumber);
  }

  if (FeatureWithValidPositiveLength(lower_media_feature, value)) {
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

    const auto* math_value = To<CSSMathFunctionValue>(value);
    CSSPrimitiveValue::UnitType expression_unit =
        math_value->ExpressionNode()->ResolvedUnitType();
    if (expression_unit == CSSPrimitiveValue::UnitType::kUnknown) {
      // TODO(crbug.com/982542): Support math expressions involving type
      // conversions properly. For example, calc(10px + 1em).
      return absl::nullopt;
    }
    return MediaQueryExpValue(math_value->DoubleValue(), expression_unit);
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

bool MediaQueryExp::operator==(const MediaQueryExp& other) const {
  return (other.media_feature_ == media_feature_) && (bounds_ == other.bounds_);
}

String MediaQueryExp::Serialize() const {
  String name = media_feature_.LowerASCII();

  StringBuilder result;
  // <mf-boolean> e.g. (color)
  // <mf-plain>  e.g. (width: 100px)
  if (!bounds_.IsRange()) {
    result.Append(name);
    if (ExpValue().IsValid()) {
      result.Append(": ");
      result.Append(ExpValue().CssText());
    }
  } else {
    if (bounds_.left.IsValid()) {
      result.Append(bounds_.left.value.CssText());
      result.Append(" ");
      result.Append(MediaQueryOperatorToString(bounds_.left.op));
      result.Append(" ");
    }
    result.Append(name);
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
  if (Bounds().left.IsValid())
    unit_flags |= Bounds().left.value.GetUnitFlags();
  if (Bounds().right.IsValid())
    unit_flags |= Bounds().right.value.GetUnitFlags();
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
  }

  return output.ReleaseString();
}

MediaQueryExpValue::UnitFlags MediaQueryExpValue::GetUnitFlags() const {
  if (!IsNumeric())
    return UnitFlags::kNone;
  switch (Unit()) {
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kChs:
      return UnitFlags::kFontRelative;
    case CSSPrimitiveValue::UnitType::kRems:
      return UnitFlags::kRootFontRelative;
    case CSSPrimitiveValue::UnitType::kDynamicViewportWidth:
    case CSSPrimitiveValue::UnitType::kDynamicViewportHeight:
    case CSSPrimitiveValue::UnitType::kDynamicViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kDynamicViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kDynamicViewportMin:
    case CSSPrimitiveValue::UnitType::kDynamicViewportMax:
      return UnitFlags::kDynamicViewport;
    default:
      return UnitFlags::kNone;
  }
}

String MediaQueryExpNode::Serialize() const {
  StringBuilder builder;
  SerializeTo(builder);
  return builder.ReleaseString();
}

std::unique_ptr<MediaQueryExpNode> MediaQueryExpNode::Not(
    std::unique_ptr<MediaQueryExpNode> operand) {
  if (!operand)
    return nullptr;
  return std::make_unique<MediaQueryNotExpNode>(std::move(operand));
}

std::unique_ptr<MediaQueryExpNode> MediaQueryExpNode::Nested(
    std::unique_ptr<MediaQueryExpNode> operand) {
  if (!operand)
    return nullptr;
  return std::make_unique<MediaQueryNestedExpNode>(std::move(operand));
}

std::unique_ptr<MediaQueryExpNode> MediaQueryExpNode::Function(
    std::unique_ptr<MediaQueryExpNode> operand,
    const AtomicString& name) {
  if (!operand)
    return nullptr;
  return std::make_unique<MediaQueryFunctionExpNode>(std::move(operand), name);
}

std::unique_ptr<MediaQueryExpNode> MediaQueryExpNode::And(
    std::unique_ptr<MediaQueryExpNode> left,
    std::unique_ptr<MediaQueryExpNode> right) {
  if (!left || !right)
    return nullptr;
  return std::make_unique<MediaQueryAndExpNode>(std::move(left),
                                                std::move(right));
}

std::unique_ptr<MediaQueryExpNode> MediaQueryExpNode::Or(
    std::unique_ptr<MediaQueryExpNode> left,
    std::unique_ptr<MediaQueryExpNode> right) {
  if (!left || !right)
    return nullptr;
  return std::make_unique<MediaQueryOrExpNode>(std::move(left),
                                               std::move(right));
}

void MediaQueryFeatureExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append(exp_.Serialize());
}

void MediaQueryFeatureExpNode::CollectExpressions(
    Vector<MediaQueryExp>& result) const {
  result.push_back(exp_);
}

MediaQueryExpNode::FeatureFlags MediaQueryFeatureExpNode::CollectFeatureFlags()
    const {
  FeatureFlags flags = 0;

  if (exp_.IsWidthDependent())
    flags |= kFeatureWidth;
  if (exp_.IsHeightDependent())
    flags |= kFeatureHeight;
  if (exp_.IsInlineSizeDependent())
    flags |= kFeatureInlineSize;
  if (exp_.IsBlockSizeDependent())
    flags |= kFeatureBlockSize;

  return flags;
}

std::unique_ptr<MediaQueryExpNode> MediaQueryFeatureExpNode::Copy() const {
  return std::make_unique<MediaQueryFeatureExpNode>(exp_);
}

void MediaQueryUnaryExpNode::CollectExpressions(
    Vector<MediaQueryExp>& result) const {
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

std::unique_ptr<MediaQueryExpNode> MediaQueryNestedExpNode::Copy() const {
  return std::make_unique<MediaQueryNestedExpNode>(Operand().Copy());
}

void MediaQueryFunctionExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append(name_);
  builder.Append("(");
  Operand().SerializeTo(builder);
  builder.Append(")");
}

std::unique_ptr<MediaQueryExpNode> MediaQueryFunctionExpNode::Copy() const {
  return std::make_unique<MediaQueryFunctionExpNode>(Operand().Copy(), name_);
}

void MediaQueryNotExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append("not ");
  Operand().SerializeTo(builder);
}

std::unique_ptr<MediaQueryExpNode> MediaQueryNotExpNode::Copy() const {
  return std::make_unique<MediaQueryNotExpNode>(Operand().Copy());
}

void MediaQueryCompoundExpNode::CollectExpressions(
    Vector<MediaQueryExp>& result) const {
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

std::unique_ptr<MediaQueryExpNode> MediaQueryAndExpNode::Copy() const {
  return std::make_unique<MediaQueryAndExpNode>(Left().Copy(), Right().Copy());
}

void MediaQueryOrExpNode::SerializeTo(StringBuilder& builder) const {
  Left().SerializeTo(builder);
  builder.Append(" or ");
  Right().SerializeTo(builder);
}

std::unique_ptr<MediaQueryExpNode> MediaQueryOrExpNode::Copy() const {
  return std::make_unique<MediaQueryOrExpNode>(Left().Copy(), Right().Copy());
}

void MediaQueryUnknownExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append(string_);
}

void MediaQueryUnknownExpNode::CollectExpressions(
    Vector<MediaQueryExp>&) const {}

MediaQueryExpNode::FeatureFlags MediaQueryUnknownExpNode::CollectFeatureFlags()
    const {
  return kFeatureUnknown;
}

std::unique_ptr<MediaQueryExpNode> MediaQueryUnknownExpNode::Copy() const {
  return std::make_unique<MediaQueryUnknownExpNode>(string_);
}

}  // namespace blink

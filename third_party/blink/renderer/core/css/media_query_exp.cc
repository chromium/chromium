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
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
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
                                         CSSValueID ident,
                                         const CSSParserContext& context) {
  if (media_feature == media_feature_names::kDisplayModeMediaFeature) {
    return ident == CSSValueID::kFullscreen ||
           ident == CSSValueID::kBorderless ||
           ident == CSSValueID::kStandalone ||
           ident == CSSValueID::kMinimalUi ||
           ident == CSSValueID::kWindowControlsOverlay ||
           ident == CSSValueID::kBrowser || ident == CSSValueID::kTabbed ||
           ident == CSSValueID::kPictureInPicture;
  }

  if (RuntimeEnabledFeatures::DesktopPWAsAdditionalWindowingControlsEnabled() &&
      media_feature == media_feature_names::kDisplayStateMediaFeature) {
    return ident == CSSValueID::kFullscreen || ident == CSSValueID::kNormal ||
           ident == CSSValueID::kMinimized || ident == CSSValueID::kMaximized;
  }

  if (RuntimeEnabledFeatures::DesktopPWAsAdditionalWindowingControlsEnabled() &&
      media_feature == media_feature_names::kResizableMediaFeature) {
    return ident == CSSValueID::kTrue || ident == CSSValueID::kFalse;
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

  if (RuntimeEnabledFeatures::InvertedColorsEnabled() &&
      media_feature == media_feature_names::kInvertedColorsMediaFeature) {
    return ident == CSSValueID::kInverted || ident == CSSValueID::kNone;
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

  if (media_feature ==
      media_feature_names::kPrefersReducedTransparencyMediaFeature) {
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

  if (RuntimeEnabledFeatures::DevicePostureEnabled(
          context.GetExecutionContext())) {
    if (media_feature == media_feature_names::kDevicePostureMediaFeature) {
      return ident == CSSValueID::kContinuous || ident == CSSValueID::kFolded;
    }
  }

  if (media_feature == media_feature_names::kOverflowInlineMediaFeature) {
    return ident == CSSValueID::kNone || ident == CSSValueID::kScroll;
  }

  if (media_feature == media_feature_names::kOverflowBlockMediaFeature) {
    return ident == CSSValueID::kNone || ident == CSSValueID::kScroll ||
           ident == CSSValueID::kPaged;
  }

  if (media_feature == media_feature_names::kUpdateMediaFeature) {
    return ident == CSSValueID::kNone || ident == CSSValueID::kFast ||
           ident == CSSValueID::kSlow;
  }

  if (media_feature == media_feature_names::kStuckMediaFeature) {
    switch (ident) {
      case CSSValueID::kNone:
      case CSSValueID::kTop:
      case CSSValueID::kLeft:
      case CSSValueID::kBottom:
      case CSSValueID::kRight:
      case CSSValueID::kBlockStart:
      case CSSValueID::kBlockEnd:
      case CSSValueID::kInlineStart:
      case CSSValueID::kInlineEnd:
        return true;
      default:
        return false;
    }
  }

  if (media_feature == media_feature_names::kScriptingMediaFeature) {
    return ident == CSSValueID::kEnabled || ident == CSSValueID::kInitialOnly ||
           ident == CSSValueID::kNone;
  }

  if (media_feature == media_feature_names::kSnappedMediaFeature) {
    switch (ident) {
      case CSSValueID::kNone:
      case CSSValueID::kBlock:
      case CSSValueID::kInline:
      case CSSValueID::kX:
      case CSSValueID::kY:
      case CSSValueID::kBoth:
        return true;
      default:
        return false;
    }
  }

  if (media_feature == media_feature_names::kScrollableMediaFeature) {
    switch (ident) {
      case CSSValueID::kNone:
      case CSSValueID::kTop:
      case CSSValueID::kLeft:
      case CSSValueID::kBottom:
      case CSSValueID::kRight:
      case CSSValueID::kBlockStart:
      case CSSValueID::kBlockEnd:
      case CSSValueID::kInlineStart:
      case CSSValueID::kInlineEnd:
      case CSSValueID::kBlock:
      case CSSValueID::kInline:
      case CSSValueID::kX:
      case CSSValueID::kY:
        return true;
      default:
        return false;
    }
  }

  if (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled()) {
    if (media_feature == media_feature_names::kScrolledMediaFeature) {
      switch (ident) {
        case CSSValueID::kNone:
        case CSSValueID::kTop:
        case CSSValueID::kLeft:
        case CSSValueID::kBottom:
        case CSSValueID::kRight:
        case CSSValueID::kBlockStart:
        case CSSValueID::kBlockEnd:
        case CSSValueID::kInlineStart:
        case CSSValueID::kInlineEnd:
        case CSSValueID::kBlock:
        case CSSValueID::kInline:
        case CSSValueID::kX:
        case CSSValueID::kY:
          return true;
        default:
          return false;
      }
    }
  }

  if (RuntimeEnabledFeatures::CSSFallbackContainerQueriesEnabled() &&
      media_feature == media_feature_names::kFallbackMediaFeature) {
    return ident == CSSValueID::kNone;
  }

  return false;
}

static inline bool FeatureWithValidLength(const String& media_feature,
                                          const CSSPrimitiveValue* value) {
  if (!(value->IsLength() ||
        (value->IsNumber() && value->GetValueIfKnown() == 0.0))) {
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
  // NOTE: The allowed range of <resolution> values always excludes negative
  // values, in addition to any explicit ranges that might be specified.
  // https://drafts.csswg.org/css-values/#resolution
  if (!value->IsResolution() || (value->GetValueIfKnown().has_value() &&
                                 *value->GetValueIfKnown() < 0.0)) {
    return false;
  }

  return media_feature == media_feature_names::kResolutionMediaFeature ||
         media_feature == media_feature_names::kMinResolutionMediaFeature ||
         media_feature == media_feature_names::kMaxResolutionMediaFeature;
}

static inline bool FeatureExpectingInteger(const String& media_feature,
                                           const CSSParserContext& context) {
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

  if (RuntimeEnabledFeatures::ViewportSegmentsEnabled(
          context.GetExecutionContext())) {
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
                                      const CSSPrimitiveValue* value,
                                      const CSSParserContext& context) {
  if (!value->IsInteger()) {
    return false;
  }
  return FeatureExpectingInteger(media_feature, context);
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
      (value->GetValueIfKnown().has_value() &&
       *value->GetValueIfKnown() != 1.0 && *value->GetValueIfKnown() != 0.0)) {
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
  if (!HasMediaFeature()) {
    return false;
  }
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
  if (!HasMediaFeature()) {
    return false;
  }
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
  if (!HasMediaFeature()) {
    return false;
  }
  return media_feature_ == media_feature_names::kWidthMediaFeature ||
         media_feature_ == media_feature_names::kMinWidthMediaFeature ||
         media_feature_ == media_feature_names::kMaxWidthMediaFeature ||
         media_feature_ == media_feature_names::kAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMinAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMaxAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kOrientationMediaFeature;
}

bool MediaQueryExp::IsHeightDependent() const {
  if (!HasMediaFeature()) {
    return false;
  }
  return media_feature_ == media_feature_names::kHeightMediaFeature ||
         media_feature_ == media_feature_names::kMinHeightMediaFeature ||
         media_feature_ == media_feature_names::kMaxHeightMediaFeature ||
         media_feature_ == media_feature_names::kAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMinAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kMaxAspectRatioMediaFeature ||
         media_feature_ == media_feature_names::kOrientationMediaFeature;
}

bool MediaQueryExp::IsInlineSizeDependent() const {
  if (!HasMediaFeature()) {
    return false;
  }
  return media_feature_ == media_feature_names::kInlineSizeMediaFeature ||
         media_feature_ == media_feature_names::kMinInlineSizeMediaFeature ||
         media_feature_ == media_feature_names::kMaxInlineSizeMediaFeature;
}

bool MediaQueryExp::IsBlockSizeDependent() const {
  if (!HasMediaFeature()) {
    return false;
  }
  return media_feature_ == media_feature_names::kBlockSizeMediaFeature ||
         media_feature_ == media_feature_names::kMinBlockSizeMediaFeature ||
         media_feature_ == media_feature_names::kMaxBlockSizeMediaFeature;
}

MediaQueryExp::MediaQueryExp(const MediaQueryExp& other)
    : type_(other.type_),
      media_feature_(other.media_feature_),
      reference_value_(other.reference_value_),
      bounds_(other.bounds_) {}

MediaQueryExp::MediaQueryExp(const String& media_feature,
                             const MediaQueryExpValue& value)
    : MediaQueryExp(media_feature,
                    MediaQueryExpBounds(MediaQueryExpComparison(value)),
                    Type::kMediaFeature) {}

MediaQueryExp::MediaQueryExp(const String& media_feature,
                             const MediaQueryExpBounds& bounds,
                             Type type)
    : type_(type), media_feature_(media_feature), bounds_(bounds) {}

MediaQueryExp::MediaQueryExp(const CSSUnparsedDeclarationValue& reference_value,
                             const MediaQueryExpBounds& bounds)
    : type_(Type::kStyleRange),
      reference_value_(reference_value),
      bounds_(bounds) {}

MediaQueryExp MediaQueryExp::Create(const AtomicString& media_feature,
                                    CSSParserTokenStream& stream,
                                    const CSSParserContext& context,
                                    bool supports_element_dependent) {
  std::optional<MediaQueryExpValue> value = MediaQueryExpValue::Consume(
      media_feature, stream, context, supports_element_dependent);
  if (value.has_value()) {
    return MediaQueryExp(media_feature, value.value());
  }
  return Invalid();
}

std::optional<MediaQueryExpValue> MediaQueryExpValue::Consume(
    const String& media_feature,
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    bool supports_element_dependent) {
  CSSParserContext::ParserModeOverridingScope scope(context, kHTMLStandardMode);

  if (CSSVariableParser::IsValidVariableName(media_feature)) {
    // Parse style queries for container queries, e.g. “style(--foo: bar)”.
    // (These look like a declaration, but are really a test as part of
    // a media query expression.) !important, if present, is stripped
    // and ignored.
    if (const CSSValue* value =
            CSSVariableParser::ParseDeclarationIncludingCSSWide(stream, false,
                                                                context)) {
      while (!stream.AtEnd()) {
        stream.Consume();
      }
      return MediaQueryExpValue(*value);
    }
    return std::nullopt;
  }

  DCHECK_EQ(media_feature, media_feature.LowerASCII())
      << "Under the assumption that custom properties in style() container "
         "queries are currently the only case sensitive features";

  if (media_feature == media_feature_names::kFallbackMediaFeature) {
    if (CSSValue* fallback_value =
            css_parsing_utils::ConsumeAnchoredFallbackQueryValue(stream,
                                                                 context)) {
      return MediaQueryExpValue(*fallback_value);
    }
  }

  CSSPrimitiveValue* value = css_parsing_utils::ConsumeInteger(
      stream, context, -std::numeric_limits<double>::max() /* minimum_value */);
  if (!value && !FeatureExpectingInteger(media_feature, context)) {
    value = css_parsing_utils::ConsumeNumber(
        stream, context, CSSPrimitiveValue::ValueRange::kAll);
  }
  if (!value) {
    value = css_parsing_utils::ConsumeLength(
        stream, context, CSSPrimitiveValue::ValueRange::kAll);
  }
  if (!value) {
    value = css_parsing_utils::ConsumeResolution(stream, context);
  }

  if (!value) {
    if (CSSIdentifierValue* ident = css_parsing_utils::ConsumeIdent(stream)) {
      CSSValueID ident_id = ident->GetValueID();
      if (!FeatureWithValidIdent(media_feature, ident_id, context)) {
        return std::nullopt;
      }
      return MediaQueryExpValue(ident_id);
    }
    return std::nullopt;
  }

  if (!supports_element_dependent && value->IsElementDependent()) {
    return std::nullopt;
  }

  // Now we have |value| as a number, length or resolution
  // Create value for media query expression that must have 1 or more values.
  if (FeatureWithAspectRatio(media_feature)) {
    if (value->GetValueIfKnown().has_value() &&
        *value->GetValueIfKnown() < 0.0) {
      return std::nullopt;
    }
    if (!css_parsing_utils::ConsumeSlashIncludingWhitespace(stream)) {
      return MediaQueryExpValue(*value,
                                *CSSNumericLiteralValue::Create(
                                    1, CSSPrimitiveValue::UnitType::kNumber));
    }
    CSSPrimitiveValue* denominator = css_parsing_utils::ConsumeNumber(
        stream, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    if (!denominator) {
      return std::nullopt;
    }
    if (value->GetValueIfKnown() == 0.0 &&
        denominator->GetValueIfKnown() == 0.0) {
      return MediaQueryExpValue(*CSSNumericLiteralValue::Create(
                                    1, CSSPrimitiveValue::UnitType::kNumber),
                                *CSSNumericLiteralValue::Create(
                                    0, CSSPrimitiveValue::UnitType::kNumber));
    }
    return MediaQueryExpValue(*value, *denominator);
  }

  if (FeatureWithInteger(media_feature, value, context) ||
      FeatureWithNumber(media_feature, value) ||
      FeatureWithZeroOrOne(media_feature, value) ||
      FeatureWithValidLength(media_feature, value) ||
      FeatureWithValidDensity(media_feature, value)) {
    return MediaQueryExpValue(*value);
  }

  return std::nullopt;
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
}

}  // namespace

MediaQueryExp MediaQueryExp::Create(const AtomicString& media_feature,
                                    const MediaQueryExpBounds& bounds) {
  return MediaQueryExp(media_feature, bounds, Type::kMediaFeature);
}

MediaQueryExp MediaQueryExp::Create(const AtomicString& custom_media) {
  return MediaQueryExp(custom_media, MediaQueryExpBounds(), Type::kCustomMedia);
}

MediaQueryExp MediaQueryExp::Create(const MediaQueryExpValue& reference_value,
                                    const MediaQueryExpBounds& bounds) {
  DCHECK(RuntimeEnabledFeatures::CSSContainerStyleQueriesRangeEnabled());
  const CSSUnparsedDeclarationValue* value =
      DynamicTo<CSSUnparsedDeclarationValue>(reference_value.GetCSSValue());
  DCHECK(value);
  return MediaQueryExp(*value, bounds);
}

MediaQueryExp::~MediaQueryExp() = default;

void MediaQueryExp::Trace(Visitor* visitor) const {
  visitor->Trace(reference_value_);
  visitor->Trace(bounds_);
}

bool MediaQueryExp::operator==(const MediaQueryExp& other) const {
  return (other.type_ == type_) && (other.media_feature_ == media_feature_) &&
         (other.reference_value_ == reference_value_) &&
         (bounds_ == other.bounds_);
}

String MediaQueryExp::Serialize() const {
  StringBuilder result;
  // <mf-boolean> e.g. (color)
  // <mf-plain>  e.g. (width: 100px)
  if (!bounds_.IsRange()) {
    if (HasMediaFeature() || IsCustomMedia()) {
      result.Append(media_feature_);
    } else {
      result.Append(reference_value_->CssText());
    }
    if (bounds_.right.IsValid()) {
      DCHECK(!IsCustomMedia());
      result.Append(": ");
      result.Append(bounds_.right.value.CssText());
    }
  } else {
    DCHECK(!IsCustomMedia());
    if (bounds_.left.IsValid()) {
      result.Append(bounds_.left.value.CssText());
      result.Append(" ");
      result.Append(MediaQueryOperatorToString(bounds_.left.op));
      result.Append(" ");
    }
    if (HasMediaFeature()) {
      result.Append(media_feature_);
    } else {
      result.Append(reference_value_->CssText());
    }
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

String MediaQueryExpValue::CssText() const {
  StringBuilder output;
  switch (type_) {
    case Type::kInvalid:
      break;
    case Type::kValue:
      output.Append(GetCSSValue().CssText());
      break;
    case Type::kRatio:
      output.Append(Numerator().CssText());
      output.Append(" / ");
      output.Append(Denominator().CssText());
      break;
    case Type::kId:
      output.Append(GetCSSValueNameAs<StringView>(Id()));
      break;
  }

  return output.ReleaseString();
}

unsigned MediaQueryExpValue::GetUnitFlags() const {
  CSSPrimitiveValue::LengthTypeFlags length_type_flags;

  unsigned unit_flags = 0;

  if (IsValue()) {
    if (auto* primitive = DynamicTo<CSSPrimitiveValue>(GetCSSValue())) {
      primitive->AccumulateLengthUnitTypes(length_type_flags);
      if (primitive->IsElementDependent()) {
        unit_flags |= UnitFlags::kTreeCounting;
      }
    }
  }

  if (length_type_flags.test(CSSPrimitiveValue::kUnitTypeFontSize) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeFontXSize) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeZeroCharacterWidth) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeFontCapitalHeight) ||
      length_type_flags.test(
          CSSPrimitiveValue::kUnitTypeIdeographicFullWidth) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeLineHeight)) {
    unit_flags |= UnitFlags::kFontRelative;
  }

  if (length_type_flags.test(CSSPrimitiveValue::kUnitTypeRootFontSize) ||
      length_type_flags.test(CSSPrimitiveValue::kUnitTypeRootFontXSize) ||
      length_type_flags.test(
          CSSPrimitiveValue::kUnitTypeRootFontCapitalHeight) ||
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

KleeneValue MediaQueryFeatureExpNode::Evaluate(
    ConditionalExpNodeVisitor& visitor) const {
  return visitor.EvaluateMediaQueryFeatureExpNode(*this);
}

void MediaQueryFeatureExpNode::SerializeTo(StringBuilder& builder) const {
  builder.Append(exp_.Serialize());
}

void MediaQueryFeatureExpNode::Trace(Visitor* visitor) const {
  visitor->Trace(exp_);
  ConditionalExpNode::Trace(visitor);
}

}  // namespace blink

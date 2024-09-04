// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_feature_overrides.h"

#include "third_party/blink/renderer/core/css/media_features.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"

namespace blink {

namespace {

std::optional<ColorSpaceGamut> ConvertColorGamut(
    const MediaQueryExpValue& value) {
  if (!value.IsValid()) {
    return std::nullopt;
  }
  if (value.Id() == CSSValueID::kSRGB) {
    return ColorSpaceGamut::SRGB;
  }
  if (value.Id() == CSSValueID::kP3) {
    return ColorSpaceGamut::P3;
  }
  // Rec. 2020 is also known as ITU-R-Empfehlung BT.2020.
  if (value.Id() == CSSValueID::kRec2020) {
    return ColorSpaceGamut::BT2020;
  }
  return std::nullopt;
}

std::optional<ForcedColors> ConvertForcedColors(
    const MediaQueryExpValue& value) {
  if (!value.IsValid()) {
    return std::nullopt;
  }
  return CSSValueIDToForcedColors(value.Id());
}

}  // namespace

std::optional<mojom::blink::PreferredColorScheme>
MediaFeatureOverrides::ConvertPreferredColorScheme(
    const MediaQueryExpValue& value) {
  if (!value.IsValid()) {
    return std::nullopt;
  }
  return CSSValueIDToPreferredColorScheme(value.Id());
}

std::optional<mojom::blink::PreferredContrast>
MediaFeatureOverrides::ConvertPreferredContrast(
    const MediaQueryExpValue& value) {
  if (!value.IsValid()) {
    return std::nullopt;
  }
  return CSSValueIDToPreferredContrast(value.Id());
}

std::optional<bool> MediaFeatureOverrides::ConvertPrefersReducedMotion(
    const MediaQueryExpValue& value) {
  if (!value.IsValid()) {
    return std::nullopt;
  }
  return value.Id() == CSSValueID::kReduce;
}

std::optional<bool> MediaFeatureOverrides::ConvertPrefersReducedData(
    const MediaQueryExpValue& value) {
  if (!value.IsValid()) {
    return std::nullopt;
  }
  return value.Id() == CSSValueID::kReduce;
}

std::optional<bool> MediaFeatureOverrides::ConvertPrefersReducedTransparency(
    const MediaQueryExpValue& value) {
  if (!value.IsValid()) {
    return std::nullopt;
  }
  return value.Id() == CSSValueID::kReduce;
}

MediaQueryExpValue MediaFeatureOverrides::ParseMediaQueryValue(
    const AtomicString& feature,
    const String& value_string,
    const Document* document) {
  CSSParserTokenStream stream(value_string);

  // TODO(xiaochengh): This is a fake CSSParserContext that only passes
  // down the CSSParserMode. Plumb the real CSSParserContext through, so that
  // web features can be counted correctly.
  const CSSParserContext* fake_context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext, document);

  // MediaFeatureOverrides are used to emulate various media feature values.
  // These don't need to pass an ExecutionContext, since the parsing of
  // the actual CSS will determine whether or not the emulated values will come
  // into play (i.e. if you can parse an origin trial enabled feature, you
  // will never ask for the emulated override value).
  // Note that once a real CSSParserContext is plumbed through we can use its
  // Document to get the ExecutionContext so the extra parameter should be
  // removed.
  MediaQueryExpBounds bounds =
      MediaQueryExp::Create(feature, stream, *fake_context).Bounds();
  DCHECK(!bounds.left.IsValid());
  return bounds.right.value;
}

void MediaFeatureOverrides::SetOverride(const AtomicString& feature,
                                        const String& value_string,
                                        const Document* document) {
  MediaQueryExpValue value =
      ParseMediaQueryValue(feature, value_string, document);

  if (feature == media_feature_names::kColorGamutMediaFeature) {
    color_gamut_ = ConvertColorGamut(value);
  } else if (feature == media_feature_names::kPrefersColorSchemeMediaFeature) {
    preferred_color_scheme_ = ConvertPreferredColorScheme(value);
  } else if (feature == media_feature_names::kPrefersContrastMediaFeature) {
    preferred_contrast_ = ConvertPreferredContrast(value);
  } else if (feature ==
             media_feature_names::kPrefersReducedMotionMediaFeature) {
    prefers_reduced_motion_ = ConvertPrefersReducedMotion(value);
  } else if (feature == media_feature_names::kPrefersReducedDataMediaFeature) {
    prefers_reduced_data_ = ConvertPrefersReducedData(value);
  } else if (feature ==
             media_feature_names::kPrefersReducedTransparencyMediaFeature) {
    prefers_reduced_transparency_ = ConvertPrefersReducedTransparency(value);
  } else if (feature == media_feature_names::kForcedColorsMediaFeature) {
    forced_colors_ = ConvertForcedColors(value);
  }
}

}  // namespace blink

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_CSS_DOMAINS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_CSS_DOMAINS_H_

#include <string>

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {
enum class CSSValueID;

// CSS-specific domain functions for use in any FuzzTest test.

// Generates any CSSPropertyID.
fuzztest::Domain<CSSPropertyID> AnyCSSProperty();

// Generates any CSSValueID.
fuzztest::Domain<CSSValueID> AnyCSSValue();

// Generates CSS display values from the EDisplay enum.
// Examples: "block", "inline", "flex", "grid", "none", etc.
fuzztest::Domain<std::string> AnyCSSDisplayValue();

// Generates CSS position values from the EPosition enum.
// Examples: "static", "relative", "absolute", "fixed", "sticky"
fuzztest::Domain<std::string> AnyCSSPositionValue();

// Generates CSS visibility values from the EVisibility enum.
// Examples: "visible", "hidden", "collapse"
fuzztest::Domain<std::string> AnyCSSVisibilityValue();

// Generates CSS content-visibility values from the EContentVisibility enum.
// Examples: "visible", "hidden", "auto"
fuzztest::Domain<std::string> AnyCSSContentVisibilityValue();

// Generates CSS overflow values from the EOverflow enum.
// Examples: "visible", "hidden", "scroll", "auto", "clip"
fuzztest::Domain<std::string> AnyCSSOverflowValue();

// Generates CSS text-orientation values from the ETextOrientation enum.
// Examples: "mixed", "upright", "sideways"
fuzztest::Domain<std::string> AnyCSSTextOrientationValue();

// Generates CSS flex-direction values from the EFlexDirection enum.
// Examples: "row", "column", "row-reverse", "column-reverse"
fuzztest::Domain<std::string> AnyCSSFlexDirectionValue();

// Generates reading-flow values from the EReadingFlow enum.
fuzztest::Domain<std::string> AnyCSSReadingFlowValue();

// Generates text-overflow values: either "clip"/"ellipsis" from the enum,
// or custom string values generated from arbitrary strings.
fuzztest::Domain<std::string> AnyCSSTextOverflowValue();

// Generates CSS animation-direction values.
// Examples: "normal", "reverse", "alternate", "alternate-reverse"
fuzztest::Domain<std::string> AnyCSSAnimationDirectionValue();

// Generates CSS animation-fill-mode values.
// Examples: "none", "forwards", "backwards", "both"
fuzztest::Domain<std::string> AnyCSSAnimationFillModeValue();

// Generates CSS animation-play-state values: "running" or "paused".
fuzztest::Domain<std::string> AnyCSSAnimationPlayStateValue();

// Generates CSS animation-timing-function values.
// Examples: "linear", "step-start", "steps(4, jump-none)",
// "cubic-bezier(0.25, 0.1, 0.25, 1)", "linear(0, 0.5 50%, 1)"
fuzztest::Domain<std::string> AnyCSSAnimationTimingFunctionValue();

// Generates CSS animation-duration values as time strings.
fuzztest::Domain<std::string> AnyCSSAnimationDurationValue();

// Generates CSS animation-delay values as time strings, including negatives.
fuzztest::Domain<std::string> AnyCSSAnimationDelayValue();

// Generates CSS animation-iteration-count values.
fuzztest::Domain<std::string> AnyCSSAnimationIterationCountValue();

// Generates CSS animation-name values referencing predefined @keyframes
// injected by the runner.
fuzztest::Domain<std::string> AnyCSSAnimationNameValue();

// Generates CSS scroll-marker-group values.
// Examples: "none", "after", "before", "after tabs", "before links"
fuzztest::Domain<std::string> AnyCSSScrollMarkerGroupValue();

// Generates CSS scroll-target-group values: "none" or "auto".
fuzztest::Domain<std::string> AnyCSSScrollTargetGroupValue();

// Generates CSS scroll-snap-align values.
// Examples: "none", "start", "center", "end"
fuzztest::Domain<std::string> AnyCSSScrollSnapAlignValue();

// Generates CSS scroll-snap-type values: "none" or "<axis> <strictness>".
// Examples: "none", "x mandatory", "y proximity", "both mandatory"
fuzztest::Domain<std::string> AnyCSSScrollSnapTypeValue();

// Generates CSS opacity values as decimal strings.
fuzztest::Domain<std::string> AnyCSSOpacityValue();

// Generates CSS <length> values: numeric with units (px, em, vh, vw).
fuzztest::Domain<std::string> AnyCSSLengthValue();

// Generates CSS size values for width/height: lengths, sizing keywords
// (auto, fit-content, min-content, max-content, stretch), or calc-size().
fuzztest::Domain<std::string> AnyCSSSizeValue();

// Generates CSS size values for max-width/max-height: like AnyCSSSizeValue
// but uses none instead of auto.
fuzztest::Domain<std::string> AnyCSSMaxSizeValue();

// Generates CSS transform values: translate, scale, rotate, skew,
// perspective, matrix, matrix3d, or none.
fuzztest::Domain<std::string> AnyCSSTransformValue();

// Parameters for a Web Animation (element.animate() equivalent).
struct WebAnimationParams {
  CSSPropertyID property;
  std::string from_value;
  std::string to_value;
};

// Generates fuzzed Web Animation parameters: picks an animatable CSS property
// and generates from/to values appropriate for that property.
fuzztest::Domain<WebAnimationParams> AnyWebAnimationParams();

// Generates a value that is appropriate for certain CSS properties.
// It uses the utilities above and in the case of color-related properties,
// it also uses `AnyColorValue()` from `fuzztest_domains_util.h`. For all other
// properties it returns `AnyCSSValue()`. Users of this domain are encouraged to
// extend it to cover additional properties as needed.
fuzztest::Domain<std::string> AnyPlausibleValueForCSSProperty(
    CSSPropertyID property);

// Generates a value from either `AnyPlausibleValueForCSSProperty()` or
// `fuzztest::Arbitrary<std::string>()`.
fuzztest::Domain<std::string> AnyValueForCSSProperty(CSSPropertyID property);

// Generates a CSS property-value declaration string from `AnyCSSProperty()`
// and `AnyValueForCSSProperty()`. Example: "color: red;"
fuzztest::Domain<std::string> AnyCSSPropertyNameValuePair();

// Generates a complete CSS declaration string with up to three items from
// `AnyCSSPropertyNameValuePair()`.
fuzztest::Domain<std::string> AnyCssDeclaration();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_CSS_DOMAINS_H_

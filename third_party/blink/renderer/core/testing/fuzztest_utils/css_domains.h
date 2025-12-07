// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_CSS_DOMAINS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_CSS_DOMAINS_H_

#include <string>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

enum class CSSPropertyID;
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

// Generates text-overflow values: either "clip"/"ellipsis" from the enum,
// or custom string values generated from arbitrary strings.
fuzztest::Domain<std::string> AnyCSSTextOverflowValue();

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

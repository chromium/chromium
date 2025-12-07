// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_SVG_DOMAINS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_SVG_DOMAINS_H_

#include <string>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

class QualifiedName;

// SVG-specific domain functions for use in any FuzzTest test.

// Generates any SVG tag.
fuzztest::Domain<QualifiedName> AnySvgTag();

// Generates any SVG attribute.
fuzztest::Domain<QualifiedName> AnySvgAttribute();

// Generates a viewBox value as four space-separated integers, e.g. "10 20 100
// 150". Format: "min-x min-y width height"
fuzztest::Domain<std::string> AnySvgViewBoxValue();

// Generates SVG path data with various path commands. Creates sequences of:
// - Move commands: "M x y"
// - Line commands: "L x y"
// - Horizontal/vertical lines: "H x", "V y"
// - Close path: "Z"
// Example output: "M 10 20 L 30 40 H 50 V 60 Z"
fuzztest::Domain<std::string> AnySvgPathValue();

// Generates SVG transform functions as space-separated list. Creates sequences
// of:
// - translate(x y): translation
// - rotate(angle): rotation in degrees
// - scale(x y): scaling factors
// - skewX(angle), skewY(angle): skew transformations
// Example output: "translate(10 20) rotate(45) scale(2 1.5)"
fuzztest::Domain<std::string> AnySvgTransformValue();

// Generates a value that is appropriate for certain SVG attributes.
// It uses the utilities above and in the case of fill/stroke attributes,
// it also uses `AnyColorValue()` from `fuzztest_domains_util.h`. For all other
// attributes it returns `fuzztest::Arbitrary<std::string>()`. Users of this
// domain are encouraged to extend it to cover additional attributes as needed.
fuzztest::Domain<std::string> AnyPlausibleValueForSvgAttribute(
    const QualifiedName& attribute);

// Generates a value from either `AnyPlausibleValueForSvgAttribute()` or
// `fuzztest::Arbitrary<std::string>()`.
fuzztest::Domain<std::string> AnyValueForSvgAttribute(
    const QualifiedName& attribute);

// Generates a (`AnySvgAttribute()`, `AnyValueForSvgAttribute()`) pair.
fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnySvgAttributeNameValuePair();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_SVG_DOMAINS_H_

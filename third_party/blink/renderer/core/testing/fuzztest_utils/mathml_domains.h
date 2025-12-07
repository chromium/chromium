// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_MATHML_DOMAINS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_MATHML_DOMAINS_H_

#include <string>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

class QualifiedName;

// MathML-specific domain functions for use in any FuzzTest test.

// Generates any MathML tag.
fuzztest::Domain<QualifiedName> AnyMathMlTag();

// Generates any MathML attribute.
fuzztest::Domain<QualifiedName> AnyMathMlAttribute();

// Generates a plausible value for a MathML attribute.
// Takes into account the specific attribute's expected values.
fuzztest::Domain<std::string> AnyPlausibleValueForMathMlAttribute(
    const QualifiedName& attribute);

// Generates a value from either `AnyPlausibleValueForMathMlAttribute()` or
// `fuzztest::Arbitrary<std::string>()`.
fuzztest::Domain<std::string> AnyValueForMathMlAttribute(
    const QualifiedName& attribute);

// Generates a (`AnyMathMlAttribute()`, `AnyValueForMathMlAttribute()`) pair.
fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyMathMlAttributeNameValuePair();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_MATHML_DOMAINS_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_COMMON_DOMAINS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_COMMON_DOMAINS_H_

#include <string>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

// Convenience domain intended for use by the other `fuzztest_domains_*.cc`
// files, but can also be used independently in any FuzzTest test.

// Generates a string based on `fuzztest::Positive<int>()`
fuzztest::Domain<std::string> AnyPositiveIntegerString();

// Generates a string based on `fuzztest::Arbitrary<int>()`
fuzztest::Domain<std::string> AnyIntegerString();

// Generates either "true" or "false"
fuzztest::Domain<std::string> AnyTrueFalseString();

// Generates one of a set of common color values, including named colors,
// hex colors, rgb/rgba, hsl, and special values like "transparent".
// URL references are also included to cover gradients and patterns.
fuzztest::Domain<std::string> AnyColorValue();

// Generates an idref value in the form of "id_<number>", e.g. "id_2", etc.
// If the test assigns IDs to elements in this format, it can cover both
// matching and non-matching idrefs.
fuzztest::Domain<std::string> AnyPlausibleIdRefValue();

// Generates a space-separated list of 1-3 ID references, e.g. "id_1 id_3 id_7"
// using `AnyPlausibleIdRefValue()`.
fuzztest::Domain<std::string> AnyPlausibleIdRefListValue();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_COMMON_DOMAINS_H_

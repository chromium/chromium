// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_ARIA_DOMAINS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_ARIA_DOMAINS_H_

#include <string>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

class QualifiedName;

// ARIA-specific domain functions for use in any FuzzTest test.

// These generate ARIA attributes, values, and roles using the functions found
// in `ax_utilities_generated.h`.
fuzztest::Domain<QualifiedName> AnyAriaAttribute();
fuzztest::Domain<std::string> AnyNonAbstractAriaRole();
fuzztest::Domain<std::string> AnyAriaAutocompleteValue();
fuzztest::Domain<std::string> AnyAriaCheckedValue();
fuzztest::Domain<std::string> AnyAriaCurrentValue();
fuzztest::Domain<std::string> AnyAriaHasPopupValue();
fuzztest::Domain<std::string> AnyAriaInvalidValue();
fuzztest::Domain<std::string> AnyAriaLiveValue();
fuzztest::Domain<std::string> AnyAriaOrientationValue();
fuzztest::Domain<std::string> AnyAriaPressedValue();
fuzztest::Domain<std::string> AnyAriaRelevantValue();
fuzztest::Domain<std::string> AnyAriaSortValue();
fuzztest::Domain<QualifiedName> AnyAriaTableAttribute();

// Generates a value that is appropriate for the given ARIA attribute, using the
// utilities above as well as those found in `fuzztest_domains_html.h`.
// Note that appropriate does not necessarily mean valid. For instance, if an
// idref is generated, it may or may not match the id of an element in the DOM.
fuzztest::Domain<std::string> AnyPlausibleValueForAriaAttribute(
    const QualifiedName& attribute);

// Generates a value from either `AnyPlausibleValueForAriaAttribute()` or
// `fuzztest::Arbitrary<std::string>()`.
fuzztest::Domain<std::string> AnyValueForAriaAttribute(
    const QualifiedName& attribute);

// Generates a (`AnyAriaAttribute()`, `AnyValueForAriaAttribute()`) pair.
fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyAriaAttributeNameValuePair();

// Generates a (table-specific ARIA attribute, value) pair.
fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyAriaTableAttributeNameValuePair();

// Generates a (role attribute, table-related role value) pair.
fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyAriaTableRoleNameValuePair();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_ARIA_DOMAINS_H_

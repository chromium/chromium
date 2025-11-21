// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_HTML_DOMAINS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_HTML_DOMAINS_H_

#include <string>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace blink {

class QualifiedName;

// HTML-specific domain functions for use in any FuzzTest test.

// Generates any HTML tag.
fuzztest::Domain<QualifiedName> AnyHtmlTag();

// Generates any HTML attribute.
fuzztest::Domain<QualifiedName> AnyHtmlAttribute();

// Generates a value that is appropriate for the given HTML attribute.
// Note that appropriate does not necessarily mean valid. For instance, if an
// idref is generated, it may or may not match the id of an element in the DOM.
// The number generated for colspan, may exceed the number of columns in the
// table. Etc.
fuzztest::Domain<std::string> AnyPlausibleValueForHtmlAttribute(
    const QualifiedName& attribute);

// Generates a value from either `AnyPlausibleValueForHtmlAttribute()` or
// `fuzztest::Arbitrary<std::string>()`.
fuzztest::Domain<std::string> AnyValueForHtmlAttribute(
    const QualifiedName& attribute);

// Generates a (`AnyHtmlAttribute()`, `AnyValueForHtmlAttribute()`) pair.
fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyHtmlAttributeNameValuePair();

// Generates a table-specific HTML attribute (scope, headers, colspan, rowspan).
fuzztest::Domain<const QualifiedName*> AnyHtmlTableAttribute();

// Generates a (table-specific HTML attribute, value) pair.
fuzztest::Domain<std::pair<QualifiedName, std::string>>
AnyHtmlTableAttributeNameValuePair();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FUZZTEST_UTILS_HTML_DOMAINS_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_CANON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_CANON_H_

#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;

namespace url_pattern {

// An enum indicating whether the associated component values to be operated
// on are for patterns or URLs.  Validation and canonicalization will
// do different things depending on the type.
enum class ValueType {
  kPattern,
  kURL,
};

// The following functions are callbacks that may be passed to the
// liburlpattern::Parse() method.  Each performs validation and encoding for
// a different URL component.
//
// Note that there are two different pathname callbacks for "standard" URLs
// like `https://foo` // vs "path" URLs like `data:foo`.  Select the correct
// callback by calling `ShouldTreatAsStandardURL()`.
absl::StatusOr<std::string> ProtocolEncodeCallback(absl::string_view input);
absl::StatusOr<std::string> UsernameEncodeCallback(absl::string_view input);
absl::StatusOr<std::string> PasswordEncodeCallback(absl::string_view input);
absl::StatusOr<std::string> HostnameEncodeCallback(absl::string_view input);
absl::StatusOr<std::string> IPv6HostnameEncodeCallback(absl::string_view input);
absl::StatusOr<std::string> PortEncodeCallback(absl::string_view input);
absl::StatusOr<std::string> StandardURLPathnameEncodeCallback(
    absl::string_view input);
absl::StatusOr<std::string> PathURLPathnameEncodeCallback(
    absl::string_view input);
absl::StatusOr<std::string> SearchEncodeCallback(absl::string_view input);
absl::StatusOr<std::string> HashEncodeCallback(absl::string_view input);

// Utility functions to canonicalize different component strings.  They will
// throw an exception if the input is invalid.  The canonicalization and/or
// validation will only be applied if the `type` is kURL.  These functions
// simply pass through the value when the `type` is kPattern.  Encoding is
// for patterns are handled later during compilation via the encode callbacks
// above.
//
// The result is returned, except for `CanonicalizeUsernameAndPassword` which
// uses separate out parameters for the resulting username and password.
String CanonicalizeProtocol(const String& input,
                            ValueType type,
                            ExceptionState& exception_state);
void CanonicalizeUsernameAndPassword(const String& username,
                                     const String& password,
                                     ValueType type,
                                     String& username_out,
                                     String& password_out,
                                     ExceptionState& exception_state);
String CanonicalizeHostname(const String& input,
                            ValueType type,
                            ExceptionState& exception_state);
String CanonicalizePort(const String& input,
                        ValueType type,
                        const String& protocol,
                        ExceptionState& exception_state);
String CanonicalizePathname(const String& protocol,
                            const String& input,
                            ValueType type,
                            ExceptionState& exception_state);
String CanonicalizeSearch(const String& input,
                          ValueType type,
                          ExceptionState& exception_state);
String CanonicalizeHash(const String& input,
                        ValueType type,
                        ExceptionState& exception_state);

}  // namespace url_pattern
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_CANON_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_DUMMY_URL_CANON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_DUMMY_URL_CANON_H_

#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;

// TODO(crbug.com/439718679): This code is in blink which prevents
// some dependencies from moving to use it. See bug for details.
namespace url_pattern_dummy_url_canon {

// An enum indicating whether the associated component values to be operated
// on are for patterns or URLs.  Validation and canonicalization will
// do different things depending on the type.
enum class ValueType {
  kPattern,
  kURL,
};

// The Canonicalize* utility functions below are used by URLPatternInit
// processing to canonicalize and validate different component strings as
// defined in the URLPattern spec. See
//     https://urlpattern.spec.whatwg.org/#canon-processing-for-init
// They will set an exception on `exception_state` if the input is invalid.
// The canonicalization and/or validation will only be applied if the `type`
// is kURL. These functions simply pass through the value when the `type` is
// kPattern.
// Canonicalization and validation for patterns are handled later during
// compilation via the *EncodeCallback functions.
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

// The *EncodeCallback functions are used to canonicalize and validate
// different component strings during URLPattern compilation. The non-canonical
// input component is provided as the `input` parameter, and the return value is
// the canonicalized output string. Unless the input is invalid, in which case
// an absl::Status error is returned. These are an implementation of the
// `Encoding callbacks` defined in the URLPattern spec:
//    https://urlpattern.spec.whatwg.org/#canon-encoding-callbacks
base::expected<std::string, absl::Status> ProtocolEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> UsernameEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> PasswordEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> HostnameEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> IPv6HostnameEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> PortEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> StandardPathnameEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> OpaquePathnameEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> SearchEncodeCallback(
    std::string_view input);
base::expected<std::string, absl::Status> HashEncodeCallback(
    std::string_view input);

}  // namespace url_pattern_dummy_url_canon
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_DUMMY_URL_CANON_H_

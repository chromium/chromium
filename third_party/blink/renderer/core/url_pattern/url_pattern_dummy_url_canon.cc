// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern_dummy_url_canon.h"

#include <ranges>

#include "components/url_pattern/url_pattern_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace blink::url_pattern_dummy_url_canon {

namespace {

constexpr char kDummyUrlWithoutSchemeName[] = "://dummy.invalid/";
constexpr char kDummyUrl[] = "https://dummy.invalid/";

String MaybeStripPrefix(const String& value, StringView prefix) {
  CHECK_EQ(prefix.length(), 1u);
  if (value.StartsWith(prefix)) {
    return value.Substring(1, value.length() - 1);
  }
  return value;
}

String MaybeStripSuffix(const String& value, StringView suffix) {
  CHECK_EQ(suffix.length(), 1u);
  if (value.EndsWith(suffix)) {
    return value.Substring(0, value.length() - 1);
  }
  return value;
}

wtf_size_t FindFirstDelimiter(const String& value, StringView delimiters) {
  CHECK(delimiters.Is8Bit());
  for (wtf_size_t value_idx = 0; value_idx < value.length(); ++value_idx) {
    auto delimiters_span8 = delimiters.Span8();
    if (std::ranges::find(delimiters_span8, value[value_idx]) !=
        delimiters_span8.end()) {
      return value_idx;
    }
  }
  return kNotFound;
}

// Returns the start of the string `value` up to the first character from
// `delimiters`. For example, given value "abc#def" , and delimiters "?/#",
// this will return "abc".
String MaybeStripAfterFirstDelimiter(const String& value,
                                     StringView delimiters) {
  const wtf_size_t first_delim = FindFirstDelimiter(value, delimiters);
  if (first_delim == kNotFound) {
    return value;
  }
  return value.Substring(0, first_delim);
}

bool ContainsForbiddenHostnameCodePoint(const String& input,
                                        const bool allow_ipv6_delimiters) {
  StringUtf8Adaptor input_utf8(input);
  return url_pattern::ContainsForbiddenHostnameCodePoint(
      input_utf8.AsStringView(), allow_ipv6_delimiters);
}

String StringFromCanonOutput(const url::CanonOutput& output,
                             const url::Component& component) {
  return String::FromUTF8(output.view().substr(component.begin, component.len));
}

// Convert from the output of the CanonicalizeInternal* functions to
// the one used by the Canonicalize* functions.
String CanonicalizeInternalHelper(const base::expected<String, String>& result,
                                  ExceptionState& exception_state) {
  if (result.has_value()) {
    return result.value();
  } else {
    exception_state.ThrowTypeError(result.error());
    return String();
  }
}

// Convert from the output of the CanonicalizeInternal* functions to
// the one used by the *EncodeCallback functions.
base::expected<std::string, absl::Status> EncodeCallbackHelper(
    const base::expected<String, String>& result) {
  if (result.has_value()) {
    return std::string(StringUtf8Adaptor(result.value()).AsStringView());
  } else {
    return base::unexpected(absl::InvalidArgumentError(
        StringUtf8Adaptor(result.error()).AsStringView()));
  }
}

std::optional<bool> IsStandardProtocol(const String& protocol) {
  if (protocol.empty()) {
    return std::nullopt;
  } else if (protocol.Is8Bit()) {
    StringUtf8Adaptor utf8(protocol);
    return url::IsStandard(utf8.AsStringView());
  } else {
    return url::IsStandard(protocol.View16());
  }
}

// The Canonicalize*Internal functions are shared between the Canonicalize*
// functions and the *EncodeCallback functions. They implement the
// 'Encoding callbacks' defined in the URLPattern spec:
//    https://urlpattern.spec.whatwg.org/#canon-encoding-callbacks
//
// Note that the two sets of similar canonicalization functions use different
// string types.
// Canonicalize* functions use `blink::String` while the *EncodeCallback
// functions use `std::string`.
//
// The Canonicalize*Internal functions favor `blink::String` to reduce the
// number of conversions between `blink::String` and `std::string` since
// this is ultimately for the URLPattern DOM API which must
// use `blink::String` and this code has an underlying dependency on
// `blink::KURL` which also uses `blink::String`.
base::expected<String, String> CanonicalizeProtocolInternal(
    const String& input) {
  if (input.empty()) {
    return base::ok(input);
  }

  KURL dummy_url(StrCat({input, kDummyUrlWithoutSchemeName}));
  if (dummy_url.IsValid()) {
    if (input.length() == 1 && dummy_url.ProtocolIs("file")) {
      // If we do this with a single letter it looks to KURL like a Windows
      // file path and is turned into a file URL. Canonicalizing 'a' should
      // not return 'file'.
      return base::ok(input.LowerASCII());
    } else {
      return base::ok(dummy_url.Protocol());
    }
  } else {
    return base::unexpected(blink::StrCat({"Invalid protocol '", input, "'."}));
  }
}

base::expected<String, String> CanonicalizeUsernameInternal(
    const String& input) {
  if (input.empty()) {
    return base::ok(input);
  }

  KURL dummy_url(kDummyUrl);
  dummy_url.SetUser(input);
  if (dummy_url.IsValid()) {
    return base::ok(dummy_url.User().ToString());
  } else {
    return base::unexpected(blink::StrCat({"Invalid username '", input, "'."}));
  }
}

base::expected<String, String> CanonicalizePasswordInternal(
    const String& input) {
  if (input.empty()) {
    return base::ok(input);
  }

  KURL dummy_url(kDummyUrl);
  dummy_url.SetPass(input);
  if (dummy_url.IsValid()) {
    return base::ok(dummy_url.Pass().ToString());
  } else {
    return base::unexpected(blink::StrCat({"Invalid password '", input, "'."}));
  }
}

// allow_ipv6 exists because this internal function is used from the
// Canonicalize* function which needs to handle non-pattern hostnames
// including both IPV6 and non-IPV6 hostnames. This internal function
// is also used by the *EncodeCallback function which has separate
// functions for IPv6 and non-IPv6 hostnames and so does not need to
// allow for both at once.
base::expected<String, String> CanonicalizeHostnameInternal(
    const String& input,
    const bool allow_ipv6) {
  if (input.empty()) {
    return base::ok(input);
  }

  KURL dummy_url(kDummyUrl);
  const String stripped = MaybeStripAfterFirstDelimiter(input, "?/#");
  // Due to crbug.com/1065667 the url::CanonicalizeHost() call below will
  // permit and possibly encode some illegal code points.  Since we want
  // to ultimately fix that in the future we don't want to encourage more
  // use of these characters in URLPattern.  Therefore we apply an additional
  // restrictive check for these forbidden code points.
  //
  // TODO(crbug.com/40124263): Remove this check after the URL parser is fixed.
  if (ContainsForbiddenHostnameCodePoint(stripped, allow_ipv6)) {
    return base::unexpected(
        blink::StrCat({"Invalid hostname '", stripped, "'."}));
  }

  if (stripped.empty()) {
    return base::ok(stripped);
  }

  dummy_url.SetHost(stripped);
  if (dummy_url.IsValid()) {
    return base::ok(dummy_url.Host().ToString());
  } else {
    return base::unexpected(blink::StrCat({"Invalid hostname '", input, "'."}));
  }
}

base::expected<String, String> CanonicalizeIPv6HostnameInternal(
    const String& input) {
  std::string result;
  result.reserve(input.length());

  // This implements a light validation and canonicalization of IPv6 hostname
  // content.  Ideally we would use the URL parser's hostname canonicalizer
  // here, but that is too strict for the encoding callback.  The callback may
  // see only bits and pieces of the hostname pattern; e.g. for `[:address]` it
  // sees the `[` and `]` strings as separate calls.  Since the full URL
  // hostname parser wants to completely parse IPv6 hostnames, this will always
  // trigger an error.  Therefore, to allow pattern syntax within IPv6 brackets
  // we simply check for valid characters and lowercase any hex digits.
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input[i];
    if (!blink::IsASCIIHexDigit(c) && c != '[' && c != ']' && c != ':') {
      return base::unexpected(blink::StrCat(
          {"Invalid IPv6 hostname character '", String(std::string_view(&c, 1)),
           "' in '", input, "'."}));
    }
    result += blink::ToASCIILower(c);
  }
  return base::ok(String::FromUTF8(result));
}

base::expected<String, String> CanonicalizePortInternal(const String& protocol,
                                                        const String& input) {
  if (input.empty()) {
    return base::ok(input);
  }

  KURL dummy_url(kDummyUrl);
  if (!protocol.empty() && !dummy_url.SetProtocol(protocol)) {
    return base::unexpected(
        blink::StrCat({"Invalid protocol '", protocol, "'."}));
  }

  if (!dummy_url.SetPort(input) || !dummy_url.IsValid()) {
    return base::unexpected(blink::StrCat({"Invalid port '", input, "'."}));
  }

  return base::ok(dummy_url.HasPort() ? String::Number(dummy_url.Port())
                                      : String());
}

base::expected<String, String> CanonicalizePathnameInternal(
    const bool standard,
    const String& input) {
  if (input.empty()) {
    return base::ok(input);
  }

  // Do not enforce absolute pathnames here since we can't enforce it
  // it consistently in the URLPattern constructor.  This allows us to
  // produce a match when the exact same fixed pathname string is passed
  // to both the constructor and test()/exec()
  if (standard) {
    bool leading_slash = input.StartsWith("/");
    KURL dummy_url(kDummyUrl);
    String maybe_preprended_input = input;

    // If it doesn't start with a leading slash then we prepend '/-' to
    // ensure we don't end up with a leading slash in the pathname
    // canonicalization result. See the following for more information:
    // https://urlpattern.spec.whatwg.org/#canonicalize-a-pathname
    if (!leading_slash) {
      maybe_preprended_input = StrCat({"/-", maybe_preprended_input});
    }

    dummy_url.SetPath(maybe_preprended_input);
    if (!dummy_url.IsValid()) {
      return base::unexpected(
          blink::StrCat({"Invalid pathname '", input, "'."}));
    }
    String canonicalized_path =
        dummy_url.HasPath() ? dummy_url.GetPath().ToString() : String();

    if (!leading_slash) {
      if (canonicalized_path.StartsWith("/-")) {
        // If we prepended a slash then we need to remove it again since the
        // pathname canonicalization should not add a leading slash.
        canonicalized_path = canonicalized_path.Substring(2);
      } else {
        return base::unexpected(
            blink::StrCat({"Invalid pathname '", input, "'."}));
      }
    }
    return base::ok(canonicalized_path);
  } else {
    url::RawCanonOutputT<char> canon_output;
    url::Component component;
    url::CanonicalizePathUrlPath(StringUtf8Adaptor(input).AsStringView(),
                                 &canon_output, &component);
    return base::ok(StringFromCanonOutput(canon_output, component));
  }
}

base::expected<String, String> CanonicalizeSearchInternal(const String& input) {
  if (input.empty()) {
    return base::ok(input);
  }

  KURL dummy_url(kDummyUrl);
  dummy_url.SetQuery(input);

  if (dummy_url.IsValid()) {
    return base::ok(dummy_url.Query().ToString());
  } else {
    return base::unexpected(blink::StrCat({"Invalid search '", input, "'."}));
  }
}

base::expected<String, String> CanonicalizeHashInternal(const String& input) {
  if (input.empty()) {
    return base::ok(input);
  }

  KURL dummy_url(kDummyUrl);
  dummy_url.SetFragmentIdentifier(input);

  if (dummy_url.IsValid()) {
    return base::ok(dummy_url.FragmentIdentifier().ToString());
  } else {
    return base::unexpected(blink::StrCat({"Invalid hash '", input, "'."}));
  }
}
}  // anonymous namespace

String CanonicalizeProtocol(const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  // We allow the protocol input to optionally contain a ":" suffix.  Strip
  // this for both URL and pattern protocols.
  const String stripped = MaybeStripSuffix(input, ":");
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return stripped;
  }

  return CanonicalizeInternalHelper(CanonicalizeProtocolInternal(stripped),
                                    exception_state);
}

void CanonicalizeUsernameAndPassword(const String& username,
                                     const String& password,
                                     ValueType type,
                                     String& username_out,
                                     String& password_out,
                                     ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    username_out = username;
    password_out = password;
    return;
  }

  const auto username_result = CanonicalizeUsernameInternal(username);
  const auto password_result = CanonicalizePasswordInternal(password);

  if (username_result.has_value() && password_result.has_value()) {
    username_out = username_result.value();
    password_out = password_result.value();
    return;
  } else if (!username_result.has_value()) {
    exception_state.ThrowTypeError(username_result.error());
    return;
  } else if (!password_result.has_value()) {
    exception_state.ThrowTypeError(password_result.error());
    return;
  }
}

String CanonicalizeHostname(const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  return CanonicalizeInternalHelper(CanonicalizeHostnameInternal(input, true),
                                    exception_state);
}

String CanonicalizePort(const String& input,
                        ValueType type,
                        const String& protocol,
                        ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  return CanonicalizeInternalHelper(CanonicalizePortInternal(protocol, input),
                                    exception_state);
}

String CanonicalizePathname(const String& protocol,
                            const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  // Determine if we are using "standard" or "path" URL canonicalization
  // for the pathname.  In spec terms the "path" URL behavior corresponds
  // to "cannot-be-a-base" URLs.  We make this determination based on the
  // protocol string since we cannot look at the number of slashes between
  // components like the URL spec.  If this is inadequate the developer
  // can use the baseURL property to get more strict URL behavior.
  //
  // We default to "standard" URL behavior to match how the empty protocol
  // string in the URLPattern constructor results in the pathname pattern
  // getting "standard" URL canonicalization.
  const bool standard = IsStandardProtocol(protocol).value_or(true);
  return CanonicalizeInternalHelper(
      CanonicalizePathnameInternal(standard, input), exception_state);
}

String CanonicalizeSearch(const String& input,
                          ValueType type,
                          ExceptionState& exception_state) {
  // We allow the search input to optionally contain a "?" prefix.  Strip
  // this for both URL and pattern protocols.
  const String stripped = MaybeStripPrefix(input, "?");
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return stripped;
  }

  return CanonicalizeInternalHelper(CanonicalizeSearchInternal(stripped),
                                    exception_state);
}

String CanonicalizeHash(const String& input,
                        ValueType type,
                        ExceptionState& exception_state) {
  // We allow the hash input to optionally contain a "#" prefix.  Strip
  // this for both URL and pattern protocols.
  const String stripped = MaybeStripPrefix(input, "#");
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return stripped;
  }

  return CanonicalizeInternalHelper(CanonicalizeHashInternal(stripped),
                                    exception_state);
}

base::expected<std::string, absl::Status> ProtocolEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizeProtocolInternal(String::FromUTF8(input)));
}

base::expected<std::string, absl::Status> UsernameEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizeUsernameInternal(String::FromUTF8(input)));
}

base::expected<std::string, absl::Status> PasswordEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizePasswordInternal(String::FromUTF8(input)));
}

base::expected<std::string, absl::Status> HostnameEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizeHostnameInternal(String::FromUTF8(input), false));
}

base::expected<std::string, absl::Status> IPv6HostnameEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizeIPv6HostnameInternal(String::FromUTF8(input)));
}

base::expected<std::string, absl::Status> PortEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizePortInternal(String(), String::FromUTF8(input)));
}

base::expected<std::string, absl::Status> StandardPathnameEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizePathnameInternal(true, String::FromUTF8(input)));
}

base::expected<std::string, absl::Status> OpaquePathnameEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizePathnameInternal(false, String::FromUTF8(input)));
}

base::expected<std::string, absl::Status> SearchEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizeSearchInternal(String::FromUTF8(input)));
}

base::expected<std::string, absl::Status> HashEncodeCallback(
    std::string_view input) {
  return EncodeCallbackHelper(
      CanonicalizeHashInternal(String::FromUTF8(input)));
}

}  // namespace blink::url_pattern_dummy_url_canon

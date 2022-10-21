// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern_canon.h"

#include "third_party/blink/renderer/core/url_pattern/url_pattern_component.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace blink {
namespace url_pattern {

namespace {

String MaybeStripPrefix(const String& value, StringView prefix) {
  if (value.StartsWith(prefix))
    return value.Substring(1, value.length() - 1);
  return value;
}

String MaybeStripSuffix(const String& value, StringView suffix) {
  if (value.EndsWith(suffix))
    return value.Substring(0, value.length() - 1);
  return value;
}

String StringFromCanonOutput(const url::CanonOutput& output,
                             const url::Component& component) {
  return String::FromUTF8(output.data() + component.begin, component.len);
}

std::string StdStringFromCanonOutput(const url::CanonOutput& output,
                                     const url::Component& component) {
  return std::string(output.data() + component.begin, component.len);
}

bool ContainsForbiddenHostnameCodePoint(absl::string_view input) {
  for (auto c : input) {
    // The full list of forbidden code points is defined at:
    //
    //  https://url.spec.whatwg.org/#forbidden-host-code-point
    //
    // We only check the code points the chromium URL parser incorrectly
    // permits.  See: crbug.com/1065667#c18
    if (c == ' ' || c == '#' || c == ':' || c == '<' || c == '>' || c == '@' ||
        c == '[' || c == ']' || c == '|') {
      return true;
    }
  }
  return false;
}

}  // anonymous namespace

absl::StatusOr<std::string> ProtocolEncodeCallback(absl::string_view input) {
  if (input.empty())
    return std::string();

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  bool result = url::CanonicalizeScheme(
      input.data(), url::Component(0, static_cast<int>(input.size())),
      &canon_output, &component);

  if (!result) {
    return absl::InvalidArgumentError("Invalid protocol '" +
                                      std::string(input) + "'.");
  }

  return StdStringFromCanonOutput(canon_output, component);
}

absl::StatusOr<std::string> UsernameEncodeCallback(absl::string_view input) {
  if (input.empty())
    return std::string();

  url::RawCanonOutputT<char> canon_output;
  url::Component username_component;
  url::Component password_component;

  bool result = url::CanonicalizeUserInfo(
      input.data(), url::Component(0, static_cast<int>(input.size())), "",
      url::Component(0, 0), &canon_output, &username_component,
      &password_component);

  if (!result) {
    return absl::InvalidArgumentError("Invalid username pattern '" +
                                      std::string(input) + "'.");
  }

  return StdStringFromCanonOutput(canon_output, username_component);
}

absl::StatusOr<std::string> PasswordEncodeCallback(absl::string_view input) {
  if (input.empty())
    return std::string();

  url::RawCanonOutputT<char> canon_output;
  url::Component username_component;
  url::Component password_component;

  bool result = url::CanonicalizeUserInfo(
      "", url::Component(0, 0), input.data(),
      url::Component(0, static_cast<int>(input.size())), &canon_output,
      &username_component, &password_component);

  if (!result) {
    return absl::InvalidArgumentError("Invalid password pattern '" +
                                      std::string(input) + "'.");
  }

  return StdStringFromCanonOutput(canon_output, password_component);
}

absl::StatusOr<std::string> HostnameEncodeCallback(absl::string_view input) {
  if (input.empty())
    return std::string();

  // Due to crbug.com/1065667 the url::CanonicalizeHost() call below will
  // permit and possibly encode some illegal code points.  Since we want
  // to ultimately fix that in the future we don't want to encourage more
  // use of these characters in URLPattern.  Therefore we apply an additional
  // restrictive check for these forbidden code points.
  //
  // TODO(crbug.com/1065667): Remove this check after the URL parser is fixed.
  if (ContainsForbiddenHostnameCodePoint(input)) {
    return absl::InvalidArgumentError("Invalid hostname pattern '" +
                                      std::string(input) + "'.");
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  bool result = url::CanonicalizeHost(
      input.data(), url::Component(0, static_cast<int>(input.size())),
      &canon_output, &component);

  if (!result) {
    return absl::InvalidArgumentError("Invalid hostname pattern '" +
                                      std::string(input) + "'.");
  }

  return StdStringFromCanonOutput(canon_output, component);
}

absl::StatusOr<std::string> IPv6HostnameEncodeCallback(
    absl::string_view input) {
  std::string result;
  result.reserve(input.size());
  // This implements a light validation and canonicalization of IPv6 hostname
  // content.  Ideally we would use the URL parser's hostname canonicalizer
  // here, but that is too strict for the encoding callback.  The callback may
  // see only bits and pieces of the hostname pattern; e.g. for `[:address]` it
  // sees the `[` and `]` strings as separate calls.  Since the full URL
  // hostname parser wants to completely parse IPv6 hostnames, this will always
  // trigger an error.  Therefore, to allow pattern syntax within IPv6 brackets
  // we simply check for valid characters and lowercase any hex digits.
  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (!IsASCIIHexDigit(c) && c != '[' && c != ']' && c != ':') {
      return absl::InvalidArgumentError(
          std::string("Invalid IPv6 hostname character '") + c + "' in '" +
          std::string(input) + "'.");
    }
    result += ToASCIILower(c);
  }
  return result;
}

absl::StatusOr<std::string> PortEncodeCallback(absl::string_view input) {
  if (input.empty())
    return std::string();

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  bool result = url::CanonicalizePort(
      input.data(), url::Component(0, static_cast<int>(input.size())),
      url::PORT_UNSPECIFIED, &canon_output, &component);

  if (!result) {
    return absl::InvalidArgumentError("Invalid port pattern '" +
                                      std::string(input) + "'.");
  }

  return StdStringFromCanonOutput(canon_output, component);
}

absl::StatusOr<std::string> StandardURLPathnameEncodeCallback(
    absl::string_view input) {
  if (input.empty())
    return std::string();

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  bool result = url::CanonicalizePartialPath(
      input.data(), url::Component(0, static_cast<int>(input.size())),
      &canon_output, &component);

  if (!result) {
    return absl::InvalidArgumentError("Invalid pathname pattern '" +
                                      std::string(input) + "'.");
  }

  return StdStringFromCanonOutput(canon_output, component);
}

absl::StatusOr<std::string> PathURLPathnameEncodeCallback(
    absl::string_view input) {
  if (input.empty())
    return std::string();

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  url::CanonicalizePathURLPath(
      input.data(), url::Component(0, static_cast<int>(input.size())),
      &canon_output, &component);

  return StdStringFromCanonOutput(canon_output, component);
}

absl::StatusOr<std::string> SearchEncodeCallback(absl::string_view input) {
  if (input.empty())
    return std::string();

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  url::CanonicalizeQuery(input.data(),
                         url::Component(0, static_cast<int>(input.size())),
                         /*converter=*/nullptr, &canon_output, &component);

  return StdStringFromCanonOutput(canon_output, component);
}

absl::StatusOr<std::string> HashEncodeCallback(absl::string_view input) {
  if (input.empty())
    return std::string();

  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  url::CanonicalizeRef(input.data(),
                       url::Component(0, static_cast<int>(input.size())),
                       &canon_output, &component);

  return StdStringFromCanonOutput(canon_output, component);
}

String CanonicalizeProtocol(const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  // We allow the protocol input to optionally contain a ":" suffix.  Strip
  // this for both URL and pattern protocols.
  String stripped = MaybeStripSuffix(input, ":");

  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return stripped;
  }

  bool result = false;
  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (stripped.Is8Bit()) {
    StringUTF8Adaptor utf8(stripped);
    result = url::CanonicalizeScheme(
        utf8.data(), url::Component(0, utf8.size()), &canon_output, &component);
  } else {
    result = url::CanonicalizeScheme(stripped.Characters16(),
                                     url::Component(0, stripped.length()),
                                     &canon_output, &component);
  }

  if (!result) {
    exception_state.ThrowTypeError("Invalid protocol '" + stripped + "'.");
    return String();
  }

  return StringFromCanonOutput(canon_output, component);
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

  bool result = false;
  url::RawCanonOutputT<char> canon_output;
  url::Component username_component;
  url::Component password_component;

  if (username && password && username.Is8Bit() && password.Is8Bit()) {
    StringUTF8Adaptor username_utf8(username);
    StringUTF8Adaptor password_utf8(password);
    result = url::CanonicalizeUserInfo(
        username_utf8.data(), url::Component(0, username_utf8.size()),
        password_utf8.data(), url::Component(0, password_utf8.size()),
        &canon_output, &username_component, &password_component);

  } else {
    String username16(username);
    String password16(password);
    username16.Ensure16Bit();
    password16.Ensure16Bit();
    result = url::CanonicalizeUserInfo(
        username16.Characters16(), url::Component(0, username16.length()),
        password16.Characters16(), url::Component(0, password16.length()),
        &canon_output, &username_component, &password_component);
  }

  if (!result) {
    exception_state.ThrowTypeError("Invalid username '" + username +
                                   "' and/or password '" + password + "'.");
    return;
  }

  if (username_component.len != -1)
    username_out = StringFromCanonOutput(canon_output, username_component);
  if (password_component.len != -1)
    password_out = StringFromCanonOutput(canon_output, password_component);
}

String CanonicalizeHostname(const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  bool success = false;
  String result = SecurityOrigin::CanonicalizeHost(input, &success);
  if (!success) {
    exception_state.ThrowTypeError("Invalid hostname '" + input + "'.");
    return String();
  }

  return result;
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

  int default_port = url::PORT_UNSPECIFIED;
  if (!input.empty()) {
    StringUTF8Adaptor protocol_utf8(protocol);
    default_port =
        url::DefaultPortForScheme(protocol_utf8.data(), protocol_utf8.size());
  }

  // Since ports only consist of digits there should be no encoding needed.
  // Therefore we directly use the UTF8 encoding version of CanonicalizePort().
  StringUTF8Adaptor utf8(input);
  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (!url::CanonicalizePort(utf8.data(), url::Component(0, utf8.size()),
                             default_port, &canon_output, &component)) {
    exception_state.ThrowTypeError("Invalid port '" + input + "'.");
    return String();
  }

  return component.len == -1 ? g_empty_string
                             : StringFromCanonOutput(canon_output, component);
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
  bool standard = false;
  if (protocol.empty()) {
    standard = true;
  } else if (protocol.Is8Bit()) {
    StringUTF8Adaptor utf8(protocol);
    standard = url::IsStandard(utf8.data(), url::Component(0, utf8.size()));
  } else {
    standard = url::IsStandard(protocol.Characters16(),
                               url::Component(0, protocol.length()));
  }

  // Do not enforce absolute pathnames here since we can't enforce it
  // it consistently in the URLPattern constructor.  This allows us to
  // produce a match when the exact same fixed pathname string is passed
  // to both the constructor and test()/exec().  Similarly, we use
  // url::CanonicalizePartialPath() below instead of url::CanonicalizePath()
  // to avoid pre-pending a slash at the start of the string.

  bool result = false;
  url::RawCanonOutputT<char> canon_output;
  url::Component component;

  const auto canonicalize_path = [&](const auto* data, int length) {
    if (standard) {
      return url::CanonicalizePartialPath(data, url::Component(0, length),
                                          &canon_output, &component);
    }
    url::CanonicalizePathURLPath(data, url::Component(0, length), &canon_output,
                                 &component);
    return true;
  };

  if (input.Is8Bit()) {
    StringUTF8Adaptor utf8(input);
    result = canonicalize_path(utf8.data(), utf8.size());
  } else {
    result = canonicalize_path(input.Characters16(), input.length());
  }

  if (!result) {
    exception_state.ThrowTypeError("Invalid pathname '" + input + "'.");
    return String();
  }

  return StringFromCanonOutput(canon_output, component);
}

String CanonicalizeSearch(const String& input,
                          ValueType type,
                          ExceptionState& exception_state) {
  // We allow the search input to optionally contain a "?" prefix.  Strip
  // this for both URL and pattern protocols.
  String stripped = MaybeStripPrefix(input, "?");

  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return stripped;
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (stripped.Is8Bit()) {
    StringUTF8Adaptor utf8(stripped);
    url::CanonicalizeQuery(utf8.data(), url::Component(0, utf8.size()),
                           /*converter=*/nullptr, &canon_output, &component);
  } else {
    url::CanonicalizeQuery(stripped.Characters16(),
                           url::Component(0, stripped.length()),
                           /*converter=*/nullptr, &canon_output, &component);
  }

  return StringFromCanonOutput(canon_output, component);
}

String CanonicalizeHash(const String& input,
                        ValueType type,
                        ExceptionState& exception_state) {
  // We allow the hash input to optionally contain a "#" prefix.  Strip
  // this for both URL and pattern protocols.
  String stripped = MaybeStripPrefix(input, "#");

  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return stripped;
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (stripped.Is8Bit()) {
    StringUTF8Adaptor utf8(stripped);
    url::CanonicalizeRef(utf8.data(), url::Component(0, utf8.size()),
                         &canon_output, &component);
  } else {
    url::CanonicalizeRef(stripped.Characters16(),
                         url::Component(0, stripped.length()), &canon_output,
                         &component);
  }

  return StringFromCanonOutput(canon_output, component);
}

}  // namespace url_pattern
}  // namespace blink

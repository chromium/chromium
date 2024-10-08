// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
  return String::FromUTF8(output.view().substr(component.begin, component.len));
}

}  // anonymous namespace

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
  String result = SecurityOrigin::CanonicalizeSpecialHost(input, &success);
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
    default_port = url::DefaultPortForScheme(protocol_utf8.AsStringView());
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

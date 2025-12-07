// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern_canon.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_component.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_dummy_url_canon.h"
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
  if (base::FeatureList::IsEnabled(
          blink::features::kURLPatternDummyURLCanonicalization)) {
    return blink::url_pattern_dummy_url_canon::CanonicalizeProtocol(
        input, static_cast<blink::url_pattern_dummy_url_canon::ValueType>(type),
        exception_state);
  }

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
    StringUtf8Adaptor utf8(stripped);
    result =
        url::CanonicalizeScheme(utf8.AsStringView(), &canon_output, &component);
  } else {
    result =
        url::CanonicalizeScheme(stripped.View16(), &canon_output, &component);
  }

  if (!result) {
    exception_state.ThrowTypeError(
        StrCat({"Invalid protocol '", stripped, "'."}));
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
  if (base::FeatureList::IsEnabled(
          blink::features::kURLPatternDummyURLCanonicalization)) {
    blink::url_pattern_dummy_url_canon::CanonicalizeUsernameAndPassword(
        username, password,
        static_cast<blink::url_pattern_dummy_url_canon::ValueType>(type),
        username_out, password_out, exception_state);
    return;
  }

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
    StringUtf8Adaptor username_utf8(username);
    StringUtf8Adaptor password_utf8(password);
    result = url::CanonicalizeUserInfo(
        username_utf8.AsStringView(), password_utf8.AsStringView(),
        &canon_output, &username_component, &password_component);

  } else {
    String username16(username);
    String password16(password);
    username16.Ensure16Bit();
    password16.Ensure16Bit();
    result = url::CanonicalizeUserInfo(username16.View16(), password16.View16(),
                                       &canon_output, &username_component,
                                       &password_component);
  }

  if (!result) {
    exception_state.ThrowTypeError(
        StrCat({"Invalid username '", username, "' and/or password '", password,
                "'."}));
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
  if (base::FeatureList::IsEnabled(
          blink::features::kURLPatternDummyURLCanonicalization)) {
    return blink::url_pattern_dummy_url_canon::CanonicalizeHostname(
        input, static_cast<blink::url_pattern_dummy_url_canon::ValueType>(type),
        exception_state);
  }

  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  bool success = false;
  String result = SecurityOrigin::CanonicalizeSpecialHost(input, &success);
  if (!success) {
    exception_state.ThrowTypeError(StrCat({"Invalid hostname '", input, "'."}));
    return String();
  }

  return result;
}

String CanonicalizePort(const String& input,
                        ValueType type,
                        const String& protocol,
                        ExceptionState& exception_state) {
  if (base::FeatureList::IsEnabled(
          blink::features::kURLPatternDummyURLCanonicalization)) {
    return blink::url_pattern_dummy_url_canon::CanonicalizePort(
        input, static_cast<blink::url_pattern_dummy_url_canon::ValueType>(type),
        protocol, exception_state);
  }

  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  int default_port = url::PORT_UNSPECIFIED;
  if (!input.empty()) {
    StringUtf8Adaptor protocol_utf8(protocol);
    default_port = url::DefaultPortForScheme(protocol_utf8.AsStringView());
  }

  // Since ports only consist of digits there should be no encoding needed.
  // Therefore we directly use the UTF8 encoding version of CanonicalizePort().
  StringUtf8Adaptor utf8(input);
  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (!url::CanonicalizePort(utf8.AsStringView(), default_port, &canon_output,
                             &component)) {
    exception_state.ThrowTypeError(StrCat({"Invalid port '", input, "'."}));
    return String();
  }

  return component.len == -1 ? g_empty_string
                             : StringFromCanonOutput(canon_output, component);
}

String CanonicalizePathname(const String& protocol,
                            const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  if (base::FeatureList::IsEnabled(
          blink::features::kURLPatternDummyURLCanonicalization)) {
    return blink::url_pattern_dummy_url_canon::CanonicalizePathname(
        protocol, input,
        static_cast<blink::url_pattern_dummy_url_canon::ValueType>(type),
        exception_state);
  }

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
    StringUtf8Adaptor utf8(protocol);
    standard = url::IsStandard(utf8.AsStringView());
  } else {
    standard = url::IsStandard(protocol.View16());
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

  const auto canonicalize_path =
      [&]<typename CharType>(std::basic_string_view<CharType> data) {
        if (standard) {
          return url::CanonicalizePartialPath(data, &canon_output, &component);
        }
        url::CanonicalizePathUrlPath(data, &canon_output, &component);
        return true;
      };

  if (input.Is8Bit()) {
    StringUtf8Adaptor utf8(input);
    result = canonicalize_path(utf8.AsStringView());
  } else {
    result = canonicalize_path(input.View16());
  }

  if (!result) {
    exception_state.ThrowTypeError(StrCat({"Invalid pathname '", input, "'."}));
    return String();
  }

  return StringFromCanonOutput(canon_output, component);
}

String CanonicalizeSearch(const String& input,
                          ValueType type,
                          ExceptionState& exception_state) {
  if (base::FeatureList::IsEnabled(
          blink::features::kURLPatternDummyURLCanonicalization)) {
    return blink::url_pattern_dummy_url_canon::CanonicalizeSearch(
        input, static_cast<blink::url_pattern_dummy_url_canon::ValueType>(type),
        exception_state);
  }

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
    StringUtf8Adaptor utf8(stripped);
    url::CanonicalizeQuery(utf8.AsStringView(),
                           /*converter=*/nullptr, &canon_output, &component);
  } else {
    url::CanonicalizeQuery(stripped.View16(),
                           /*converter=*/nullptr, &canon_output, &component);
  }

  return StringFromCanonOutput(canon_output, component);
}

String CanonicalizeHash(const String& input,
                        ValueType type,
                        ExceptionState& exception_state) {
  if (base::FeatureList::IsEnabled(
          blink::features::kURLPatternDummyURLCanonicalization)) {
    return blink::url_pattern_dummy_url_canon::CanonicalizeHash(
        input, static_cast<blink::url_pattern_dummy_url_canon::ValueType>(type),
        exception_state);
  }

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
    StringUtf8Adaptor utf8(stripped);
    url::CanonicalizeRef(utf8.AsStringView(), &canon_output, &component);
  } else {
    url::CanonicalizeRef(stripped.View16(), &canon_output, &component);
  }

  return StringFromCanonOutput(canon_output, component);
}

}  // namespace url_pattern
}  // namespace blink

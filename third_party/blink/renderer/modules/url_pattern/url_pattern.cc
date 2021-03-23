// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/url_pattern/url_pattern.h"

#include "base/strings/string_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/bindings/modules/v8/usv_string_or_url_pattern_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_url_pattern_component_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_url_pattern_result.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

// A struct representing all the information needed to match a particular
// component of a URL.
class URLPattern::Component final
    : public GarbageCollected<URLPattern::Component> {
 public:
  bool Match(StringView input, Vector<String>* group_list) const {
    return regexp->Match(input, /*start_from=*/0, /*match_length=*/nullptr,
                         group_list) == 0;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(regexp); }

  // The parsed pattern.
  liburlpattern::Pattern pattern;

  // The pattern compiled down to a js regular expression.
  Member<ScriptRegexp> regexp;

  // The names to be applied to the regular expression capture groups.  Note,
  // liburlpattern regular expressions do not use named capture groups directly.
  Vector<String> name_list;

  Component(liburlpattern::Pattern p, ScriptRegexp* r, Vector<String> n)
      : pattern(p), regexp(r), name_list(std::move(n)) {}
};

namespace {

// The default pattern string for components that are not specified in the
// URLPattern constructor.
const char* kDefaultPattern = "*";

// The liburlpattern::Options to use for most component patterns.  We
// default to strict mode and case sensitivity.  In addition, most
// components have no concept of a delimiter or prefix character.
const liburlpattern::Options& DefaultOptions() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(liburlpattern::Options, options,
                                  ({.delimiter_list = "",
                                    .prefix_list = "",
                                    .sensitive = true,
                                    .strict = true}));
  return options;
}

// The liburlpattern::Options to use for hostname patterns.  This uses a
// "." delimiter controlling how far a named group like ":bar" will match
// by default.  Note, hostnames are case insensitive but we require case
// sensitivity here.  This assumes that the hostname values have already
// been normalized to lower case as in URL().
const liburlpattern::Options& HostnameOptions() {
  DEFINE_STATIC_LOCAL(liburlpattern::Options, options,
                      ({.delimiter_list = ".",
                        .prefix_list = "",
                        .sensitive = true,
                        .strict = true}));
  return options;
}

// The liburlpattern::Options to use for pathname patterns.  This uses a
// "/" delimiter controlling how far a named group like ":bar" will match
// by default.  It also configures "/" to be treated as an automatic
// prefix before groups.
const liburlpattern::Options& PathnameOptions() {
  DEFINE_STATIC_LOCAL(liburlpattern::Options, options,
                      ({.delimiter_list = "/",
                        .prefix_list = "/",
                        .sensitive = true,
                        .strict = true}));
  return options;
}

// An enum indicating whether the associated component values be operated
// on are for patterns or URLs.  Validation and canonicalization will
// do different things depending on the type.
enum class ValueType {
  kPattern,
  kURL,
};

// Utility function to determine if a pathname is absolute or not.  For
// kURL values this mainly consists of a check for a leading slash.  For
// patterns we do some additional checking for escaped or grouped slashes.
bool IsAbsolutePathname(const String& pathname, ValueType type) {
  if (pathname.IsEmpty())
    return false;

  if (pathname[0] == '/')
    return true;

  if (type == ValueType::kURL)
    return false;

  if (pathname.length() < 2)
    return false;

  // Patterns treat escaped slashes and slashes within an explicit grouping as
  // valid leading slashes.  For example, "\/foo" or "{/foo}".  Patterns do
  // not consider slashes within a custom regexp group as valid for the leading
  // pathname slash for now.  To support that we would need to be able to
  // detect things like ":name_123(/foo)" as a valid leading group in a pattern,
  // but that is considered too complex for now.
  if ((pathname[0] == '\\' || pathname[0] == '{') && pathname[1] == '/') {
    return true;
  }

  return false;
}

String StringFromCanonOutput(const url::CanonOutput& output,
                             const url::Component& component) {
  return String::FromUTF8(output.data() + component.begin, component.len);
}

std::string StdStringFromCanonOutput(const url::CanonOutput& output,
                                     const url::Component& component) {
  return std::string(output.data() + component.begin, component.len);
}

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the protocol component.
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

// Utility function to canonicalize a protocol string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
String CanonicalizeProtocol(const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  bool result = false;
  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (input.Is8Bit()) {
    StringUTF8Adaptor utf8(input);
    result = url::CanonicalizeScheme(
        utf8.data(), url::Component(0, utf8.size()), &canon_output, &component);
  } else {
    result = url::CanonicalizeScheme(input.Characters16(),
                                     url::Component(0, input.length()),
                                     &canon_output, &component);
  }

  if (!result) {
    exception_state.ThrowTypeError("Invalid protocol '" + input + "'.");
    return String();
  }

  return StringFromCanonOutput(canon_output, component);
}

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the username component.
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

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the password component.
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

// Utility function to canonicalize username and/or password strings. Throws
// an exception if either is invalid.  The canonicalization and/or validation
// will differ depending on whether |type| is kURL or kPattern.  On success
// |username_out| and |password_out| will contain the canonical values.
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

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the hostname component.
absl::StatusOr<std::string> HostnameEncodeCallback(absl::string_view input) {
  if (input.empty())
    return std::string();

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

// Utility function to canonicalize a hostname string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
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

// Utility function to determine if the default port for the given protocol
// matches the given port number.
bool IsProtocolDefaultPort(const String& protocol, const String& port) {
  if (protocol.IsEmpty() || port.IsEmpty())
    return false;

  bool port_ok = false;
  int port_number =
      port.Impl()->ToInt(WTF::NumberParsingOptions::kNone, &port_ok);
  if (!port_ok)
    return false;

  StringUTF8Adaptor protocol_utf8(protocol);
  int default_port =
      url::DefaultPortForScheme(protocol_utf8.data(), protocol_utf8.size());
  return default_port != url::PORT_UNSPECIFIED && default_port == port_number;
}

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the port component.
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

// Utility function to canonicalize a port string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.  The |protocol|
// must be provided in order to handle default ports correctly.
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
  if (!input.IsEmpty()) {
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

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the pathname component using "standard" URL
// behavior.
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

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the pathname component using "path" URL
// behavior.  This is like "cannot-be-a-base" URL behavior in the spec.
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

// Utility function to canonicalize a pathname string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
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
  if (protocol.IsEmpty()) {
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

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the search component.
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

// Utility function to canonicalize a search string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
String CanonicalizeSearch(const String& input,
                          ValueType type,
                          ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (input.Is8Bit()) {
    StringUTF8Adaptor utf8(input);
    url::CanonicalizeQuery(utf8.data(), url::Component(0, utf8.size()),
                           /*converter=*/nullptr, &canon_output, &component);
  } else {
    url::CanonicalizeQuery(input.Characters16(),
                           url::Component(0, input.length()),
                           /*converter=*/nullptr, &canon_output, &component);
  }

  return StringFromCanonOutput(canon_output, component);
}

// A callback to be passed to the liburlpattern::Parse() method that performs
// validation and encoding for the hash component.
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

// Utility function to canonicalize a hash string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
String CanonicalizeHash(const String& input,
                        ValueType type,
                        ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    // Canonicalization for patterns is handled during compilation via
    // encoding callbacks.
    return input;
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (input.Is8Bit()) {
    StringUTF8Adaptor utf8(input);
    url::CanonicalizeRef(utf8.data(), url::Component(0, utf8.size()),
                         &canon_output, &component);
  } else {
    url::CanonicalizeRef(input.Characters16(),
                         url::Component(0, input.length()), &canon_output,
                         &component);
  }

  return StringFromCanonOutput(canon_output, component);
}

// A utility method that takes a URLPatternInit, splits it apart, and applies
// the individual component values in the given set of strings.  The strings
// are only applied if a value is present in the init structure.
void ApplyInit(const URLPatternInit* init,
               ValueType type,
               String& protocol,
               String& username,
               String& password,
               String& hostname,
               String& port,
               String& pathname,
               String& search,
               String& hash,
               ExceptionState& exception_state) {
  // If there is a baseURL we need to apply its component values first.  The
  // rest of the URLPatternInit structure will then later override these
  // values.  Note, the baseURL will always set either an empty string or
  // longer value for each considered component.  We do not allow null strings
  // to persist for these components past this phase since they should no
  // longer be treated as wildcards.
  KURL base_url;
  if (init->hasBaseURL()) {
    base_url = KURL(init->baseURL());
    if (!base_url.IsValid() || base_url.IsEmpty()) {
      exception_state.ThrowTypeError("Invalid baseURL '" + init->baseURL() +
                                     "'.");
      return;
    }

    protocol = base_url.Protocol() ? base_url.Protocol() : g_empty_string;
    username = base_url.User() ? base_url.User() : g_empty_string;
    password = base_url.Pass() ? base_url.Pass() : g_empty_string;
    hostname = base_url.Host() ? base_url.Host() : g_empty_string;
    port =
        base_url.Port() > 0 ? String::Number(base_url.Port()) : g_empty_string;
    pathname = base_url.GetPath() ? base_url.GetPath() : g_empty_string;

    // Do no propagate search or hash from the base URL.  This matches the
    // behavior when resolving a relative URL against a base URL.
  }

  // Apply the URLPatternInit component values on top of the default and
  // baseURL values.
  if (init->hasProtocol()) {
    protocol = CanonicalizeProtocol(init->protocol(), type, exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasUsername() || init->hasPassword()) {
    String init_username = init->hasUsername() ? init->username() : String();
    String init_password = init->hasPassword() ? init->password() : String();
    CanonicalizeUsernameAndPassword(init_username, init_password, type,
                                    username, password, exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasHostname()) {
    hostname = CanonicalizeHostname(init->hostname(), type, exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasPort()) {
    port = CanonicalizePort(init->port(), type, protocol, exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasPathname()) {
    pathname = init->pathname();
    if (base_url.IsValid() && base_url.IsHierarchical() &&
        !IsAbsolutePathname(pathname, type)) {
      // Find the last slash in the baseURL pathname.  Since the URL is
      // hierarchical it should have a slash to be valid, but we are cautious
      // and check.  If there is no slash then we cannot use resolve the
      // relative pathname and just treat the init pathname as an absolute
      // value.
      auto slash_index = base_url.GetPath().ReverseFind("/");
      if (slash_index != kNotFound) {
        // Extract the baseURL path up to and including the first slash.  Append
        // the relative init pathname to it.
        pathname = base_url.GetPath().Substring(0, slash_index + 1) + pathname;
      }
    }
    pathname = CanonicalizePathname(protocol, pathname, type, exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasSearch()) {
    search = CanonicalizeSearch(init->search(), type, exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasHash()) {
    hash = CanonicalizeHash(init->hash(), type, exception_state);
    if (exception_state.HadException())
      return;
  }
}

}  // namespace

URLPattern* URLPattern::Create(const URLPatternInit* init,
                               ExceptionState& exception_state) {
  // Each component defaults to a wildcard matching any input.  We use
  // the null string as a shorthand for the default.
  String protocol;
  String username;
  String password;
  String hostname;
  String port;
  String pathname;
  String search;
  String hash;

  // Apply the input URLPatternInit on top of the default values.
  ApplyInit(init, ValueType::kPattern, protocol, username, password, hostname,
            port, pathname, search, hash, exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Manually canonicalize port patterns that exactly match the default
  // port for the protocol.  We must do this separately from the compile
  // since the liburlpattern::Parse() method will invoke encoding callbacks
  // for partial values within the pattern and this transformation must apply
  // to the entire value.
  if (IsProtocolDefaultPort(protocol, port))
    port = "";

  // Compile each component pattern into a Component structure that can
  // be used for matching.  Components that match any input may have a
  // nullptr Component struct pointer.

  auto* protocol_component =
      CompilePattern(protocol, "protocol", ProtocolEncodeCallback,
                     DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* username_component =
      CompilePattern(username, "username", UsernameEncodeCallback,
                     DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* password_component =
      CompilePattern(password, "password", PasswordEncodeCallback,
                     DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hostname_component =
      CompilePattern(hostname, "hostname", HostnameEncodeCallback,
                     HostnameOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* port_component = CompilePattern(port, "port", PortEncodeCallback,
                                        DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Different types of URLs use different canonicalization for pathname.
  // A "standard" URL flattens `.`/`..` and performs full percent encoding.
  // A "path" URL does not flatten and uses a more lax percent encoding.
  // The spec calls "path" URLs as "cannot-be-a-base-URL" URLs:
  //
  //  https://url.spec.whatwg.org/#cannot-be-a-base-url-path-state
  //
  // We prefer "standard" URL here by checking to see if the protocol
  // pattern matches any of the known standard protocol strings.  So
  // an exact pattern of `http` will match, but so will `http{s}?` and
  // `*`.
  //
  // If the protocol pattern does not match any of the known standard URL
  // protocols then we fall back to the "path" URL behavior.  This will
  // normally be triggered by `data`, `javascript`, `about`, etc.  It
  // will also be triggered for custom protocol strings.  We favor "path"
  // behavior here because its better to under canonicalize since the
  // developer can always manually canonicalize the pathname for a custom
  // protocol.
  //
  // ShouldTreatAsStandardURL can by a bit expensive, so only do it if we
  // actually have a pathname pattern to compile.
  liburlpattern::EncodeCallback pathname_encode = PathURLPathnameEncodeCallback;
  if (!pathname.IsNull() && ShouldTreatAsStandardURL(protocol_component)) {
    pathname_encode = StandardURLPathnameEncodeCallback;
  }

  auto* pathname_component =
      CompilePattern(pathname, "pathname", pathname_encode, PathnameOptions(),
                     exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* search_component =
      CompilePattern(search, "search", SearchEncodeCallback, DefaultOptions(),
                     exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hash_component = CompilePattern(hash, "hash", HashEncodeCallback,
                                        DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  return MakeGarbageCollected<URLPattern>(
      protocol_component, username_component, password_component,
      hostname_component, port_component, pathname_component, search_component,
      hash_component, base::PassKey<URLPattern>());
}

URLPattern::URLPattern(Component* protocol,
                       Component* username,
                       Component* password,
                       Component* hostname,
                       Component* port,
                       Component* pathname,
                       Component* search,
                       Component* hash,
                       base::PassKey<URLPattern> key)
    : protocol_(protocol),
      username_(username),
      password_(password),
      hostname_(hostname),
      port_(port),
      pathname_(pathname),
      search_(search),
      hash_(hash) {}

bool URLPattern::test(URLPatternInit* input,
                      ExceptionState& exception_state) const {
  return MatchInit(input, /*result=*/nullptr, exception_state);
}

bool URLPattern::test(const String& input,
                      const String& base_url,
                      ExceptionState& exception_state) const {
  return MatchString(input, base_url, /*result=*/nullptr, exception_state);
}

bool URLPattern::test(const String& input,
                      ExceptionState& exception_state) const {
  return test(input, /*base_url=*/String(), exception_state);
}

URLPatternResult* URLPattern::exec(URLPatternInit* input,
                                   ExceptionState& exception_state) const {
  URLPatternResult* result = URLPatternResult::Create();
  if (!MatchInit(input, result, exception_state))
    return nullptr;
  return result;
}

URLPatternResult* URLPattern::exec(const String& input,
                                   const String& base_url,
                                   ExceptionState& exception_state) const {
  URLPatternResult* result = URLPatternResult::Create();
  if (!MatchString(input, base_url, result, exception_state))
    return nullptr;
  return result;
}

URLPatternResult* URLPattern::exec(const String& input,
                                   ExceptionState& exception_state) const {
  return exec(input, /*base_url=*/String(), exception_state);
}

String URLPattern::protocol() const {
  if (!protocol_)
    return kDefaultPattern;
  std::string result = protocol_->pattern.GeneratePatternString();
  return String::FromUTF8(result);
}

String URLPattern::username() const {
  if (!username_)
    return kDefaultPattern;
  std::string result = username_->pattern.GeneratePatternString();
  return String::FromUTF8(result);
}

String URLPattern::password() const {
  if (!password_)
    return kDefaultPattern;
  std::string result = password_->pattern.GeneratePatternString();
  return String::FromUTF8(result);
}

String URLPattern::hostname() const {
  if (!hostname_)
    return kDefaultPattern;
  std::string result = hostname_->pattern.GeneratePatternString();
  return String::FromUTF8(result);
}

String URLPattern::port() const {
  if (!port_)
    return kDefaultPattern;
  std::string result = port_->pattern.GeneratePatternString();
  return String::FromUTF8(result);
}

String URLPattern::pathname() const {
  if (!pathname_)
    return kDefaultPattern;
  std::string result = pathname_->pattern.GeneratePatternString();
  return String::FromUTF8(result);
}

String URLPattern::search() const {
  if (!search_)
    return kDefaultPattern;
  std::string result = search_->pattern.GeneratePatternString();
  return String::FromUTF8(result);
}

String URLPattern::hash() const {
  if (!hash_)
    return kDefaultPattern;
  std::string result = hash_->pattern.GeneratePatternString();
  return String::FromUTF8(result);
}

void URLPattern::Trace(Visitor* visitor) const {
  visitor->Trace(protocol_);
  visitor->Trace(username_);
  visitor->Trace(password_);
  visitor->Trace(hostname_);
  visitor->Trace(port_);
  visitor->Trace(pathname_);
  visitor->Trace(search_);
  visitor->Trace(hash_);
  ScriptWrappable::Trace(visitor);
}

// static
URLPattern::Component* URLPattern::CompilePattern(
    const String& pattern,
    StringView component,
    liburlpattern::EncodeCallback encode_callback,
    const liburlpattern::Options& options,
    ExceptionState& exception_state) {
  // If the pattern is null then optimize by not compiling a pattern.  Instead,
  // a nullptr Component is interpreted as matching any input value.
  if (pattern.IsNull())
    return nullptr;

  // Parse the pattern.
  StringUTF8Adaptor utf8(pattern);
  auto parse_result =
      liburlpattern::Parse(absl::string_view(utf8.data(), utf8.size()),
                           std::move(encode_callback), options);
  if (!parse_result.ok()) {
    exception_state.ThrowTypeError("Invalid " + component + " pattern '" +
                                   pattern + "'.");
    return nullptr;
  }

  // Extract a regular expression string from the parsed pattern.
  std::vector<std::string> name_list;
  std::string regexp_string =
      parse_result.value().GenerateRegexString(&name_list);

  // Compile the regular expression to verify it is valid.
  auto case_sensitive = options.sensitive ? WTF::kTextCaseSensitive
                                          : WTF::kTextCaseASCIIInsensitive;
  DCHECK(base::IsStringASCII(regexp_string));
  ScriptRegexp* regexp = MakeGarbageCollected<ScriptRegexp>(
      String(regexp_string.data(), regexp_string.size()), case_sensitive,
      kMultilineDisabled, ScriptRegexp::UTF16);
  if (!regexp->IsValid()) {
    // TODO: Figure out which embedded regex expression caused the failure
    //       by compiling each pattern kRegex part individually.
    exception_state.ThrowTypeError("Invalid " + component + " pattern '" +
                                   pattern + "'.");
    return nullptr;
  }

  Vector<String> wtf_name_list;
  wtf_name_list.ReserveInitialCapacity(
      static_cast<wtf_size_t>(name_list.size()));
  for (const auto& name : name_list) {
    wtf_name_list.push_back(String::FromUTF8(name.data(), name.size()));
  }

  return MakeGarbageCollected<URLPattern::Component>(
      std::move(parse_result.value()), std::move(regexp),
      std::move(wtf_name_list));
}

bool URLPattern::MatchInit(URLPatternInit* input,
                           URLPatternResult* result,
                           ExceptionState& exception_state) const {
  // By default each URL component value starts with an empty string.  The
  // given input is then layered on top of these defaults.
  String protocol(g_empty_string);
  String username(g_empty_string);
  String password(g_empty_string);
  String hostname(g_empty_string);
  String port(g_empty_string);
  String pathname(g_empty_string);
  String search(g_empty_string);
  String hash(g_empty_string);

  // Layer the URLPatternInit values on top of the default empty strings.
  ApplyInit(input, ValueType::kURL, protocol, username, password, hostname,
            port, pathname, search, hash, exception_state);
  if (exception_state.HadException()) {
    // Treat exceptions simply as a failure to match.
    exception_state.ClearException();
    return false;
  }

  bool success = MatchInternal(protocol, username, password, hostname, port,
                               pathname, search, hash, result, exception_state);

  if (success && result)
    result->setInput(USVStringOrURLPatternInit::FromURLPatternInit(input));

  return success;
}

bool URLPattern::MatchString(const String& input,
                             const String& base_url,
                             URLPatternResult* result,
                             ExceptionState& exception_state) const {
  // By default each URL component value starts with an empty string.  The
  // given input is then layered on top of these defaults.
  String protocol(g_empty_string);
  String username(g_empty_string);
  String password(g_empty_string);
  String hostname(g_empty_string);
  String port(g_empty_string);
  String pathname(g_empty_string);
  String search(g_empty_string);
  String hash(g_empty_string);

  KURL parsed_base_url(base_url);
  if (base_url && !parsed_base_url.IsValid()) {
    // Treat as failure to match, but don't throw an exception.
    return false;
  }

  // The compile the input string as a fully resolved URL.
  KURL url(parsed_base_url, input);
  if (!url.IsValid() || url.IsEmpty()) {
    // Treat as failure to match, but don't throw an exception.
    return false;
  }

  // Apply the parsed URL components on top of our defaults.
  if (url.Protocol())
    protocol = url.Protocol();
  if (url.User())
    username = url.User();
  if (url.Pass())
    password = url.Pass();
  if (url.Host())
    hostname = url.Host();
  if (url.Port() > 0)
    port = String::Number(url.Port());
  if (url.GetPath())
    pathname = url.GetPath();
  if (url.Query())
    search = url.Query();
  if (url.FragmentIdentifier())
    hash = url.FragmentIdentifier();

  bool success = MatchInternal(protocol, username, password, hostname, port,
                               pathname, search, hash, result, exception_state);

  if (success && result) {
    // If there is a base_url provided we expose the resulting fully resolved
    // input in the result.  This is a bit weird since otherwise you get a
    // non-canonicalized `result.input`, but it seems less weird than exposing
    // an array or secondary property for the baseURL value.
    result->setInput(USVStringOrURLPatternInit::FromUSVString(
        base_url ? url.GetString() : input));
  }

  return success;
}

bool URLPattern::MatchInternal(const String& protocol,
                               const String& username,
                               const String& password,
                               const String& hostname,
                               const String& port,
                               const String& pathname,
                               const String& search,
                               const String& hash,
                               URLPatternResult* result,
                               ExceptionState& exception_state) const {
  Vector<String> protocol_group_list;
  Vector<String> username_group_list;
  Vector<String> password_group_list;
  Vector<String> hostname_group_list;
  Vector<String> port_group_list;
  Vector<String> pathname_group_list;
  Vector<String> search_group_list;
  Vector<String> hash_group_list;

  // If we are not generating a full result then we don't need to populate
  // group lists.
  auto* protocol_group_list_ref = result ? &protocol_group_list : nullptr;
  auto* username_group_list_ref = result ? &username_group_list : nullptr;
  auto* password_group_list_ref = result ? &password_group_list : nullptr;
  auto* hostname_group_list_ref = result ? &hostname_group_list : nullptr;
  auto* port_group_list_ref = result ? &port_group_list : nullptr;
  auto* pathname_group_list_ref = result ? &pathname_group_list : nullptr;
  auto* search_group_list_ref = result ? &search_group_list : nullptr;
  auto* hash_group_list_ref = result ? &hash_group_list : nullptr;

  // Each component of the pattern must match the corresponding component of
  // the input.  If a pattern Component is nullptr, then it matches any
  // input and we can avoid running a real regular expression match.
  bool matched =
      (!protocol_ || protocol_->Match(protocol, protocol_group_list_ref)) &&
      (!username_ || username_->Match(username, username_group_list_ref)) &&
      (!password_ || password_->Match(password, password_group_list_ref)) &&
      (!hostname_ || hostname_->Match(hostname, hostname_group_list_ref)) &&
      (!port_ || port_->Match(port, port_group_list_ref)) &&
      (!pathname_ || pathname_->Match(pathname, pathname_group_list_ref)) &&
      (!search_ || search_->Match(search, search_group_list_ref)) &&
      (!hash_ || hash_->Match(hash, hash_group_list_ref));

  if (!matched || !result)
    return matched;

  result->setProtocol(
      MakeComponentResult(protocol_, protocol, protocol_group_list));
  result->setUsername(
      MakeComponentResult(username_, username, username_group_list));
  result->setPassword(
      MakeComponentResult(password_, password, password_group_list));
  result->setHostname(
      MakeComponentResult(hostname_, hostname, hostname_group_list));
  result->setPort(MakeComponentResult(port_, port, port_group_list));
  result->setPathname(
      MakeComponentResult(pathname_, pathname, pathname_group_list));
  result->setSearch(MakeComponentResult(search_, search, search_group_list));
  result->setHash(MakeComponentResult(hash_, hash, hash_group_list));
  return true;
}

// static
URLPatternComponentResult* URLPattern::MakeComponentResult(
    Component* component,
    const String& input,
    const Vector<String>& group_list) {
  Vector<std::pair<String, String>> groups;
  if (!component) {
    // When there is not Component we must act as if there was a default
    // wildcard pattern with a group.  The group includes the entire input.
    groups.emplace_back("0", input);
  } else {
    DCHECK_EQ(component->name_list.size(), group_list.size());
    for (wtf_size_t i = 0; i < group_list.size(); ++i) {
      groups.emplace_back(component->name_list[i], group_list[i]);
    }
  }

  auto* result = URLPatternComponentResult::Create();
  result->setInput(input);
  result->setGroups(groups);
  return result;
}

bool URLPattern::ShouldTreatAsStandardURL(Component* protocol) {
  if (!protocol)
    return true;
  const auto protocol_matches = [&](const std::string& scheme) {
    DCHECK(base::IsStringASCII(scheme));
    return protocol->Match(
        StringView(scheme.data(), static_cast<unsigned>(scheme.size())),
        /*group_list=*/nullptr);
  };
  return base::ranges::any_of(url::GetStandardSchemes(), protocol_matches);
}

}  // namespace blink

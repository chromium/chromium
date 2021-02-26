// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/url_pattern/url_pattern.h"

#include "base/i18n/uchar.h"
#include "base/strings/string_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/bindings/modules/v8/usv_string_or_url_pattern_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_url_pattern_component_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_url_pattern_result.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/liburlpattern/parse.h"
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

  // The pattern compiled down to a js regular expression.
  Member<ScriptRegexp> regexp;

  // The names to be applied to the regular expression capture groups.  Note,
  // liburlpattern regular expressions do not use named capture groups directly.
  Vector<String> name_list;

  Component(ScriptRegexp* r, Vector<String> n)
      : regexp(r), name_list(std::move(n)) {}
};

namespace {

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
                             url::Component component) {
  return String::FromUTF8(output.data() + component.begin, component.len);
}

// Utility function to canonicalize a protocol string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
String CanonicalizeProtocol(const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
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
    result = url::CanonicalizeScheme(
        base::i18n::ToChar16Ptr(input.Characters16()),
        url::Component(0, input.length()), &canon_output, &component);
  }

  if (!result) {
    exception_state.ThrowTypeError("Invalid protocol '" + input + "'.");
    return String();
  }

  return StringFromCanonOutput(canon_output, component);
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
        base::i18n::ToChar16Ptr(username16.Characters16()),
        url::Component(0, username16.length()),
        base::i18n::ToChar16Ptr(password16.Characters16()),
        url::Component(0, password16.length()), &canon_output,
        &username_component, &password_component);
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

// Utility function to canonicalize a hostname string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
String CanonicalizeHostname(const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
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

// Utility function to canonicalize a port string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.  The |protocol|
// must be provided in order to handle default ports correctly.
String CanonicalizePort(const String& input,
                        ValueType type,
                        const String& protocol,
                        ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
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

// Utility function to canonicalize a pathname string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
String CanonicalizePathname(const String& input,
                            ValueType type,
                            ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    return input;
  }

  if (!IsAbsolutePathname(input, type)) {
    exception_state.ThrowTypeError("Cannot resolve absolute pathname  for '" +
                                   input + "'.");
    return String();
  }

  bool result = false;
  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (input.Is8Bit()) {
    StringUTF8Adaptor utf8(input);
    result = url::CanonicalizePath(utf8.data(), url::Component(0, utf8.size()),
                                   &canon_output, &component);
  } else {
    result = url::CanonicalizePath(
        base::i18n::ToChar16Ptr(input.Characters16()),
        url::Component(0, input.length()), &canon_output, &component);
  }

  if (!result) {
    exception_state.ThrowTypeError("Invalid pathname '" + input + "'.");
    return String();
  }

  return StringFromCanonOutput(canon_output, component);
}

// Utility function to canonicalize a search string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
String CanonicalizeSearch(const String& input,
                          ValueType type,
                          ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    return input;
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (input.Is8Bit()) {
    StringUTF8Adaptor utf8(input);
    url::CanonicalizeQuery(utf8.data(), url::Component(0, utf8.size()),
                           /*converter=*/nullptr, &canon_output, &component);
  } else {
    url::CanonicalizeQuery(base::i18n::ToChar16Ptr(input.Characters16()),
                           url::Component(0, input.length()),
                           /*converter=*/nullptr, &canon_output, &component);
  }

  return StringFromCanonOutput(canon_output, component);
}

// Utility function to canonicalize a hash string.  Throws an exception
// if the input is invalid.  The canonicalization and/or validation will
// differ depending on whether |type| is kURL or kPattern.
String CanonicalizeHash(const String& input,
                        ValueType type,
                        ExceptionState& exception_state) {
  if (type == ValueType::kPattern) {
    return input;
  }

  url::RawCanonOutputT<char> canon_output;
  url::Component component;
  if (input.Is8Bit()) {
    StringUTF8Adaptor utf8(input);
    url::CanonicalizeRef(utf8.data(), url::Component(0, utf8.size()),
                         &canon_output, &component);
  } else {
    url::CanonicalizeRef(base::i18n::ToChar16Ptr(input.Characters16()),
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
    CanonicalizeUsernameAndPassword(init->username(), init->password(), type,
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
    pathname = CanonicalizePathname(pathname, type, exception_state);
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

  // Compile each component pattern into a Component structure that can
  // be used for matching.  Components that match any input may have a
  // nullptr Component struct pointer.

  auto* protocol_component =
      CompilePattern(protocol, "protocol", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* username_component =
      CompilePattern(username, "username", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* password_component =
      CompilePattern(password, "password", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hostname_component =
      CompilePattern(hostname, "hostname", HostnameOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* port_component =
      CompilePattern(port, "port", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* pathname_component =
      CompilePattern(pathname, "pathname", PathnameOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* search_component =
      CompilePattern(search, "search", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hash_component =
      CompilePattern(hash, "hash", DefaultOptions(), exception_state);
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

bool URLPattern::test(const USVStringOrURLPatternInit& input,
                      ExceptionState& exception_state) const {
  return Match(input, /*result=*/nullptr, exception_state);
}

URLPatternResult* URLPattern::exec(const USVStringOrURLPatternInit& input,
                                   ExceptionState& exception_state) const {
  URLPatternResult* result = URLPatternResult::Create();
  if (!Match(input, result, exception_state))
    return nullptr;
  return result;
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
    const liburlpattern::Options& options,
    ExceptionState& exception_state) {
  // If the pattern is null then optimize by not compiling a pattern.  Instead,
  // a nullptr Component is interpreted as matching any input value.
  if (pattern.IsNull())
    return nullptr;

  // Parse the pattern.
  StringUTF8Adaptor utf8(pattern);
  auto parse_result = liburlpattern::Parse(
      absl::string_view(utf8.data(), utf8.size()), options);
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
      String(regexp_string.data(), regexp_string.size()), case_sensitive);
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

  return MakeGarbageCollected<URLPattern::Component>(std::move(regexp),
                                                     std::move(wtf_name_list));
}

bool URLPattern::Match(const USVStringOrURLPatternInit& input,
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

  if (input.IsURLPatternInit()) {
    // Layer the URLPatternInit values on top of the default empty strings.
    ApplyInit(input.GetAsURLPatternInit(), ValueType::kURL, protocol, username,
              password, hostname, port, pathname, search, hash,
              exception_state);
    if (exception_state.HadException()) {
      // Treat exceptions simply as a failure to match.
      exception_state.ClearException();
      return false;
    }
  } else {
    DCHECK(input.IsUSVString());

    // The compile the input string as a fully resolved URL.
    KURL url(input.GetAsUSVString());
    if (!url.IsValid() || url.IsEmpty()) {
      // Treat as failure to match, but don't throw an exception.
      return false;
    }

    // TODO: Support relative URLs here by taking a string in a second argument.

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
  }

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

  // TODO: The result.input contains the data before canonicalization, but the
  //       component results will contain inputs after canonicalization.  Is
  //       this what we want?  See: https://github.com/WICG/urlpattern/issues/34
  result->setInput(input);

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

}  // namespace blink

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/url_pattern/url_pattern.h"

#include "base/strings/string_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/bindings/modules/v8/usv_string_or_url_pattern_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

// A struct representing all the information needed to match a particular
// component of a URL.
struct URLPattern::Component {
  // The pattern compiled down to a js regular expression.
  std::unique_ptr<ScriptRegexp> regexp;

  // The names to be applied to the regular expression capture groups.  Note,
  // liburlpattern regular expressions do not use named capture groups directly.
  WTF::Vector<String> name_list;

  Component(std::unique_ptr<ScriptRegexp> r, WTF::Vector<String> n)
      : regexp(std::move(r)), name_list(std::move(n)) {}
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

// The default wildcard pattern used for a component when the constructor
// input does not provide an explicit value.
constexpr const char* kDefaultPattern = "(.*)";

// The default wildcard pattern for the pathname component.
constexpr const char* kDefaultPathnamePattern = "/(.*)";

// A utility method that takes a URLPatternInit, splits it apart, and applies
// the individual component values in the given set of strings.  The strings
// are only applied if a value is present in the init structure.
void ApplyInit(const URLPatternInit* init,
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
  // values.
  if (init->hasBaseURL()) {
    KURL baseURL(init->baseURL());
    if (!baseURL.IsValid() || baseURL.IsEmpty()) {
      exception_state.ThrowTypeError("Invalid baseURL '" + init->baseURL() +
                                     "'.");
      return;
    }

    if (baseURL.Protocol())
      protocol = baseURL.Protocol();
    if (baseURL.User())
      username = baseURL.User();
    if (baseURL.Pass())
      password = baseURL.Pass();
    if (baseURL.Host())
      hostname = baseURL.Host();
    if (baseURL.HasPort() && baseURL.Port() > 0)
      port = String::Number(baseURL.Port());
    if (baseURL.GetPath())
      pathname = baseURL.GetPath();

    // Do no propagate search or hash from the base URL.  This matches the
    // behavior when resolving a relative URL against a base URL.
  }

  // Apply the URLPatternInit component values on top of the default and
  // baseURL values.
  if (init->hasProtocol())
    protocol = init->protocol();
  if (init->hasUsername())
    username = init->username();
  if (init->hasPassword())
    password = init->password();
  if (init->hasHostname())
    hostname = init->hostname();
  if (init->hasPort())
    port = init->port();
  if (init->hasPathname()) {
    // TODO: handle relative pathnames
    pathname = init->pathname();
  }
  if (init->hasSearch())
    search = init->search();
  if (init->hasHash())
    hash = init->hash();
}

// Utility function that encodes the given pattern string into ASCII.
// Non-ascii characters should be percent encoded.
std::string EncodePattern(const String& input) {
  // TODO: Implement percent encoding by adapting url::EncodeURIComponent() to
  //       not escape pattern special characters.
  // TODO: Should we somehow percent encode pattern special characters that have
  //       been backslash escaped?  For example, "\{".
  return input.Utf8();
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
  ApplyInit(init, protocol, username, password, hostname, port, pathname,
            search, hash, exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Compile each component pattern into a Component structure that can
  // be used for matching.  Components that match any input may have a
  // nullptr Component struct pointer.

  auto protocol_component = CompilePattern(
      protocol, kDefaultPattern, "protocol", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto username_component = CompilePattern(
      username, kDefaultPattern, "username", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto password_component = CompilePattern(
      password, kDefaultPattern, "password", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto hostname_component =
      CompilePattern(hostname, kDefaultPattern, "hostname", HostnameOptions(),
                     exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto port_component = CompilePattern(port, kDefaultPattern, "port",
                                       DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto pathname_component =
      CompilePattern(pathname, kDefaultPathnamePattern, "pathname",
                     PathnameOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto search_component = CompilePattern(search, kDefaultPattern, "search",
                                         DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto hash_component = CompilePattern(hash, kDefaultPattern, "hash",
                                       DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  return MakeGarbageCollected<URLPattern>(
      std::move(protocol_component), std::move(username_component),
      std::move(password_component), std::move(hostname_component),
      std::move(port_component), std::move(pathname_component),
      std::move(search_component), std::move(hash_component),
      base::PassKey<URLPattern>());
}

URLPattern::URLPattern(std::unique_ptr<Component> protocol,
                       std::unique_ptr<Component> username,
                       std::unique_ptr<Component> password,
                       std::unique_ptr<Component> hostname,
                       std::unique_ptr<Component> port,
                       std::unique_ptr<Component> pathname,
                       std::unique_ptr<Component> search,
                       std::unique_ptr<Component> hash,
                       base::PassKey<URLPattern> key)
    : protocol_(std::move(protocol)),
      username_(std::move(username)),
      password_(std::move(password)),
      hostname_(std::move(hostname)),
      port_(std::move(port)),
      pathname_(std::move(pathname)),
      search_(std::move(search)),
      hash_(std::move(hash)) {}

bool URLPattern::test(const USVStringOrURLPatternInit& input,
                      ExceptionState& exception_state) {
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

  // TODO: Refactor the following input processing into a utility method that
  //       can be shared with exec().

  if (input.IsURLPatternInit()) {
    // Layer the URLPatternInit values on top of the default empty strings.
    ApplyInit(input.GetAsURLPatternInit(), protocol, username, password,
              hostname, port, pathname, search, hash, exception_state);
    if (exception_state.HadException()) {
      // Treat exceptions simply as a failure to match.
      exception_state.ClearException();
      return false;
    }
    // TODO: Canonicalize the hostname manually since we did not run the
    //       input through KURL.
    // TODO: URL encode input component values using url::EncodeURIComponent()
    //       since we did not go through KURL
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
    if (url.HasPort() && url.Port() > 0)
      port = String::Number(url.Port());
    if (url.GetPath())
      pathname = url.GetPath();
    if (url.Query())
      search = url.Query();
    if (url.FragmentIdentifier())
      hash = url.FragmentIdentifier();
  }

  // TODO: Should we do special processing for port to make "default" values
  //       match things like "80" for an http protocol?

  // Each component of the pattern must match the corresponding component of
  // the input.  If a pattern Component is nullptr, then it matches any
  // input and we can avoid running a real regular expression match.
  return (!protocol_ || protocol_->regexp->Match(protocol) == 0) &&
         (!username_ || username_->regexp->Match(username) == 0) &&
         (!password_ || password_->regexp->Match(password) == 0) &&
         (!hostname_ || hostname_->regexp->Match(hostname) == 0) &&
         (!port_ || port_->regexp->Match(port) == 0) &&
         (!pathname_ || pathname_->regexp->Match(pathname) == 0) &&
         (!search_ || search_->regexp->Match(search) == 0) &&
         (!hash_ || hash_->regexp->Match(hash) == 0);
}

URLPatternResult* URLPattern::exec(const USVStringOrURLPatternInit& input,
                                   ExceptionState& exception_state) {
  // TODO: Implement
  // TODO: Modernize ScriptRegexp() to support returning capture group values.
  exception_state.ThrowTypeError("The exec() method is not implemented yet.");
  return nullptr;
}

String URLPattern::toRegExp(const String& component,
                            ExceptionState& exception_state) {
  // TODO: Implement
  exception_state.ThrowTypeError(
      "The toRegExp() method is not implemented yet.");
  return String();
}

// static
std::unique_ptr<URLPattern::Component> URLPattern::CompilePattern(
    const String& pattern,
    const String& default_pattern,
    StringView component,
    const liburlpattern::Options& options,
    ExceptionState& exception_state) {
  // If the pattern is null or matches the component's default wildcard pattern
  // then optimize by not compiling the pattern.  Instead, a nullptr Component
  // is interpreted as matching any input value.
  // TODO: Should we match after Parse() so that we can treat different
  //       equivalent input as valid wildcards?
  if (pattern.IsNull() || pattern == default_pattern)
    return nullptr;

  // Parse the pattern.
  auto parse_result = liburlpattern::Parse(EncodePattern(pattern), options);
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
  auto regexp = std::make_unique<ScriptRegexp>(
      String(regexp_string.data(), regexp_string.size()), case_sensitive);
  if (!regexp->IsValid()) {
    // TODO: Figure out which embedded regex expression caused the failure
    //       by compiling each pattern kRegex part individually.
    exception_state.ThrowTypeError("Invalid " + component + " pattern '" +
                                   pattern + "'.");
    return nullptr;
  }

  WTF::Vector<String> wtf_name_list;
  wtf_name_list.ReserveInitialCapacity(
      static_cast<wtf_size_t>(name_list.size()));
  for (const auto& name : name_list) {
    DCHECK(base::IsStringASCII(name));
    wtf_name_list.push_back(String(name.data(), name.size()));
  }

  return std::make_unique<URLPattern::Component>(std::move(regexp),
                                                 std::move(wtf_name_list));
}

}  // namespace blink

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/url_pattern/url_pattern.h"

#include "base/strings/string_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/bindings/modules/v8/usv_string_or_url_pattern_init.h"
#include "third_party/blink/renderer/modules/url_pattern/url_pattern_component_result.h"
#include "third_party/blink/renderer/modules/url_pattern/url_pattern_result.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
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
  // values.  Note, the baseURL will always set either an empty string or
  // longer value for each considered component.  We do not allow null strings
  // to persist for these components past this phase since they should no
  // longer be treated as wildcards.
  if (init->hasBaseURL()) {
    KURL baseURL(init->baseURL());
    if (!baseURL.IsValid() || baseURL.IsEmpty()) {
      exception_state.ThrowTypeError("Invalid baseURL '" + init->baseURL() +
                                     "'.");
      return;
    }

    if (baseURL.Protocol())
      protocol = baseURL.Protocol();
    else
      protocol = g_empty_string;

    if (baseURL.User())
      username = baseURL.User();
    else
      username = g_empty_string;

    if (baseURL.Pass())
      password = baseURL.Pass();
    else
      password = g_empty_string;

    if (baseURL.Host())
      hostname = baseURL.Host();
    else
      hostname = g_empty_string;

    if (baseURL.Port() > 0)
      port = String::Number(baseURL.Port());
    else
      port = g_empty_string;

    if (baseURL.GetPath())
      pathname = baseURL.GetPath();
    else
      pathname = "/";

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
    if (pathname.IsEmpty() || pathname[0] != '/') {
      exception_state.ThrowTypeError(
          "Could not resolve absolute pathname for '" + pathname + "'.");
      return;
    }
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

  auto* protocol_component = CompilePattern(
      protocol, kDefaultPattern, "protocol", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* username_component = CompilePattern(
      username, kDefaultPattern, "username", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* password_component = CompilePattern(
      password, kDefaultPattern, "password", DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hostname_component =
      CompilePattern(hostname, kDefaultPattern, "hostname", HostnameOptions(),
                     exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* port_component = CompilePattern(port, kDefaultPattern, "port",
                                        DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* pathname_component =
      CompilePattern(pathname, kDefaultPathnamePattern, "pathname",
                     PathnameOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* search_component = CompilePattern(search, kDefaultPattern, "search",
                                          DefaultOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hash_component = CompilePattern(hash, kDefaultPattern, "hash",
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
    DCHECK(base::IsStringASCII(name));
    wtf_name_list.push_back(String(name.data(), name.size()));
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
  String pathname("/");
  String search(g_empty_string);
  String hash(g_empty_string);

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
    if (url.Port() > 0)
      port = String::Number(url.Port());
    if (url.GetPath())
      pathname = url.GetPath();
    if (url.Query())
      search = url.Query();
    if (url.FragmentIdentifier())
      hash = url.FragmentIdentifier();
  }

  // The pathname should be resolved to an absolute value before now.
  DCHECK(!pathname.IsEmpty());
  DCHECK(pathname.StartsWith("/"));

  // TODO: Should we do special processing for port to make "default" values
  //       match things like "80" for an http protocol?  See
  //       https://github.com/WICG/urlpattern/issues/31.

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
  result->setPathname(MakeComponentResult(
      pathname_, pathname, pathname_group_list, /*is_pathname=*/true));
  result->setSearch(MakeComponentResult(search_, search, search_group_list));
  result->setHash(MakeComponentResult(hash_, hash, hash_group_list));
  return true;
}

// static
URLPatternComponentResult* URLPattern::MakeComponentResult(
    Component* component,
    const String& input,
    const Vector<String>& group_list,
    bool is_pathname) {
  Vector<std::pair<String, String>> groups;
  if (!component) {
    // When there is not Component we must act as if there was a default
    // wildcard pattern with a group.  For most components the group ends
    // up including the entire input.  For pathname, however, the leading "/"
    // is excluded from the group since its considered a prefix.
    if (is_pathname) {
      DCHECK(!input.IsEmpty());
      DCHECK_EQ(input[0], '/');
      groups.emplace_back("0", input.Substring(1));
    } else {
      groups.emplace_back("0", input);
    }
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

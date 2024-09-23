// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/simple_url_pattern_matcher.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "components/url_pattern/url_pattern_util.h"
#include "third_party/liburlpattern/constructor_string_parser.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/liburlpattern/utils.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace network {

namespace {

// https://urlpattern.spec.whatwg.org/#default-options
constexpr liburlpattern::Options kDefaultOptions = {.delimiter_list = "",
                                                    .prefix_list = "",
                                                    .sensitive = true,
                                                    .strict = true};
// https://urlpattern.spec.whatwg.org/#hostname-options
constexpr liburlpattern::Options kHostnameOptions = {.delimiter_list = ".",
                                                     .prefix_list = "",
                                                     .sensitive = true,
                                                     .strict = true};
// https://urlpattern.spec.whatwg.org/#pathname-options
constexpr liburlpattern::Options kPathnameOptions = {.delimiter_list = "/",
                                                     .prefix_list = "/",
                                                     .sensitive = true,
                                                     .strict = true};

std::string EscapePatternString(std::string_view input) {
  std::string result;
  result.reserve(input.length());
  liburlpattern::EscapePatternStringAndAppend(input, result);
  return result;
}

// Utility function to determine if a pathname is absolute or not. We do some
// additional checking for escaped or grouped slashes.
//
// Note: This is partially copied from
// third_party/blink/renderer/core/url_pattern/url_pattern.cc
bool IsAbsolutePathname(std::string_view pathname) {
  if (pathname.empty()) {
    return false;
  }

  if (pathname[0] == '/') {
    return true;
  }

  if (pathname.length() < 2) {
    return false;
  }

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

std::string ResolveRelativePathnamePattern(const GURL& base_url,
                                           std::string_view pathname) {
  if (base_url.IsStandard() && !IsAbsolutePathname(pathname)) {
    std::string base_path = EscapePatternString(base_url.path());
    auto slash_index = base_path.rfind('/');
    if (slash_index != std::string::npos) {
      // Extract the baseURL path up to and including the first slash.  Append
      // the relative init pathname to it.
      base_path.resize(slash_index + 1);
      base_path += pathname;
      return base_path;
    }
  }
  return std::string(pathname);
}

}  // namespace

SimpleUrlPatternMatcher::PatternInit::PatternInit(
    std::optional<std::string> protocol,
    std::optional<std::string> username,
    std::optional<std::string> password,
    std::optional<std::string> hostname,
    std::optional<std::string> port,
    std::optional<std::string> pathname,
    std::optional<std::string> search,
    std::optional<std::string> hash)
    : protocol_(std::move(protocol)),
      username_(std::move(username)),
      password_(std::move(password)),
      hostname_(std::move(hostname)),
      port_(std::move(port)),
      pathname_(std::move(pathname)),
      search_(std::move(search)),
      hash_(std::move(hash)) {}
SimpleUrlPatternMatcher::PatternInit::~PatternInit() = default;
SimpleUrlPatternMatcher::PatternInit::PatternInit(PatternInit&&) = default;
SimpleUrlPatternMatcher::PatternInit&
SimpleUrlPatternMatcher::PatternInit::operator=(PatternInit&&) = default;

// static
base::expected<SimpleUrlPatternMatcher::Component, std::string>
SimpleUrlPatternMatcher::Component::Create(
    std::optional<std::string_view> pattern,
    liburlpattern::EncodeCallback encode_callback,
    const liburlpattern::Options& options) {
  absl::StatusOr<liburlpattern::Pattern> parse_result =
      liburlpattern::Parse(pattern.value_or("*"), encode_callback, options);
  if (!parse_result.ok()) {
    return base::unexpected("Failed to parse pattern");
  }
  if (parse_result->HasRegexGroups()) {
    return base::unexpected("Regexp groups are not supported");
  }
  std::unique_ptr<re2::RE2> regex;
  if (!parse_result->CanDirectMatch()) {
    const std::string regex_string = parse_result->GenerateRegexString();
    regex = std::make_unique<RE2>(regex_string);
    if (!regex->ok()) {
      return base::unexpected(
          base::StrCat({"Failed to compile pattern ", regex_string}));
    }
  }
  return Component(std::move(parse_result.value()), std::move(regex),
                   base::PassKey<Component>());
}

SimpleUrlPatternMatcher::Component::Component(liburlpattern::Pattern pattern,
                                              std::unique_ptr<re2::RE2> regex,
                                              base::PassKey<Component>)
    : pattern_(std::move(pattern)), regex_(std::move(regex)) {}
SimpleUrlPatternMatcher::Component::Component(Component&&) = default;
SimpleUrlPatternMatcher::Component&
SimpleUrlPatternMatcher::Component::operator=(Component&&) = default;
SimpleUrlPatternMatcher::Component::~Component() = default;

bool SimpleUrlPatternMatcher::Component::Match(std::string_view value) const {
  if (pattern_.CanDirectMatch()) {
    return pattern_.DirectMatch(value, nullptr);
  }
  CHECK(regex_);
  return RE2::FullMatch(value, *regex_);
}

// static
base::expected<std::unique_ptr<SimpleUrlPatternMatcher>, std::string>
SimpleUrlPatternMatcher::Create(std::string_view constructor_string,
                                const GURL& base_url) {
  std::optional<Component> protocol_component;
  bool protocol_matches_a_special_scheme_flag = false;
  auto pattern_result =
      CreatePatternInit(constructor_string, base_url, &protocol_component,
                        &protocol_matches_a_special_scheme_flag);
  if (!pattern_result.has_value()) {
    return base::unexpected(pattern_result.error());
  }
  return CreateFromPatternInit(pattern_result.value(),
                               std::move(protocol_component),
                               protocol_matches_a_special_scheme_flag);
}

// static
base::expected<SimpleUrlPatternMatcher::PatternInit, std::string>
SimpleUrlPatternMatcher::CreatePatternInit(
    std::string_view constructor_string,
    const GURL& base_url,
    std::optional<Component>* protocol_component_out,
    bool* protocol_matches_a_special_scheme_flag_out) {
  if (!base_url.is_valid()) {
    return base::unexpected("Invalid base URL");
  }

  // Spec: Set init to the result of running parse a constructor string given
  // input.
  // https://urlpattern.spec.whatwg.org/#parse-a-constructor-string
  liburlpattern::ConstructorStringParser constructor_string_parser(
      constructor_string);
  std::optional<Component> protocol_component;
  bool protocol_matches_a_special_scheme_flag = false;
  absl::Status result = constructor_string_parser.Parse(
      [&protocol_component, &protocol_matches_a_special_scheme_flag](
          std::string_view protocol_string) -> absl::StatusOr<bool> {
        // Spec: Let protocol component be the result of compiling a component
        // given protocol string, canonicalize a protocol, and default options.
        auto component_result = Component::Create(
            protocol_string, url_pattern::ProtocolEncodeCallback,
            kDefaultOptions);
        if (!component_result.has_value()) {
          return absl::InvalidArgumentError(
              base::StrCat({component_result.error(), " for protocol"}));
        }
        protocol_component = std::move(component_result.value());
        // Spec: If the result of running protocol component matches a special
        // scheme given protocol component is true, then set parser’s protocol
        // matches a special scheme flag to true.
        protocol_matches_a_special_scheme_flag = base::ranges::any_of(
            url::GetStandardSchemes(),
            [&protocol_component](const std::string& scheme) {
              return protocol_component->Match(scheme);
            });
        return protocol_matches_a_special_scheme_flag;
      });
  if (!result.ok()) {
    return base::unexpected(std::string(result.message()));
  }
  // `protocol_matcher_out` can be nullptr in tests.
  if (protocol_component_out && protocol_component) {
    *protocol_component_out = std::move(*protocol_component);
  }
  // `protocol_matches_a_special_scheme_flag_out` can be nullptr in tests.
  if (protocol_matches_a_special_scheme_flag_out) {
    *protocol_matches_a_special_scheme_flag_out =
        protocol_matches_a_special_scheme_flag;
  }
  const liburlpattern::ConstructorStringParser::Result& init =
      constructor_string_parser.GetResult();

  // Spec: Let processedInit be the result of process a URLPatternInit given
  // init, "pattern", null, null, null, null, null, null, null, and null.
  //
  // The following code are running shortcuts of the steps of "process a
  // URLPatternInit".
  // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
  //
  // [protocol]
  // - Spec: If init["protocol"] does not exist, then set result["protocol"] to
  //   the result of processing a base URL string given baseURL’s scheme and
  //   type.
  // Note: "process a base URL string" returns the result of "escaping a pattern
  //       string" when |type| is "pattern".
  // - Spec: If init["protocol"] exists, then set result["protocol"] to the
  //   result of process protocol for init given init["protocol"] and type.
  // Note: "process protocol for init" removes a single trailing ":". But
  //       ConstructorStringParser doesn't set the trailing ":". So we don't
  //       need the logic for the trailing ":" removal.
  std::optional<std::string> protocol =
      init.protocol ? std::string(*init.protocol)
                    : EscapePatternString(base_url.scheme());
  // [username]
  // - Spec: If type is not "pattern" and init contains none of "protocol",
  //   "hostname", "port" and "username", then set result["username"] to the
  //   result of processing a base URL string given baseURL’s username and type.
  // Note: We do nothing here because type is "pattern".
  // - Spec: If init["username"] exists, then set result["username"] to the
  //   result of process username for init given init["username"] and type.
  // Note: "process username for init" do nothing when type is "pattern".
  std::optional<std::string> username =
      init.username ? std::make_optional(std::string(*init.username))
                    : std::nullopt;
  // [password]
  // - Spec: If type is not "pattern" and init contains none of "protocol",
  //   "hostname", "port", "username" and "password", then set
  //   result["password"] to the result of processing a base URL string given
  //   baseURL’s password and type.
  // Note: We do nothing here because type is "pattern".
  // - Spec: If init["password"] exists, then set result["password"] to the
  //   result of process password for init given init["password"] and type.
  // Note: "process password for init" do nothing when type is "pattern".
  std::optional<std::string> password =
      init.password ? std::make_optional(std::string(*init.password))
                    : std::nullopt;
  // [hostname]
  // - Spec: If init contains neither "protocol" nor "hostname", then:
  //   - Let baseHost be baseURL’s host.
  //   - If baseHost is null, then set baseHost to the empty string.
  //   - Set result["hostname"] to the result of processing a base URL string
  //     given baseHost and type.
  // - Spec: If init["hostname"] exists, then set result["hostname"] to the
  //   result of process hostname for init given init["hostname"] and type.
  // Note: "process hostname for init" do nothing when type is "pattern".
  std::optional<std::string> hostname =
      init.hostname
          ? std::make_optional(std::string(*init.hostname))
          : (init.protocol
                 ? std::nullopt
                 : std::make_optional(EscapePatternString(base_url.host())));
  // [port]
  // - Spec: If init contains none of "protocol", "hostname", and "port", then:
  //   - If baseURL’s port is null, then set result["port"] to the empty string.
  //   - Otherwise, set result["port"] to baseURL’s port, serialized.
  // - Spec: If init["port"] exists, then set result["port"] to the result of
  //   process port for init given init["port"], result["protocol"], and type.
  // Note: "process port for init" do nothing when type is "pattern".
  std::optional<std::string> port =
      init.port ? std::make_optional(std::string(*init.port))
                : ((init.protocol || init.hostname)
                       ? std::nullopt
                       : std::make_optional(base_url.port()));
  // [pathname]
  // - Spec: If init contains none of "protocol", "hostname", "port", and
  //   "pathname", then set result["pathname"] to the result of processing a
  //   base URL string given the result of URL path serializing baseURL and
  //   type.
  // - Spec: If init["pathname"] exists:
  //   - Set result["pathname"] to init["pathname"].
  //   - If the following are all true:
  //     * baseURL is not null;
  //     * baseURL has an opaque path; and
  //     * the result of running is an absolute pathname given
  //       result["pathname"] and type is false,
  //    then:
  //    -  Let baseURLPath be the result of running process a base URL string
  //       given the result of URL path serializing baseURL and type.
  //    - Let slash index be the index of the last U+002F (/) code point found
  //      in baseURLPath, interpreted as a sequence of code points, or null if
  //      there are no instances of the code point.
  //    - If slash index is not null:
  //      - Let new pathname be the code point substring from 0 to slash index +
  //        1 within baseURLPath.
  //      - Append result["pathname"] to the end of new pathname.
  //      - Set result["pathname"] to new pathname.
  //   - Set result["pathname"] to the result of process pathname for init given
  //     result["pathname"], result["protocol"], and type.
  // Note: The second logic is implemented in ResolveRelativePathnamePattern().
  //       "process pathname for init" do nothing when type is "pattern".
  std::optional<std::string> pathname =
      init.pathname
          ? std::make_optional(
                ResolveRelativePathnamePattern(base_url, *init.pathname))
          : ((init.protocol || init.hostname || init.port)
                 ? std::nullopt
                 : std::make_optional(EscapePatternString(base_url.path())));
  // [search]
  // - Spec: If init contains none of "protocol", "hostname", "port",
  //   "pathname", and "search", then:
  //   - Let baseQuery be baseURL’s query.
  //   - If baseQuery is null, then set baseQuery to the empty string.
  //   - Set result["search"] to the result of processing a base URL string
  //     given baseQuery and type.
  // - Spec: If init["search"] exists then set result["search"] to the result of
  //   process search for init given init["search"] and type.
  // Note: "process search for init" removes a single leading "?". But
  //       ConstructorStringParser doesn't set the leading "?". So we don't
  //       need the logic for the leading "?" removal.
  std::optional<std::string> search =
      init.search
          ? std::make_optional(std::string(*init.search))
          : ((init.protocol || init.hostname || init.port || init.pathname)
                 ? std::nullopt
                 : std::make_optional(EscapePatternString(base_url.query())));
  // [hash]
  // - Spec: If init contains none of "protocol", "hostname", "port",
  //   "pathname", "search", and "hash", then:
  //   - Let baseFragment be baseURL’s fragment.
  //   - If baseFragment is null, then set baseFragment to the empty string.
  //   - Set result["hash"] to the result of processing a base URL string given
  //     baseFragment and type.
  // - Spec: If init["hash"] exists then set result["hash"] to the result of
  //   process hash for init given init["hash"] and type.
  // Note: "process hash for init" removes a single leading "#". But
  //       ConstructorStringParser doesn't set the leading "#". So we don't
  //       need the logic for the leading "#" removal.
  std::optional<std::string> hash =
      init.hash
          ? std::make_optional(std::string(*init.hash))
          : ((init.protocol || init.hostname || init.port || init.pathname ||
              init.search)
                 ? std::nullopt
                 : std::make_optional(EscapePatternString(base_url.ref())));

  CHECK(protocol);

  // Treat default port as an empty string pattern. This logic is in the
  // "initialize a URLPattern" step.
  // https://urlpattern.spec.whatwg.org/#urlpattern-initialize
  // - Spec: If processedInit["protocol"] is a special scheme and
  //   processedInit["port"] is its corresponding default port, then set
  //   processedInit["port"] to the empty string.
  if (port) {
    int default_port = url::DefaultPortForScheme(*protocol);
    if (default_port != url::PORT_UNSPECIFIED &&
        base::NumberToString(default_port) == *port) {
      port = "";
    }
  }

  return PatternInit(std::move(protocol), std::move(username),
                     std::move(password), std::move(hostname), std::move(port),
                     std::move(pathname), std::move(search), std::move(hash));
}

// static
base::expected<std::unique_ptr<SimpleUrlPatternMatcher>, std::string>
SimpleUrlPatternMatcher::CreateFromPatternInit(
    const PatternInit& pattern,
    std::optional<Component> precomputed_protocol_component,
    bool protocol_matches_a_special_scheme_flag) {
  // We run the steps of compiling components in the "initialize a URLPattern"
  // step.
  // https://urlpattern.spec.whatwg.org/#urlpattern-initialize

  // We always provide `base_url` for creating a `SimpleUrlPatternMatcher`. So
  // `pattern.protocol()` must be set.
  CHECK(pattern.protocol());
  std::optional<Component> protocol_component =
      std::move(precomputed_protocol_component);
  if (!protocol_component) {
    // Spec: Set this’s protocol component to the result of compiling a
    // component given processedInit["protocol"], canonicalize a protocol,
    // and default options.
    auto protocol_component_result =
        Component::Create(*pattern.protocol(),
                          url_pattern::ProtocolEncodeCallback, kDefaultOptions);
    if (!protocol_component_result.has_value()) {
      return base::unexpected(
          base::StrCat({protocol_component_result.error(), " for protocol"}));
    }
    protocol_component = std::move(protocol_component_result.value());
    protocol_matches_a_special_scheme_flag =
        base::ranges::any_of(url::GetStandardSchemes(),
                             [&protocol_component](const std::string& scheme) {
                               return protocol_component->Match(scheme);
                             });
  }

#define MAYBE_COMPILE_PATTERN(type, callback, options)                       \
  std::optional<Component> type##_component;                                 \
  auto type##_result = Component::Create(pattern.type(), callback, options); \
  if (!type##_result.has_value()) {                                          \
    return base::unexpected(                                                 \
        base::StrCat({type##_result.error(), " for " #type}));               \
  }                                                                          \
  type##_component = std::move(type##_result.value());
  // Spec: Set this’s username component to the result of compiling a component
  // given processedInit["username"], canonicalize a username, and default
  // options.
  MAYBE_COMPILE_PATTERN(username, url_pattern::UsernameEncodeCallback,
                        kDefaultOptions);
  // Spec: Set this’s password component to the result of compiling a component
  // given processedInit["password"], canonicalize a password, and default
  // options.
  MAYBE_COMPILE_PATTERN(password, url_pattern::PasswordEncodeCallback,
                        kDefaultOptions);
  // Spec:
  // - If the result running hostname pattern is an IPv6 address given
  //   processedInit["hostname"] is true, then set this’s hostname component to
  //   the result of compiling a component given processedInit["hostname"],
  //   canonicalize an IPv6 hostname, and hostname options.
  // - Otherwise, set this’s hostname component to the result of compiling a
  //   component given processedInit["hostname"], canonicalize a hostname, and
  //   hostname options.
  MAYBE_COMPILE_PATTERN(hostname,
                        url_pattern::TreatAsIPv6Hostname(*pattern.hostname())
                            ? url_pattern::IPv6HostnameEncodeCallback
                            : url_pattern::HostnameEncodeCallback,
                        kHostnameOptions);
  // Spec: Set this’s port component to the result of compiling a component
  // given processedInit["port"], canonicalize a port, and default options.
  MAYBE_COMPILE_PATTERN(port, url_pattern::PortEncodeCallback, kDefaultOptions);
  // Spec:
  // - Let compileOptions be a copy of the default options with the ignore case
  //   property set to options["ignoreCase"].
  // - If the result of running protocol component matches a special scheme
  //   given this’s protocol component is true, then:
  //   - Let pathCompileOptions be copy of the pathname options with the ignore
  //     case property set to options["ignoreCase"].
  //   - Set this’s pathname component to the result of compiling a component
  //     given processedInit["pathname"], canonicalize a pathname, and
  //     pathCompileOptions.
  // - Otherwise set this’s pathname component to the result of compiling a
  //   component given processedInit["pathname"], canonicalize an opaque
  //   pathname, and compileOptions.
  // Note: We use the default false for options["ignoreCase"].
  MAYBE_COMPILE_PATTERN(pathname,
                        protocol_matches_a_special_scheme_flag
                            ? url_pattern::StandardURLPathnameEncodeCallback
                            : url_pattern::PathURLPathnameEncodeCallback,
                        protocol_matches_a_special_scheme_flag
                            ? kPathnameOptions
                            : kDefaultOptions);
  // Spec: Set this’s search component to the result of compiling a component
  // given processedInit["search"], canonicalize a search, and compileOptions.
  MAYBE_COMPILE_PATTERN(search, url_pattern::SearchEncodeCallback,
                        kDefaultOptions);
  // Spec: Set this’s hash component to the result of compiling a component
  // given processedInit["hash"], canonicalize a hash, and compileOptions.
  MAYBE_COMPILE_PATTERN(hash, url_pattern::HashEncodeCallback, kDefaultOptions);
#undef MAYBE_COMPILE_PATTERN

  return std::make_unique<SimpleUrlPatternMatcher>(
      std::move(*protocol_component), std::move(*username_component),
      std::move(*password_component), std::move(*hostname_component),
      std::move(*port_component), std::move(*pathname_component),
      std::move(*search_component), std::move(*hash_component),
      base::PassKey<SimpleUrlPatternMatcher>());
}

SimpleUrlPatternMatcher::SimpleUrlPatternMatcher(
    Component protocol,
    Component username,
    Component password,
    Component hostname,
    Component port,
    Component pathname,
    Component search,
    Component hash,
    base::PassKey<SimpleUrlPatternMatcher>)
    : protocol_(std::move(protocol)),
      username_(std::move(username)),
      password_(std::move(password)),
      hostname_(std::move(hostname)),
      port_(std::move(port)),
      pathname_(std::move(pathname)),
      search_(std::move(search)),
      hash_(std::move(hash)) {}

bool SimpleUrlPatternMatcher::Match(const GURL& url) const {
  return protocol_.Match(url.scheme()) && username_.Match(url.username()) &&
         password_.Match(url.password()) && hostname_.Match(url.host()) &&
         port_.Match(url.port()) && pathname_.Match(url.path()) &&
         search_.Match(url.query()) && hash_.Match(url.ref());
}

SimpleUrlPatternMatcher::~SimpleUrlPatternMatcher() = default;

}  // namespace network

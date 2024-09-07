// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/common/url_pattern.h"

#include <stddef.h>

#include <ostream>
#include <string_view>

#include "base/containers/contains.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_util.h"

const char URLPattern::kAllUrlsPattern[] = "<all_urls>";

namespace {

// TODO(aa): What about more obscure schemes like javascript: ?
// Note: keep this array in sync with kValidSchemeMasks.
const char* const kValidSchemes[] = {
    url::kHttpScheme,          url::kHttpsScheme,
    url::kFileScheme,          url::kFtpScheme,
    content::kChromeUIScheme,  extensions::kExtensionScheme,
    url::kFileSystemScheme,    url::kWsScheme,
    url::kWssScheme,           url::kDataScheme,
    url::kUuidInPackageScheme,
};

const int kValidSchemeMasks[] = {
    URLPattern::SCHEME_HTTP,
    URLPattern::SCHEME_HTTPS,
    URLPattern::SCHEME_FILE,
    URLPattern::SCHEME_FTP,
    URLPattern::SCHEME_CHROMEUI,
    URLPattern::SCHEME_EXTENSION,
    URLPattern::SCHEME_FILESYSTEM,
    URLPattern::SCHEME_WS,
    URLPattern::SCHEME_WSS,
    URLPattern::SCHEME_DATA,
    URLPattern::SCHEME_UUID_IN_PACKAGE,
};

static_assert(std::size(kValidSchemes) == std::size(kValidSchemeMasks),
              "must keep these arrays in sync");

const char kParseSuccess[] = "Success.";
const char kParseErrorMissingSchemeSeparator[] = "Missing scheme separator.";
const char kParseErrorInvalidScheme[] = "Invalid scheme.";
const char kParseErrorWrongSchemeType[] = "Wrong scheme type.";
const char kParseErrorEmptyHost[] = "Host can not be empty.";
const char kParseErrorInvalidHostWildcard[] = "Invalid host wildcard.";
const char kParseErrorEmptyPath[] = "Empty path.";
const char kParseErrorInvalidPort[] = "Invalid port.";
const char kParseErrorInvalidHost[] = "Invalid host.";

// Message explaining each URLPattern::ParseResult.
const char* const kParseResultMessages[] = {
  kParseSuccess,
  kParseErrorMissingSchemeSeparator,
  kParseErrorInvalidScheme,
  kParseErrorWrongSchemeType,
  kParseErrorEmptyHost,
  kParseErrorInvalidHostWildcard,
  kParseErrorEmptyPath,
  kParseErrorInvalidPort,
  kParseErrorInvalidHost,
};

static_assert(static_cast<int>(URLPattern::ParseResult::kNumParseResults) ==
                  std::size(kParseResultMessages),
              "must add message for each parse result");

const char kPathSeparator[] = "/";

bool IsStandardScheme(std::string_view scheme) {
  // "*" gets the same treatment as a standard scheme.
  if (scheme == "*") {
    return true;
  }

  return url::IsStandard(scheme.data(),
                         url::Component(0, static_cast<int>(scheme.length())));
}

bool IsValidPortForScheme(std::string_view scheme, std::string_view port) {
  if (port == "*") {
    return true;
  }

  // Only accept non-wildcard ports if the scheme uses ports.
  if (url::DefaultPortForScheme(scheme) == url::PORT_UNSPECIFIED) {
    return false;
  }

  int parsed_port = url::PORT_UNSPECIFIED;
  if (!base::StringToInt(port, &parsed_port)) {
    return false;
  }
  return (parsed_port >= 0) && (parsed_port < 65536);
}

// Returns |path| with the trailing wildcard stripped if one existed.
//
// The functions that rely on this (OverlapsWith and Contains) are only
// called for the patterns inside URLPatternSet. In those cases, we know that
// the path will have only a single wildcard at the end. This makes figuring
// out overlap much easier. It seems like there is probably a computer-sciency
// way to solve the general case, but we don't need that yet.
std::string_view StripTrailingWildcard(std::string_view path) {
  if (base::EndsWith(path, "*")) {
    path.remove_suffix(1);
  }
  return path;
}

// Removes trailing dot from |host_piece| if any.
std::string_view CanonicalizeHostForMatching(std::string_view host_piece) {
  if (base::EndsWith(host_piece, ".")) {
    host_piece.remove_suffix(1);
  }
  return host_piece;
}

}  // namespace

// static
bool URLPattern::IsValidSchemeForExtensions(std::string_view scheme) {
  for (auto* valid_scheme : kValidSchemes) {
    if (scheme == valid_scheme) {
      return true;
    }
  }
  return false;
}

// static
int URLPattern::GetValidSchemeMaskForExtensions() {
  int result = 0;
  for (int valid_scheme_mask : kValidSchemeMasks) {
    result |= valid_scheme_mask;
  }
  return result;
}

URLPattern::URLPattern()
    : valid_schemes_(SCHEME_NONE),
      match_all_urls_(false),
      match_subdomains_(false),
      port_("*") {}

URLPattern::URLPattern(int valid_schemes)
    : valid_schemes_(valid_schemes),
      match_all_urls_(false),
      match_subdomains_(false),
      port_("*") {}

URLPattern::URLPattern(int valid_schemes, std::string_view pattern)
    // Strict error checking is used, because this constructor is only
    // appropriate when we know |pattern| is valid.
    : valid_schemes_(valid_schemes),
      match_all_urls_(false),
      match_subdomains_(false),
      port_("*") {
  ParseResult result = Parse(pattern);
  DCHECK_EQ(ParseResult::kSuccess, result)
      << "Parsing unexpectedly failed for pattern: " << pattern << ": "
      << GetParseResultString(result);
}

URLPattern::URLPattern(const URLPattern& other) = default;

URLPattern::URLPattern(URLPattern&& other) = default;

URLPattern::~URLPattern() = default;

URLPattern& URLPattern::operator=(const URLPattern& other) = default;

URLPattern& URLPattern::operator=(URLPattern&& other) = default;

bool URLPattern::operator<(const URLPattern& other) const {
  return GetAsString() < other.GetAsString();
}

bool URLPattern::operator>(const URLPattern& other) const {
  return GetAsString() > other.GetAsString();
}

bool URLPattern::operator==(const URLPattern& other) const {
  return GetAsString() == other.GetAsString();
}

std::ostream& operator<<(std::ostream& out, const URLPattern& url_pattern) {
  return out << '"' << url_pattern.GetAsString() << '"';
}

URLPattern::ParseResult URLPattern::Parse(std::string_view pattern) {
  spec_.clear();
  SetMatchAllURLs(false);
  SetMatchSubdomains(false);
  SetPort("*");

  // Special case pattern to match every valid URL.
  if (pattern == kAllUrlsPattern) {
    SetMatchAllURLs(true);
    return ParseResult::kSuccess;
  }

  // Parse out the scheme.
  size_t scheme_end_pos = pattern.find(url::kStandardSchemeSeparator);
  bool has_standard_scheme_separator = true;

  // Some urls also use ':' alone as the scheme separator.
  if (scheme_end_pos == std::string_view::npos) {
    scheme_end_pos = pattern.find(':');
    has_standard_scheme_separator = false;
  }

  if (scheme_end_pos == std::string_view::npos) {
    return ParseResult::kMissingSchemeSeparator;
  }

  if (!SetScheme(pattern.substr(0, scheme_end_pos))) {
    return ParseResult::kInvalidScheme;
  }

  bool standard_scheme = IsStandardScheme(scheme_);
  if (standard_scheme != has_standard_scheme_separator) {
    return ParseResult::kWrongSchemeSeparator;
  }

  // Advance past the scheme separator.
  scheme_end_pos +=
      (standard_scheme ? strlen(url::kStandardSchemeSeparator) : 1);
  if (scheme_end_pos >= pattern.size()) {
    return ParseResult::kEmptyHost;
  }

  // Parse out the host and path.
  size_t host_start_pos = scheme_end_pos;
  size_t path_start_pos = 0;

  if (!standard_scheme) {
    path_start_pos = host_start_pos;
  } else if (scheme_ == url::kFileScheme) {
    size_t host_end_pos = pattern.find(kPathSeparator, host_start_pos);
    if (host_end_pos == std::string_view::npos) {
      // Allow hostname omission.
      // e.g. file://* is interpreted as file:///*,
      // file://foo* is interpreted as file:///foo*.
      path_start_pos = host_start_pos - 1;
    } else {
      // Ignore hostname if scheme is file://.
      // e.g. file://localhost/foo is equal to file:///foo.
      path_start_pos = host_end_pos;
    }
  } else {
    size_t host_end_pos = pattern.find(kPathSeparator, host_start_pos);

    // Host is required.
    if (host_start_pos == host_end_pos) {
      return ParseResult::kEmptyHost;
    }

    if (host_end_pos == std::string_view::npos) {
      return ParseResult::kEmptyPath;
    }

    std::string_view host_and_port =
        pattern.substr(host_start_pos, host_end_pos - host_start_pos);

    size_t port_separator_pos = std::string_view::npos;
    if (host_and_port[0] != '[') {
      // Not IPv6 (either IPv4 or just a normal address).
      port_separator_pos = host_and_port.find(':');
    } else {  // IPv6.
      size_t ipv6_host_end_pos = host_and_port.find(']');
      if (ipv6_host_end_pos == std::string_view::npos) {
        return ParseResult::kInvalidHost;
      }
      if (ipv6_host_end_pos == 1) {
        return ParseResult::kEmptyHost;
      }

      if (ipv6_host_end_pos < host_and_port.length() - 1) {
        // The host isn't the only component. Check for a port. This would
        // require a ':' to follow the closing ']' from the host.
        if (host_and_port[ipv6_host_end_pos + 1] != ':') {
          return ParseResult::kInvalidHost;
        }

        port_separator_pos = ipv6_host_end_pos + 1;
      }
    }

    if (port_separator_pos != std::string_view::npos &&
        !SetPort(host_and_port.substr(port_separator_pos + 1))) {
      return ParseResult::kInvalidPort;
    }

    // Note: this substr() will be the entire string if the port position
    // wasn't found.
    std::string_view host_piece = host_and_port.substr(0, port_separator_pos);

    if (host_piece.empty()) {
      return ParseResult::kEmptyHost;
    }

    if (host_piece == "*") {
      match_subdomains_ = true;
      host_piece = std::string_view();
    } else if (base::StartsWith(host_piece, "*.")) {
      if (host_piece.length() == 2) {
        // We don't allow just '*.' as a host.
        return ParseResult::kEmptyHost;
      }
      match_subdomains_ = true;
      host_piece = host_piece.substr(2);
    }

    host_ = std::string(host_piece);

    path_start_pos = host_end_pos;
  }

  SetPath(pattern.substr(path_start_pos));

  // No other '*' can occur in the host, though. This isn't necessary, but is
  // done as a convenience to developers who might otherwise be confused and
  // think '*' works as a glob in the host.
  if (base::Contains(host_, '*')) {
    return ParseResult::kInvalidHostWildcard;
  }

  if (!host_.empty()) {
    // If |host_| is present (i.e., isn't a wildcard), we need to canonicalize
    // it.
    url::CanonHostInfo host_info;
    host_ = net::CanonicalizeHost(host_, &host_info);
    // net::CanonicalizeHost() returns an empty string on failure.
    if (host_.empty()) {
      return ParseResult::kInvalidHost;
    }
  }

  // Null characters are not allowed in hosts.
  if (base::Contains(host_, '\0')) {
    return ParseResult::kInvalidHost;
  }

  return ParseResult::kSuccess;
}

void URLPattern::SetValidSchemes(int valid_schemes) {
  // TODO(devlin): Should we check that valid_schemes agrees with |scheme_|
  // here? Otherwise, valid_schemes_ and schemes_ may stop agreeing with each
  // other (e.g., in the case of `*://*/*`, where the scheme should only be
  // http or https).
  spec_.clear();
  valid_schemes_ = valid_schemes;
}

void URLPattern::SetHost(std::string_view host) {
  spec_.clear();
  host_ = host;
}

void URLPattern::SetMatchAllURLs(bool val) {
  spec_.clear();
  match_all_urls_ = val;

  if (val) {
    match_subdomains_ = true;
    scheme_ = "*";
    host_.clear();
    SetPath("/*");
  }
}

void URLPattern::SetMatchSubdomains(bool val) {
  spec_.clear();
  match_subdomains_ = val;
}

bool URLPattern::SetScheme(std::string_view scheme) {
  spec_.clear();
  scheme_ = scheme;
  if (scheme_ == "*") {
    valid_schemes_ &= (SCHEME_HTTP | SCHEME_HTTPS);
  } else if (!IsValidScheme(scheme_)) {
    return false;
  }
  return true;
}

bool URLPattern::IsValidScheme(std::string_view scheme) const {
  if (valid_schemes_ == SCHEME_ALL) {
    return true;
  }

  for (size_t i = 0; i < std::size(kValidSchemes); ++i) {
    if (scheme == kValidSchemes[i] && (valid_schemes_ & kValidSchemeMasks[i])) {
      return true;
    }
  }

  return false;
}

void URLPattern::SetPath(std::string_view path) {
  spec_.clear();
  path_ = path;
  path_escaped_ = path_;
  base::ReplaceSubstringsAfterOffset(&path_escaped_, 0, "\\", "\\\\");
  base::ReplaceSubstringsAfterOffset(&path_escaped_, 0, "?", "\\?");
}

bool URLPattern::SetPort(std::string_view port) {
  spec_.clear();
  if (IsValidPortForScheme(scheme_, port)) {
    port_ = port;
    return true;
  }
  return false;
}

bool URLPattern::MatchesURL(const GURL& test) const {
  // Invalid URLs can never match.
  if (!test.is_valid()) {
    return false;
  }

  const GURL* test_url = &test;
  bool has_inner_url = test.inner_url() != nullptr;

  if (has_inner_url) {
    if (!test.SchemeIsFileSystem()) {
      return false;  // The only nested URLs we handle are filesystem URLs.
    }
    test_url = test.inner_url();
  }

  // Ensure the scheme matches first, since <all_urls> may not match this URL if
  // the scheme is excluded.
  if (!MatchesScheme(test_url->scheme_piece())) {
    return false;
  }

  if (match_all_urls_) {
    return true;
  }

  // Unless |match_all_urls_| is true, the grammar only permits matching
  // URLs with nonempty paths.
  if (!test.has_path()) {
    return false;
  }

  std::string path_for_request = test.PathForRequest();
  if (has_inner_url) {
    path_for_request = base::StrCat({test_url->path_piece(), path_for_request});
  }

  return MatchesSecurityOriginHelper(*test_url) &&
         MatchesPath(path_for_request);
}

bool URLPattern::MatchesSecurityOrigin(const GURL& test) const {
  const GURL* test_url = &test;
  bool has_inner_url = test.inner_url() != nullptr;

  if (has_inner_url) {
    if (!test.SchemeIsFileSystem()) {
      return false;  // The only nested URLs we handle are filesystem URLs.
    }
    test_url = test.inner_url();
  }

  if (!MatchesScheme(test_url->scheme())) {
    return false;
  }

  if (match_all_urls_) {
    return true;
  }

  return MatchesSecurityOriginHelper(*test_url);
}

bool URLPattern::MatchesScheme(std::string_view test) const {
  if (!IsValidScheme(test)) {
    return false;
  }

  return scheme_ == "*" || test == scheme_;
}

bool URLPattern::MatchesHost(std::string_view host) const {
  // TODO(devlin): This is a bit sad. Parsing urls is expensive. However, it's
  // important that we do this conversion to a GURL in order to canonicalize the
  // host (the pattern's host_ already is canonicalized from Parse()). We can't
  // just do string comparison.
  return MatchesHost(GURL(base::StrCat(
      {url::kHttpScheme, url::kStandardSchemeSeparator, host, "/"})));
}

bool URLPattern::MatchesHost(const GURL& test) const {
  std::string_view test_host(CanonicalizeHostForMatching(test.host_piece()));
  const std::string_view pattern_host(CanonicalizeHostForMatching(host_));

  // If the hosts are exactly equal, we have a match.
  if (test_host == pattern_host) {
    return true;
  }

  // If we're matching subdomains, and we have no host in the match pattern,
  // that means that we're matching all hosts, which means we have a match no
  // matter what the test host is.
  if (match_subdomains_ && pattern_host.empty()) {
    return true;
  }

  // Otherwise, we can only match if our match pattern matches subdomains.
  if (!match_subdomains_) {
    return false;
  }

  // We don't do subdomain matching against IP addresses, so we can give up now
  // if the test host is an IP address.
  if (test.HostIsIPAddress()) {
    return false;
  }

  // Check if the test host is a subdomain of our host.
  if (test_host.length() <= (pattern_host.length() + 1)) {
    return false;
  }

  if (!base::EndsWith(test_host, pattern_host)) {
    return false;
  }

  return test_host[test_host.length() - pattern_host.length() - 1] == '.';
}

bool URLPattern::MatchesEffectiveTld(
    net::registry_controlled_domains::PrivateRegistryFilter private_filter,
    net::registry_controlled_domains::UnknownRegistryFilter unknown_filter)
    const {
  // Check if it matches all urls or is a pattern like http://*/*.
  if (match_all_urls_ || (match_subdomains_ && host_.empty())) {
    return true;
  }

  // If this doesn't even match subdomains, it can't possibly be a TLD wildcard.
  if (!match_subdomains_) {
    return false;
  }

  // If there was more than just a TLD in the host (e.g., *.foobar.com), it
  // doesn't match all hosts in an effective TLD.
  if (net::registry_controlled_domains::HostHasRegistryControlledDomain(
          host_, unknown_filter, private_filter)) {
    return false;
  }

  // At this point the host could either be just a TLD ("com") or some unknown
  // TLD-like string ("notatld"). To disambiguate between them construct a
  // fake URL, and check the registry.
  //
  // If we recognized this TLD, then this is a pattern like *.com, and it
  // matches an effective TLD.
  return net::registry_controlled_domains::HostHasRegistryControlledDomain(
      "notatld." + host_, unknown_filter, private_filter);
}

bool URLPattern::MatchesSingleOrigin() const {
  // Strictly speaking, the port is part of the origin, but in URLPattern it
  // defaults to *. It's not very interesting anyway, so leave it out.
  return !MatchesEffectiveTld() && scheme_ != "*" && !match_subdomains_;
}

bool URLPattern::MatchesPath(std::string_view test) const {
  // Make the behaviour of OverlapsWith consistent with MatchesURL, which is
  // need to match hosted apps on e.g. 'google.com' also run on 'google.com/'.
  // The below if is a no-copy way of doing (test + "/*" == path_escaped_).
  if (path_escaped_.length() == test.length() + 2 &&
      base::StartsWith(path_escaped_.c_str(), test) &&
      base::EndsWith(path_escaped_, "/*")) {
    return true;
  }

  return base::MatchPattern(test, path_escaped_);
}

const std::string& URLPattern::GetAsString() const {
  if (!spec_.empty()) {
    return spec_;
  }

  if (match_all_urls_) {
    spec_ = kAllUrlsPattern;
    return spec_;
  }

  bool standard_scheme = IsStandardScheme(scheme_);

  std::string spec = scheme_ +
      (standard_scheme ? url::kStandardSchemeSeparator : ":");

  if (scheme_ != url::kFileScheme && standard_scheme) {
    if (match_subdomains_) {
      spec += "*";
      if (!host_.empty()) {
        spec += ".";
      }
    }

    if (!host_.empty()) {
      spec += host_;
    }

    if (port_ != "*") {
      spec += ":";
      spec += port_;
    }
  }

  if (!path_.empty()) {
    spec += path_;
  }

  spec_ = std::move(spec);
  return spec_;
}

bool URLPattern::OverlapsWith(const URLPattern& other) const {
  if (match_all_urls() || other.match_all_urls()) {
    return true;
  }
  return (MatchesAnyScheme(other.GetExplicitSchemes()) ||
          other.MatchesAnyScheme(GetExplicitSchemes()))
      && (MatchesHost(other.host()) || other.MatchesHost(host()))
      && (MatchesPortPattern(other.port()) || other.MatchesPortPattern(port()))
      && (MatchesPath(StripTrailingWildcard(other.path())) ||
          other.MatchesPath(StripTrailingWildcard(path())));
}

bool URLPattern::Contains(const URLPattern& other) const {
  // Important: it's not enough to just check match_all_urls(); we also need to
  // make sure that the schemes in this pattern are a superset of those in
  // |other|.
  if (match_all_urls() &&
      (valid_schemes_ & other.valid_schemes_) == other.valid_schemes_) {
    return true;
  }

  return MatchesAllSchemes(other.GetExplicitSchemes()) &&
         MatchesHost(other.host()) &&
         (!other.match_subdomains_ || match_subdomains_) &&
         MatchesPortPattern(other.port()) &&
         MatchesPath(StripTrailingWildcard(other.path()));
}

std::optional<URLPattern> URLPattern::CreateIntersection(
    const URLPattern& other) const {
  // Easy case: Schemes don't overlap. Return nullopt.
  int intersection_schemes = URLPattern::SCHEME_NONE;
  if (valid_schemes_ == URLPattern::SCHEME_ALL) {
    intersection_schemes = other.valid_schemes_;
  } else if (other.valid_schemes_ == URLPattern::SCHEME_ALL) {
    intersection_schemes = valid_schemes_;
  } else {
    intersection_schemes = valid_schemes_ & other.valid_schemes_;
  }

  if (intersection_schemes == URLPattern::SCHEME_NONE) {
    return std::nullopt;
  }

  {
    // In a few cases, we can (mostly) return a copy of one of the patterns.
    // This can happen when either:
    // - The URLPattern's are identical (possibly excluding valid_schemes_)
    // - One of the patterns has match_all_urls() equal to true.
    // NOTE(devlin): Theoretically, we could use Contains() instead of
    // match_all_urls() here. However, Contains() strips the trailing wildcard
    // from the path, which could yield the incorrect result.
    const URLPattern* copy_source = nullptr;
    if (*this == other || other.match_all_urls()) {
      copy_source = this;
    } else if (match_all_urls()) {
      copy_source = &other;
    }

    if (copy_source) {
      // NOTE: equality checks don't take into account valid_schemes_, and
      // schemes can be different in the case of match_all_urls() as well, so
      // we can't always just return *copy_source.
      if (intersection_schemes == copy_source->valid_schemes_) {
        return *copy_source;
      }
      URLPattern result(intersection_schemes);
      ParseResult parse_result = result.Parse(copy_source->GetAsString());
      CHECK_EQ(ParseResult::kSuccess, parse_result);
      return result;
    }
  }

  // No more easy cases. Go through component by component to find the patterns
  // that intersect.

  // Note: Alias the function type (rather than using auto) because
  // MatchesHost() is overloaded.
  using match_function_type = bool (URLPattern::*)(std::string_view) const;

  auto get_intersection = [this, &other](std::string_view own_str,
                                         std::string_view other_str,
                                         match_function_type match_function,
                                         std::string_view* out) {
    if ((this->*match_function)(other_str)) {
      *out = other_str;
      return true;
    }
    if ((other.*match_function)(own_str)) {
      *out = own_str;
      return true;
    }
    return false;
  };

  std::string_view scheme;
  std::string_view host;
  std::string_view port;
  std::string_view path;
  // If any pieces fail to overlap, then there is no intersection.
  if (!get_intersection(scheme_, other.scheme_, &URLPattern::MatchesScheme,
                        &scheme) ||
      !get_intersection(host_, other.host_, &URLPattern::MatchesHost, &host) ||
      !get_intersection(port_, other.port_, &URLPattern::MatchesPortPattern,
                        &port) ||
      !get_intersection(path_, other.path_, &URLPattern::MatchesPath, &path)) {
    return std::nullopt;
  }

  // Only match subdomains if both patterns match subdomains.
  std::string_view subdomains;
  if (match_subdomains_ && other.match_subdomains_) {
    // The host may be empty (e.g., in the case of *://*/* - in that case, only
    // append '*' instead of '*.'.
    subdomains = host.empty() ? "*" : "*.";
  }

  std::string_view scheme_separator =
      IsStandardScheme(scheme) ? url::kStandardSchemeSeparator : ":";

  std::string pattern_str = base::StrCat(
      {scheme, scheme_separator, subdomains, host, ":", port, path});

  URLPattern pattern(intersection_schemes);
  ParseResult result = pattern.Parse(pattern_str);
  // TODO(devlin): I don't think there's any way this should ever fail, but
  // use a CHECK() to flush any cases out. If nothing crops up, downgrade this
  // to a DCHECK in M72.
  CHECK_EQ(ParseResult::kSuccess, result);

  return pattern;
}

bool URLPattern::MatchesAnyScheme(
    const std::vector<std::string>& schemes) const {
  for (const auto& scheme : schemes) {
    if (MatchesScheme(scheme)) {
      return true;
    }
  }

  return false;
}

bool URLPattern::MatchesAllSchemes(
    const std::vector<std::string>& schemes) const {
  for (const auto& scheme : schemes) {
    if (!MatchesScheme(scheme)) {
      return false;
    }
  }

  return true;
}

bool URLPattern::MatchesSecurityOriginHelper(const GURL& test) const {
  // Ignore hostname if scheme is file://.
  if (scheme_ != url::kFileScheme && !MatchesHost(test)) {
    return false;
  }

  if (!MatchesPortPattern(base::NumberToString(test.EffectiveIntPort()))) {
    return false;
  }

  return true;
}

bool URLPattern::MatchesPortPattern(std::string_view port) const {
  return port_ == "*" || port_ == port;
}

std::vector<std::string> URLPattern::GetExplicitSchemes() const {
  std::vector<std::string> result;
  const bool is_wildcard_scheme = (scheme_ == "*");

  if (!is_wildcard_scheme && !match_all_urls_ && IsValidScheme(scheme_)) {
    result.push_back(scheme_);
    return result;
  }

  result.reserve(std::size(kValidSchemes));
  for (size_t i = 0; i < std::size(kValidSchemes); ++i) {
    const bool is_valid_scheme = (valid_schemes_ == SCHEME_ALL ||
                                  (valid_schemes_ & kValidSchemeMasks[i]));
    if (!is_valid_scheme) {
      continue;
    }

    const bool scheme_matches =
        (is_wildcard_scheme || kValidSchemes[i] == scheme_);
    if (scheme_matches) {
      result.push_back(kValidSchemes[i]);
    }
  }

  return result;
}

std::vector<URLPattern> URLPattern::ConvertToExplicitSchemes() const {
  std::vector<std::string> explicit_schemes = GetExplicitSchemes();
  std::vector<URLPattern> result;

  for (std::vector<std::string>::const_iterator i = explicit_schemes.begin();
       i != explicit_schemes.end(); ++i) {
    URLPattern temp = *this;
    temp.SetScheme(*i);
    temp.SetMatchAllURLs(false);
    result.push_back(temp);
  }

  return result;
}

// static
const char* URLPattern::GetParseResultString(
    URLPattern::ParseResult parse_result) {
  return kParseResultMessages[static_cast<int>(parse_result)];
}

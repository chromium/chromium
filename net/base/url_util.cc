// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_util.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <netinet/in.h>
#elif BUILDFLAG(IS_WIN)
#include <ws2tcpip.h>
#endif

#include <optional>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/ip_address.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_canon_ip.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace net {

namespace {

bool IsHostCharAlphanumeric(char c) {
  // We can just check lowercase because uppercase characters have already been
  // normalized.
  return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'));
}

bool IsNormalizedLocalhostTLD(std::string_view host) {
  return base::EndsWith(host, ".localhost",
                        base::CompareCase::INSENSITIVE_ASCII);
}

// Helper function used by GetIdentityFromURL. If |escaped_text| can be "safely
// unescaped" to a valid UTF-8 string, return that string, as UTF-16. Otherwise,
// convert it as-is to UTF-16. "Safely unescaped" is defined as having no
// escaped character between '0x00' and '0x1F', inclusive.
std::u16string UnescapeIdentityString(std::string_view escaped_text) {
  std::string unescaped_text;
  if (base::UnescapeBinaryURLComponentSafe(
          escaped_text, false /* fail_on_path_separators */, &unescaped_text)) {
    std::u16string result;
    if (base::UTF8ToUTF16(unescaped_text.data(), unescaped_text.length(),
                          &result)) {
      return result;
    }
  }
  return base::UTF8ToUTF16(escaped_text);
}

}  // namespace

GURL AppendQueryParameter(const GURL& url,
                          std::string_view name,
                          std::string_view value) {
  std::string query(url.query());

  if (!query.empty())
    query += "&";

  query += (base::EscapeQueryParamValue(name, true) + "=" +
            base::EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

GURL AppendOrReplaceQueryParameter(const GURL& url,
                                   std::string_view name,
                                   std::optional<std::string_view> value) {
  bool replaced = false;
  std::string param_name = base::EscapeQueryParamValue(name, true);
  bool should_keep_param = value.has_value();

  std::string param_value;
  if (should_keep_param)
    param_value = base::EscapeQueryParamValue(value.value(), true);

  const std::string_view input = url.query_piece();
  url::Component cursor(0, input.size());
  std::string output;
  url::Component key_range, value_range;
  while (url::ExtractQueryKeyValue(input, &cursor, &key_range, &value_range)) {
    const std::string_view key = input.substr(key_range.begin, key_range.len);
    std::string key_value_pair;
    // Check |replaced| as only the first pair should be replaced.
    if (!replaced && key == param_name) {
      replaced = true;
      if (!should_keep_param)
        continue;

      key_value_pair = param_name + "=" + param_value;
    } else {
      key_value_pair = std::string(
          input.substr(key_range.begin, value_range.end() - key_range.begin));
    }
    if (!output.empty())
      output += "&";

    output += key_value_pair;
  }
  if (!replaced && should_keep_param) {
    if (!output.empty())
      output += "&";

    output += (param_name + "=" + param_value);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(output);
  return url.ReplaceComponents(replacements);
}

GURL AppendOrReplaceRef(const GURL& url, const std::string_view& ref) {
  GURL::Replacements replacements;
  replacements.SetRefStr(ref);
  return url.ReplaceComponents(replacements);
}

QueryIterator::QueryIterator(const GURL& url)
    : url_(url), at_end_(!url.is_valid()) {
  if (!at_end_) {
    query_ = url.parsed_for_possibly_invalid_spec().query;
    Advance();
  }
}

QueryIterator::~QueryIterator() = default;

std::string_view QueryIterator::GetKey() const {
  DCHECK(!at_end_);
  if (key_.is_nonempty())
    return std::string_view(url_->spec()).substr(key_.begin, key_.len);
  return std::string_view();
}

std::string_view QueryIterator::GetValue() const {
  DCHECK(!at_end_);
  if (value_.is_nonempty())
    return std::string_view(url_->spec()).substr(value_.begin, value_.len);
  return std::string_view();
}

const std::string& QueryIterator::GetUnescapedValue() {
  DCHECK(!at_end_);
  if (value_.is_nonempty() && unescaped_value_.empty()) {
    unescaped_value_ = base::UnescapeURLComponent(
        GetValue(),
        base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
            base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
            base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
  }
  return unescaped_value_;
}

bool QueryIterator::IsAtEnd() const {
  return at_end_;
}

void QueryIterator::Advance() {
  DCHECK(!at_end_);
  key_.reset();
  value_.reset();
  unescaped_value_.clear();
  at_end_ = !url::ExtractQueryKeyValue(url_->spec(), &query_, &key_, &value_);
}

bool GetValueForKeyInQuery(const GURL& url,
                           std::string_view search_key,
                           std::string* out_value) {
  for (QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == search_key) {
      *out_value = it.GetUnescapedValue();
      return true;
    }
  }
  return false;
}

bool ParseHostAndPort(std::string_view input, std::string* host, int* port) {
  if (input.empty())
    return false;

  url::Component auth_component(0, input.size());
  url::Component username_component;
  url::Component password_component;
  url::Component hostname_component;
  url::Component port_component;

  // `input` is not NUL-terminated, so `input.data()` must be accompanied by a
  // length. In these calls, `url::Component` provides an offset and length.
  url::ParseAuthority(input.data(), auth_component, &username_component,
                      &password_component, &hostname_component,
                      &port_component);

  // There shouldn't be a username/password.
  if (username_component.is_valid() || password_component.is_valid())
    return false;

  if (hostname_component.is_empty())
    return false;  // Failed parsing.

  int parsed_port_number = -1;
  if (port_component.is_nonempty()) {
    parsed_port_number = url::ParsePort(input.data(), port_component);

    // If parsing failed, port_number will be either PORT_INVALID or
    // PORT_UNSPECIFIED, both of which are negative.
    if (parsed_port_number < 0)
      return false;  // Failed parsing the port number.
  }

  if (port_component.len == 0)
    return false;  // Reject inputs like "foo:"

  unsigned char tmp_ipv6_addr[16];

  // If the hostname starts with a bracket, it is either an IPv6 literal or
  // invalid. If it is an IPv6 literal then strip the brackets.
  if (hostname_component.len > 0 && input[hostname_component.begin] == '[') {
    if (input[hostname_component.end() - 1] == ']' &&
        url::IPv6AddressToNumber(input.data(), hostname_component,
                                 tmp_ipv6_addr)) {
      // Strip the brackets.
      hostname_component.begin++;
      hostname_component.len -= 2;
    } else {
      return false;
    }
  }

  // Pass results back to caller.
  *host = std::string(
      input.substr(hostname_component.begin, hostname_component.len));
  *port = parsed_port_number;

  return true;  // Success.
}

std::string GetHostAndPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets so it is
  // safe to just append a colon.
  return base::StringPrintf("%s:%d", url.host().c_str(),
                            url.EffectiveIntPort());
}

std::string GetHostAndOptionalPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets
  // so it is safe to just append a colon.
  if (url.has_port())
    return base::StringPrintf("%s:%s", url.host().c_str(), url.port().c_str());
  return url.host();
}

NET_EXPORT std::string GetHostAndOptionalPort(
    const url::SchemeHostPort& scheme_host_port) {
  int default_port = url::DefaultPortForScheme(scheme_host_port.scheme());
  if (default_port != scheme_host_port.port()) {
    return base::StringPrintf("%s:%i", scheme_host_port.host().c_str(),
                              scheme_host_port.port());
  }
  return scheme_host_port.host();
}

std::string TrimEndingDot(std::string_view host) {
  std::string_view host_trimmed = host;
  size_t len = host_trimmed.length();
  if (len > 1 && host_trimmed[len - 1] == '.') {
    host_trimmed.remove_suffix(1);
  }
  return std::string(host_trimmed);
}

std::string GetHostOrSpecFromURL(const GURL& url) {
  return url.has_host() ? TrimEndingDot(url.host_piece()) : url.spec();
}

std::string GetSuperdomain(std::string_view domain) {
  size_t dot_pos = domain.find('.');
  if (dot_pos == std::string::npos)
    return "";
  return std::string(domain.substr(dot_pos + 1));
}

bool IsSubdomainOf(std::string_view subdomain, std::string_view superdomain) {
  // Subdomain must be identical or have strictly more labels than the
  // superdomain.
  if (subdomain.length() <= superdomain.length())
    return subdomain == superdomain;

  // Superdomain must be suffix of subdomain, and the last character not
  // included in the matching substring must be a dot.
  if (!subdomain.ends_with(superdomain)) {
    return false;
  }
  subdomain.remove_suffix(superdomain.length());
  return subdomain.back() == '.';
}

namespace {
std::string CanonicalizeHost(std::string_view host,
                             bool is_file_scheme,
                             url::CanonHostInfo* host_info) {
  // Try to canonicalize the host.
  const url::Component raw_host_component(0, static_cast<int>(host.length()));
  std::string canon_host;
  url::StdStringCanonOutput canon_host_output(&canon_host);
  // A url::StdStringCanonOutput starts off with a zero length buffer. The
  // first time through Grow() immediately resizes it to 32 bytes, incurring
  // a malloc. With libcxx a 22 byte or smaller request can be accommodated
  // within the std::string itself (i.e. no malloc occurs). Start the buffer
  // off at the max size to avoid a malloc on short strings.
  // NOTE: To ensure the final size is correctly reflected, it's necessary
  // to call Complete() which will adjust the size to the actual bytes written.
  // This is handled below for success cases, while failure cases discard all
  // the output.
  const int kCxxMaxStringBufferSizeWithoutMalloc = 22;
  canon_host_output.Resize(kCxxMaxStringBufferSizeWithoutMalloc);
  if (is_file_scheme) {
    url::CanonicalizeFileHostVerbose(host.data(), raw_host_component,
                                     canon_host_output, *host_info);
  } else {
    url::CanonicalizeSpecialHostVerbose(host.data(), raw_host_component,
                                        canon_host_output, *host_info);
  }

  if (host_info->out_host.is_nonempty() &&
      host_info->family != url::CanonHostInfo::BROKEN) {
    // Success!  Assert that there's no extra garbage.
    canon_host_output.Complete();
    DCHECK_EQ(host_info->out_host.len, static_cast<int>(canon_host.length()));
  } else {
    // Empty host, or canonicalization failed.  We'll return empty.
    canon_host.clear();
  }

  return canon_host;
}
}  // namespace

std::string CanonicalizeHost(std::string_view host,
                             url::CanonHostInfo* host_info) {
  return CanonicalizeHost(host, /*is_file_scheme=*/false, host_info);
}

std::string CanonicalizeFileHost(std::string_view host,
                                 url::CanonHostInfo* host_info) {
  return CanonicalizeHost(host, /*is_file_scheme=*/true, host_info);
}

bool IsCanonicalizedHostCompliant(std::string_view host) {
  if (host.empty() || host.size() > 254 ||
      (host.back() != '.' && host.size() == 254)) {
    return false;
  }

  bool in_component = false;
  bool most_recent_component_started_alphanumeric = false;
  size_t label_size = 0;

  for (char c : host) {
    ++label_size;
    if (!in_component) {
      most_recent_component_started_alphanumeric = IsHostCharAlphanumeric(c);
      if (!most_recent_component_started_alphanumeric && (c != '-') &&
          (c != '_')) {
        return false;
      }
      in_component = true;
    } else if (c == '.') {
      in_component = false;
      if (label_size > 64 || label_size == 1) {
        // Label should not be empty or longer than 63 characters (+1 for '.'
        // character included in `label_size`).
        return false;
      } else {
        label_size = 0;
      }
    } else if (!IsHostCharAlphanumeric(c) && (c != '-') && (c != '_')) {
      return false;
    }
  }

  // Check for too-long label when not ended with final '.'.
  if (label_size > 63)
    return false;

  return most_recent_component_started_alphanumeric;
}

bool IsHostnameNonUnique(std::string_view hostname) {
  // CanonicalizeHost requires surrounding brackets to parse an IPv6 address.
  const std::string host_or_ip = hostname.find(':') != std::string::npos
                                     ? base::StrCat({"[", hostname, "]"})
                                     : std::string(hostname);
  url::CanonHostInfo host_info;
  std::string canonical_name = CanonicalizeHost(host_or_ip, &host_info);

  // If canonicalization fails, then the input is truly malformed. However,
  // to avoid mis-reporting bad inputs as "non-unique", treat them as unique.
  if (canonical_name.empty())
    return false;

  // If |hostname| is an IP address, check to see if it's in an IANA-reserved
  // range reserved for non-publicly routable networks.
  if (host_info.IsIPAddress()) {
    IPAddress host_addr;
    if (!host_addr.AssignFromIPLiteral(hostname.substr(
            host_info.out_host.begin, host_info.out_host.len))) {
      return false;
    }
    switch (host_info.family) {
      case url::CanonHostInfo::IPV4:
      case url::CanonHostInfo::IPV6:
        return !host_addr.IsPubliclyRoutable();
      case url::CanonHostInfo::NEUTRAL:
      case url::CanonHostInfo::BROKEN:
        return false;
    }
  }

  // Check for a registry controlled portion of |hostname|, ignoring private
  // registries, as they already chain to ICANN-administered registries,
  // and explicitly ignoring unknown registries. Registry identifiers themselves
  // are also treated as unique, since a TLD is a valid hostname and can host a
  // web server.
  //
  // Note: This means that as new gTLDs are introduced on the Internet, they
  // will be treated as non-unique until the registry controlled domain list
  // is updated. However, because gTLDs are expected to provide significant
  // advance notice to deprecate older versions of this code, this an
  // acceptable tradeoff.
  return !registry_controlled_domains::HostHasRegistryControlledDomain(
             canonical_name,
             registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
             registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES) &&
         !registry_controlled_domains::HostIsRegistryIdentifier(
             canonical_name,
             registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

bool IsLocalhost(const GURL& url) {
  return HostStringIsLocalhost(url.HostNoBracketsPiece());
}

bool HostStringIsLocalhost(std::string_view host) {
  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(host))
    return ip_address.IsLoopback();
  return IsLocalHostname(host);
}

GURL SimplifyUrlForRequest(const GURL& url) {
  DCHECK(url.is_valid());
  // Fast path to avoid re-canonicalization via ReplaceComponents.
  if (!url.has_username() && !url.has_password() && !url.has_ref())
    return url;
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

GURL ChangeWebSocketSchemeToHttpScheme(const GURL& url) {
  DCHECK(url.SchemeIsWSOrWSS());
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr(url.SchemeIs(url::kWssScheme) ? url::kHttpsScheme
                                                            : url::kHttpScheme);
  return url.ReplaceComponents(replace_scheme);
}

bool IsStandardSchemeWithNetworkHost(std::string_view scheme) {
  // file scheme is special. Windows file share origins can have network hosts.
  if (scheme == url::kFileScheme)
    return true;

  url::SchemeType scheme_type;
  if (!url::GetStandardSchemeType(
          scheme.data(), url::Component(0, scheme.length()), &scheme_type)) {
    return false;
  }
  return scheme_type == url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION ||
         scheme_type == url::SCHEME_WITH_HOST_AND_PORT;
}

void GetIdentityFromURL(const GURL& url,
                        std::u16string* username,
                        std::u16string* password) {
  *username = UnescapeIdentityString(url.username());
  *password = UnescapeIdentityString(url.password());
}

bool HasGoogleHost(const GURL& url) {
  return IsGoogleHost(url.host_piece());
}

bool IsGoogleHost(std::string_view host) {
  static const char* kGoogleHostSuffixes[] = {
      ".google.com",
      ".youtube.com",
      ".gmail.com",
      ".doubleclick.net",
      ".gstatic.com",
      ".googlevideo.com",
      ".googleusercontent.com",
      ".googlesyndication.com",
      ".google-analytics.com",
      ".googleadservices.com",
      ".googleapis.com",
      ".ytimg.com",
  };
  for (const char* suffix : kGoogleHostSuffixes) {
    // Here it's possible to get away with faster case-sensitive comparisons
    // because the list above is all lowercase, and a GURL's host name will
    // always be canonicalized to lowercase as well.
    if (host.ends_with(suffix)) {
      return true;
    }
  }
  return false;
}

bool IsGoogleHostWithAlpnH3(std::string_view host) {
  return base::EqualsCaseInsensitiveASCII(host, "google.com") ||
         base::EqualsCaseInsensitiveASCII(host, "www.google.com");
}

bool IsLocalHostname(std::string_view host) {
  // Remove any trailing '.'.
  if (!host.empty() && *host.rbegin() == '.')
    host.remove_suffix(1);

  return base::EqualsCaseInsensitiveASCII(host, "localhost") ||
         IsNormalizedLocalhostTLD(host);
}

std::string UnescapePercentEncodedUrl(std::string_view input) {
  std::string result(input);
  // Replace any 0x2B (+) with 0x20 (SP).
  for (char& c : result) {
    if (c == '+') {
      c = ' ';
    }
  }
  // Run UTF-8 decoding without BOM on the percent-decoding.
  url::RawCanonOutputT<char16_t> canon_output;
  url::DecodeURLEscapeSequences(result, url::DecodeURLMode::kUTF8,
                                &canon_output);
  return base::UTF16ToUTF8(canon_output.view());
}

}  // namespace net

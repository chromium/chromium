// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_util.h"

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <netinet/in.h>
#elif defined(OS_WIN)
#include <ws2tcpip.h>
#endif

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/base/ip_address.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_canon_ip.h"
#include "url/url_constants.h"

namespace net {

namespace {

bool IsHostCharAlphanumeric(char c) {
  // We can just check lowercase because uppercase characters have already been
  // normalized.
  return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'));
}

bool IsNormalizedLocalhostTLD(const std::string& host) {
  return base::EndsWith(host, ".localhost", base::CompareCase::SENSITIVE);
}

// Helper function used by GetIdentityFromURL. If |escaped_text| can be "safely
// unescaped" to a valid UTF-8 string, return that string, as UTF-16. Otherwise,
// convert it as-is to UTF-16. "Safely unescaped" is defined as having no
// escaped character between '0x00' and '0x1F', inclusive.
base::string16 UnescapeIdentityString(base::StringPiece escaped_text) {
  std::string unescaped_text;
  if (UnescapeBinaryURLComponentSafe(
          escaped_text, false /* fail_on_path_separators */, &unescaped_text)) {
    base::string16 result;
    if (base::UTF8ToUTF16(unescaped_text.data(), unescaped_text.length(),
                          &result)) {
      return result;
    }
  }
  return base::UTF8ToUTF16(escaped_text);
}

}  // namespace

GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value) {
  std::string query(url.query());

  if (!query.empty())
    query += "&";

  query += (EscapeQueryParamValue(name, true) + "=" +
            EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

GURL AppendOrReplaceQueryParameter(const GURL& url,
                                   const std::string& name,
                                   const std::string& value) {
  bool replaced = false;
  std::string param_name = EscapeQueryParamValue(name, true);
  std::string param_value = EscapeQueryParamValue(value, true);

  const std::string input = url.query();
  url::Component cursor(0, input.size());
  std::string output;
  url::Component key_range, value_range;
  while (url::ExtractQueryKeyValue(input.data(), &cursor, &key_range,
                                   &value_range)) {
    const base::StringPiece key(
        input.data() + key_range.begin, key_range.len);
    std::string key_value_pair;
    // Check |replaced| as only the first pair should be replaced.
    if (!replaced && key == param_name) {
      replaced = true;
      key_value_pair = (param_name + "=" + param_value);
    } else {
      key_value_pair.assign(input, key_range.begin,
                            value_range.end() - key_range.begin);
    }
    if (!output.empty())
      output += "&";

    output += key_value_pair;
  }
  if (!replaced) {
    if (!output.empty())
      output += "&";

    output += (param_name + "=" + param_value);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(output);
  return url.ReplaceComponents(replacements);
}

QueryIterator::QueryIterator(const GURL& url)
    : url_(url),
      at_end_(!url.is_valid()) {
  if (!at_end_) {
    query_ = url.parsed_for_possibly_invalid_spec().query;
    Advance();
  }
}

QueryIterator::~QueryIterator() = default;

std::string QueryIterator::GetKey() const {
  DCHECK(!at_end_);
  if (key_.is_nonempty())
    return url_.spec().substr(key_.begin, key_.len);
  return std::string();
}

std::string QueryIterator::GetValue() const {
  DCHECK(!at_end_);
  if (value_.is_nonempty())
    return url_.spec().substr(value_.begin, value_.len);
  return std::string();
}

const std::string& QueryIterator::GetUnescapedValue() {
  DCHECK(!at_end_);
  if (value_.is_nonempty() && unescaped_value_.empty()) {
    unescaped_value_ = UnescapeURLComponent(
        GetValue(), UnescapeRule::SPACES | UnescapeRule::PATH_SEPARATORS |
                        UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
                        UnescapeRule::REPLACE_PLUS_WITH_SPACE);
  }
  return unescaped_value_;
}

bool QueryIterator::IsAtEnd() const {
  return at_end_;
}

void QueryIterator::Advance() {
  DCHECK (!at_end_);
  key_.reset();
  value_.reset();
  unescaped_value_.clear();
  at_end_ =
      !url::ExtractQueryKeyValue(url_.spec().c_str(), &query_, &key_, &value_);
}

bool GetValueForKeyInQuery(const GURL& url,
                           const std::string& search_key,
                           std::string* out_value) {
  for (QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == search_key) {
      *out_value = it.GetUnescapedValue();
      return true;
    }
  }
  return false;
}

bool ParseHostAndPort(base::StringPiece input, std::string* host, int* port) {
  if (input.empty())
    return false;

  url::Component auth_component(0, input.size());
  url::Component username_component;
  url::Component password_component;
  url::Component hostname_component;
  url::Component port_component;

  url::ParseAuthority(input.data(), auth_component, &username_component,
                      &password_component, &hostname_component,
                      &port_component);

  // There shouldn't be a username/password.
  if (username_component.is_valid() || password_component.is_valid())
    return false;

  if (!hostname_component.is_nonempty())
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
  host->assign(input.data() + hostname_component.begin, hostname_component.len);
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

std::string TrimEndingDot(base::StringPiece host) {
  base::StringPiece host_trimmed = host;
  size_t len = host_trimmed.length();
  if (len > 1 && host_trimmed[len - 1] == '.') {
    host_trimmed.remove_suffix(1);
  }
  return host_trimmed.as_string();
}

std::string GetHostOrSpecFromURL(const GURL& url) {
  return url.has_host() ? TrimEndingDot(url.host_piece()) : url.spec();
}

std::string CanonicalizeHost(base::StringPiece host,
                             url::CanonHostInfo* host_info) {
  // Try to canonicalize the host.
  const url::Component raw_host_component(0, static_cast<int>(host.length()));
  std::string canon_host;
  url::StdStringCanonOutput canon_host_output(&canon_host);
  url::CanonicalizeHostVerbose(host.data(), raw_host_component,
                               &canon_host_output, host_info);

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

bool IsCanonicalizedHostCompliant(const std::string& host) {
  if (host.empty())
    return false;

  bool in_component = false;
  bool most_recent_component_started_alphanumeric = false;

  for (std::string::const_iterator i(host.begin()); i != host.end(); ++i) {
    const char c = *i;
    if (!in_component) {
      most_recent_component_started_alphanumeric = IsHostCharAlphanumeric(c);
      if (!most_recent_component_started_alphanumeric && (c != '-') &&
          (c != '_')) {
        return false;
      }
      in_component = true;
    } else if (c == '.') {
      in_component = false;
    } else if (!IsHostCharAlphanumeric(c) && (c != '-') && (c != '_')) {
      return false;
    }
  }

  return most_recent_component_started_alphanumeric;
}

bool IsHostnameNonUnique(const std::string& hostname) {
  // CanonicalizeHost requires surrounding brackets to parse an IPv6 address.
  const std::string host_or_ip = hostname.find(':') != std::string::npos ?
      "[" + hostname + "]" : hostname;
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
  // and explicitly ignoring unknown registries.
  //
  // Note: This means that as new gTLDs are introduced on the Internet, they
  // will be treated as non-unique until the registry controlled domain list
  // is updated. However, because gTLDs are expected to provide significant
  // advance notice to deprecate older versions of this code, this an
  // acceptable tradeoff.
  return !registry_controlled_domains::HostHasRegistryControlledDomain(
      canonical_name, registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

bool IsLocalhost(const GURL& url) {
  return HostStringIsLocalhost(url.HostNoBracketsPiece());
}

bool HostStringIsLocalhost(base::StringPiece host) {
  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(host))
    return ip_address.IsLoopback();
  return IsLocalHostname(host, nullptr);
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

void GetIdentityFromURL(const GURL& url,
                        base::string16* username,
                        base::string16* password) {
  *username = UnescapeIdentityString(url.username());
  *password = UnescapeIdentityString(url.password());
}

bool HasGoogleHost(const GURL& url) {
  return IsGoogleHost(url.host_piece());
}

bool IsGoogleHost(base::StringPiece host) {
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
    if (base::EndsWith(host, suffix, base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

bool IsTLS13ExperimentHost(base::StringPiece host) {
  return host == "inbox.google.com" || host == "mail.google.com" ||
         host == "gmail.com";
}

bool IsLocalHostname(base::StringPiece host, bool* is_local6) {
  std::string normalized_host = base::ToLowerASCII(host);
  // Remove any trailing '.'.
  if (!normalized_host.empty() && *normalized_host.rbegin() == '.')
    normalized_host.resize(normalized_host.size() - 1);

  if (normalized_host == "localhost6" ||
      normalized_host == "localhost6.localdomain6") {
    if (is_local6)
      *is_local6 = true;
    return true;
  }

  if (is_local6)
    *is_local6 = false;
  return normalized_host == "localhost" ||
         normalized_host == "localhost.localdomain" ||
         IsNormalizedLocalhostTLD(normalized_host);
}

}  // namespace net

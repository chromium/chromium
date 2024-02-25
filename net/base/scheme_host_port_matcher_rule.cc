// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/scheme_host_port_matcher_rule.h"

#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/host_port_pair.h"
#include "net/base/parse_number.h"
#include "net/base/url_util.h"
#include "url/url_util.h"

namespace net {

namespace {

std::string AddBracketsIfIPv6(const IPAddress& ip_address) {
  std::string ip_host = ip_address.ToString();
  if (ip_address.IsIPv6())
    return base::StringPrintf("[%s]", ip_host.c_str());
  return ip_host;
}

}  // namespace

// static
std::unique_ptr<SchemeHostPortMatcherRule>
SchemeHostPortMatcherRule::FromUntrimmedRawString(
    std::string_view raw_untrimmed) {
  std::string_view raw =
      base::TrimWhitespaceASCII(raw_untrimmed, base::TRIM_ALL);

  // Extract any scheme-restriction.
  std::string::size_type scheme_pos = raw.find("://");
  std::string scheme;
  if (scheme_pos != std::string::npos) {
    scheme = std::string(raw.substr(0, scheme_pos));
    raw = raw.substr(scheme_pos + 3);
    if (scheme.empty())
      return nullptr;
  }

  if (raw.empty())
    return nullptr;

  // If there is a forward slash in the input, it is probably a CIDR style
  // mask.
  if (raw.find('/') != std::string::npos) {
    IPAddress ip_prefix;
    size_t prefix_length_in_bits;

    if (!ParseCIDRBlock(raw, &ip_prefix, &prefix_length_in_bits))
      return nullptr;

    return std::make_unique<SchemeHostPortMatcherIPBlockRule>(
        std::string(raw), scheme, ip_prefix, prefix_length_in_bits);
  }

  // Check if we have an <ip-address>[:port] input. We need to treat this
  // separately since the IP literal may not be in a canonical form.
  std::string host;
  int port;
  if (ParseHostAndPort(raw, &host, &port)) {
    IPAddress ip_address;
    if (ip_address.AssignFromIPLiteral(host)) {
      // Instead of -1, 0 is invalid for IPEndPoint.
      int adjusted_port = port == -1 ? 0 : port;
      return std::make_unique<SchemeHostPortMatcherIPHostRule>(
          scheme, IPEndPoint(ip_address, adjusted_port));
    }
  }

  // Otherwise assume we have <hostname-pattern>[:port].
  std::string::size_type pos_colon = raw.rfind(':');
  port = -1;
  if (pos_colon != std::string::npos) {
    if (!ParseInt32(
            base::MakeStringPiece(raw.begin() + pos_colon + 1, raw.end()),
            ParseIntFormat::NON_NEGATIVE, &port) ||
        port > 0xFFFF) {
      return nullptr;  // Port was invalid.
    }
    raw = raw.substr(0, pos_colon);
  }

  // Special-case hostnames that begin with a period.
  // For example, we remap ".google.com" --> "*.google.com".
  std::string hostname_pattern;
  if (raw.starts_with(".")) {
    hostname_pattern = base::StrCat({"*", raw});
  } else {
    hostname_pattern = std::string(raw);
  }

  return std::make_unique<SchemeHostPortMatcherHostnamePatternRule>(
      scheme, hostname_pattern, port);
}

bool SchemeHostPortMatcherRule::IsHostnamePatternRule() const {
  return false;
}

#if !BUILDFLAG(CRONET_BUILD)
size_t SchemeHostPortMatcherRule::EstimateMemoryUsage() const {
  return 0;
}
#endif  // !BUILDFLAG(CRONET_BUILD)

SchemeHostPortMatcherHostnamePatternRule::
    SchemeHostPortMatcherHostnamePatternRule(
        const std::string& optional_scheme,
        const std::string& hostname_pattern,
        int optional_port)
    : optional_scheme_(base::ToLowerASCII(optional_scheme)),
      hostname_pattern_(base::ToLowerASCII(hostname_pattern)),
      optional_port_(optional_port) {
  // |hostname_pattern| shouldn't be an IP address.
  DCHECK(!url::HostIsIPAddress(hostname_pattern));
}

SchemeHostPortMatcherResult SchemeHostPortMatcherHostnamePatternRule::Evaluate(
    const GURL& url) const {
  if (optional_port_ != -1 && url.EffectiveIntPort() != optional_port_) {
    // Didn't match port expectation.
    return SchemeHostPortMatcherResult::kNoMatch;
  }

  if (!optional_scheme_.empty() && url.scheme() != optional_scheme_) {
    // Didn't match scheme expectation.
    return SchemeHostPortMatcherResult::kNoMatch;
  }

  // Note it is necessary to lower-case the host, since GURL uses capital
  // letters for percent-escaped characters.
  return base::MatchPattern(url.host(), hostname_pattern_)
             ? SchemeHostPortMatcherResult::kInclude
             : SchemeHostPortMatcherResult::kNoMatch;
}

std::string SchemeHostPortMatcherHostnamePatternRule::ToString() const {
  std::string str;
  if (!optional_scheme_.empty())
    base::StringAppendF(&str, "%s://", optional_scheme_.c_str());
  str += hostname_pattern_;
  if (optional_port_ != -1)
    base::StringAppendF(&str, ":%d", optional_port_);
  return str;
}

bool SchemeHostPortMatcherHostnamePatternRule::IsHostnamePatternRule() const {
  return true;
}

std::unique_ptr<SchemeHostPortMatcherHostnamePatternRule>
SchemeHostPortMatcherHostnamePatternRule::GenerateSuffixMatchingRule() const {
  if (!hostname_pattern_.starts_with("*")) {
    return std::make_unique<SchemeHostPortMatcherHostnamePatternRule>(
        optional_scheme_, "*" + hostname_pattern_, optional_port_);
  }
  // return a new SchemeHostPortMatcherHostNamePatternRule with the same data.
  return std::make_unique<SchemeHostPortMatcherHostnamePatternRule>(
      optional_scheme_, hostname_pattern_, optional_port_);
}

#if !BUILDFLAG(CRONET_BUILD)
size_t SchemeHostPortMatcherHostnamePatternRule::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(optional_scheme_) +
         base::trace_event::EstimateMemoryUsage(hostname_pattern_);
}
#endif  // !BUILDFLAG(CRONET_BUILD)

SchemeHostPortMatcherIPHostRule::SchemeHostPortMatcherIPHostRule(
    const std::string& optional_scheme,
    const IPEndPoint& ip_end_point)
    : optional_scheme_(base::ToLowerASCII(optional_scheme)),
      ip_host_(AddBracketsIfIPv6(ip_end_point.address())),
      optional_port_(ip_end_point.port()) {}

SchemeHostPortMatcherResult SchemeHostPortMatcherIPHostRule::Evaluate(
    const GURL& url) const {
  if (optional_port_ != 0 && url.EffectiveIntPort() != optional_port_) {
    // Didn't match port expectation.
    return SchemeHostPortMatcherResult::kNoMatch;
  }

  if (!optional_scheme_.empty() && url.scheme() != optional_scheme_) {
    // Didn't match scheme expectation.
    return SchemeHostPortMatcherResult::kNoMatch;
  }

  // Note it is necessary to lower-case the host, since GURL uses capital
  // letters for percent-escaped characters.
  return base::MatchPattern(url.host(), ip_host_)
             ? SchemeHostPortMatcherResult::kInclude
             : SchemeHostPortMatcherResult::kNoMatch;
}

std::string SchemeHostPortMatcherIPHostRule::ToString() const {
  std::string str;
  if (!optional_scheme_.empty())
    base::StringAppendF(&str, "%s://", optional_scheme_.c_str());
  str += ip_host_;
  if (optional_port_ != 0)
    base::StringAppendF(&str, ":%d", optional_port_);
  return str;
}

#if !BUILDFLAG(CRONET_BUILD)
size_t SchemeHostPortMatcherIPHostRule::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(optional_scheme_) +
         base::trace_event::EstimateMemoryUsage(ip_host_);
}
#endif  // !BUILDFLAG(CRONET_BUILD)

SchemeHostPortMatcherIPBlockRule::SchemeHostPortMatcherIPBlockRule(
    const std::string& description,
    const std::string& optional_scheme,
    const IPAddress& ip_prefix,
    size_t prefix_length_in_bits)
    : description_(description),
      optional_scheme_(optional_scheme),
      ip_prefix_(ip_prefix),
      prefix_length_in_bits_(prefix_length_in_bits) {}

SchemeHostPortMatcherResult SchemeHostPortMatcherIPBlockRule::Evaluate(
    const GURL& url) const {
  if (!url.HostIsIPAddress())
    return SchemeHostPortMatcherResult::kNoMatch;

  if (!optional_scheme_.empty() && url.scheme() != optional_scheme_) {
    // Didn't match scheme expectation.
    return SchemeHostPortMatcherResult::kNoMatch;
  }

  // Parse the input IP literal to a number.
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(url.HostNoBracketsPiece()))
    return SchemeHostPortMatcherResult::kNoMatch;

  // Test if it has the expected prefix.
  return IPAddressMatchesPrefix(ip_address, ip_prefix_, prefix_length_in_bits_)
             ? SchemeHostPortMatcherResult::kInclude
             : SchemeHostPortMatcherResult::kNoMatch;
}

std::string SchemeHostPortMatcherIPBlockRule::ToString() const {
  return description_;
}

#if !BUILDFLAG(CRONET_BUILD)
size_t SchemeHostPortMatcherIPBlockRule::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(description_) +
         base::trace_event::EstimateMemoryUsage(optional_scheme_) +
         base::trace_event::EstimateMemoryUsage(ip_prefix_);
}
#endif  // !BUILDFLAG(CRONET_BUILD)

}  // namespace net

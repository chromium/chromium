// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_bypass_rules.h"

#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/parse_number.h"
#include "net/base/url_util.h"

namespace net {

namespace {

// The <-loopback> rule corresponds with "remove the implicitly added bypass
// rules".
//
// The name <-loopback> is not a very precise name (as the implicit rules cover
// more than strictly loopback addresses), however this is the name that is
// used on Windows so re-used here.
//
// For platform-differences between implicit rules see
// ProxyResolverRules::MatchesImplicitRules().
const char kSubtractImplicitBypasses[] = "<-loopback>";

// The <local> rule bypasses any hostname that has no dots (and is not
// an IP literal). The name is misleading as it has nothing to do with
// localhost/loopback addresses, and would have better been called
// something like "simple hostnames". However this is the name used on
// Windows so is matched here.
const char kBypassSimpleHostnames[] = "<local>";

bool IsLinkLocalIP(const GURL& url) {
  // Quick fail if definitely not link-local, to avoid doing unnecessary work in
  // common case.
  if (!(url.host_piece().starts_with("169.254.") ||
        url.host_piece().starts_with("["))) {
    return false;
  }

  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(url.HostNoBracketsPiece()))
    return false;

  return ip_address.IsLinkLocal();
}

// Returns true if the URL's host is an IPv6 literal in the range
// [::ffff:127.0.0.1]/104.
//
// Note that net::IsLocalhost() does not currently return true for such
// addresses. However for proxy resolving such URLs should bypass the use
// of a PAC script, since the destination is local.
bool IsIPv4MappedLoopback(const GURL& url) {
  if (!url.host_piece().starts_with("[::ffff"))
    return false;

  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(url.HostNoBracketsPiece()))
    return false;

  if (!ip_address.IsIPv4MappedIPv6())
    return false;

  return ip_address.bytes()[12] == 127;
}

class HostnamePatternRule : public ProxyBypassRules::Rule {
 public:
  HostnamePatternRule(const std::string& optional_scheme,
                      const std::string& hostname_pattern,
                      int optional_port)
      : optional_scheme_(base::ToLowerASCII(optional_scheme)),
        hostname_pattern_(base::ToLowerASCII(hostname_pattern)),
        optional_port_(optional_port) {}

  Result Evaluate(const GURL& url) const override {
    if (optional_port_ != -1 && url.EffectiveIntPort() != optional_port_)
      return Result::kNoMatch;  // Didn't match port expectation.

    if (!optional_scheme_.empty() && url.scheme() != optional_scheme_)
      return Result::kNoMatch;  // Didn't match scheme expectation.

    // Note it is necessary to lower-case the host, since GURL uses capital
    // letters for percent-escaped characters.
    return base::MatchPattern(url.host(), hostname_pattern_) ? Result::kBypass
                                                             : Result::kNoMatch;
  }

  std::string ToString() const override {
    std::string str;
    if (!optional_scheme_.empty())
      base::StringAppendF(&str, "%s://", optional_scheme_.c_str());
    str += hostname_pattern_;
    if (optional_port_ != -1)
      base::StringAppendF(&str, ":%d", optional_port_);
    return str;
  }

 private:
  const std::string optional_scheme_;
  const std::string hostname_pattern_;
  const int optional_port_;

  DISALLOW_COPY_AND_ASSIGN(HostnamePatternRule);
};

class BypassSimpleHostnamesRule : public ProxyBypassRules::Rule {
 public:
  BypassSimpleHostnamesRule() = default;

  Result Evaluate(const GURL& url) const override {
    return ((url.host_piece().find('.') == std::string::npos) &&
            !url.HostIsIPAddress())
               ? Result::kBypass
               : Result::kNoMatch;
  }

  std::string ToString() const override { return kBypassSimpleHostnames; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BypassSimpleHostnamesRule);
};

class SubtractImplicitBypassesRule : public ProxyBypassRules::Rule {
 public:
  SubtractImplicitBypassesRule() = default;

  Result Evaluate(const GURL& url) const override {
    return ProxyBypassRules::MatchesImplicitRules(url) ? Result::kDontBypass
                                                       : Result::kNoMatch;
  }

  std::string ToString() const override { return kSubtractImplicitBypasses; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubtractImplicitBypassesRule);
};

// Rule for matching a URL that is an IP address, if that IP address falls
// within a certain numeric range. For example, you could use this rule to
// match all the IPs in the CIDR block 10.10.3.4/24.
class IPBlockRule : public ProxyBypassRules::Rule {
 public:
  // |ip_prefix| + |prefix_length| define the IP block to match.
  IPBlockRule(const std::string& description,
              const std::string& optional_scheme,
              const IPAddress& ip_prefix,
              size_t prefix_length_in_bits)
      : description_(description),
        optional_scheme_(optional_scheme),
        ip_prefix_(ip_prefix),
        prefix_length_in_bits_(prefix_length_in_bits) {}

  Result Evaluate(const GURL& url) const override {
    if (!url.HostIsIPAddress())
      return Result::kNoMatch;

    if (!optional_scheme_.empty() && url.scheme() != optional_scheme_)
      return Result::kNoMatch;  // Didn't match scheme expectation.

    // Parse the input IP literal to a number.
    IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(url.HostNoBracketsPiece()))
      return Result::kNoMatch;

    // Test if it has the expected prefix.
    return IPAddressMatchesPrefix(ip_address, ip_prefix_,
                                  prefix_length_in_bits_)
               ? Result::kBypass
               : Result::kNoMatch;
  }

  std::string ToString() const override { return description_; }

 private:
  const std::string description_;
  const std::string optional_scheme_;
  const IPAddress ip_prefix_;
  const size_t prefix_length_in_bits_;

  DISALLOW_COPY_AND_ASSIGN(IPBlockRule);
};

// Returns true if the given string represents an IP address.
// IPv6 addresses are expected to be bracketed.
bool IsIPAddress(const std::string& domain) {
  // From GURL::HostIsIPAddress()
  url::RawCanonOutputT<char, 128> ignored_output;
  url::CanonHostInfo host_info;
  url::Component domain_comp(0, domain.size());
  url::CanonicalizeIPAddress(domain.c_str(), domain_comp, &ignored_output,
                             &host_info);
  return host_info.IsIPAddress();
}

std::unique_ptr<ProxyBypassRules::Rule> ParseRule(
    const std::string& raw_untrimmed,
    ProxyBypassRules::ParseFormat format) {
  std::string raw;
  base::TrimWhitespaceASCII(raw_untrimmed, base::TRIM_ALL, &raw);

  // <local> and <-loopback> are special syntax used by WinInet's bypass list
  // -- we allow it on all platforms and interpret it the same way.
  if (base::LowerCaseEqualsASCII(raw, kBypassSimpleHostnames))
    return std::make_unique<BypassSimpleHostnamesRule>();
  if (base::LowerCaseEqualsASCII(raw, kSubtractImplicitBypasses))
    return std::make_unique<SubtractImplicitBypassesRule>();

  // Extract any scheme-restriction.
  std::string::size_type scheme_pos = raw.find("://");
  std::string scheme;
  if (scheme_pos != std::string::npos) {
    scheme = raw.substr(0, scheme_pos);
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

    return std::make_unique<IPBlockRule>(raw, scheme, ip_prefix,
                                         prefix_length_in_bits);
  }

  // Check if we have an <ip-address>[:port] input. We need to treat this
  // separately since the IP literal may not be in a canonical form.
  std::string host;
  int port;
  if (ParseHostAndPort(raw, &host, &port)) {
    // TODO(eroman): HostForURL() below DCHECKs() when |host| contains an
    // embedded NULL.
    if (host.find('\0') != std::string::npos)
      return nullptr;

    // Note that HostPortPair is used to merely to convert any IPv6 literals to
    // a URL-safe format that can be used by canonicalization below.
    std::string bracketed_host = HostPortPair(host, 80).HostForURL();
    if (IsIPAddress(bracketed_host)) {
      // Canonicalize the IP literal before adding it as a string pattern.
      GURL tmp_url("http://" + bracketed_host);
      return std::make_unique<HostnamePatternRule>(scheme, tmp_url.host(),
                                                   port);
    }
  }

  // Otherwise assume we have <hostname-pattern>[:port].
  std::string::size_type pos_colon = raw.rfind(':');
  port = -1;
  if (pos_colon != std::string::npos) {
    if (!ParseInt32(base::StringPiece(raw.begin() + pos_colon + 1, raw.end()),
                    ParseIntFormat::NON_NEGATIVE, &port) ||
        port > 0xFFFF) {
      return nullptr;  // Port was invalid.
    }
    raw = raw.substr(0, pos_colon);
  }

  // Special-case hostnames that begin with a period.
  // For example, we remap ".google.com" --> "*.google.com".
  if (base::StartsWith(raw, ".", base::CompareCase::SENSITIVE))
    raw = "*" + raw;

  // If suffix matching was asked for, make sure the pattern starts with a
  // wildcard.
  if (format == ProxyBypassRules::ParseFormat::kHostnameSuffixMatching &&
      !base::StartsWith(raw, "*", base::CompareCase::SENSITIVE))
    raw = "*" + raw;

  return std::make_unique<HostnamePatternRule>(scheme, raw, port);
}

}  // namespace

constexpr char net::ProxyBypassRules::kBypassListDelimeter[];

ProxyBypassRules::Rule::Rule() = default;

ProxyBypassRules::Rule::~Rule() = default;

bool ProxyBypassRules::Rule::Equals(const Rule& rule) const {
  return ToString() == rule.ToString();
}

ProxyBypassRules::ProxyBypassRules() = default;

ProxyBypassRules::ProxyBypassRules(const ProxyBypassRules& rhs) {
  *this = rhs;
}

ProxyBypassRules::ProxyBypassRules(ProxyBypassRules&& rhs) {
  *this = std::move(rhs);
}

ProxyBypassRules::~ProxyBypassRules() = default;

ProxyBypassRules& ProxyBypassRules::operator=(const ProxyBypassRules& rhs) {
  ParseFromString(rhs.ToString());
  return *this;
}

ProxyBypassRules& ProxyBypassRules::operator=(ProxyBypassRules&& rhs) {
  rules_ = std::move(rhs.rules_);
  return *this;
}

bool ProxyBypassRules::Matches(const GURL& url, bool reverse) const {
  // Later rules override earlier rules, so evaluating the rule list can be
  // done by iterating over it in reverse and short-circuiting when a match is
  // found. If no matches are found then the implicit rules are consulted.
  //
  // The order of evaluation generally doesn't matter, since the common
  // case is to have a set of (positive) bypass rules.
  //
  // However when mixing positive and negative bypass rules evaluation
  // order makes a difference. The chosen evaluation order here matches
  // WinInet (which supports <-loopback> as a negative rule).
  //
  // Consider these two rule lists:
  //  (a) "localhost; <-loopback>"
  //  (b) "<-loopback>; localhost"
  //
  // The expectation is that Matches("http://localhost/") returns false
  // for (a) since the final rule <-loopback> unbypasses it. Whereas it is
  // expected to return true for (b), since the final rule "localhost"
  // bypasses it again.
  for (auto it = rules_.rbegin(); it != rules_.rend(); ++it) {
    const std::unique_ptr<Rule>& rule = *it;

    switch (rule->Evaluate(url)) {
      case Rule::Result::kBypass:
        return !reverse;
      case Rule::Result::kDontBypass:
        return reverse;
      case Rule::Result::kNoMatch:
        continue;
    }
  }

  // If none of the explicit rules matched, fall back to the implicit rules.
  bool matches_implicit = MatchesImplicitRules(url);
  if (matches_implicit)
    return matches_implicit;

  return reverse;
}

bool ProxyBypassRules::operator==(const ProxyBypassRules& other) const {
  if (rules_.size() != other.rules_.size())
    return false;

  for (size_t i = 0; i < rules_.size(); ++i) {
    if (!rules_[i]->Equals(*other.rules_[i]))
      return false;
  }
  return true;
}

void ProxyBypassRules::ParseFromString(const std::string& raw,
                                       ParseFormat format) {
  Clear();

  base::StringTokenizer entries(raw, ",;");
  while (entries.GetNext()) {
    AddRuleFromString(entries.token(), format);
  }
}

bool ProxyBypassRules::AddRuleForHostname(const std::string& optional_scheme,
                                          const std::string& hostname_pattern,
                                          int optional_port) {
  if (hostname_pattern.empty())
    return false;

  rules_.push_back(std::make_unique<HostnamePatternRule>(
      optional_scheme, hostname_pattern, optional_port));
  return true;
}

void ProxyBypassRules::PrependRuleToBypassSimpleHostnames() {
  rules_.insert(rules_.begin(), std::make_unique<BypassSimpleHostnamesRule>());
}

bool ProxyBypassRules::AddRuleFromString(const std::string& raw_untrimmed,
                                         ParseFormat format) {
  auto rule = ParseRule(raw_untrimmed, format);

  if (rule) {
    rules_.push_back(std::move(rule));
    return true;
  }

  return false;
}

void ProxyBypassRules::AddRulesToSubtractImplicit() {
  rules_.push_back(std::make_unique<SubtractImplicitBypassesRule>());
}

std::string ProxyBypassRules::GetRulesToSubtractImplicit() {
  ProxyBypassRules rules;
  rules.AddRulesToSubtractImplicit();
  return rules.ToString();
}

std::string ProxyBypassRules::ToString() const {
  std::string result;
  for (auto rule(rules_.begin()); rule != rules_.end(); ++rule) {
    result += (*rule)->ToString();
    result += kBypassListDelimeter;
  }
  return result;
}

void ProxyBypassRules::Clear() {
  rules_.clear();
}

bool ProxyBypassRules::MatchesImplicitRules(const GURL& url) {
  // On Windows the implict rules are:
  //
  //     localhost
  //     loopback
  //     127.0.0.1
  //     [::1]
  //     169.254/16
  //     [FE80::]/10
  //
  // And on macOS they are:
  //
  //     localhost
  //     127.0.0.1/8
  //     [::1]
  //     169.254/16
  //
  // Our implicit rules are approximately:
  //
  //     localhost
  //     localhost.
  //     *.localhost
  //     localhost6
  //     localhost6.localdomain6
  //     loopback  [Windows only]
  //     loopback. [Windows only]
  //     [::1]
  //     127.0.0.1/8
  //     169.254/16
  //     [FE80::]/10
  return IsLocalhost(url) || IsIPv4MappedLoopback(url) ||
         IsLinkLocalIP(url)
#if defined(OS_WIN)
         // See http://crbug.com/904889
         || (url.host_piece() == "loopback") ||
         (url.host_piece() == "loopback.")
#endif
      ;
}

}  // namespace net

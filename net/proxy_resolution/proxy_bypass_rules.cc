// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_bypass_rules.h"

#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/parse_number.h"
#include "net/base/url_util.h"

namespace net {

namespace {

class HostnamePatternRule : public ProxyBypassRules::Rule {
 public:
  HostnamePatternRule(const std::string& optional_scheme,
                      const std::string& hostname_pattern,
                      int optional_port)
      : optional_scheme_(base::ToLowerASCII(optional_scheme)),
        hostname_pattern_(base::ToLowerASCII(hostname_pattern)),
        optional_port_(optional_port) {}

  bool Matches(const GURL& url) const override {
    if (optional_port_ != -1 && url.EffectiveIntPort() != optional_port_)
      return false;  // Didn't match port expectation.

    if (!optional_scheme_.empty() && url.scheme() != optional_scheme_)
      return false;  // Didn't match scheme expectation.

    // Note it is necessary to lower-case the host, since GURL uses capital
    // letters for percent-escaped characters.
    return base::MatchPattern(url.host(), hostname_pattern_);
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

  std::unique_ptr<Rule> Clone() const override {
    return std::make_unique<HostnamePatternRule>(
        optional_scheme_, hostname_pattern_, optional_port_);
  }

 private:
  const std::string optional_scheme_;
  const std::string hostname_pattern_;
  const int optional_port_;
};

class BypassLocalRule : public ProxyBypassRules::Rule {
 public:
  bool Matches(const GURL& url) const override {
    const std::string& host = url.host();
    if (host == "127.0.0.1" || host == "[::1]")
      return true;
    return host.find('.') == std::string::npos;
  }

  std::string ToString() const override { return "<local>"; }

  std::unique_ptr<Rule> Clone() const override {
    return std::make_unique<BypassLocalRule>();
  }
};

// Rule for matching a URL that is an IP address, if that IP address falls
// within a certain numeric range. For example, you could use this rule to
// match all the IPs in the CIDR block 10.10.3.4/24.
class BypassIPBlockRule : public ProxyBypassRules::Rule {
 public:
  // |ip_prefix| + |prefix_length| define the IP block to match.
  BypassIPBlockRule(const std::string& description,
                    const std::string& optional_scheme,
                    const IPAddress& ip_prefix,
                    size_t prefix_length_in_bits)
      : description_(description),
        optional_scheme_(optional_scheme),
        ip_prefix_(ip_prefix),
        prefix_length_in_bits_(prefix_length_in_bits) {}

  bool Matches(const GURL& url) const override {
    if (!url.HostIsIPAddress())
      return false;

    if (!optional_scheme_.empty() && url.scheme() != optional_scheme_)
      return false;  // Didn't match scheme expectation.

    // Parse the input IP literal to a number.
    IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(url.HostNoBracketsPiece()))
      return false;

    // Test if it has the expected prefix.
    return IPAddressMatchesPrefix(ip_address, ip_prefix_,
                                  prefix_length_in_bits_);
  }

  std::string ToString() const override { return description_; }

  std::unique_ptr<Rule> Clone() const override {
    return std::make_unique<BypassIPBlockRule>(
        description_, optional_scheme_, ip_prefix_, prefix_length_in_bits_);
  }

 private:
  const std::string description_;
  const std::string optional_scheme_;
  const IPAddress ip_prefix_;
  const size_t prefix_length_in_bits_;
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

}  // namespace

ProxyBypassRules::Rule::Rule() = default;

ProxyBypassRules::Rule::~Rule() = default;

bool ProxyBypassRules::Rule::Equals(const Rule& rule) const {
  return ToString() == rule.ToString();
}

ProxyBypassRules::ProxyBypassRules() = default;

ProxyBypassRules::ProxyBypassRules(const ProxyBypassRules& rhs) {
  AssignFrom(rhs);
}

ProxyBypassRules::~ProxyBypassRules() {
  Clear();
}

ProxyBypassRules& ProxyBypassRules::operator=(const ProxyBypassRules& rhs) {
  AssignFrom(rhs);
  return *this;
}

bool ProxyBypassRules::Matches(const GURL& url) const {
  for (auto it = rules_.begin(); it != rules_.end(); ++it) {
    if ((*it)->Matches(url))
      return true;
  }
  return false;
}

bool ProxyBypassRules::Equals(const ProxyBypassRules& other) const {
  if (rules_.size() != other.rules_.size())
    return false;

  for (size_t i = 0; i < rules_.size(); ++i) {
    if (!rules_[i]->Equals(*other.rules_[i]))
      return false;
  }
  return true;
}

void ProxyBypassRules::ParseFromString(const std::string& raw) {
  ParseFromStringInternal(raw, false);
}

void ProxyBypassRules::ParseFromStringUsingSuffixMatching(
    const std::string& raw) {
  ParseFromStringInternal(raw, true);
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

void ProxyBypassRules::AddRuleToBypassLocal() {
  rules_.push_back(std::make_unique<BypassLocalRule>());
}

bool ProxyBypassRules::AddRuleFromString(const std::string& raw) {
  return AddRuleFromStringInternalWithLogging(raw, false);
}

bool ProxyBypassRules::AddRuleFromStringUsingSuffixMatching(
    const std::string& raw) {
  return AddRuleFromStringInternalWithLogging(raw, true);
}

std::string ProxyBypassRules::ToString() const {
  std::string result;
  for (auto rule(rules_.begin()); rule != rules_.end(); ++rule) {
    result += (*rule)->ToString();
    result += ";";
  }
  return result;
}

void ProxyBypassRules::Clear() {
  rules_.clear();
}

void ProxyBypassRules::AssignFrom(const ProxyBypassRules& other) {
  Clear();

  // Make a copy of the rules list.
  for (auto it = other.rules_.begin(); it != other.rules_.end(); ++it) {
    rules_.push_back((*it)->Clone());
  }
}

void ProxyBypassRules::ParseFromStringInternal(
    const std::string& raw,
    bool use_hostname_suffix_matching) {
  Clear();

  base::StringTokenizer entries(raw, ",;");
  while (entries.GetNext()) {
    AddRuleFromStringInternalWithLogging(entries.token(),
                                         use_hostname_suffix_matching);
  }
}

bool ProxyBypassRules::AddRuleFromStringInternal(
    const std::string& raw_untrimmed,
    bool use_hostname_suffix_matching) {
  std::string raw;
  base::TrimWhitespaceASCII(raw_untrimmed, base::TRIM_ALL, &raw);

  // This is the special syntax used by WinInet's bypass list -- we allow it
  // on all platforms and interpret it the same way.
  if (base::LowerCaseEqualsASCII(raw, "<local>")) {
    AddRuleToBypassLocal();
    return true;
  }

  // Extract any scheme-restriction.
  std::string::size_type scheme_pos = raw.find("://");
  std::string scheme;
  if (scheme_pos != std::string::npos) {
    scheme = raw.substr(0, scheme_pos);
    raw = raw.substr(scheme_pos + 3);
    if (scheme.empty())
      return false;
  }

  if (raw.empty())
    return false;

  // If there is a forward slash in the input, it is probably a CIDR style
  // mask.
  if (raw.find('/') != std::string::npos) {
    IPAddress ip_prefix;
    size_t prefix_length_in_bits;

    if (!ParseCIDRBlock(raw, &ip_prefix, &prefix_length_in_bits))
      return false;

    rules_.push_back(std::make_unique<BypassIPBlockRule>(
        raw, scheme, ip_prefix, prefix_length_in_bits));

    return true;
  }

  // Check if we have an <ip-address>[:port] input. We need to treat this
  // separately since the IP literal may not be in a canonical form.
  std::string host;
  int port;
  if (ParseHostAndPort(raw, &host, &port)) {
    // TODO(eroman): HostForURL() below DCHECKs() when |host| contains an
    // embedded NULL.
    if (host.find('\0') != std::string::npos)
      return false;

    // Note that HostPortPair is used to merely to convert any IPv6 literals to
    // a URL-safe format that can be used by canonicalization below.
    std::string bracketed_host = HostPortPair(host, 80).HostForURL();
    if (IsIPAddress(bracketed_host)) {
      // Canonicalize the IP literal before adding it as a string pattern.
      GURL tmp_url("http://" + bracketed_host);
      return AddRuleForHostname(scheme, tmp_url.host(), port);
    }
  }

  // Otherwise assume we have <hostname-pattern>[:port].
  std::string::size_type pos_colon = raw.rfind(':');
  port = -1;
  if (pos_colon != std::string::npos) {
    if (!ParseInt32(base::StringPiece(raw.begin() + pos_colon + 1, raw.end()),
                    ParseIntFormat::NON_NEGATIVE, &port) ||
        port > 0xFFFF) {
      return false;  // Port was invalid.
    }
    raw = raw.substr(0, pos_colon);
  }

  // Special-case hostnames that begin with a period.
  // For example, we remap ".google.com" --> "*.google.com".
  if (base::StartsWith(raw, ".", base::CompareCase::SENSITIVE))
    raw = "*" + raw;

  // If suffix matching was asked for, make sure the pattern starts with a
  // wildcard.
  if (use_hostname_suffix_matching &&
      !base::StartsWith(raw, "*", base::CompareCase::SENSITIVE))
    raw = "*" + raw;

  return AddRuleForHostname(scheme, raw, port);
}

bool ProxyBypassRules::AddRuleFromStringInternalWithLogging(
    const std::string& raw,
    bool use_hostname_suffix_matching) {
  return AddRuleFromStringInternal(raw, use_hostname_suffix_matching);
}

}  // namespace net

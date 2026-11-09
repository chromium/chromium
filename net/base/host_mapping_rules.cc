// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_mapping_rules.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/url_util.h"
#include "url/scheme_host_port.h"
#include "url/url_canon.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace net {

struct HostMappingRules::MapRule {
  MapRule() = default;

  std::string hostname_pattern;
  std::string replacement_hostname;
  int replacement_port = -1;
};

struct HostMappingRules::ExclusionRule {
  std::string hostname_pattern;
};

HostMappingRules::HostMappingRules() = default;

HostMappingRules::HostMappingRules(const HostMappingRules& host_mapping_rules) =
    default;

HostMappingRules::~HostMappingRules() = default;

HostMappingRules& HostMappingRules::operator=(
    const HostMappingRules& host_mapping_rules) = default;

bool HostMappingRules::RewriteHost(HostPortPair* host_port) const {
  // Check if the hostname was remapped.
  for (const auto& map_rule : map_rules_) {
    // The rule's hostname_pattern will be something like:
    //     www.foo.com
    //     *.foo.com
    //     www.foo.com:1234
    //     *.foo.com:1234
    // First, we'll check for a match just on hostname.
    // If that fails, we'll check for a match with both hostname and port.
    if (!base::MatchPattern(host_port->host(), map_rule.hostname_pattern)) {
      std::string host_port_string = host_port->ToString();
      if (!base::MatchPattern(host_port_string, map_rule.hostname_pattern))
        continue;  // This rule doesn't apply.
    }

    // Check if the hostname was excluded.
    for (const auto& exclusion_rule : exclusion_rules_) {
      if (base::MatchPattern(host_port->host(),
                             exclusion_rule.hostname_pattern))
        return false;
    }

    host_port->set_host(map_rule.replacement_hostname);
    if (map_rule.replacement_port != -1)
      host_port->set_port(static_cast<uint16_t>(map_rule.replacement_port));
    return true;
  }

  return false;
}

HostMappingRules::RewriteResult HostMappingRules::RewriteSchemeHostPort(
    url::SchemeHostPort& scheme_host_port) const {
  if (!scheme_host_port.IsValid() || scheme_host_port.host().empty()) {
    return RewriteResult::kNoMatchingRule;
  }

  std::string canonicalized_input_host;
  std::string_view host = scheme_host_port.host();
  if (!url::IsStandard(scheme_host_port.scheme())) {
    // Local non-standard schemes (url::GetLocalSchemes()) skip
    // IsCanonicalHost in url::SchemeHostPort, so canonicalize before matching
    // lowercased rules or constructing HostPortPair (whose HostForURL()
    // asserts against embedded NULs and unmatched leading '[').
    url::CanonHostInfo input_host_info;
    canonicalized_input_host = CanonicalizeHost(host, &input_host_info);
    if (input_host_info.family == url::CanonHostInfo::BROKEN ||
        canonicalized_input_host.empty()) {
      return RewriteResult::kNoMatchingRule;
    }
    host = canonicalized_input_host;
  }
  if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
    host = host.substr(1, host.size() - 2);
  }

  HostPortPair host_port_pair(host, scheme_host_port.port());
  if (!RewriteHost(&host_port_pair)) {
    return RewriteResult::kNoMatchingRule;
  }

  if (host_port_pair.host() == "^NOTFOUND") {
    return RewriteResult::kInvalidRewrite;
  }

  url::CanonHostInfo host_info;
  std::string canonicalized_host =
      scheme_host_port.scheme() == url::kFileScheme
          ? CanonicalizeFileHost(host_port_pair.HostForURL(), &host_info)
          : CanonicalizeHost(host_port_pair.HostForURL(), &host_info);
  if (host_info.family == url::CanonHostInfo::BROKEN ||
      canonicalized_host.empty()) {
    return RewriteResult::kInvalidRewrite;
  }

  uint16_t port = host_port_pair.port();
  url::SchemeType scheme_type = url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  if (url::GetStandardSchemeType(scheme_host_port.scheme(), &scheme_type) &&
      scheme_type == url::SCHEME_WITH_HOST) {
    port = 0;
  }

  url::SchemeHostPort rewritten(scheme_host_port.scheme(), canonicalized_host,
                                port);
  if (!rewritten.IsValid()) {
    return RewriteResult::kInvalidRewrite;
  }

  scheme_host_port = std::move(rewritten);
  return RewriteResult::kRewritten;
}

bool HostMappingRules::AddRuleFromString(std::string_view rule_string) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      base::TrimWhitespaceASCII(rule_string, base::TRIM_ALL), " ",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Test for EXCLUSION rule.
  if (parts.size() == 2 &&
      base::EqualsCaseInsensitiveASCII(parts[0], "exclude")) {
    ExclusionRule rule;
    rule.hostname_pattern = base::ToLowerASCII(parts[1]);
    exclusion_rules_.push_back(rule);
    return true;
  }

  // Test for MAP rule.
  if (parts.size() == 3 && base::EqualsCaseInsensitiveASCII(parts[0], "map")) {
    MapRule rule;
    rule.hostname_pattern = base::ToLowerASCII(parts[1]);

    if (!ParseHostAndPort(parts[2], &rule.replacement_hostname,
                          &rule.replacement_port)) {
      return false;  // Failed parsing the hostname/port.
    }

    map_rules_.push_back(rule);
    return true;
  }

  return false;
}

void HostMappingRules::SetRulesFromString(std::string_view rules_string) {
  exclusion_rules_.clear();
  map_rules_.clear();

  std::vector<std::string_view> rules = base::SplitStringPiece(
      rules_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (std::string_view rule : rules) {
    bool ok = AddRuleFromString(rule);
    LOG_IF(ERROR, !ok) << "Failed parsing rule: " << rule;
  }
}

}  // namespace net

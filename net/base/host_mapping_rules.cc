// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_mapping_rules.h"

#include <string>

#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"

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

HostMappingRules::RewriteResult HostMappingRules::RewriteUrl(GURL& url) const {
  // Must be a valid and standard URL. Otherwise, Chrome might not know how to
  // find/replace the contained host or port.
  DCHECK(url.is_valid());
  DCHECK(url.IsStandard());
  DCHECK(url.has_host());

  HostPortPair host_port_pair = HostPortPair::FromURL(url);
  if (!RewriteHost(&host_port_pair))
    return RewriteResult::kNoMatchingRule;

  GURL::Replacements replacements;
  std::string port_str = base::NumberToString(host_port_pair.port());
  replacements.SetPortStr(port_str);
  std::string host_str = host_port_pair.HostForURL();
  replacements.SetHostStr(host_str);
  GURL new_url = url.ReplaceComponents(replacements);

  if (!new_url.is_valid())
    return RewriteResult::kInvalidRewrite;

  DCHECK(new_url.IsStandard());
  DCHECK(new_url.has_host());
  DCHECK_EQ(url.EffectiveIntPort() == url::PORT_UNSPECIFIED,
            new_url.EffectiveIntPort() == url::PORT_UNSPECIFIED);

  url = std::move(new_url);
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

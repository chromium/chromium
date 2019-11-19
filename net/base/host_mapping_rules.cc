// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_mapping_rules.h"

#include <string>

#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/url_util.h"

namespace net {

struct HostMappingRules::MapRule {
  MapRule() : replacement_port(-1) {}

  std::string hostname_pattern;
  std::string replacement_hostname;
  int replacement_port;
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
  for (const auto& rule : map_rules_) {
    // The rule's hostname_pattern will be something like:
    //     www.foo.com
    //     *.foo.com
    //     www.foo.com:1234
    //     *.foo.com:1234
    // First, we'll check for a match just on hostname.
    // If that fails, we'll check for a match with both hostname and port.
    if (!base::MatchPattern(host_port->host(), rule.hostname_pattern)) {
      std::string host_port_string = host_port->ToString();
      if (!base::MatchPattern(host_port_string, rule.hostname_pattern))
        continue;  // This rule doesn't apply.
    }

    // Check if the hostname was excluded.
    for (const auto& rule : exclusion_rules_) {
      if (base::MatchPattern(host_port->host(), rule.hostname_pattern))
        return false;
    }

    host_port->set_host(rule.replacement_hostname);
    if (rule.replacement_port != -1)
      host_port->set_port(static_cast<uint16_t>(rule.replacement_port));
    return true;
  }

  return false;
}

bool HostMappingRules::AddRuleFromString(base::StringPiece rule_string) {
  std::vector<base::StringPiece> parts = base::SplitStringPiece(
      base::TrimWhitespaceASCII(rule_string, base::TRIM_ALL), " ",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Test for EXCLUSION rule.
  if (parts.size() == 2 && base::LowerCaseEqualsASCII(parts[0], "exclude")) {
    ExclusionRule rule;
    rule.hostname_pattern = base::ToLowerASCII(parts[1]);
    exclusion_rules_.push_back(rule);
    return true;
  }

  // Test for MAP rule.
  if (parts.size() == 3 && base::LowerCaseEqualsASCII(parts[0], "map")) {
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

void HostMappingRules::SetRulesFromString(base::StringPiece rules_string) {
  exclusion_rules_.clear();
  map_rules_.clear();

  std::vector<base::StringPiece> rules = base::SplitStringPiece(
      rules_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (base::StringPiece rule : rules) {
    bool ok = AddRuleFromString(rule);
    LOG_IF(ERROR, !ok) << "Failed parsing rule: " << rule;
  }
}

}  // namespace net

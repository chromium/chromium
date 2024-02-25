// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_MAPPING_RULES_H_
#define NET_BASE_HOST_MAPPING_RULES_H_

#include <string_view>
#include <vector>

#include "net/base/net_export.h"

class GURL;

namespace net {

class HostPortPair;

class NET_EXPORT_PRIVATE HostMappingRules {
 public:
  enum class RewriteResult {
    kRewritten,
    kNoMatchingRule,
    kInvalidRewrite,
  };

  HostMappingRules();
  HostMappingRules(const HostMappingRules& host_mapping_rules);
  ~HostMappingRules();

  HostMappingRules& operator=(const HostMappingRules& host_mapping_rules);

  // Modifies `*host_port` based on the current rules. Returns true if
  // `*host_port` was modified, false otherwise.
  bool RewriteHost(HostPortPair* host_port) const;

  // Modifies the host and port of `url` based on current rules. May only be
  // called for URLs with a host and a scheme that is standard, and if the
  // scheme does not allow ports, only the host will be rewritten.
  //
  // If `url` is rewritten, returns `kRewritten`. If no matching rule is found,
  // returns `kNoMatchingRule` and `url` is not modified. If a matching rule is
  // found but it results in an invalid URL, e.g. if the rule maps to
  // "^NOTFOUND", returns `kInvalidRewrite` and `url` is not modified.
  RewriteResult RewriteUrl(GURL& url) const;

  // Adds a rule to this mapper. The format of the rule can be one of:
  //
  //   "MAP" <hostname_pattern> <replacement_host> [":" <replacement_port>]
  //   "EXCLUDE" <hostname_pattern>
  //
  // The <replacement_host> can be either a hostname, or an IP address literal.
  //
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(std::string_view rule_string);

  // Sets the rules from a comma separated list of rules.
  void SetRulesFromString(std::string_view rules_string);

 private:
  struct MapRule;
  struct ExclusionRule;

  typedef std::vector<MapRule> MapRuleList;
  typedef std::vector<ExclusionRule> ExclusionRuleList;

  MapRuleList map_rules_;
  ExclusionRuleList exclusion_rules_;
};

}  // namespace net

#endif  // NET_BASE_HOST_MAPPING_RULES_H_

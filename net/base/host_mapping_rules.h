// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_MAPPING_RULES_H_
#define NET_BASE_HOST_MAPPING_RULES_H_

#include <string_view>
#include <vector>

#include "net/base/net_export.h"

namespace url {
class SchemeHostPort;
}  // namespace url

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

  // Modifies the host and port of `scheme_host_port` based on current rules,
  // without round-tripping through `GURL`. Supports both standard and valid
  // non-standard `url::SchemeHostPort` instances (canonicalizing non-standard
  // hosts for matching); if the scheme does not allow ports, only the host
  // will be rewritten. Invalid or hostless `scheme_host_port` instances (and
  // non-standard instances whose host cannot be canonicalized) return
  // `kNoMatchingRule` without modification.
  //
  // Returns `kRewritten` if `scheme_host_port` was modified, `kNoMatchingRule`
  // if no rule matched (leaving `scheme_host_port` unmodified), or
  // `kInvalidRewrite` if a matching rule produced an invalid replacement (e.g.
  // "^NOTFOUND"), leaving `scheme_host_port` unmodified.
  RewriteResult RewriteSchemeHostPort(
      url::SchemeHostPort& scheme_host_port) const;

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

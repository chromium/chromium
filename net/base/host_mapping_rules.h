// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_MAPPING_RULES_H_
#define NET_BASE_HOST_MAPPING_RULES_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

class HostPortPair;

class NET_EXPORT_PRIVATE HostMappingRules {
 public:
  HostMappingRules();
  HostMappingRules(const HostMappingRules& host_mapping_rules);
  ~HostMappingRules();

  HostMappingRules& operator=(const HostMappingRules& host_mapping_rules);

  // Modifies |*host_port| based on the current rules. Returns true if
  // |*host_port| was modified, false otherwise.
  bool RewriteHost(HostPortPair* host_port) const;

  // Adds a rule to this mapper. The format of the rule can be one of:
  //
  //   "MAP" <hostname_pattern> <replacement_host> [":" <replacement_port>]
  //   "EXCLUDE" <hostname_pattern>
  //
  // The <replacement_host> can be either a hostname, or an IP address literal.
  //
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(base::StringPiece rule_string);

  // Sets the rules from a comma separated list of rules.
  void SetRulesFromString(base::StringPiece rules_string);

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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULE_COUNTS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULE_COUNTS_H_

#include <cstddef>
#include <optional>

namespace extensions::declarative_net_request {

// Represents the pair of total rule count and regex rule count for a ruleset.
struct RuleCounts {
  RuleCounts();
  RuleCounts(size_t rule_count,
             std::optional<size_t> unsafe_rule_count,
             size_t regex_rule_count);

  RuleCounts& operator+=(const RuleCounts& that);

  // Tracks the total rule count of a ruleset.
  size_t rule_count = 0;
  // Tracks the "unsafe" rule count of a ruleset (see `IsRuleSafe` inside
  // utils.cc in the same directory for what constitutes an "unsafe" rule). This
  // is only counted for dynamic or session rulesets and is null for static
  // rulesets.
  std::optional<size_t> unsafe_rule_count = std::nullopt;
  // Tracks the total regex rule count of a ruleset.
  size_t regex_rule_count = 0;
};

RuleCounts operator+(const RuleCounts& lhs, const RuleCounts& rhs);

bool operator==(const RuleCounts& lhs, const RuleCounts& rhs);

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULE_COUNTS_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULE_COUNTS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULE_COUNTS_H_

#include <cstddef>

namespace extensions::declarative_net_request {

// Represents the pair of total rule count and regex rule count for a ruleset.
struct RuleCounts {
  RuleCounts();
  RuleCounts(size_t rule_count, size_t regex_rule_count);

  RuleCounts& operator+=(const RuleCounts& that);

  // This CHECKs that counts in |that| are smaller than or equal to the one in
  // |this|.
  RuleCounts& operator-=(const RuleCounts& that);

  size_t rule_count = 0;
  size_t regex_rule_count = 0;
  // TODO(crbug.com/1485747): Add an unsafe_rule_count here.
};

RuleCounts operator+(const RuleCounts& lhs, const RuleCounts& rhs);

// This CHECKs that counts in |rhs| are smaller than or equal to the one in
// |lhs|.
RuleCounts operator-(const RuleCounts& lhs, const RuleCounts& rhs);

bool operator==(const RuleCounts& lhs, const RuleCounts& rhs);

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULE_COUNTS_H_

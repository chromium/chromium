// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_COUNT_PAIR_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_COUNT_PAIR_H_

#include <cstddef>

namespace extensions {
namespace declarative_net_request {

// Represents the pair of total rule count and regex rule count for a ruleset.
struct RulesCountPair {
  RulesCountPair();
  RulesCountPair(size_t rule_count, size_t regex_rule_count);

  RulesCountPair& operator+=(const RulesCountPair& that);

  // This CHECKs that counts in |that| are smaller than or equal to the one in
  // |this|.
  RulesCountPair& operator-=(const RulesCountPair& that);

  size_t rule_count = 0;
  size_t regex_rule_count = 0;
};

RulesCountPair operator+(const RulesCountPair& lhs, const RulesCountPair& rhs);

// This CHECKs that counts in |rhs| are smaller than or equal to the one in
// |lhs|.
RulesCountPair operator-(const RulesCountPair& lhs, const RulesCountPair& rhs);

bool operator==(const RulesCountPair& lhs, const RulesCountPair& rhs);

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULES_COUNT_PAIR_H_

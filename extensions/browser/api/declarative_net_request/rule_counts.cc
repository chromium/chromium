// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/rule_counts.h"

#include "base/check_op.h"

namespace extensions::declarative_net_request {

RuleCounts::RuleCounts() = default;
RuleCounts::RuleCounts(size_t rule_count,
                       std::optional<size_t> unsafe_rule_count,
                       size_t regex_rule_count)
    : rule_count(rule_count),
      unsafe_rule_count(unsafe_rule_count),
      regex_rule_count(regex_rule_count) {}

RuleCounts& RuleCounts::operator+=(const RuleCounts& that) {
  rule_count += that.rule_count;
  if (unsafe_rule_count.has_value()) {
    *unsafe_rule_count += that.unsafe_rule_count.value_or(0);
  }
  regex_rule_count += that.regex_rule_count;
  return *this;
}

RuleCounts operator+(const RuleCounts& lhs, const RuleCounts& rhs) {
  RuleCounts result = lhs;
  return result += rhs;
}

bool operator==(const RuleCounts& lhs, const RuleCounts& rhs) {
  return lhs.rule_count == rhs.rule_count &&
         lhs.unsafe_rule_count == rhs.unsafe_rule_count &&
         lhs.regex_rule_count == rhs.regex_rule_count;
}

}  // namespace extensions::declarative_net_request

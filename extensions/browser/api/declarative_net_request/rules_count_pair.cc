// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/rules_count_pair.h"

#include "base/check_op.h"

namespace extensions {
namespace declarative_net_request {

RulesCountPair::RulesCountPair() = default;
RulesCountPair::RulesCountPair(size_t rule_count, size_t regex_rule_count)
    : rule_count(rule_count), regex_rule_count(regex_rule_count) {}

RulesCountPair& RulesCountPair::operator+=(const RulesCountPair& that) {
  rule_count += that.rule_count;
  regex_rule_count += that.regex_rule_count;
  return *this;
}

RulesCountPair& RulesCountPair::operator-=(const RulesCountPair& that) {
  CHECK_GE(rule_count, that.rule_count);
  CHECK_GE(regex_rule_count, that.regex_rule_count);
  rule_count -= that.rule_count;
  regex_rule_count -= that.regex_rule_count;
  return *this;
}

RulesCountPair operator+(const RulesCountPair& lhs, const RulesCountPair& rhs) {
  RulesCountPair result = lhs;
  return result += rhs;
}

RulesCountPair operator-(const RulesCountPair& lhs, const RulesCountPair& rhs) {
  RulesCountPair result = lhs;
  return result -= rhs;
}

bool operator==(const RulesCountPair& lhs, const RulesCountPair& rhs) {
  return lhs.rule_count == rhs.rule_count &&
         lhs.regex_rule_count == rhs.regex_rule_count;
}

}  // namespace declarative_net_request
}  // namespace extensions

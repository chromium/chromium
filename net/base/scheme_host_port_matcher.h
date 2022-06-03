// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEME_HOST_PORT_MATCHER_H_
#define NET_BASE_SCHEME_HOST_PORT_MATCHER_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/base/scheme_host_port_matcher_rule.h"

namespace net {

// SchemeHostPortMatcher holds an ordered list of rules for matching URLs, that
// is serializable to a string.
//
// Rules are evaluated in reverse, and each can be either an "include this URL"
// or an "exclude this URL".
//
// In a simple configuration, all rules are "include this URL" so evaluation
// order doesn't matter. When combining include and exclude rules,
// later rules will have precedence over earlier rules.
class NET_EXPORT SchemeHostPortMatcher {
 public:
  using RuleList = std::vector<std::unique_ptr<SchemeHostPortMatcherRule>>;

  // Note: This class is movable but not copiable.
  SchemeHostPortMatcher() = default;
  SchemeHostPortMatcher(SchemeHostPortMatcher&& rhs) = default;
  SchemeHostPortMatcher& operator=(SchemeHostPortMatcher&& rhs) = default;
  ~SchemeHostPortMatcher() = default;

  // The delimiter used by |ToString()|.
  constexpr static char kPrintRuleListDelimiter = ';';

  // The delimiters to use when parsing rules.
  constexpr static char kParseRuleListDelimiterList[] = ",;";

  // Creates a SchemeHostPortMatcher by best-effort parsing each of the
  // |kParseRuleListDelimiterList| separated rules. Any rules that could not be
  // parsed are silently rejected. It only parses all the rule types that appear
  // in scheme_host_port_rules.h, types with other serializations will need to
  // be handled by the caller.
  static SchemeHostPortMatcher FromRawString(const std::string& raw);

  // Returns the current list of rules.
  const RuleList& rules() const { return rules_; }

  // Add rule to the matcher as the first one in the rule list.
  void AddAsFirstRule(std::unique_ptr<SchemeHostPortMatcherRule> rule);

  // Add rule to the matcher as the last one in the rule list.
  void AddAsLastRule(std::unique_ptr<SchemeHostPortMatcherRule> rule);

  // Replace rule on |index| in the internal RuleList.
  void ReplaceRule(size_t index,
                   std::unique_ptr<SchemeHostPortMatcherRule> rule);

  // Returns true if |url| was positively matched by the rules.
  bool Includes(const GURL& url) const;

  // Returns the result of evaluating the rule list. Prefer using Includes()
  // over this function. Evaluate() can be used when the caller needs to
  // distinguish when matches were due to negative rules.
  SchemeHostPortMatcherResult Evaluate(const GURL& url) const;

  // Serializes the rules to a string representation. The serialized
  // representation is a |kPrintRuleListDelimiter| delimited list of rules,
  // where each rule defines its own serialization.
  std::string ToString() const;

  // Removes all the rules.
  void Clear();

 private:
  RuleList rules_;
};

}  // namespace net

#endif  // NET_BASE_SCHEME_HOST_PORT_MATCHER_H_

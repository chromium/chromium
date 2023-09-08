// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/scheme_host_port_matcher.h"

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_usage_estimator.h"

namespace net {

SchemeHostPortMatcher::SchemeHostPortMatcher() = default;
SchemeHostPortMatcher::SchemeHostPortMatcher(SchemeHostPortMatcher&& rhs) =
    default;
SchemeHostPortMatcher& SchemeHostPortMatcher::operator=(
    SchemeHostPortMatcher&& rhs) = default;
SchemeHostPortMatcher::~SchemeHostPortMatcher() = default;

// Declares SchemeHostPortMatcher::kParseRuleListDelimiterList[], not a
// redefinition. This is needed for link.
// static
constexpr char SchemeHostPortMatcher::kParseRuleListDelimiterList[];

// Declares SchemeHostPortMatcher::kPrintRuleListDelimiter, not a
// redefinition. This is needed for link.
// static
constexpr char SchemeHostPortMatcher::kPrintRuleListDelimiter;

// static
SchemeHostPortMatcher SchemeHostPortMatcher::FromRawString(
    const std::string& raw) {
  SchemeHostPortMatcher result;

  base::StringTokenizer entries(raw, kParseRuleListDelimiterList);
  while (entries.GetNext()) {
    auto rule = SchemeHostPortMatcherRule::FromUntrimmedRawString(
        entries.token_piece());
    if (rule) {
      result.AddAsLastRule(std::move(rule));
    }
  }

  return result;
}

void SchemeHostPortMatcher::AddAsFirstRule(
    std::unique_ptr<SchemeHostPortMatcherRule> rule) {
  DCHECK(rule);
  rules_.insert(rules_.begin(), std::move(rule));
}

void SchemeHostPortMatcher::AddAsLastRule(
    std::unique_ptr<SchemeHostPortMatcherRule> rule) {
  DCHECK(rule);
  rules_.push_back(std::move(rule));
}

void SchemeHostPortMatcher::ReplaceRule(
    size_t index,
    std::unique_ptr<SchemeHostPortMatcherRule> rule) {
  DCHECK_LT(index, rules_.size());
  rules_[index] = std::move(rule);
}

bool SchemeHostPortMatcher::Includes(const GURL& url) const {
  return Evaluate(url) == SchemeHostPortMatcherResult::kInclude;
}

SchemeHostPortMatcherResult SchemeHostPortMatcher::Evaluate(
    const GURL& url) const {
  // Later rules override earlier rules, so evaluating the rule list can be
  // done by iterating over it in reverse and short-circuiting when a match is
  // found.
  //
  // The order of evaluation generally doesn't matter if all the rules are
  // positive rules, so matches are just additive.
  //
  // However when mixing positive and negative rules, evaluation order makes a
  // difference.
  for (const auto& rule : base::Reversed(rules_)) {
    SchemeHostPortMatcherResult result = rule->Evaluate(url);
    if (result != SchemeHostPortMatcherResult::kNoMatch)
      return result;
  }

  return SchemeHostPortMatcherResult::kNoMatch;
}

std::string SchemeHostPortMatcher::ToString() const {
  std::string result;
  for (const auto& rule : rules_) {
    DCHECK(!base::Contains(rule->ToString(), kParseRuleListDelimiterList));
    result += rule->ToString();
    result.push_back(kPrintRuleListDelimiter);
  }
  return result;
}

void SchemeHostPortMatcher::Clear() {
  rules_.clear();
}

#if !BUILDFLAG(CRONET_BUILD)
size_t SchemeHostPortMatcher::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(rules_);
}
#endif  // !BUILDFLAG(CRONET_BUILD)

}  // namespace net

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REGEX_RULES_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REGEX_RULES_MATCHER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/substring_set_matcher/substring_set_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher_base.h"
#include "third_party/re2/src/re2/filtered_re2.h"

namespace extensions::declarative_net_request {

// Structure to hold a RegexRule together with its corresponding compiled
// re2::Re2 object.
struct RegexRuleInfo {
  RegexRuleInfo(const flat::RegexRule* regex_rule, const re2::RE2* regex);
  RegexRuleInfo(const RegexRuleInfo& info);
  RegexRuleInfo& operator=(const RegexRuleInfo& info);
  raw_ptr<const flat::RegexRule> regex_rule;
  raw_ptr<const re2::RE2, DanglingUntriaged> regex;
};

// RegexRulesMatcher deals with matching of regular expression rules. It is an
// implementation detail of RulesetMatcher. This uses the FilteredRE2 class from
// the re2 library to achieve fast matching of a set of declarative regex rules
// against a request. How this works:
//
// Initialization:
// 1. During initialization, we add each regex to the FilteredRE2 class.
// 2. We compile the FilteredRE2 object which returns us a set of substrings.
//    These are added to `substring_matcher_` for use in #3 below.
//
// Matching
// 3. Given a request url, we find the set of strings from #2. that are
//    substrings of the request url. This uses the
//    url_matcher::SubstringSetMatcher class which internally uses the
//    Aho-Corasick algorithm.
// 4. Given the list of matched strings from #3, FilteredRE2 returns the list
//    of regexes (rules) that might potentially match. To reduce the number of
//    regexes that need to be matched (since it's expensive), we prune the list
//    even further by checking if the rule metadata matches the request.
// 5. Given the list of potentially matching rules, we finally match the actual
//    regexes against the request url, as required.
class RegexRulesMatcher final : public RulesetMatcherBase {
 public:
  using RegexRulesList =
      ::flatbuffers::Vector<flatbuffers::Offset<flat::RegexRule>>;
  using RegexMatchKey =
      std::pair<const RegexRulesMatcher*, RulesetMatchingStage>;
  RegexRulesMatcher(const ExtensionId& extension_id,
                    RulesetID ruleset_id,
                    const RegexRulesList* before_request_regex_list,
                    const RegexRulesList* headers_received_regex_list,
                    const ExtensionMetadataList* metadata_list);

  RegexRulesMatcher(const RegexRulesMatcher&) = delete;
  RegexRulesMatcher& operator=(const RegexRulesMatcher&) = delete;

  // RulesetMatcherBase override:
  ~RegexRulesMatcher() override;
  std::vector<RequestAction> GetModifyHeadersActions(
      const RequestParams& params,
      RulesetMatchingStage stage,
      std::optional<uint64_t> min_priority) const override;
  bool IsExtraHeadersMatcher() const override;
  size_t GetRulesCount() const override;
  size_t GetBeforeRequestRulesCount() const override;
  size_t GetHeadersReceivedRulesCount() const override;

 private:
  // A helper class used to match regex rules from a single ruleset for a single
  // request stage.
  class MatchHelper {
   public:
    MatchHelper(const raw_ptr<const RegexRulesList> regex_list,
                const RegexRulesMatcher* parent_matcher,
                RulesetMatchingStage stage);
    ~MatchHelper();
    MatchHelper(const MatchHelper& other) = delete;
    MatchHelper& operator=(const MatchHelper& other) = delete;

    // Returns the rule count for regex rules to be matched in the request stage
    // corresponding to this MatchHelper.
    size_t GetRulesCount() const;

    // Returns the potentially matching rules for the given request. A
    // potentially matching rule is one whose metadata matches the given request
    // `params` and which is not ruled out as a potential match by the
    // `filtered_re2_` object. Note: The returned vector is sorted in descending
    // order of rule priority.
    const std::vector<RegexRuleInfo>& GetPotentialMatches(
        const RequestParams& params) const;

   private:
    // Helper to build the necessary data structures for matching.
    void InitializeMatcher();

    // Returns true if this matcher doesn't correspond to any rules.
    bool IsEmpty() const;

    // The backing regex rules list for this MatchHelper. Contains all rules
    // that are meant to be matched for a single request stage.
    const raw_ptr<const RegexRulesList> regex_list_;

    // The key used to cache potential regex matches from this helper in a
    // RequestParams. Consists of a pointer to the RegexRulesMatcher which owns
    // this helper and the request stage in which this helper will match rules.
    RegexMatchKey regex_match_key_;

    // Data structures used for matching. Initialized during construction in
    // InitializeMatcher() and immutable for the rest of the object lifetime.

    // This provides a pre-filtering mechanism, to reduce the number of regular
    // expressions that are actually matched against a request.
    re2::FilteredRE2 filtered_re2_;

    // Map from re2 ID (as used by `filtered_re2_`) to the flat::RegexRule in
    // `regex_list_`.
    std::map<int, raw_ptr<const flat::RegexRule, CtnExperimental>>
        re2_id_to_rules_map_;

    // Structure for fast substring matching. Given a string S and a set of
    // candidate strings, returns the sub-set of candidate strings that are a
    // substring of S. Uses the Aho-Corasick algorithm internally. Will be null
    // iff IsEmpty() returns false.
    std::unique_ptr<base::SubstringSetMatcher> substring_matcher_;
  };

  // RulesetMatcherBase override:
  std::optional<RequestAction> GetAllowAllRequestsAction(
      const RequestParams& params,
      RulesetMatchingStage stage) const override;
  std::optional<RequestAction> GetActionIgnoringAncestors(
      const RequestParams& params,
      RulesetMatchingStage stage) const override;

  // Returns a RequestAction for the given matched regex rule `info`.
  std::optional<RequestAction> CreateActionFromInfo(
      const RequestParams& params,
      const RegexRuleInfo& info) const;

  // Returns a RequestAction for the the given regex substitution rule.
  std::optional<RequestAction> CreateRegexSubstitutionRedirectAction(
      const RequestParams& params,
      const RegexRuleInfo& info) const;

  // Returns the corresponding rule matching helper for the given rule matching
  // `stage`.
  const MatchHelper& GetMatcherForStage(RulesetMatchingStage stage) const;

  // A helper for matching regex rules in the onBeforeRequest stage.
  MatchHelper before_request_matcher_;

  // A helper for matching regex rules in the onHeadersReceived stage.
  MatchHelper headers_received_matcher_;

  const raw_ptr<const ExtensionMetadataList> metadata_list_;

  // Whether this matcher contains rules that will match on, or modify headers.
  const bool is_extra_headers_matcher_;
};

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REGEX_RULES_MATCHER_H_

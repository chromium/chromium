// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REGEX_RULES_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REGEX_RULES_MATCHER_H_

#include <memory>

#include "base/macros.h"
#include "components/url_matcher/substring_set_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher_base.h"
#include "third_party/re2/src/re2/filtered_re2.h"

namespace extensions {
namespace declarative_net_request {

// Structure to hold a RegexRule together with its corresponding compiled
// re2::Re2 object.
struct RegexRuleInfo {
  RegexRuleInfo(const flat::RegexRule* regex_rule, const re2::RE2* regex);
  RegexRuleInfo(const RegexRuleInfo& info);
  RegexRuleInfo& operator=(const RegexRuleInfo& info);
  const flat::RegexRule* regex_rule;
  const re2::RE2* regex;
};

// RegexRulesMatcher deals with matching of regular expression rules. It is an
// implementation detail of RulesetMatcher. This uses the FilteredRE2 class from
// the re2 library to achieve fast matching of a set of declarative regex rules
// against a request. How this works:
//
// Initialization:
// 1. During initialization, we add each regex to the FilteredRE2 class.
// 2. We compile the FilteredRE2 object which returns us a set of substrings.
//    These are added to |substring_matcher_| for use in #3 below.
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
  RegexRulesMatcher(const ExtensionId& extension_id,
                    RulesetID ruleset_id,
                    const RegexRulesList* regex_list,
                    const ExtensionMetadataList* metadata_list);

  // RulesetMatcherBase override:
  ~RegexRulesMatcher() override;
  std::vector<RequestAction> GetModifyHeadersActions(
      const RequestParams& params,
      base::Optional<uint64_t> min_priority) const override;
  bool IsExtraHeadersMatcher() const override {
    return is_extra_headers_matcher_;
  }
  size_t GetRulesCount() const override { return regex_list_->size(); }

 private:
  // RulesetMatcherBase override:
  base::Optional<RequestAction> GetAllowAllRequestsAction(
      const RequestParams& params) const override;
  base::Optional<RequestAction> GetBeforeRequestActionIgnoringAncestors(
      const RequestParams& params) const override;

  // Helper to build the necessary data structures for matching.
  void InitializeMatcher();

  // Returns true if this matcher doesn't correspond to any rules.
  bool IsEmpty() const;

  // Returns the potentially matching rules for the given request. A potentially
  // matching rule is one whose metadata matches the given request |params| and
  // which is not ruled out as a potential match by the |filtered_re2_| object.
  // Note: The returned vector is sorted in descending order of rule priority.
  const std::vector<RegexRuleInfo>& GetPotentialMatches(
      const RequestParams& params) const;

  // Returns a RequestAction for the the given regex substitution rule.
  base::Optional<RequestAction> CreateRegexSubstitutionRedirectAction(
      const RequestParams& params,
      const RegexRuleInfo& info) const;

  // Pointers to flatbuffer indexed data. Guaranteed to be valid through the
  // lifetime of the object.
  const RegexRulesList* const regex_list_;
  const ExtensionMetadataList* const metadata_list_;

  const bool is_extra_headers_matcher_;

  // Data structures used for matching. Initialized during construction in
  // InitializeMatcher() and immutable for the rest of the object lifetime.

  // This provides a pre-filtering mechanism, to reduce the number of regular
  // expressions that are actually matched against a request.
  re2::FilteredRE2 filtered_re2_;

  // Map from re2 ID (as used by |filtered_re2_|) to the flat::RegexRule in
  // |regex_list_|.
  std::map<int, const flat::RegexRule*> re2_id_to_rules_map_;

  // Structure for fast substring matching. Given a string S and a set of
  // candidate strings, returns the sub-set of candidate strings that are a
  // substring of S. Uses the Aho-Corasick algorithm internally. Will be null
  // iff IsEmpty() returns false.
  std::unique_ptr<url_matcher::SubstringSetMatcher> substring_matcher_;

  DISALLOW_COPY_AND_ASSIGN(RegexRulesMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REGEX_RULES_MATCHER_H_

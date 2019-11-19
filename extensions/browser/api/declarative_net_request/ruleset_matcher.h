// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "extensions/browser/api/declarative_net_request/extension_url_pattern_index_matcher.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher_interface.h"

namespace extensions {

namespace declarative_net_request {
class RulesetSource;

namespace flat {
struct ExtensionIndexedRuleset;
struct UrlRuleMetadata;
}  // namespace flat

// RulesetMatcher encapsulates the Declarative Net Request API ruleset
// corresponding to a single RulesetSource. Since this class is immutable, it is
// thread-safe.
class RulesetMatcher : public RulesetMatcherInterface {
 public:
  // Describes the result of creating a RulesetMatcher instance.
  // This is logged as part of UMA. Hence existing values should not be re-
  // numbered or deleted. New values should be added before kLoadRulesetMax.
  enum LoadRulesetResult {
    // Ruleset loading succeeded.
    kLoadSuccess = 0,

    // Ruleset loading failed since the provided path did not exist.
    kLoadErrorInvalidPath = 1,

    // Ruleset loading failed due to a file read error.
    kLoadErrorFileRead = 2,

    // Ruleset loading failed due to a checksum mismatch.
    kLoadErrorChecksumMismatch = 3,

    // Ruleset loading failed due to version header mismatch.
    // TODO(karandeepb): This should be split into two cases:
    //    - When the indexed ruleset doesn't have the version header in the
    //      correct format.
    //    - When the indexed ruleset's version is not the same as that used by
    //      Chrome.
    kLoadErrorVersionMismatch = 4,

    kLoadResultMax
  };

  // Factory function to create a verified RulesetMatcher for |source|. Must be
  // called on a sequence where file IO is allowed. Returns kLoadSuccess on
  // success along with the ruleset |matcher|.
  static LoadRulesetResult CreateVerifiedMatcher(
      const RulesetSource& source,
      int expected_ruleset_checksum,
      std::unique_ptr<RulesetMatcher>* matcher);

  // RulesetMatcherInterface overrides:
  ~RulesetMatcher() override;

  base::Optional<RequestAction> GetBlockOrCollapseAction(
      const RequestParams& params) const override;
  base::Optional<RequestAction> GetAllowAction(
      const RequestParams& params) const override;
  base::Optional<RequestAction> GetRedirectAction(
      const RequestParams& params) const override;
  base::Optional<RequestAction> GetUpgradeAction(
      const RequestParams& params) const override;
  uint8_t GetRemoveHeadersMask(
      const RequestParams& params,
      uint8_t ignored_mask,
      std::vector<RequestAction>* remove_headers_actions) const override;
  bool IsExtraHeadersMatcher() const override;

  // Returns a RequestAction constructed from the matching redirect or upgrade
  // rule with the highest priority, or base::nullopt if no matching redirect or
  // upgrade rules are found for this request.
  base::Optional<RequestAction> GetRedirectOrUpgradeActionByPriority(
      const RequestParams& params) const;

  // ID of the ruleset. Each extension can have multiple rulesets with
  // their own unique ids.
  size_t id() const { return id_; }

  // Priority of the ruleset. Each extension can have multiple rulesets with
  // their own different priorities.
  size_t priority() const { return priority_; }

 private:
  explicit RulesetMatcher(std::string ruleset_data,
                          size_t id,
                          size_t priority,
                          api::declarative_net_request::SourceType source_type,
                          const ExtensionId& extension_id);

  const std::string ruleset_data_;

  const flat::ExtensionIndexedRuleset* const root_;

  const size_t id_;
  const size_t priority_;

  // Underlying matcher for filter-list style rules supported using the
  // |url_pattern_index| component.
  const ExtensionUrlPatternIndexMatcher url_pattern_index_matcher_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_

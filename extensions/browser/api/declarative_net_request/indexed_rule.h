// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEXED_RULE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEXED_RULE_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"

class GURL;

namespace extensions::declarative_net_request {

enum class ParseResult;

// An intermediate structure to store a Declarative Net Request API rule while
// indexing. This structure aids in the subsequent conversion to a flatbuffer
// UrlRule as specified by the url_pattern_index component.
struct IndexedRule {
  IndexedRule();
  IndexedRule(const IndexedRule&) = delete;
  IndexedRule(IndexedRule&& other);

  IndexedRule& operator=(const IndexedRule&) = delete;
  IndexedRule& operator=(IndexedRule&& other);

  ~IndexedRule();

  static ParseResult CreateIndexedRule(
      extensions::api::declarative_net_request::Rule parsed_rule,
      const GURL& base_url,
      RulesetID ruleset_id,
      IndexedRule* indexed_rule);

  api::declarative_net_request::RuleActionType action_type =
      api::declarative_net_request::RuleActionType::kNone;

  // These fields correspond to the attributes of a flatbuffer UrlRule, as
  // specified by the url_pattern_index component.
  uint32_t id = 0;
  uint32_t priority = 0;
  uint8_t options = url_pattern_index::flat::OptionFlag_NONE;
  uint16_t element_types = url_pattern_index::flat::ElementType_NONE;
  uint16_t request_methods = url_pattern_index::flat::RequestMethod_NONE;
  uint8_t activation_types = url_pattern_index::flat::ActivationType_NONE;
  url_pattern_index::flat::UrlPatternType url_pattern_type =
      url_pattern_index::flat::UrlPatternType_SUBSTRING;
  url_pattern_index::flat::AnchorType anchor_left =
      url_pattern_index::flat::AnchorType_NONE;
  url_pattern_index::flat::AnchorType anchor_right =
      url_pattern_index::flat::AnchorType_NONE;
  std::string url_pattern;

  // Lower-cased and sorted as required by the url_pattern_index component.
  std::vector<std::string> initiator_domains;
  std::vector<std::string> excluded_initiator_domains;
  std::vector<std::string> request_domains;
  std::vector<std::string> excluded_request_domains;

  // Note: For redirect rules, exactly one of |redirect_url|,
  // |regex_substitution| or |url_transform| will be set.
  // The redirect url for the rule.
  std::optional<std::string> redirect_url;
  // The regex substitution for this rule.
  std::optional<std::string> regex_substitution;
  // UrlTransform for this rule.
  std::optional<api::declarative_net_request::URLTransform> url_transform;

  // List of request headers to modify. Valid iff this is a modify headers rule.
  std::vector<api::declarative_net_request::ModifyHeaderInfo>
      request_headers_to_modify;

  // List of response headers to modify. Valid iff this is a modify headers
  // rule.
  std::vector<api::declarative_net_request::ModifyHeaderInfo>
      response_headers_to_modify;

  // Set of tab IDs this rule applies to.
  base::flat_set<int> tab_ids;

  // Set of tab IDs this rule doesn't apply to.
  base::flat_set<int> excluded_tab_ids;

  // List of response headers this rule applies to.
  std::vector<api::declarative_net_request::HeaderInfo> response_headers;

  // List of response headers this rule doesn't apply to.
  std::vector<api::declarative_net_request::HeaderInfo>
      excluded_response_headers;
};

// Compute the rule priority for indexing, by combining the priority from
// the JSON rule and the priority of the action type. Exposed for testing.
uint64_t ComputeIndexedRulePriority(
    int parsed_rule_priority,
    api::declarative_net_request::RuleActionType action_type);

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEXED_RULE_H_

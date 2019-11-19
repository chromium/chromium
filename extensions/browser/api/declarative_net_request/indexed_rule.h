// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEXED_RULE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEXED_RULE_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "extensions/common/api/declarative_net_request.h"

class GURL;

namespace extensions {
namespace declarative_net_request {

enum class ParseResult;

// An intermediate structure to store a Declarative Net Request API rule while
// indexing. This structure aids in the subsequent conversion to a flatbuffer
// UrlRule as specified by the url_pattern_index component.
struct IndexedRule {
  IndexedRule();
  ~IndexedRule();
  IndexedRule(IndexedRule&& other);
  IndexedRule& operator=(IndexedRule&& other);

  static ParseResult CreateIndexedRule(
      extensions::api::declarative_net_request::Rule parsed_rule,
      const GURL& base_url,
      IndexedRule* indexed_rule);

  api::declarative_net_request::RuleActionType action_type =
      api::declarative_net_request::RULE_ACTION_TYPE_NONE;

  // These fields correspond to the attributes of a flatbuffer UrlRule, as
  // specified by the url_pattern_index component.
  uint32_t id = 0;
  uint32_t priority = 0;
  uint8_t options = url_pattern_index::flat::OptionFlag_NONE;
  uint16_t element_types = url_pattern_index::flat::ElementType_NONE;
  uint8_t activation_types = url_pattern_index::flat::ActivationType_NONE;
  url_pattern_index::flat::UrlPatternType url_pattern_type =
      url_pattern_index::flat::UrlPatternType_SUBSTRING;
  url_pattern_index::flat::AnchorType anchor_left =
      url_pattern_index::flat::AnchorType_NONE;
  url_pattern_index::flat::AnchorType anchor_right =
      url_pattern_index::flat::AnchorType_NONE;
  std::string url_pattern;

  // Lower-cased and sorted as required by the url_pattern_index component.
  std::vector<std::string> domains;
  std::vector<std::string> excluded_domains;

  // The redirect url for the rule. For redirect rules, exactly one of
  // |redirect_url| or |url_transform| will be valid.
  base::Optional<std::string> redirect_url;

  // UrlTransform for this rule. For redirect rules, exactly one of
  // |redirect_url| or |url_transform| will be valid.
  std::unique_ptr<api::declarative_net_request::URLTransform> url_transform;

  // List of headers to remove, valid iff this is a remove headers rule.
  std::set<api::declarative_net_request::RemoveHeaderType> remove_headers_set;

  DISALLOW_COPY_AND_ASSIGN(IndexedRule);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEXED_RULE_H_

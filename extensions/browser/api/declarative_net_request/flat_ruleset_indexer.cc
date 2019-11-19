// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"

#include <string>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "net/base/escape.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;
namespace flat_rule = url_pattern_index::flat;

template <typename T>
using FlatOffset = flatbuffers::Offset<T>;

template <typename T>
using FlatVectorOffset = FlatOffset<flatbuffers::Vector<FlatOffset<T>>>;

using FlatStringOffset = FlatOffset<flatbuffers::String>;
using FlatStringListOffset = FlatVectorOffset<flatbuffers::String>;

// Writes to |builder| a flatbuffer vector of shared strings corresponding to
// |container| and returns the offset to it. If |container| is empty, returns an
// empty offset.
template <typename T>
FlatStringListOffset BuildVectorOfSharedStrings(
    flatbuffers::FlatBufferBuilder* builder,
    const T& container) {
  if (container.empty())
    return FlatStringListOffset();

  std::vector<FlatStringOffset> offsets;
  offsets.reserve(container.size());
  for (const std::string& str : container)
    offsets.push_back(builder->CreateSharedString(str));
  return builder->CreateVector(offsets);
}

std::vector<std::unique_ptr<url_pattern_index::UrlPatternIndexBuilder>>
CreateIndexBuilders(flatbuffers::FlatBufferBuilder* builder) {
  std::vector<std::unique_ptr<url_pattern_index::UrlPatternIndexBuilder>>
      result(flat::ActionIndex_count);
  for (size_t i = 0; i < flat::ActionIndex_count; ++i) {
    result[i] =
        std::make_unique<url_pattern_index::UrlPatternIndexBuilder>(builder);
  }
  return result;
}

FlatOffset<flat::UrlTransform> BuildTransformOffset(
    flatbuffers::FlatBufferBuilder* builder,
    const dnr_api::URLTransform& transform) {
  auto create_string_offset =
      [builder](const std::unique_ptr<std::string>& str) {
        if (!str)
          return FlatStringOffset();

        return builder->CreateSharedString(*str);
      };

  auto skip_separator_and_create_string_offset =
      [builder](const std::unique_ptr<std::string>& str, char separator) {
        if (!str)
          return FlatStringOffset();

        DCHECK(!str->empty());
        DCHECK_EQ(separator, str->at(0));

        return builder->CreateSharedString(str->c_str() + 1, str->length() - 1);
      };

  auto should_clear_component = [](const std::unique_ptr<std::string>& str) {
    return str && str->empty();
  };

  const FlatStringOffset kNullOffset;

  FlatStringOffset scheme = create_string_offset(transform.scheme);
  FlatStringOffset host = create_string_offset(transform.host);

  bool clear_port = should_clear_component(transform.port);
  FlatStringOffset port =
      clear_port ? kNullOffset : create_string_offset(transform.port);

  // Don't skip separator for path. Not all paths begin with '/'.
  bool clear_path = should_clear_component(transform.path);
  FlatStringOffset path =
      clear_path ? kNullOffset : create_string_offset(transform.path);

  bool clear_query = should_clear_component(transform.query);
  FlatStringOffset query =
      clear_query
          ? kNullOffset
          : skip_separator_and_create_string_offset(transform.query, '?');

  bool clear_fragment = should_clear_component(transform.fragment);
  FlatStringOffset fragment =
      clear_fragment
          ? kNullOffset
          : skip_separator_and_create_string_offset(transform.fragment, '#');

  FlatStringOffset username = create_string_offset(transform.username);
  FlatStringOffset password = create_string_offset(transform.password);

  FlatStringListOffset remove_query_params;
  const bool use_plus = true;
  if (transform.query_transform && transform.query_transform->remove_params) {
    // Escape, sort and remove duplicates.
    std::set<std::string> remove_params_escaped;
    for (const std::string& remove_param :
         *transform.query_transform->remove_params) {
      remove_params_escaped.insert(
          net::EscapeQueryParamValue(remove_param, use_plus));
    }

    remove_query_params =
        BuildVectorOfSharedStrings(builder, remove_params_escaped);
  }

  FlatVectorOffset<flat::QueryKeyValue> add_or_replace_params;
  if (transform.query_transform &&
      transform.query_transform->add_or_replace_params &&
      !transform.query_transform->add_or_replace_params->empty()) {
    std::vector<FlatOffset<flat::QueryKeyValue>> add_or_replace_queries;
    add_or_replace_queries.reserve(
        transform.query_transform->add_or_replace_params->size());
    for (const dnr_api::QueryKeyValue& query_pair :
         *transform.query_transform->add_or_replace_params) {
      FlatStringOffset key = builder->CreateSharedString(
          net::EscapeQueryParamValue(query_pair.key, use_plus));
      FlatStringOffset value = builder->CreateSharedString(
          net::EscapeQueryParamValue(query_pair.value, use_plus));
      add_or_replace_queries.push_back(
          flat::CreateQueryKeyValue(*builder, key, value));
    }
    add_or_replace_params = builder->CreateVector(add_or_replace_queries);
  }

  return flat::CreateUrlTransform(*builder, scheme, host, clear_port, port,
                                  clear_path, path, clear_query, query,
                                  remove_query_params, add_or_replace_params,
                                  clear_fragment, fragment, username, password);
}

}  // namespace

FlatRulesetIndexer::FlatRulesetIndexer()
    : index_builders_(CreateIndexBuilders(&builder_)) {}

FlatRulesetIndexer::~FlatRulesetIndexer() = default;

void FlatRulesetIndexer::AddUrlRule(const IndexedRule& indexed_rule) {
  DCHECK(!finished_);

  ++indexed_rules_count_;

  FlatStringListOffset domains_included_offset =
      BuildVectorOfSharedStrings(&builder_, indexed_rule.domains);
  FlatStringListOffset domains_excluded_offset =
      BuildVectorOfSharedStrings(&builder_, indexed_rule.excluded_domains);
  FlatStringOffset url_pattern_offset =
      builder_.CreateSharedString(indexed_rule.url_pattern);

  FlatOffset<flat_rule::UrlRule> offset = flat_rule::CreateUrlRule(
      builder_, indexed_rule.options, indexed_rule.element_types,
      indexed_rule.activation_types, indexed_rule.url_pattern_type,
      indexed_rule.anchor_left, indexed_rule.anchor_right,
      domains_included_offset, domains_excluded_offset, url_pattern_offset,
      indexed_rule.id, indexed_rule.priority);

  if (indexed_rule.url_pattern_type !=
      url_pattern_index::flat::UrlPatternType_REGEXP) {
    std::vector<UrlPatternIndexBuilder*> builders = GetBuilders(indexed_rule);
    DCHECK(!builders.empty());
    for (UrlPatternIndexBuilder* builder : builders)
      builder->IndexUrlRule(offset);
  } else {
    // A UrlPatternIndex is not built for regex rules. These are stored
    // separately.
    regex_rules_.push_back(
        flat::CreateRegexRule(builder_, offset, GetActionType(indexed_rule),
                              GetRemoveHeadersMask(indexed_rule)));
  }

  // Store additional metadata required for a redirect rule.
  if (indexed_rule.action_type == dnr_api::RULE_ACTION_TYPE_REDIRECT) {
    DCHECK(indexed_rule.redirect_url || indexed_rule.url_transform);

    if (indexed_rule.redirect_url) {
      DCHECK(!indexed_rule.redirect_url->empty());
      FlatStringOffset redirect_url_offset =
          builder_.CreateSharedString(*indexed_rule.redirect_url);
      metadata_.push_back(flat::CreateUrlRuleMetadata(
          builder_, indexed_rule.id, redirect_url_offset,
          FlatOffset<flat::UrlTransform>()));
    } else {
      FlatOffset<flat::UrlTransform> transform_offset =
          BuildTransformOffset(&builder_, *indexed_rule.url_transform);
      metadata_.push_back(flat::CreateUrlRuleMetadata(
          builder_, indexed_rule.id, FlatStringOffset() /* redirect_url */,
          transform_offset));
    }
  }
}

void FlatRulesetIndexer::Finish() {
  DCHECK(!finished_);
  finished_ = true;

  std::vector<url_pattern_index::UrlPatternIndexOffset> index_offsets;
  index_offsets.reserve(index_builders_.size());
  for (const auto& builder : index_builders_)
    index_offsets.push_back(builder->Finish());

  FlatVectorOffset<url_pattern_index::flat::UrlPatternIndex>
      index_vector_offset = builder_.CreateVector(index_offsets);

  // Store the extension metadata sorted by ID to support fast lookup through
  // binary search.
  FlatVectorOffset<flat::UrlRuleMetadata> extension_metadata_offset =
      builder_.CreateVectorOfSortedTables(&metadata_);

  FlatVectorOffset<flat::RegexRule> regex_rules_offset =
      builder_.CreateVector(regex_rules_);

  FlatOffset<flat::ExtensionIndexedRuleset> root_offset =
      flat::CreateExtensionIndexedRuleset(builder_, index_vector_offset,
                                          regex_rules_offset,
                                          extension_metadata_offset);
  flat::FinishExtensionIndexedRulesetBuffer(builder_, root_offset);
}

base::span<const uint8_t> FlatRulesetIndexer::GetData() {
  DCHECK(finished_);
  return base::make_span(builder_.GetBufferPointer(), builder_.GetSize());
}

flat::ActionType FlatRulesetIndexer::GetActionType(
    const IndexedRule& indexed_rule) const {
  switch (indexed_rule.action_type) {
    case dnr_api::RULE_ACTION_TYPE_BLOCK:
      return flat::ActionType_block;
    case dnr_api::RULE_ACTION_TYPE_ALLOW:
      return flat::ActionType_allow;
    case dnr_api::RULE_ACTION_TYPE_REDIRECT:
      return flat::ActionType_redirect;
    case dnr_api::RULE_ACTION_TYPE_REMOVEHEADERS:
      return flat::ActionType_remove_headers;
    case dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME:
      return flat::ActionType_upgrade_scheme;
    case dnr_api::RULE_ACTION_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return flat::ActionType_block;
}

uint8_t FlatRulesetIndexer::GetRemoveHeadersMask(
    const IndexedRule& indexed_rule) const {
  uint8_t mask = 0;
  for (const dnr_api::RemoveHeaderType type : indexed_rule.remove_headers_set) {
    switch (type) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        NOTREACHED();
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        mask |= flat::RemoveHeaderType_cookie;
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        mask |= flat::RemoveHeaderType_referer;
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        mask |= flat::RemoveHeaderType_set_cookie;
        break;
    }
  }
  return mask;
}

std::vector<FlatRulesetIndexer::UrlPatternIndexBuilder*>
FlatRulesetIndexer::GetBuilders(const IndexedRule& indexed_rule) {
  switch (indexed_rule.action_type) {
    case dnr_api::RULE_ACTION_TYPE_BLOCK:
      return {index_builders_[flat::ActionIndex_block].get()};
    case dnr_api::RULE_ACTION_TYPE_ALLOW:
      return {index_builders_[flat::ActionIndex_allow].get()};
    case dnr_api::RULE_ACTION_TYPE_REDIRECT:
      return {index_builders_[flat::ActionIndex_redirect].get()};
    case dnr_api::RULE_ACTION_TYPE_REMOVEHEADERS:
      return GetRemoveHeaderBuilders(indexed_rule.remove_headers_set);
    case dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME:
      return {index_builders_[flat::ActionIndex_upgrade_scheme].get()};
    case dnr_api::RULE_ACTION_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return {};
}

std::vector<FlatRulesetIndexer::UrlPatternIndexBuilder*>
FlatRulesetIndexer::GetRemoveHeaderBuilders(
    const std::set<dnr_api::RemoveHeaderType>& types) {
  // A single "removeHeaders" JSON/indexed rule does still correspond to a
  // single flatbuffer rule but can be stored in multiple indices.
  DCHECK(!types.empty());
  std::vector<UrlPatternIndexBuilder*> result;
  for (dnr_api::RemoveHeaderType type : types) {
    switch (type) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        NOTREACHED();
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        result.push_back(
            index_builders_[flat::ActionIndex_remove_cookie_header].get());
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        result.push_back(
            index_builders_[flat::ActionIndex_remove_referer_header].get());
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        result.push_back(
            index_builders_[flat::ActionIndex_remove_set_cookie_header].get());
        break;
    }
  }
  return result;
}

}  // namespace declarative_net_request
}  // namespace extensions

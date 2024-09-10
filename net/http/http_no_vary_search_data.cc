// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_no_vary_search_data.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/types/expected.h"
#include "net/base/features.h"
#include "net/base/url_search_params.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace net {

namespace {
// Tries to parse a list of ParameterizedItem as a list of strings.
// Returns std::nullopt if unsuccessful.
std::optional<std::vector<std::string>> ParseStringList(
    const std::vector<structured_headers::ParameterizedItem>& items) {
  std::vector<std::string> keys;
  keys.reserve(items.size());
  for (const auto& item : items) {
    if (!item.item.is_string()) {
      return std::nullopt;
    }
    keys.push_back(UnescapePercentEncodedUrl(item.item.GetString()));
  }
  return keys;
}

}  // namespace

HttpNoVarySearchData::HttpNoVarySearchData() = default;
HttpNoVarySearchData::HttpNoVarySearchData(const HttpNoVarySearchData&) =
    default;
HttpNoVarySearchData::HttpNoVarySearchData(HttpNoVarySearchData&&) = default;
HttpNoVarySearchData::~HttpNoVarySearchData() = default;
HttpNoVarySearchData& HttpNoVarySearchData::operator=(
    const HttpNoVarySearchData&) = default;
HttpNoVarySearchData& HttpNoVarySearchData::operator=(HttpNoVarySearchData&&) =
    default;

bool HttpNoVarySearchData::AreEquivalent(const GURL& a, const GURL& b) const {
  // Check urls without query and reference (fragment) for equality first.
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  if (a.ReplaceComponents(replacements) != b.ReplaceComponents(replacements)) {
    return false;
  }

  // If equal, look at how HttpNoVarySearchData argument affects
  // search params variance.
  UrlSearchParams a_search_params(a);
  UrlSearchParams b_search_params(b);
  // Ignore all the query search params that the URL is not varying on.
  if (vary_by_default()) {
    a_search_params.DeleteAllWithNames(no_vary_params());
    b_search_params.DeleteAllWithNames(no_vary_params());
  } else {
    a_search_params.DeleteAllExceptWithNames(vary_params());
    b_search_params.DeleteAllExceptWithNames(vary_params());
  }
  // Sort the params if the order of the search params in the query
  // is ignored.
  if (!vary_on_key_order()) {
    a_search_params.Sort();
    b_search_params.Sort();
  }
  // Check Search Params for equality
  // All search params, in order, need to have the same keys and the same
  // values.
  return a_search_params.params() == b_search_params.params();
}

// static
HttpNoVarySearchData HttpNoVarySearchData::CreateFromNoVaryParams(
    const std::vector<std::string>& no_vary_params,
    bool vary_on_key_order) {
  HttpNoVarySearchData no_vary_search;
  no_vary_search.vary_on_key_order_ = vary_on_key_order;
  no_vary_search.no_vary_params_.insert(no_vary_params.cbegin(),
                                        no_vary_params.cend());
  return no_vary_search;
}

// static
HttpNoVarySearchData HttpNoVarySearchData::CreateFromVaryParams(
    const std::vector<std::string>& vary_params,
    bool vary_on_key_order) {
  HttpNoVarySearchData no_vary_search;
  no_vary_search.vary_on_key_order_ = vary_on_key_order;
  no_vary_search.vary_by_default_ = false;
  no_vary_search.vary_params_.insert(vary_params.cbegin(), vary_params.cend());
  return no_vary_search;
}

// static
base::expected<HttpNoVarySearchData, HttpNoVarySearchData::ParseErrorEnum>
HttpNoVarySearchData::ParseFromHeaders(
    const HttpResponseHeaders& response_headers) {
  std::string normalized_header;
  if (!response_headers.GetNormalizedHeader("No-Vary-Search",
                                            &normalized_header)) {
    // This means there is no No-Vary-Search header. Return nullopt.
    return base::unexpected(ParseErrorEnum::kOk);
  }

  // The no-vary-search header is a dictionary type structured field.
  const auto dict = structured_headers::ParseDictionary(normalized_header);
  if (!dict.has_value()) {
    // We don't recognize anything else. So this is an authoring error.
    return base::unexpected(ParseErrorEnum::kNotDictionary);
  }

  return ParseNoVarySearchDictionary(dict.value());
}

const base::flat_set<std::string>& HttpNoVarySearchData::no_vary_params()
    const {
  return no_vary_params_;
}

const base::flat_set<std::string>& HttpNoVarySearchData::vary_params() const {
  return vary_params_;
}

bool HttpNoVarySearchData::vary_on_key_order() const {
  return vary_on_key_order_;
}
bool HttpNoVarySearchData::vary_by_default() const {
  return vary_by_default_;
}

// static
base::expected<HttpNoVarySearchData, HttpNoVarySearchData::ParseErrorEnum>
HttpNoVarySearchData::ParseNoVarySearchDictionary(
    const structured_headers::Dictionary& dict) {
  static constexpr const char* kKeyOrder = "key-order";
  static constexpr const char* kParams = "params";
  static constexpr const char* kExcept = "except";
  constexpr std::string_view kValidKeys[] = {kKeyOrder, kParams, kExcept};

  base::flat_set<std::string> no_vary_params;
  base::flat_set<std::string> vary_params;
  bool vary_on_key_order = true;
  bool vary_by_default = true;

  // If the dictionary contains unknown keys, maybe fail parsing.
  const bool has_unrecognized_keys = !base::ranges::all_of(
      dict,
      [&](const auto& pair) { return base::Contains(kValidKeys, pair.first); });

  UMA_HISTOGRAM_BOOLEAN("Net.HttpNoVarySearch.HasUnrecognizedKeys",
                        has_unrecognized_keys);
  if (has_unrecognized_keys &&
      !base::FeatureList::IsEnabled(
          features::kNoVarySearchIgnoreUnrecognizedKeys)) {
    return base::unexpected(ParseErrorEnum::kUnknownDictionaryKey);
  }

  // Populate `vary_on_key_order` based on the `key-order` key.
  if (dict.contains(kKeyOrder)) {
    const auto& key_order = dict.at(kKeyOrder);
    if (key_order.member_is_inner_list ||
        !key_order.member[0].item.is_boolean()) {
      return base::unexpected(ParseErrorEnum::kNonBooleanKeyOrder);
    }
    vary_on_key_order = !key_order.member[0].item.GetBoolean();
  }

  // Populate `no_vary_params` or `vary_by_default` based on the "params" key.
  if (dict.contains(kParams)) {
    const auto& params = dict.at(kParams);
    if (params.member_is_inner_list) {
      auto keys = ParseStringList(params.member);
      if (!keys.has_value()) {
        return base::unexpected(ParseErrorEnum::kParamsNotStringList);
      }
      no_vary_params = std::move(*keys);
    } else if (params.member[0].item.is_boolean()) {
      vary_by_default = !params.member[0].item.GetBoolean();
    } else {
      return base::unexpected(ParseErrorEnum::kParamsNotStringList);
    }
  }

  // Populate `vary_params` based on the "except" key.
  // This should be present only if "params" was true
  // (i.e., params don't vary by default).
  if (dict.contains(kExcept)) {
    const auto& excepted_params = dict.at(kExcept);
    if (vary_by_default) {
      return base::unexpected(ParseErrorEnum::kExceptWithoutTrueParams);
    }
    if (!excepted_params.member_is_inner_list) {
      return base::unexpected(ParseErrorEnum::kExceptNotStringList);
    }
    auto keys = ParseStringList(excepted_params.member);
    if (!keys.has_value()) {
      return base::unexpected(ParseErrorEnum::kExceptNotStringList);
    }
    vary_params = std::move(*keys);
  }

  // "params" controls both `vary_by_default` and `no_vary_params`. Check to
  // make sure that when "params" is a boolean, `no_vary_params` is empty.
  if (!vary_by_default)
    DCHECK(no_vary_params.empty());

  if (no_vary_params.empty() && vary_params.empty() && vary_by_default &&
      vary_on_key_order) {
    // If header is present but it's value is equivalent to only default values
    // then it is the same as if there were no header present.
    return base::unexpected(ParseErrorEnum::kDefaultValue);
  }

  HttpNoVarySearchData no_vary_search;
  no_vary_search.no_vary_params_ = std::move(no_vary_params);
  no_vary_search.vary_params_ = std::move(vary_params);
  no_vary_search.vary_on_key_order_ = vary_on_key_order;
  no_vary_search.vary_by_default_ = vary_by_default;

  return base::ok(no_vary_search);
}

}  // namespace net

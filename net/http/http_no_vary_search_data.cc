// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_no_vary_search_data.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "net/base/features.h"
#include "net/base/pickle.h"
#include "net/base/url_search_params.h"
#include "net/base/url_search_params_view.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace net {

namespace {

namespace keys {
constexpr std::string_view kKeyOrder = "key-order";
constexpr std::string_view kParams = "params";
constexpr std::string_view kExcept = "except";
}  // namespace keys

// Tries to parse a list of ParameterizedItem as a list of strings.
// Returns std::nullopt if unsuccessful.
std::optional<std::vector<std::string>> ParseStringList(
    const std::vector<structured_headers::ParameterizedItem>& items) {
  std::vector<std::string> keys;
  keys.reserve(items.size());
  for (const auto& item : items) {
    const std::string* string = item.item.GetIfString();
    if (!string) {
      return std::nullopt;
    }
    keys.push_back(UnescapePercentEncodedUrl(*string));
  }
  return keys;
}

// Extracts the "base URL" (everything before the query or fragment) from `url`.
// It relies on the fact that GURL canonicalizes http(s) URLs to not contain '?'
// or '#' before the start of the query. It's a lot faster than using
// GURL::Replacements to do the same thing, as no allocations or copies are
// needed.
std::string_view ExtractBaseUrl(const GURL& url) {
  const std::string_view view(url.possibly_invalid_spec());
  size_t end_of_base = view.find_first_of("?#");
  // This returns the whole of `view` if `end_of_base` is std::string::npos.
  return view.substr(0, end_of_base);
}

std::optional<bool>& GetHttpNoVarySearchDataUseNewAreEquivalentOverride() {
  static constinit std::optional<bool> override_value;
  return override_value;
}

bool IsHttpNoVarySearchDataUseNewAreEquivalentEnabled() {
  if (GetHttpNoVarySearchDataUseNewAreEquivalentOverride().has_value()) {
    return *GetHttpNoVarySearchDataUseNewAreEquivalentOverride();
  }

  static const bool kEnabled = base::FeatureList::IsEnabled(
      features::kHttpNoVarySearchDataUseNewAreEquivalent);

  return kEnabled;
}

}  // namespace

ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting::
    ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting(
        bool use_new_implementation) {
  GetHttpNoVarySearchDataUseNewAreEquivalentOverride() = use_new_implementation;
}

ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting::
    ~ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting() {
  GetHttpNoVarySearchDataUseNewAreEquivalentOverride() = std::nullopt;
}

HttpNoVarySearchData::HttpNoVarySearchData() = default;
HttpNoVarySearchData::HttpNoVarySearchData(const HttpNoVarySearchData&) =
    default;
HttpNoVarySearchData::HttpNoVarySearchData(HttpNoVarySearchData&&) = default;
HttpNoVarySearchData::~HttpNoVarySearchData() = default;
HttpNoVarySearchData& HttpNoVarySearchData::operator=(
    const HttpNoVarySearchData&) = default;
HttpNoVarySearchData& HttpNoVarySearchData::operator=(HttpNoVarySearchData&&) =
    default;

std::vector<std::string> HttpNoVarySearchData::GetAffectedParams() const {
  return std::vector<std::string>(affected_params_.begin(),
                                  affected_params_.end());
}

template <typename ParamsType>
void HttpNoVarySearchData::ApplyRulesToParams(ParamsType& params) const {
  if (vary_by_default_) {
    params.DeleteAllWithNames(affected_params_);
  } else {
    params.DeleteAllExceptWithNames(affected_params_);
  }
  if (!vary_on_key_order_) {
    params.Sort();
  }
}

bool HttpNoVarySearchData::AreEquivalent(const GURL& a, const GURL& b) const {
  CHECK(a.is_valid());
  CHECK(b.is_valid());
  if (IsHttpNoVarySearchDataUseNewAreEquivalentEnabled()) {
    return AreEquivalentNewImpl(a, b);
  }

  return AreEquivalentOldImpl(a, b);
}

std::string HttpNoVarySearchData::CanonicalizeQuery(const GURL& url) const {
  UrlSearchParamsView search_params(url);
  ApplyRulesToParams(search_params);

  return search_params.SerializeAsUtf8();
}

// static
HttpNoVarySearchData HttpNoVarySearchData::CreateFromNoVaryParams(
    const std::vector<std::string>& no_vary_params,
    bool vary_on_key_order) {
  // Check that this call creates a non-default configuration.
  CHECK(!vary_on_key_order || !no_vary_params.empty());

  HttpNoVarySearchData no_vary_search;
  no_vary_search.vary_on_key_order_ = vary_on_key_order;
  no_vary_search.affected_params_.insert(no_vary_params.cbegin(),
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
  no_vary_search.affected_params_.insert(vary_params.cbegin(),
                                         vary_params.cend());
  return no_vary_search;
}

// static
base::expected<HttpNoVarySearchData, HttpNoVarySearchData::ParseErrorEnum>
HttpNoVarySearchData::ParseFromHeaderValue(std::string_view value) {
  // The no-vary-search header is a dictionary type structured field.
  const auto dict = structured_headers::ParseDictionary(value);
  if (!dict.has_value()) {
    // We don't recognize anything else. So this is an authoring error.
    return base::unexpected(ParseErrorEnum::kNotDictionary);
  }

  return ParseNoVarySearchDictionary(dict.value());
}

// static
base::expected<HttpNoVarySearchData, HttpNoVarySearchData::ParseErrorEnum>
HttpNoVarySearchData::ParseFromHeaders(
    const HttpResponseHeaders& response_headers) {
  std::optional<std::string> normalized_header =
      response_headers.GetNormalizedHeader("No-Vary-Search");
  if (!normalized_header) {
    // This means there is no No-Vary-Search header.
    return base::unexpected(ParseErrorEnum::kOk);
  }

  return ParseFromHeaderValue(*normalized_header);
}

// static
bool HttpNoVarySearchData::HasBooleanParamsMember(
    std::string_view header_value) {
  const auto dict = structured_headers::ParseDictionary(header_value);
  if (!dict.has_value()) {
    return false;
  }
  auto it = dict->find(keys::kParams);
  if (it == dict->end()) {
    return false;
  }
  const auto item_and_params = it->second.GetWithParamsIfItem();
  if (!item_and_params.has_value()) {
    return false;
  }
  return item_and_params->first.is_boolean();
}

bool HttpNoVarySearchData::operator==(const HttpNoVarySearchData& rhs) const =
    default;
std::strong_ordering HttpNoVarySearchData::operator<=>(
    const HttpNoVarySearchData& rhs) const = default;

bool HttpNoVarySearchData::AreEquivalentOldImplForTesting(const GURL& a,
                                                          const GURL& b) const {
  return AreEquivalentOldImpl(a, b);
}

bool HttpNoVarySearchData::AreEquivalentNewImplForTesting(const GURL& a,
                                                          const GURL& b) const {
  return AreEquivalentNewImpl(a, b);
}

// static
base::expected<HttpNoVarySearchData, HttpNoVarySearchData::ParseErrorEnum>
HttpNoVarySearchData::ParseNoVarySearchDictionary(
    const structured_headers::Dictionary& dict) {
  constexpr std::string_view kValidKeys[] = {keys::kKeyOrder, keys::kParams,
                                             keys::kExcept};

  base::flat_set<std::string> affected_params;
  bool vary_on_key_order = true;
  bool vary_by_default = true;

  // If the dictionary contains unknown keys, maybe fail parsing.
  const bool has_unrecognized_keys =
      !std::ranges::all_of(dict, [&](const auto& pair) {
        return std::ranges::contains(kValidKeys, pair.first);
      });

  UMA_HISTOGRAM_BOOLEAN("Net.HttpNoVarySearch.HasUnrecognizedKeys",
                        has_unrecognized_keys);
  if (has_unrecognized_keys &&
      !base::FeatureList::IsEnabled(
          features::kNoVarySearchIgnoreUnrecognizedKeys)) {
    return base::unexpected(ParseErrorEnum::kUnknownDictionaryKey);
  }

  // Populate `vary_on_key_order` based on the `key-order` key.
  if (auto keyorder_it = dict.find(keys::kKeyOrder);
      keyorder_it != dict.end()) {
    const auto item_with_params = keyorder_it->second.GetWithParamsIfItem();
    const bool* boolean = item_with_params.has_value()
                              ? item_with_params->first.GetIfBoolean()
                              : nullptr;
    if (!boolean) {
      return base::unexpected(ParseErrorEnum::kNonBooleanKeyOrder);
    }
    vary_on_key_order = !*boolean;
  }

  // Populate `affected_params` or `vary_by_default` based on the "params" key.
  if (auto params_it = dict.find(keys::kParams); params_it != dict.end()) {
    const auto& params = params_it->second;
    if (const auto inner_list_and_params = params.GetWithParamsIfInnerList()) {
      auto keys = ParseStringList(inner_list_and_params->first);
      if (!keys.has_value()) {
        return base::unexpected(ParseErrorEnum::kParamsNotStringList);
      }
      affected_params = std::move(*keys);
    } else if (const auto item_and_params = params.GetWithParamsIfItem()) {
      if (const bool* boolean = item_and_params->first.GetIfBoolean()) {
        vary_by_default = !*boolean;
      } else {
        return base::unexpected(ParseErrorEnum::kParamsNotStringList);
      }
    } else {
      return base::unexpected(ParseErrorEnum::kParamsNotStringList);
    }
  }

  // Populate `affected_params` based on the "except" key.
  // This should be present only if "params" was true
  // (i.e., params don't vary by default).
  if (auto except_it = dict.find(keys::kExcept); except_it != dict.end()) {
    const auto& excepted_params = except_it->second;
    if (vary_by_default) {
      return base::unexpected(ParseErrorEnum::kExceptWithoutTrueParams);
    }
    const auto inner_list_with_params =
        excepted_params.GetWithParamsIfInnerList();
    if (!inner_list_with_params.has_value()) {
      return base::unexpected(ParseErrorEnum::kExceptNotStringList);
    }
    auto keys = ParseStringList(inner_list_with_params->first);
    if (!keys.has_value()) {
      return base::unexpected(ParseErrorEnum::kExceptNotStringList);
    }
    affected_params = std::move(*keys);
  }

  if (affected_params.empty() && vary_by_default && vary_on_key_order) {
    // If header is present but it's value is equivalent to only default values
    // then it is the same as if there were no header present.
    return base::unexpected(ParseErrorEnum::kDefaultValue);
  }

  HttpNoVarySearchData no_vary_search;
  no_vary_search.affected_params_ = std::move(affected_params);
  no_vary_search.vary_on_key_order_ = vary_on_key_order;
  no_vary_search.vary_by_default_ = vary_by_default;

  return no_vary_search;
}

bool HttpNoVarySearchData::AreEquivalentOldImpl(const GURL& a,
                                                const GURL& b) const {
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
  ApplyRulesToParams(a_search_params);
  ApplyRulesToParams(b_search_params);

  // Check Search Params for equality
  // All search params, in order, need to have the same keys and the same
  // values.
  return a_search_params.params() == b_search_params.params();
}

bool HttpNoVarySearchData::AreEquivalentNewImpl(const GURL& a,
                                                const GURL& b) const {
  if (ExtractBaseUrl(a) != ExtractBaseUrl(b)) {
    return false;
  }

  // If equal, look at how HttpNoVarySearchData argument affects
  // search params variance.
  UrlSearchParamsView a_search_params(a);
  UrlSearchParamsView b_search_params(b);
  ApplyRulesToParams(a_search_params);
  ApplyRulesToParams(b_search_params);

  return a_search_params == b_search_params;
}

// LINT.IfChange(Serialization)
void PickleTraits<HttpNoVarySearchData>::Serialize(
    base::Pickle& pickle,
    const HttpNoVarySearchData& value) {
  WriteToPickle(pickle, HttpNoVarySearchData::kMagicNumber,
                value.affected_params_, value.vary_on_key_order_,
                value.vary_by_default_);
}

std::optional<HttpNoVarySearchData>
PickleTraits<HttpNoVarySearchData>::Deserialize(base::PickleIterator& iter) {
  HttpNoVarySearchData result;
  uint32_t magic_number = 0u;
  if (!ReadPickleInto(iter, magic_number, result.affected_params_,
                      result.vary_on_key_order_, result.vary_by_default_)) {
    return std::nullopt;
  }

  if (magic_number != HttpNoVarySearchData::kMagicNumber) {
    return std::nullopt;
  }

  if (result.vary_by_default_ && result.vary_on_key_order_ &&
      result.affected_params_.empty()) {
    // This is the default configuration in the absence of a No-Vary-Search
    // header, and should never be stored in a HttpNoVarySearchData object.
    return std::nullopt;
  }

  return result;
}

size_t PickleTraits<HttpNoVarySearchData>::PickleSize(
    const HttpNoVarySearchData& value) {
  return EstimatePickleSize(HttpNoVarySearchData::kMagicNumber,
                            value.affected_params_, value.vary_on_key_order_,
                            value.vary_by_default_);
}
// LINT.ThenChange(//net/http/http_no_vary_search_data.h:MagicNumber)

std::ostream& operator<<(std::ostream& ostream,
                         const HttpNoVarySearchData& no_vary_search_data) {
  no_vary_search_data.DescribeForLog(ostream);
  return ostream;
}

void HttpNoVarySearchData::DescribeForLog(std::ostream& ostream) const {
  ostream << std::boolalpha;
  ostream << "HttpNoVarySearchData{";
  ostream << "vary_on_key_order: " << vary_on_key_order_ << ", ";
  ostream << "vary_by_default: " << vary_by_default_ << ", ";
  ostream << R"(affected_params: [")"
          << base::JoinString(affected_params_, R"(", ")") << R"("])";
  ostream << "}";
}

std::optional<std::string> HttpNoVarySearchData::SerializeToString() const {
  std::vector<structured_headers::DictionaryMember> members;

  if (!vary_on_key_order_) {
    members.emplace_back(keys::kKeyOrder,
                         structured_headers::ParameterizedMember(
                             structured_headers::Item(true), {}));
  }

  if (vary_by_default_) {
    if (!affected_params_.empty()) {
      std::vector<structured_headers::ParameterizedItem> param_items;
      for (const auto& param : affected_params_) {
        param_items.push_back(structured_headers::ParameterizedItem(
            structured_headers::Item(
                base::EscapeQueryParamValue(param, /*use_plus=*/true),
                structured_headers::Item::kStringType),
            {}));
      }
      members.emplace_back(
          keys::kParams,
          structured_headers::ParameterizedMember(std::move(param_items), {}));
    }
  } else {
    members.emplace_back(keys::kParams,
                         structured_headers::ParameterizedMember(
                             structured_headers::Item(true), {}));

    if (!affected_params_.empty()) {
      std::vector<structured_headers::ParameterizedItem> except_items;
      for (const auto& param : affected_params_) {
        except_items.push_back(structured_headers::ParameterizedItem(
            structured_headers::Item(
                base::EscapeQueryParamValue(param, /*use_plus=*/true),
                structured_headers::Item::kStringType),
            {}));
      }
      members.emplace_back(
          keys::kExcept,
          structured_headers::ParameterizedMember(std::move(except_items), {}));
    }
  }

  return structured_headers::SerializeDictionary(
      structured_headers::Dictionary(std::move(members)));
}

}  // namespace net

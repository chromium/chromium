// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_no_vary_search_data.h"

#include <optional>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
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

template <typename ParamsType>
void ApplyNoVarySearchRulesToParams(const HttpNoVarySearchData& rules,
                                    ParamsType& params) {
  // Ignore all the query search params that the URL is not varying on.
  if (rules.vary_by_default()) {
    params.DeleteAllWithNames(rules.no_vary_params());
  } else {
    params.DeleteAllExceptWithNames(rules.vary_params());
  }
  // Sort the params if the order of the search params in the query
  // is ignored.
  if (!rules.vary_on_key_order()) {
    params.Sort();
  }
}

template <typename ParamsType>
void ApplyNoVarySearchRulesToBothParams(const HttpNoVarySearchData& rules,
                                        ParamsType& params_a,
                                        ParamsType& params_b) {
  ApplyNoVarySearchRulesToParams(rules, params_a);
  ApplyNoVarySearchRulesToParams(rules, params_b);
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

std::optional<bool>& GetHttpNoVarySearchDataAreEquivalentCheckResultOverride() {
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

bool IsHttpNoVarySearchDataAreEquivalentCheckResultEnabled() {
  if (GetHttpNoVarySearchDataAreEquivalentCheckResultOverride().has_value()) {
    return *GetHttpNoVarySearchDataAreEquivalentCheckResultOverride();
  }

  static const bool kEnabled =
      features::kHttpNoVarySearchDataAreEquivalentCheckResult.Get();

  return kEnabled;
}

}  // namespace

ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting::
    ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting(
        bool use_new_implementation,
        bool check_result) {
  GetHttpNoVarySearchDataUseNewAreEquivalentOverride() = use_new_implementation;
  GetHttpNoVarySearchDataAreEquivalentCheckResultOverride() = check_result;
}

ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting::
    ~ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting() {
  GetHttpNoVarySearchDataUseNewAreEquivalentOverride() = std::nullopt;
  GetHttpNoVarySearchDataAreEquivalentCheckResultOverride() = std::nullopt;
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

bool HttpNoVarySearchData::AreEquivalent(const GURL& a, const GURL& b) const {
  CHECK(a.is_valid());
  CHECK(b.is_valid());
  if (IsHttpNoVarySearchDataUseNewAreEquivalentEnabled()) {
    const bool result = AreEquivalentNewImpl(a, b);
    if (IsHttpNoVarySearchDataAreEquivalentCheckResultEnabled()) {
      const bool old_result = AreEquivalentOldImpl(a, b);
      if (old_result != result) {
        SCOPED_CRASH_KEY_BOOL("NoVarySearch", "old_result", old_result);
        SCOPED_CRASH_KEY_BOOL("NoVarySearch", "new_result", result);
        // The full URLs are necessary to debug issues if they occur. This
        // debugging code will be removed as quickly as possible once the old
        // and new implementations are proved to have identical behavior.
        SCOPED_CRASH_KEY_STRING1024("NoVarySearch", "url_a",
                                    a.possibly_invalid_spec());
        SCOPED_CRASH_KEY_STRING1024("NoVarySearch", "url_b",
                                    b.possibly_invalid_spec());
        SCOPED_CRASH_KEY_STRING256("NoVarySearch", "nv_params",
                                   base::JoinString(no_vary_params_, ","));
        SCOPED_CRASH_KEY_STRING256("NoVarySearch", "v_params",
                                   base::JoinString(vary_params_, ","));
        SCOPED_CRASH_KEY_BOOL("NoVarySearch", "key_order", vary_on_key_order_);
        SCOPED_CRASH_KEY_BOOL("NoVarySearch", "by_default", vary_by_default_);
        base::debug::DumpWithoutCrashing();
      }
    }
    return result;
  }

  return AreEquivalentOldImpl(a, b);
}

std::string HttpNoVarySearchData::CanonicalizeQuery(const GURL& url) const {
  UrlSearchParamsView search_params(url);
  ApplyNoVarySearchRulesToParams(*this, search_params);

  return search_params.SerializeAsUtf8();
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
  std::optional<std::string> normalized_header =
      response_headers.GetNormalizedHeader("No-Vary-Search");
  if (!normalized_header) {
    // This means there is no No-Vary-Search header.
    return base::unexpected(ParseErrorEnum::kOk);
  }

  // The no-vary-search header is a dictionary type structured field.
  const auto dict = structured_headers::ParseDictionary(*normalized_header);
  if (!dict.has_value()) {
    // We don't recognize anything else. So this is an authoring error.
    return base::unexpected(ParseErrorEnum::kNotDictionary);
  }

  return ParseNoVarySearchDictionary(dict.value());
}

bool HttpNoVarySearchData::operator==(const HttpNoVarySearchData& rhs) const =
    default;
std::strong_ordering HttpNoVarySearchData::operator<=>(
    const HttpNoVarySearchData& rhs) const = default;

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
  static constexpr const char* kKeyOrder = "key-order";
  static constexpr const char* kParams = "params";
  static constexpr const char* kExcept = "except";
  constexpr std::string_view kValidKeys[] = {kKeyOrder, kParams, kExcept};

  base::flat_set<std::string> no_vary_params;
  base::flat_set<std::string> vary_params;
  bool vary_on_key_order = true;
  bool vary_by_default = true;

  // If the dictionary contains unknown keys, maybe fail parsing.
  const bool has_unrecognized_keys = !std::ranges::all_of(
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
  ApplyNoVarySearchRulesToBothParams(*this, a_search_params, b_search_params);

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
  ApplyNoVarySearchRulesToBothParams(*this, a_search_params, b_search_params);

  return a_search_params == b_search_params;
}

// LINT.IfChange(Serialization)
void PickleTraits<HttpNoVarySearchData>::Serialize(
    base::Pickle& pickle,
    const HttpNoVarySearchData& value) {
  WriteToPickle(pickle, HttpNoVarySearchData::kMagicNumber,
                value.no_vary_params_, value.vary_params_,
                value.vary_on_key_order_, value.vary_by_default_);
}

std::optional<HttpNoVarySearchData>
PickleTraits<HttpNoVarySearchData>::Deserialize(base::PickleIterator& iter) {
  HttpNoVarySearchData result;
  uint32_t magic_number = 0u;
  if (!ReadPickleInto(iter, magic_number, result.no_vary_params_,
                      result.vary_params_, result.vary_on_key_order_,
                      result.vary_by_default_)) {
    return std::nullopt;
  }

  if (magic_number != HttpNoVarySearchData::kMagicNumber) {
    return std::nullopt;
  }

  if (result.vary_by_default_) {
    if (result.vary_on_key_order_ && result.vary_params_.empty() &&
        result.no_vary_params_.empty()) {
      // This is the default configuration in the absence of a No-Vary-Search
      // header, and should never be stored in a HttpNoVarySearchData object.
      return std::nullopt;
    }
    if (!result.vary_params_.empty()) {
      return std::nullopt;
    }
  } else {
    if (!result.no_vary_params_.empty()) {
      return std::nullopt;
    }
  }

  return result;
}

size_t PickleTraits<HttpNoVarySearchData>::PickleSize(
    const HttpNoVarySearchData& value) {
  return EstimatePickleSize(HttpNoVarySearchData::kMagicNumber,
                            value.no_vary_params_, value.vary_params_,
                            value.vary_on_key_order_, value.vary_by_default_);
}
// LINT.ThenChange(//net/http/http_no_vary_search_data.h:MagicNumber)

}  // namespace net

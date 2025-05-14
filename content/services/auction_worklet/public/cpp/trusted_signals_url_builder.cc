// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/trusted_signals_url_builder.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "url/gurl.h"

namespace auction_worklet {

namespace {

// Computes a piece of a query param containing a comma-delimited list.
//
// If `keys` is empty, returns an empty string.
// If `is_first_of_query_param` is true, returns
//    "&<name>=<values in comma-delimited list>"
// If it's false, returns ",<values in a comma-delimited list>"
//
// The list items are extracted from `keys` with help of `proj`.
// `name` will not be escaped, but the extracted values will be will be, unless
// `escape` is false.
template <typename Container, typename Proj = std::identity>
std::string CreateQueryParamPiece(std::string_view name,
                                  bool is_first_of_query_param,
                                  const Container& keys,
                                  Proj proj = {},
                                  bool escape = true) {
  std::string computed_piece;
  for (const auto& key : keys) {
    if (is_first_of_query_param) {
      computed_piece = base::StringPrintf("&%s=", name);
      is_first_of_query_param = false;
    } else {
      computed_piece.append(",");
    }
    if (escape) {
      computed_piece.append(
          base::EscapeQueryParamValue(proj(key), /*use_plus=*/true));
    } else {
      computed_piece.append(proj(key));
    }
  }
  return computed_piece;
}

GURL SetQueryParam(const GURL& base_url, const std::string& new_query_params) {
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query_params);
  return base_url.ReplaceComponents(replacements);
}

// If creative scanning metadata is not being set, it's important that the
// AdDescriptors used have everything but the URL discarded, so we don't
// needlessly duplicate creative URLs, which might cause compatibility
// problems.
bool ContainsNonUrlInfo(const std::set<CreativeInfo>& creative_info_set) {
  for (const auto& creative_info : creative_info_set) {
    if (creative_info.ad_descriptor.size.has_value() ||
        !creative_info.creative_scanning_metadata.empty() ||
        creative_info.interest_group_owner.has_value() ||
        !creative_info.buyer_and_seller_reporting_id.empty()) {
      return true;
    }
  }
  return false;
}

template <typename T>
std::set<T> AddAndReturnNew(const std::set<T>& to_add,
                            std::set<T>& accumulator) {
  std::set<T> new_vals;
  for (const T& key : to_add) {
    auto inserted = accumulator.insert(key);
    if (inserted.second) {
      new_vals.insert(key);
    }
  }
  return new_vals;
}

template <typename T>
std::set<T> AddAndReturnNew(const T& to_add, std::set<T>& accumulator) {
  std::set<T> new_vals;
  auto inserted = accumulator.insert(to_add);
  if (inserted.second) {
    new_vals.insert(to_add);
  }
  return new_vals;
}

}  // namespace

TrustedSignalsUrlBuilder::~TrustedSignalsUrlBuilder() = default;

GURL TrustedSignalsUrlBuilder::ComposeURL() {
  std::array<std::vector<std::string>, static_cast<int>(UrlField::kNumValues)>
      all_fragments;
  size_t total_len = 0;
  for (auto& fragment_val : main_fragments_) {
    total_len += fragment_val.text.length();
    all_fragments[static_cast<int>(fragment_val.field)].push_back(
        std::move(fragment_val.text));
  }
  main_fragments_.clear();

  for (auto& fragment_val : aux_fragments_) {
    total_len += fragment_val.text.length();
    all_fragments[static_cast<int>(fragment_val.field)].push_back(
        std::move(fragment_val.text));
  }
  aux_fragments_.clear();

  CHECK(!all_fragments.empty());

  std::string out;
  out.reserve(total_len);
  for (const auto& fragments_for_field : all_fragments) {
    for (const std::string& fragment : fragments_for_field) {
      out += fragment;
    }
  }
  return GURL(out);
}

void TrustedSignalsUrlBuilder::Reset() {
  main_fragments_.clear();
  aux_fragments_.clear();
  length_thus_far_ = 0;
  interest_group_names_.clear();
  bidding_signals_keys_.clear();
  ads_.clear();
  ad_components_.clear();
  length_limit_ = std::numeric_limits<size_t>::max();
  added_first_request_ = false;
}

std::set<std::string> TrustedSignalsUrlBuilder::TakeInterestGroupNames() {
  // We should never try to build a bidding signals URL without
  // any interest group names.
  DCHECK(interest_group_names_.size());
  return std::move(interest_group_names_);
}

std::set<std::string> TrustedSignalsUrlBuilder::TakeBiddingSignalsKeys() {
  return std::move(bidding_signals_keys_);
}

std::set<CreativeInfo> TrustedSignalsUrlBuilder::TakeAds() {
  // We should never try to build a scoring signals URL without any ads.
  DCHECK(ads_.size());
  return std::move(ads_);
}

std::set<CreativeInfo> TrustedSignalsUrlBuilder::TakeAdComponents() {
  return std::move(ad_components_);
}

TrustedSignalsUrlBuilder::TrustedSignalsUrlBuilder(
    std::string hostname,
    GURL trusted_signals_url,
    std::optional<uint16_t> experiment_group_id)
    : hostname_(std::move(hostname)),
      trusted_signals_url_(std::move(trusted_signals_url)),
      experiment_group_id_(experiment_group_id) {}

bool TrustedSignalsUrlBuilder::CommitOrRollback(
    size_t initial_num_main_fragments,
    size_t initial_num_aux_fragments,
    size_t max_trusted_signals_url_length) {
  size_t attempted_len = length_thus_far_;
  for (size_t i = initial_num_main_fragments; i < main_fragments_.size(); ++i) {
    attempted_len += main_fragments_[i].text.length();
  }
  for (size_t i = initial_num_aux_fragments; i < aux_fragments_.size(); ++i) {
    attempted_len += aux_fragments_[i].text.length();
  }

  size_t len_target = std::min(length_limit_, max_trusted_signals_url_length);
  if (!added_first_request_ || attempted_len <= len_target) {
    length_limit_ = len_target;
    added_first_request_ = true;
    length_thus_far_ = attempted_len;
    return true;
  } else {
    // Subclass is responsible for commit/rollback of changes to bidder/scorer
    // specific fields.
    main_fragments_.resize(initial_num_main_fragments);
    aux_fragments_.resize(initial_num_aux_fragments);
    return false;
  }
}

TrustedBiddingSignalsUrlBuilder::TrustedBiddingSignalsUrlBuilder(
    std::string hostname,
    GURL trusted_signals_url,
    std::optional<uint16_t> experiment_group_id,
    std::string trusted_bidding_signals_slot_size_param)
    : TrustedSignalsUrlBuilder(std::move(hostname),
                               std::move(trusted_signals_url),
                               experiment_group_id),
      trusted_bidding_signals_slot_size_param_(
          std::move(trusted_bidding_signals_slot_size_param)) {}

bool TrustedBiddingSignalsUrlBuilder::TryToAddRequest(
    const std::string& interest_group_name,
    const std::set<std::string>& bidder_keys,
    size_t max_trusted_signals_url_length) {
  // Figure out which fields are new.
  std::set<std::string> new_interest_group_names =
      AddAndReturnNew(interest_group_name, interest_group_names_);
  std::set<std::string> new_keys =
      AddAndReturnNew(bidder_keys, bidding_signals_keys_);

  size_t initial_num_main_fragments = main_fragments_.size();
  size_t initial_num_aux_fragments = aux_fragments_.size();
  BuildTrustedBiddingSignalsURL(new_interest_group_names, new_keys);

  if (!CommitOrRollback(initial_num_main_fragments, initial_num_aux_fragments,
                        max_trusted_signals_url_length)) {
    for (const auto& key : new_interest_group_names) {
      interest_group_names_.erase(key);
    }
    for (const auto& key : new_keys) {
      bidding_signals_keys_.erase(key);
    }
    return false;
  }
  return true;
}

// static
void TrustedBiddingSignalsUrlBuilder::BuildTrustedBiddingSignalsURL(
    const std::set<std::string>& new_interest_group_names,
    const std::set<std::string>& new_bidding_signals_keys) {
  bool creating_new_url = main_fragments_.empty();
  if (creating_new_url) {
    std::string host_name_param =
        base::StrCat({"hostname=", base::EscapeQueryParamValue(
                                       hostname_, /*use_plus=*/true)});
    main_fragments_.emplace_back(
        UrlField::kBase,
        SetQueryParam(trusted_signals_url_, host_name_param).spec());
  }

  main_fragments_.emplace_back(
      UrlField::kInterestGroupNames,
      CreateQueryParamPiece("interestGroupNames", creating_new_url,
                            new_interest_group_names));

  if (!new_bidding_signals_keys.empty()) {
    bool key_was_empty = aux_fragments_.empty();
    aux_fragments_.emplace_back(
        UrlField::kKeys,
        CreateQueryParamPiece("keys", key_was_empty, new_bidding_signals_keys));
  }

  if (creating_new_url) {
    if (experiment_group_id_.has_value()) {
      main_fragments_.emplace_back(
          UrlField::kExperimentGroupId,
          base::StrCat({"&experimentGroupId=",
                        base::NumberToString(experiment_group_id_.value())}));
    }
    if (!trusted_bidding_signals_slot_size_param_.empty()) {
      main_fragments_.emplace_back(
          UrlField::kSlotSizeParam,
          base::StrCat({"&", trusted_bidding_signals_slot_size_param_}));
    }
  }
}

TrustedScoringSignalsUrlBuilder::TrustedScoringSignalsUrlBuilder(
    std::string hostname,
    GURL trusted_signals_url,
    std::optional<uint16_t> experiment_group_id,
    bool send_creative_scanning_metadata)
    : TrustedSignalsUrlBuilder(std::move(hostname),
                               std::move(trusted_signals_url),
                               experiment_group_id),
      send_creative_scanning_metadata_(send_creative_scanning_metadata) {}

bool TrustedScoringSignalsUrlBuilder::TryToAddRequest(
    const CreativeInfo& ad,
    const std::set<CreativeInfo>& ad_components,
    size_t max_trusted_signals_url_length) {
  std::set<CreativeInfo> new_ads = AddAndReturnNew(ad, ads_);
  std::set<CreativeInfo> new_ad_components =
      AddAndReturnNew(ad_components, ad_components_);

  size_t initial_num_main_fragments = main_fragments_.size();
  size_t initial_num_aux_fragments = aux_fragments_.size();

  BuildTrustedScoringSignalsURL(new_ads, new_ad_components);

  if (!CommitOrRollback(initial_num_main_fragments, initial_num_aux_fragments,
                        max_trusted_signals_url_length)) {
    for (const auto& key : new_ads) {
      ads_.erase(key);
    }
    for (const auto& key : new_ad_components) {
      ad_components_.erase(key);
    }
    return false;
  }
  return true;
}

void TrustedScoringSignalsUrlBuilder::BuildTrustedScoringSignalsURL(
    const std::set<CreativeInfo>& new_ads,
    const std::set<CreativeInfo>& new_component_ads) {
  // When we start the URL; and also when we start query params for ads, as
  // the first call must have at least one ad.
  bool creating_new_url = main_fragments_.empty();
  if (creating_new_url) {
    std::string host_name_param =
        base::StrCat({"hostname=", base::EscapeQueryParamValue(
                                       hostname_, /*use_plus=*/true)});
    main_fragments_.emplace_back(
        UrlField::kBase,
        SetQueryParam(trusted_signals_url_, host_name_param).spec());
  }

  auto extract_render_url =
      [](const CreativeInfo& creative_info) -> const std::string& {
    return creative_info.ad_descriptor.url.spec();
  };

  // TODO(crbug.com/40264073): Find a way to rename renderUrls to renderURLs.
  main_fragments_.emplace_back(
      UrlField::kRenderUrls,
      CreateQueryParamPiece("renderUrls", creating_new_url, new_ads,
                            extract_render_url));

  bool components_was_empty = aux_fragments_.empty();
  if (!new_component_ads.empty()) {
    aux_fragments_.emplace_back(
        UrlField::kAdComponentRenderUrls,
        CreateQueryParamPiece("adComponentRenderUrls", components_was_empty,
                              new_component_ads, extract_render_url));
  }

  if (creating_new_url) {
    if (experiment_group_id_.has_value()) {
      main_fragments_.emplace_back(
          UrlField::kExperimentGroupId,
          base::StrCat({"&experimentGroupId=",
                        base::NumberToString(experiment_group_id_.value())}));
    }
  }

  if (send_creative_scanning_metadata_) {
    auto extract_creative_scan_metadata =
        [](const CreativeInfo& creative_info) -> const std::string& {
      return creative_info.creative_scanning_metadata;
    };
    auto extract_size = [](const CreativeInfo& creative_info) -> std::string {
      // When no size is provided we return "," and not an empty string so that
      // splitting size params by , will always produce 2 entries for each
      // creative.
      return creative_info.ad_descriptor.size.has_value()
                 ? blink::ConvertAdSizeToString(
                       *creative_info.ad_descriptor.size)
                 : std::string(",");
    };
    auto extract_buyer = [](const CreativeInfo& creative_info) -> std::string {
      DCHECK(creative_info.interest_group_owner.has_value());
      return creative_info.interest_group_owner->Serialize();
    };

    auto extract_buyer_and_seller_reporting_id =
        [](const CreativeInfo& creative_info) -> const std::string& {
      return creative_info.buyer_and_seller_reporting_id;
    };

    main_fragments_.emplace_back(
        UrlField::kAdCreativeScanningMetadata,
        CreateQueryParamPiece("adCreativeScanningMetadata", creating_new_url,
                              new_ads, extract_creative_scan_metadata));
    if (!new_component_ads.empty()) {
      aux_fragments_.emplace_back(
          UrlField::kAdComponentCreativeScanningMetadata,
          CreateQueryParamPiece("adComponentCreativeScanningMetadata",
                                components_was_empty, new_component_ads,
                                extract_creative_scan_metadata));
    }

    main_fragments_.emplace_back(
        UrlField::kAdSizes, CreateQueryParamPiece("adSizes", creating_new_url,
                                                  new_ads, extract_size,
                                                  /*escape=*/false));

    if (!new_component_ads.empty()) {
      aux_fragments_.emplace_back(
          UrlField::kAdComponentSizes,
          CreateQueryParamPiece("adComponentSizes", components_was_empty,
                                new_component_ads, extract_size,
                                /*escape=*/false));
    }

    main_fragments_.emplace_back(
        UrlField::kAdBuyer, CreateQueryParamPiece("adBuyer", creating_new_url,
                                                  new_ads, extract_buyer));
    if (!new_component_ads.empty()) {
      aux_fragments_.emplace_back(
          UrlField::kAdComponentBuyer,
          CreateQueryParamPiece("adComponentBuyer", components_was_empty,
                                new_component_ads, extract_buyer));
    }
    main_fragments_.emplace_back(
        UrlField::kBuyerAndSellerReportingIds,
        CreateQueryParamPiece("adBuyerAndSellerReportingIds", creating_new_url,
                              new_ads, extract_buyer_and_seller_reporting_id));
  } else {
    DCHECK(!ContainsNonUrlInfo(new_ads));
    DCHECK(!ContainsNonUrlInfo(new_component_ads));
  }
}

}  // namespace auction_worklet

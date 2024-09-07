// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_update_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/features.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

// 1 MB update size limit. We are potentially fetching many interest group
// updates, so don't let this get too large.
constexpr size_t kMaxUpdateSize = 1 * 1024 * 1024;

// The maximum amount of time that the update process can run before it gets
// cancelled for taking too long.
constexpr base::TimeDelta kMaxUpdateRoundDuration = base::Minutes(10);

// The maximum number of groups that can be updated at the same time.
constexpr int kMaxParallelUpdates = 5;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// For group update batch resize operations count.
enum class GroupUpdateResizeOperation {
  // Batch size is less then the limit and not resized
  kNoResize = 0,
  // Batch is resized to the limit with same head and tail origins
  kEqualEndsResize = 1,
  // Batch is resized to the limit with different origins across the
  // limit boundary
  kNonSplitResize = 2,
  // Batch is resized to a smaller size than the limit due to having same
  // origins across the limit boundary
  kSplitResize = 3,
  kMaxValue = kSplitResize,
};

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("interest_group_update_fetcher", R"(
        semantics {
          sender: "Interest group periodic update fetcher"
          description:
            "Fetches periodic updates of Protected Audiences interest groups "
            "previously joined by navigator.joinAdInterestGroup(). Protected "
            "Audiences allow sites to store persistent interest groups that "
            "are only accessible to special on-device ad auction worklets run "
            "via navigator.runAdAuction(). JavaScript running in the context "
            "of a frame cannot read interest groups, but it can request that "
            "all interest groups owned by the current frame's origin be "
            "updated by fetching JSON from the registered update URL for each "
            "interest group."
            "See https://github.com/WICG/turtledove/blob/main/FLEDGE.md and "
            "https://developer.chrome.com/docs/privacy-sandbox/fledge/"
          trigger:
            "Fetched upon a navigator.updateAdInterestGroups() call. Also "
            "triggered upon navigator.runAdAuction() completion for interest "
            "groups that participated in the auction."
          data: "URL registered for updating this interest group."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable this via Settings > Privacy and Security > Ads "
            "privacy > Site-suggested ads."
          chrome_policy {
            PrivacySandboxSiteEnabledAdsEnabled {
              PrivacySandboxSiteEnabledAdsEnabled: false
            }
          }
        })");

// TODO(crbug.com/40172488): Report errors to devtools for the TryToCopy*().
// functions.

// Name and owner are optional in `dict` (parsed server JSON response), but
// must match `name` and `owner`, respectively, if either is specified. Returns
// true if the check passes, and false otherwise.
[[nodiscard]] bool ValidateNameAndOwnerIfPresent(
    const blink::InterestGroupKey& group_key,
    const base::Value::Dict& dict) {
  const std::string* maybe_owner = dict.FindString("owner");
  if (maybe_owner &&
      url::Origin::Create(GURL(*maybe_owner)) != group_key.owner) {
    return false;
  }
  const std::string* maybe_name = dict.FindString("name");
  if (maybe_name && *maybe_name != group_key.name) {
    return false;
  }
  return true;
}

// Copies the `priorityVector` JSON field into `priority_vector`. Returns
// true if the JSON is valid and the copy completed.
[[nodiscard]] bool TryToCopyPriorityVector(
    const base::Value::Dict& dict,
    std::optional<base::flat_map<std::string, double>>& priority_vector) {
  const base::Value* maybe_dict = dict.Find("priorityVector");
  if (!maybe_dict) {
    return true;
  }
  if (!maybe_dict->is_dict()) {
    return false;
  }

  // Extract all key/value pairs to a vector before writing to a flat_map, since
  // flat_map insertion is O(n).
  std::vector<std::pair<std::string, double>> pairs;
  for (const std::pair<const std::string&, const base::Value&> pair :
       maybe_dict->GetDict()) {
    if (pair.second.is_int() || pair.second.is_double()) {
      pairs.emplace_back(pair.first, pair.second.GetDouble());
      continue;
    }
    return false;
  }
  priority_vector = base::flat_map<std::string, double>(std::move(pairs));
  return true;
}

// Copies the prioritySignalsOverrides JSON field into
// `priority_signals_overrides`, returns true if the JSON is valid and the copy
// completed. Maps nulls to nullopt, which means a value should be deleted from
// the stored interest group.
[[nodiscard]] bool TryToCopyPrioritySignalsOverrides(
    const base::Value::Dict& dict,
    std::optional<base::flat_map<std::string, std::optional<double>>>&
        priority_signals_overrides) {
  const base::Value* maybe_dict = dict.Find("prioritySignalsOverrides");
  if (!maybe_dict) {
    return true;
  }
  if (!maybe_dict->is_dict()) {
    return false;
  }

  std::vector<std::pair<std::string, std::optional<double>>> pairs;
  for (const std::pair<const std::string&, const base::Value&> pair :
       maybe_dict->GetDict()) {
    if (pair.second.is_none()) {
      pairs.emplace_back(pair.first, std::nullopt);
      continue;
    }
    if (pair.second.is_int() || pair.second.is_double()) {
      pairs.emplace_back(pair.first, pair.second.GetDouble());
      continue;
    }
    return false;
  }
  priority_signals_overrides =
      base::flat_map<std::string, std::optional<double>>(std::move(pairs));
  return true;
}

// Copies the sellerCapabilities JSON field into
// `seller_capabilities` and `all_sellers_capabilities`, returns true if the
// JSON is valid and the copy completed.
[[nodiscard]] bool TryToCopySellerCapabilities(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value* maybe_dict = dict.Find("sellerCapabilities");
  if (!maybe_dict) {
    return true;
  }
  if (!maybe_dict->is_dict()) {
    return false;
  }

  std::vector<std::pair<url::Origin, blink::SellerCapabilitiesType>>
      seller_capabilities_vec;
  for (const std::pair<const std::string&, const base::Value&> pair :
       maybe_dict->GetDict()) {
    if (!pair.second.is_list()) {
      return false;
    }
    blink::SellerCapabilitiesType capabilities;
    for (const base::Value& maybe_capability : pair.second.GetList()) {
      if (!maybe_capability.is_string()) {
        return false;
      }
      const std::string& capability = maybe_capability.GetString();
      base::UmaHistogramBoolean(
          "Ads.InterestGroup.EnumNaming.Update.SellerCapabilities",
          capability == "interestGroupCounts" || capability == "latencyStats");
      if (capability == "interest-group-counts" ||
          capability == "interestGroupCounts") {
        capabilities.Put(blink::SellerCapabilities::kInterestGroupCounts);
      } else if (capability == "latency-stats" ||
                 capability == "latencyStats") {
        capabilities.Put(blink::SellerCapabilities::kLatencyStats);
      } else {
        continue;
      }
    }
    if (pair.first == "*") {
      interest_group_update.all_sellers_capabilities = capabilities;
    } else {
      seller_capabilities_vec.emplace_back(
          url::Origin::Create(GURL(pair.first)), capabilities);
    }
  }
  if (!seller_capabilities_vec.empty()) {
    interest_group_update.seller_capabilities.emplace(seller_capabilities_vec);
  }
  return true;
}

// Copies the executionMode JSON field into `interest_group_update`, returns
// true iff the JSON is valid and the copy completed.
[[nodiscard]] bool TryToCopyExecutionMode(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const std::string* maybe_execution_mode = dict.FindString("executionMode");
  if (!maybe_execution_mode) {
    return true;
  }
  if (*maybe_execution_mode == "group-by-origin" ||
      *maybe_execution_mode == "groupByOrigin") {
    interest_group_update.execution_mode =
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.EnumNaming.Update.WorkletExecutionMode",
        *maybe_execution_mode == "groupByOrigin");
  } else if (*maybe_execution_mode == "frozen-context") {
    interest_group_update.execution_mode =
        blink::InterestGroup::ExecutionMode::kFrozenContext;
  } else {
    // We fallback to compatibility mode both when an update explicitly
    // specifies 'compatibility' as the execution mode, and anytime an update
    // specifies any unrecognized execution mode.
    interest_group_update.execution_mode =
        blink::InterestGroup::ExecutionMode::kCompatibilityMode;
  }
  base::UmaHistogramEnumeration("Ads.InterestGroup.Update.AuctionExecutionMode",
                                interest_group_update.execution_mode.value());
  return true;
}

// Copies the trustedBiddingSignalsKeys list JSON field into
// `interest_group_update`, returns true iff the JSON is valid and the copy
// completed.
[[nodiscard]] bool TryToCopyTrustedBiddingSignalsKeys(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value::List* maybe_update_trusted_bidding_signals_keys =
      dict.FindList("trustedBiddingSignalsKeys");
  if (!maybe_update_trusted_bidding_signals_keys) {
    return true;
  }
  std::vector<std::string> trusted_bidding_signals_keys;
  for (const base::Value& keys_value :
       *maybe_update_trusted_bidding_signals_keys) {
    const std::string* maybe_key = keys_value.GetIfString();
    if (!maybe_key) {
      return false;
    }
    trusted_bidding_signals_keys.push_back(*maybe_key);
  }
  interest_group_update.trusted_bidding_signals_keys =
      trusted_bidding_signals_keys;
  return true;
}

// Copies the userBiddingSignals JSON "any" field into
// `interest_group_update` as a string, returns true iff re-serialization
// succeeded and the copy completed.
[[nodiscard]] bool TryToCopyUserBiddingSignals(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value* maybe_user_bidding_signals =
      dict.Find("userBiddingSignals");
  if (!maybe_user_bidding_signals) {
    return true;
  }
  std::string user_bidding_signals;
  JSONStringValueSerializer serializer(&user_bidding_signals);
  if (!serializer.Serialize(*maybe_user_bidding_signals)) {
    return false;
  }
  interest_group_update.user_bidding_signals = std::move(user_bidding_signals);
  return true;
}

// Copies the trustedBiddingSignalsSlotSizeMode JSON field into
// `trusted_bidding_signals_slot_size_mode`. Always succeeds.
[[nodiscard]] bool TryToCopyTrustedBiddingSignalsSlotSizeMode(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const std::string* trusted_bidding_signals_slot_size_mode =
      dict.FindString("trustedBiddingSignalsSlotSizeMode");
  if (!trusted_bidding_signals_slot_size_mode) {
    return true;
  }
  interest_group_update.trusted_bidding_signals_slot_size_mode =
      blink::InterestGroup::ParseTrustedBiddingSignalsSlotSizeMode(
          *trusted_bidding_signals_slot_size_mode);
  return true;
}

// Copies the maxTrustedBiddingSignalsURLLength JSON field into
// `max_trusted_bidding_signals_url_length`.
[[nodiscard]] bool TryToCopyMaxTrustedBiddingSignalsURLLength(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value* maybe_max_trusted_bidding_signals_url_length =
      dict.Find("maxTrustedBiddingSignalsURLLength");

  if (!maybe_max_trusted_bidding_signals_url_length) {
    return true;
  }

  // `max_trusted_bidding_signals_url_length` must be a valid 32-bit integer.
  if (!maybe_max_trusted_bidding_signals_url_length->is_int()) {
    return false;
  }
  int32_t max_trusted_bidding_signals_url_length =
      maybe_max_trusted_bidding_signals_url_length->GetInt();

  // `max_trusted_bidding_signals_url_length` must not be negative.
  if (max_trusted_bidding_signals_url_length < 0) {
    return false;
  }

  interest_group_update.max_trusted_bidding_signals_url_length =
      max_trusted_bidding_signals_url_length;
  return true;
}

// Copies the trustedBiddingSignalsCoordinator JSON field into
// `trusted_bidding_signals_coordinator`.
[[nodiscard]] bool TryToCopyTrustedBiddingSignalsCoordinator(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value* maybe_trusted_bidding_signals_coordinator =
      dict.Find("trustedBiddingSignalsCoordinator");

  // No `trustedBiddingSignalsCoordinator` field in the update JSON.
  if (!maybe_trusted_bidding_signals_coordinator) {
    return true;
  }

  // `trustedBiddingSignalsCoordinator` field is `null` in the update JSON.
  if (maybe_trusted_bidding_signals_coordinator->is_none()) {
    interest_group_update.trusted_bidding_signals_coordinator.emplace(
        std::nullopt);
    return true;
  }

  // If `trusted_bidding_signals_coordinator` is present and not null, it must
  // be a valid URL origin string.
  if (!maybe_trusted_bidding_signals_coordinator->is_string()) {
    return false;
  }

  GURL trusted_bidding_signals_coordinator_url =
      GURL(maybe_trusted_bidding_signals_coordinator->GetString());

  if (!trusted_bidding_signals_coordinator_url.is_valid()) {
    return false;
  }

  url::Origin trusted_bidding_signals_coordinator_url_origin =
      url::Origin::Create(trusted_bidding_signals_coordinator_url);

  if (trusted_bidding_signals_coordinator_url_origin.scheme() !=
      url::kHttpsScheme) {
    return false;
  }

  interest_group_update.trusted_bidding_signals_coordinator =
      std::move(trusted_bidding_signals_coordinator_url_origin);
  return true;
}

// Helper for TryToCopyAds() and TryToCopyAdComponents().
[[nodiscard]] std::optional<std::vector<blink::InterestGroup::Ad>> ExtractAds(
    const base::Value::List& ads_list,
    bool for_components) {
  std::vector<blink::InterestGroup::Ad> ads;
  for (const base::Value& ads_value : ads_list) {
    const base::Value::Dict* ads_dict = ads_value.GetIfDict();
    if (!ads_dict) {
      return std::nullopt;
    }
    const std::string* maybe_render_url = ads_dict->FindString("renderURL");
    const std::string* maybe_render_url_deprecated =
        ads_dict->FindString("renderUrl");
    if (maybe_render_url_deprecated) {
      if (maybe_render_url) {
        if (*maybe_render_url != *maybe_render_url_deprecated) {
          return std::nullopt;
        }
      } else {
        maybe_render_url = maybe_render_url_deprecated;
      }
    }
    if (!maybe_render_url) {
      return std::nullopt;
    }
    GURL render_gurl = GURL(*maybe_render_url);
    if (!render_gurl.is_valid()) {
      return std::nullopt;
    }
    blink::InterestGroup::Ad ad(render_gurl, /*metadata=*/std::nullopt);
    const std::string* maybe_size_group = ads_dict->FindString("sizeGroup");
    if (maybe_size_group) {
      ad.size_group = *maybe_size_group;
    }
    if (!for_components) {
      const std::string* maybe_buyer_reporting_id =
          ads_dict->FindString("buyerReportingId");
      if (maybe_buyer_reporting_id) {
        ad.buyer_reporting_id = *maybe_buyer_reporting_id;
      }
      const std::string* maybe_buyer_and_seller_reporting_id =
          ads_dict->FindString("buyerAndSellerReportingId");
      if (maybe_buyer_and_seller_reporting_id) {
        ad.buyer_and_seller_reporting_id = *maybe_buyer_and_seller_reporting_id;
      }
      const base::Value::List* maybe_selectable_buyer_and_seller_reporting_ids =
          ads_dict->FindList("selectableBuyerAndSellerReportingIds");
      if (maybe_selectable_buyer_and_seller_reporting_ids &&
          base::FeatureList::IsEnabled(
              blink::features::kFledgeAuctionDealSupport)) {
        std::vector<std::string> selectable_buyer_and_seller_reporting_ids;
        for (const auto& id :
             *maybe_selectable_buyer_and_seller_reporting_ids) {
          selectable_buyer_and_seller_reporting_ids.push_back(id.GetString());
        }
        ad.selectable_buyer_and_seller_reporting_ids =
            std::move(selectable_buyer_and_seller_reporting_ids);
      }
      const base::Value::List* maybe_allowed_reporting_origins =
          ads_dict->FindList("allowedReportingOrigins");
      if (maybe_allowed_reporting_origins) {
        ad.allowed_reporting_origins.emplace();
        for (const auto& maybe_origin : *maybe_allowed_reporting_origins) {
          const std::string* origin_string = maybe_origin.GetIfString();
          if (origin_string) {
            ad.allowed_reporting_origins->emplace_back(
                url::Origin::Create(GURL(*origin_string)));
          }
        }
      }
    }
    const base::Value* maybe_metadata = ads_dict->Find("metadata");
    if (maybe_metadata) {
      std::string metadata;
      JSONStringValueSerializer serializer(&metadata);
      if (!serializer.Serialize(*maybe_metadata)) {
        // Binary blobs shouldn't be present, but it's possible we exceeded the
        // max JSON depth.
        return std::nullopt;
      }
      ad.metadata = std::move(metadata);
    }
    const std::string* maybe_ad_render_id = ads_dict->FindString("adRenderId");
    if (maybe_ad_render_id) {
      ad.ad_render_id = *maybe_ad_render_id;
    }
    ads.push_back(std::move(ad));
  }
  return ads;
}

// Copies the `ads` list JSON field into `interest_group_update`, returns true
// iff the JSON is valid and the copy completed.
[[nodiscard]] bool TryToCopyAds(const base::Value::Dict& dict,
                                InterestGroupUpdate& interest_group_update) {
  const base::Value::List* maybe_ads = dict.FindList("ads");
  if (!maybe_ads) {
    return true;
  }
  std::optional<std::vector<blink::InterestGroup::Ad>> maybe_extracted_ads =
      ExtractAds(*maybe_ads, /*for_components=*/false);
  if (!maybe_extracted_ads) {
    return false;
  }
  interest_group_update.ads = std::move(*maybe_extracted_ads);
  return true;
}

// Copies the `adComponents` list JSON field into `interest_group_update`,
// returns true iff the JSON is valid and the copy completed.
[[nodiscard]] bool TryToCopyAdComponents(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value::List* maybe_ads = dict.FindList("adComponents");
  if (!maybe_ads) {
    return true;
  }
  std::optional<std::vector<blink::InterestGroup::Ad>> maybe_extracted_ads =
      ExtractAds(*maybe_ads, /*for_components=*/true);
  if (!maybe_extracted_ads) {
    return false;
  }
  interest_group_update.ad_components = std::move(*maybe_extracted_ads);
  return true;
}

[[nodiscard]] bool TryToCopyAdSizes(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value::Dict* maybe_sizes = dict.FindDict("adSizes");
  if (!maybe_sizes) {
    return true;
  }
  base::flat_map<std::string, blink::AdSize> size_map;
  for (std::pair<const std::string&, const base::Value&> pair : *maybe_sizes) {
    const base::Value::Dict* maybe_size = pair.second.GetIfDict();
    if (!maybe_size) {
      return false;
    }
    const std::string* width_str = maybe_size->FindString("width");
    const std::string* height_str = maybe_size->FindString("height");
    if (!width_str || !height_str) {
      return false;
    }

    auto [width_val, width_units] = blink::ParseAdSizeString(*width_str);
    auto [height_val, height_units] = blink::ParseAdSizeString(*height_str);

    size_map.emplace(pair.first, blink::AdSize(width_val, width_units,
                                               height_val, height_units));
  }
  interest_group_update.ad_sizes.emplace(size_map);
  return true;
}

[[nodiscard]] bool TryToCopySizeGroups(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value::Dict* maybe_groups = dict.FindDict("sizeGroups");
  if (!maybe_groups) {
    return true;
  }
  base::flat_map<std::string, std::vector<std::string>> group_map;
  for (std::pair<const std::string&, const base::Value&> pair : *maybe_groups) {
    const base::Value::List* maybe_sizes = pair.second.GetIfList();
    if (!maybe_sizes) {
      return false;
    }
    std::vector<std::string> pair_sizes;
    for (const base::Value& size : *maybe_sizes) {
      const std::string* maybe_string = size.GetIfString();
      if (!maybe_string) {
        return false;
      }
      pair_sizes.emplace_back(*maybe_string);
    }
    group_map.emplace(pair.first, pair_sizes);
  }
  interest_group_update.size_groups.emplace(group_map);
  return true;
}

[[nodiscard]] bool TryToCopyAuctionServerRequestFlags(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value::List* maybe_flags =
      dict.FindList("auctionServerRequestFlags");
  if (!maybe_flags) {
    return true;
  }
  blink::AuctionServerRequestFlags auction_server_request_flags;
  for (const base::Value& maybe_flag : *maybe_flags) {
    if (!maybe_flag.is_string()) {
      return false;
    }
    const std::string& flag = maybe_flag.GetString();
    if (flag == "omit-ads") {
      auction_server_request_flags.Put(
          blink::AuctionServerRequestFlagsEnum::kOmitAds);
    } else if (flag == "include-full-ads") {
      auction_server_request_flags.Put(
          blink::AuctionServerRequestFlagsEnum::kIncludeFullAds);
    } else if (flag == "omit-user-bidding-signals") {
      auction_server_request_flags.Put(
          blink::AuctionServerRequestFlagsEnum::kOmitUserBiddingSignals);
    }
  }
  interest_group_update.auction_server_request_flags =
      auction_server_request_flags;
  return true;
}

[[nodiscard]] bool TryToCopyPrivateAggregationConfig(
    const base::Value::Dict& dict,
    InterestGroupUpdate& interest_group_update) {
  const base::Value::Dict* maybe_config =
      dict.FindDict("privateAggregationConfig");
  if (!maybe_config) {
    return true;
  }
  const std::string* maybe_aggregation_coordinator_origin =
      maybe_config->FindString("aggregationCoordinatorOrigin");
  if (!maybe_aggregation_coordinator_origin) {
    return true;
  }

  url::Origin aggregation_coordinator_origin =
      url::Origin::Create(GURL(*maybe_aggregation_coordinator_origin));

  if (!aggregation_service::IsAggregationCoordinatorOriginAllowed(
          aggregation_coordinator_origin)) {
    return false;
  }

  interest_group_update.aggregation_coordinator_origin =
      std::move(aggregation_coordinator_origin);
  return true;
}

std::optional<InterestGroupUpdate> ParseUpdateJson(
    const blink::InterestGroupKey& group_key,
    const data_decoder::DataDecoder::ValueOrError& result) {
  // TODO(crbug.com/40172488): Report to devtools.
  if (!result.has_value()) {
    return std::nullopt;
  }
  const base::Value::Dict* dict = result->GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  if (!ValidateNameAndOwnerIfPresent(group_key, *dict)) {
    return std::nullopt;
  }
  InterestGroupUpdate interest_group_update;
  const base::Value* maybe_priority_value = dict->Find("priority");
  if (maybe_priority_value) {
    // If the field is specified, it must be an integer or a double.
    if (!maybe_priority_value->is_int() && !maybe_priority_value->is_double()) {
      return std::nullopt;
    }
    interest_group_update.priority = maybe_priority_value->GetDouble();
  }
  const base::Value* maybe_enable_bidding_signals_prioritization =
      dict->Find("enableBiddingSignalsPrioritization");
  if (maybe_enable_bidding_signals_prioritization) {
    // If the field is specified, it must be a bool.
    if (!maybe_enable_bidding_signals_prioritization->is_bool()) {
      return std::nullopt;
    }
    interest_group_update.enable_bidding_signals_prioritization =
        maybe_enable_bidding_signals_prioritization->GetBool();
  }
  if (!TryToCopyPriorityVector(*dict, interest_group_update.priority_vector) ||
      !TryToCopyPrioritySignalsOverrides(
          *dict, interest_group_update.priority_signals_overrides) ||
      !TryToCopyExecutionMode(*dict, interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopySellerCapabilities(*dict, interest_group_update)) {
    return std::nullopt;
  }
  const std::string* maybe_bidding_url = dict->FindString("biddingLogicURL");
  const std::string* maybe_bidding_url_deprecated =
      dict->FindString("biddingLogicUrl");
  if (maybe_bidding_url_deprecated) {
    if (maybe_bidding_url) {
      if (*maybe_bidding_url_deprecated != *maybe_bidding_url) {
        return std::nullopt;
      }
    } else {
      maybe_bidding_url = maybe_bidding_url_deprecated;
    }
  }
  if (maybe_bidding_url) {
    interest_group_update.bidding_url = GURL(*maybe_bidding_url);
  }
  const std::string* maybe_bidding_wasm_helper_url =
      dict->FindString("biddingWasmHelperURL");
  const std::string* maybe_bidding_wasm_helper_url_deprecated =
      dict->FindString("biddingWasmHelperUrl");
  if (maybe_bidding_wasm_helper_url_deprecated) {
    if (maybe_bidding_wasm_helper_url) {
      if (*maybe_bidding_wasm_helper_url !=
          *maybe_bidding_wasm_helper_url_deprecated) {
        return std::nullopt;
      }
    } else {
      maybe_bidding_wasm_helper_url = maybe_bidding_wasm_helper_url_deprecated;
    }
  }
  if (maybe_bidding_wasm_helper_url) {
    interest_group_update.bidding_wasm_helper_url =
        GURL(*maybe_bidding_wasm_helper_url);
  }
  const std::string* maybe_update_url =
      dict->FindString("updateURL");  // TODO check if we use this or updateURL
  if (maybe_update_url) {
    interest_group_update.daily_update_url = GURL(*maybe_update_url);
  }
  const std::string* maybe_trusted_bidding_signals_url =
      dict->FindString("trustedBiddingSignalsURL");
  const std::string* maybe_trusted_bidding_signals_url_deprecated =
      dict->FindString("trustedBiddingSignalsUrl");
  if (maybe_trusted_bidding_signals_url_deprecated) {
    if (maybe_trusted_bidding_signals_url) {
      if (*maybe_trusted_bidding_signals_url !=
          *maybe_trusted_bidding_signals_url_deprecated) {
        return std::nullopt;
      }
    } else {
      maybe_trusted_bidding_signals_url =
          maybe_trusted_bidding_signals_url_deprecated;
    }
  }
  if (maybe_trusted_bidding_signals_url) {
    interest_group_update.trusted_bidding_signals_url =
        GURL(*maybe_trusted_bidding_signals_url);
  }
  if (!TryToCopyTrustedBiddingSignalsSlotSizeMode(*dict,
                                                  interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyMaxTrustedBiddingSignalsURLLength(*dict,
                                                  interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyTrustedBiddingSignalsKeys(*dict, interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyTrustedBiddingSignalsCoordinator(*dict,
                                                 interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyUserBiddingSignals(*dict, interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyAds(*dict, interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyAdComponents(*dict, interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyAdSizes(*dict, interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopySizeGroups(*dict, interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyAuctionServerRequestFlags(*dict, interest_group_update)) {
    return std::nullopt;
  }
  if (!TryToCopyPrivateAggregationConfig(*dict, interest_group_update)) {
    return std::nullopt;
  }
  return interest_group_update;
}

}  // namespace

InterestGroupUpdateManager::InterestGroupUpdateManager(
    InterestGroupManagerImpl* manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : manager_(manager),
      max_update_round_duration_(kMaxUpdateRoundDuration),
      max_parallel_updates_(kMaxParallelUpdates),
      url_loader_factory_(std::move(url_loader_factory)) {}

InterestGroupUpdateManager::~InterestGroupUpdateManager() = default;

void InterestGroupUpdateManager::UpdateInterestGroupsOfOwner(
    const url::Origin& owner,
    network::mojom::ClientSecurityStatePtr client_security_state,
    AreReportingOriginsAttestedCallback callback) {
  attestation_callback_ = std::move(callback);
  owners_to_update_.Enqueue(owner, std::move(client_security_state));
  MaybeContinueUpdatingCurrentOwner();
}

void InterestGroupUpdateManager::UpdateInterestGroupsOfOwners(
    base::span<url::Origin> owners,
    network::mojom::ClientSecurityStatePtr client_security_state,
    AreReportingOriginsAttestedCallback callback) {
  // Shuffle the list of interest group owners for fairness.
  base::RandomShuffle(owners.begin(), owners.end());
  for (const url::Origin& owner : owners) {
    UpdateInterestGroupsOfOwner(owner, client_security_state.Clone(), callback);
  }
}

void InterestGroupUpdateManager::set_max_update_round_duration_for_testing(
    base::TimeDelta delta) {
  max_update_round_duration_ = delta;
}

void InterestGroupUpdateManager::set_max_parallel_updates_for_testing(
    int max_parallel_updates) {
  max_parallel_updates_ = max_parallel_updates;
}

InterestGroupUpdateManager::OwnersToUpdate::OwnersToUpdate() = default;

InterestGroupUpdateManager::OwnersToUpdate::~OwnersToUpdate() = default;

bool InterestGroupUpdateManager::OwnersToUpdate::Empty() const {
  return owners_to_update_.empty();
}

const url::Origin& InterestGroupUpdateManager::OwnersToUpdate::FrontOwner()
    const {
  return owners_to_update_.front();
}

network::mojom::ClientSecurityStatePtr
InterestGroupUpdateManager::OwnersToUpdate::FrontSecurityState() const {
  return security_state_map_.at(FrontOwner()).Clone();
}

bool InterestGroupUpdateManager::OwnersToUpdate::Enqueue(
    const url::Origin& owner,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  if (!security_state_map_.emplace(owner, std::move(client_security_state))
           .second) {
    return false;
  }
  owners_to_update_.emplace_back(owner);
  return true;
}

void InterestGroupUpdateManager::OwnersToUpdate::PopFront() {
  security_state_map_.erase(owners_to_update_.front());
  owners_to_update_.pop_front();

  if (owners_to_update_.empty()) {
    joining_origin_isolation_info_map_.clear();
  }
}

net::IsolationInfo*
InterestGroupUpdateManager::OwnersToUpdate::GetIsolationInfoByJoiningOrigin(
    const url::Origin& joining_origin) {
  auto isolation_info_it =
      joining_origin_isolation_info_map_.find(joining_origin);
  if (isolation_info_it != joining_origin_isolation_info_map_.end()) {
    return &isolation_info_it->second;
  } else {
    net::IsolationInfo isolation_info = net::IsolationInfo::CreateTransient();
    const auto [it, success] = joining_origin_isolation_info_map_.insert(
        {joining_origin, std::move(isolation_info)});
    CHECK(success);
    return &it->second;
  }
}

void InterestGroupUpdateManager::OwnersToUpdate::
    ClearJoiningOriginIsolationInfoMap() {
  joining_origin_isolation_info_map_.clear();
}

void InterestGroupUpdateManager::OwnersToUpdate::Clear() {
  owners_to_update_.clear();
  security_state_map_.clear();
  joining_origin_isolation_info_map_.clear();
}

void InterestGroupUpdateManager::MaybeContinueUpdatingCurrentOwner() {
  if (num_in_flight_updates_ > 0 || waiting_on_db_read_) {
    return;
  }

  if (owners_to_update_.Empty()) {
    base::UmaHistogramLongTimes(
        "Ads.InterestGroup.Round.GroupsUpdated.TotalTime",
        base::TimeTicks::Now() - last_update_started_);
    base::UmaHistogramCounts1000(
        "Ads.InterestGroup.Round.GroupsUpdated.TotalCount",
        num_groups_updated_in_current_round_);
    base::UmaHistogramBoolean("Ads.InterestGroup.Round.GroupsUpdated.Cancelled",
                              false);
    // This update round is finished, there's no more work to do.
    last_update_started_ = base::TimeTicks::Min();
    num_groups_updated_in_current_round_ = 0;

    return;
  }

  if (last_update_started_ == base::TimeTicks::Min()) {
    // It appears we're staring a new update round; mark the time we started the
    // round.
    last_update_started_ = base::TimeTicks::Now();
  } else if (base::TimeTicks::Now() - last_update_started_ >
             max_update_round_duration_) {
    base::UmaHistogramLongTimes(
        "Ads.InterestGroup.Round.GroupsUpdated.TotalTime",
        base::TimeTicks::Now() - last_update_started_);
    base::UmaHistogramCounts1000(
        "Ads.InterestGroup.Round.GroupsUpdated.TotalCount",
        num_groups_updated_in_current_round_);
    base::UmaHistogramBoolean("Ads.InterestGroup.Round.GroupsUpdated.Cancelled",
                              true);
    // We've been updating for too long; cancel all outstanding updates.
    owners_to_update_.Clear();
    last_update_started_ = base::TimeTicks::Min();
    num_groups_updated_in_current_round_ = 0;

    return;
  }

  GetInterestGroupsForUpdate(
      owners_to_update_.FrontOwner(),
      base::BindOnce(
          &InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerDbLoad,
          weak_factory_.GetWeakPtr(), owners_to_update_.FrontOwner()));
}

void InterestGroupUpdateManager::GetInterestGroupsForUpdate(
    const url::Origin& owner,
    base::OnceCallback<void(std::vector<InterestGroupUpdateParameter>)>
        callback) {
  DCHECK_EQ(num_in_flight_updates_, 0);
  DCHECK(!waiting_on_db_read_);
  waiting_on_db_read_ = true;

  // Read one more interest group than `max_parallel_updates_` from database to
  //  support the batching logic in `DidUpdateInterestGroupsOfOwnerDbLoad()`.
  manager_->GetInterestGroupsForUpdate(
      owner, /*groups_limit=*/max_parallel_updates_ + 1, std::move(callback));
}

void InterestGroupUpdateManager::UpdateInterestGroupByBatch(
    const url::Origin& owner,
    std::vector<InterestGroupUpdateParameter> update_parameters) {
  DCHECK_LE(update_parameters.size(),
            static_cast<unsigned int>(max_parallel_updates_));

  // If feature kGroupNIKByJoiningOriginPerOwner is not enabled, use one single
  // NIK for all storage interest groups.
  net::IsolationInfo per_update_isolation_info;
  if (!base::FeatureList::IsEnabled(features::kGroupNIKByJoiningOrigin)) {
    per_update_isolation_info = net::IsolationInfo::CreateTransient();
  }

  for (auto& [interest_group_key, update_url, joining_origin] :
       update_parameters) {
    ++num_in_flight_updates_;
    base::UmaHistogramCounts100000(
        "Ads.InterestGroup.Net.RequestUrlSizeBytes.Update",
        update_url.spec().size());
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = std::move(update_url);
    resource_request->redirect_mode = network::mojom::RedirectMode::kError;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->request_initiator = owner;
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    if (base::FeatureList::IsEnabled(features::kGroupNIKByJoiningOrigin)) {
      resource_request->trusted_params->isolation_info =
          *owners_to_update_.GetIsolationInfoByJoiningOrigin(joining_origin);
    } else {
      resource_request->trusted_params->isolation_info =
          per_update_isolation_info;
    }
    resource_request->trusted_params->client_security_state =
        owners_to_update_.FrontSecurityState();
    auto simple_url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), kTrafficAnnotation);
    simple_url_loader->SetTimeoutDuration(base::Seconds(30));
    auto simple_url_loader_it =
        url_loaders_.insert(url_loaders_.end(), std::move(simple_url_loader));
    (*simple_url_loader_it)
        ->DownloadToString(
            url_loader_factory_.get(),
            base::BindOnce(&InterestGroupUpdateManager::
                               DidUpdateInterestGroupsOfOwnerNetFetch,
                           weak_factory_.GetWeakPtr(), simple_url_loader_it,
                           interest_group_key,
                           /*start_time=*/base::TimeTicks::Now()),
            kMaxUpdateSize);
  }

  num_groups_updated_in_current_round_ =
      num_groups_updated_in_current_round_ + update_parameters.size();

  // To avoid the possibility of groups that join during the current update
  // process being updated in the next batch with the same isolation
  // information, clear the `joining_origin_isolation_info_map_` when mixed
  // joining origins are detected in a single update batch.
  if (base::FeatureList::IsEnabled(features::kGroupNIKByJoiningOrigin)) {
    if (update_parameters.size() > 1 &&
        !update_parameters.at(0).joining_origin.IsSameOriginWith(
            update_parameters.back().joining_origin)) {
      owners_to_update_.ClearJoiningOriginIsolationInfoMap();
    }
  }
}

void InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerDbLoad(
    url::Origin owner,
    std::vector<InterestGroupUpdateParameter> update_parameters) {
  DCHECK_EQ(owner, owners_to_update_.FrontOwner());
  DCHECK_EQ(num_in_flight_updates_, 0);
  DCHECK(waiting_on_db_read_);
  waiting_on_db_read_ = false;

  if (update_parameters.empty()) {
    // All interest groups for `owner` are up to date, so we can pop it off the
    // queue.
    owners_to_update_.PopFront();
    MaybeContinueUpdatingCurrentOwner();
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kGroupNIKByJoiningOrigin)) {
    update_parameters.resize(std::min(
        update_parameters.size(), static_cast<size_t>(max_parallel_updates_)));
    DCHECK_LE(update_parameters.size(),
              static_cast<unsigned int>(max_parallel_updates_));
    UpdateInterestGroupByBatch(owner, std::move(update_parameters));
    return;
  }

  // A group of IGs of the same joining origin and update NIK may only be
  // updated across batches if all but the last of those batches contain only
  // IGs of that joining origin / NIK -- otherwise, a server that knows the
  // batch size could deduce information about the number of interest groups
  // that had a different joining origin for prior batches. For details, see the
  // discussion at
  // https://chromium-review.googlesource.com/c/chromium/src/+/4794574/17..20/content/browser/interest_group/interest_group_update_manager.cc#b736.

  // If the size of storage groups vector is not larger than the limitation,
  // the storage groups can be put into one batch and update together.
  if (update_parameters.size() <= static_cast<size_t>(max_parallel_updates_)) {
    UpdateInterestGroupByBatch(owner, std::move(update_parameters));
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.Round.GroupsUpdated.ResizeOperation",
        GroupUpdateResizeOperation::kNoResize);
    return;
  }

  // If the first group and the last group have same joining origin, it is
  // safe to put them in the same update batch.
  if (update_parameters.at(0).joining_origin.IsSameOriginWith(
          update_parameters.at(static_cast<size_t>(max_parallel_updates_) - 1)
              .joining_origin)) {
    update_parameters.resize(max_parallel_updates_);
    UpdateInterestGroupByBatch(owner, std::move(update_parameters));
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.Round.GroupsUpdated.ResizeOperation",
        GroupUpdateResizeOperation::kEqualEndsResize);
    return;
  }

  // Resize the interest group to the limit if the last storage group has
  // different joining origin than the next storage group after the batch
  // limit.
  if (!update_parameters.at(max_parallel_updates_ - 1)
           .joining_origin.IsSameOriginWith(
               update_parameters.at(max_parallel_updates_).joining_origin)) {
    update_parameters.resize(max_parallel_updates_);

    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.Round.GroupsUpdated.ResizeOperation",
        GroupUpdateResizeOperation::kNonSplitResize);
  } else {
    // Interest groups with same joining origin cannot be put into
    // different batches, unless it can fill all the batches except the
    // last one. Therefore, after resize, all the interest groups with
    // same joining origin as the last one need to be popped out to be
    // loaded in the next batch.
    update_parameters.resize(max_parallel_updates_);
    url::Origin pop_out_origin =
        update_parameters.at(max_parallel_updates_ - 1).joining_origin;

    while (update_parameters.size() > 0 and
           update_parameters.back().joining_origin.IsSameOriginWith(
               pop_out_origin)) {
      update_parameters.pop_back();
    }

    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.Round.GroupsUpdated.ResizeOperation",
        GroupUpdateResizeOperation::kSplitResize);
  }

  UpdateInterestGroupByBatch(owner, std::move(update_parameters));
  return;
}

void InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerNetFetch(
    UrlLoadersList::iterator simple_url_loader_it,
    blink::InterestGroupKey group_key,
    base::TimeTicks start_time,
    std::unique_ptr<std::string> fetch_body) {
  DCHECK_EQ(group_key.owner, owners_to_update_.FrontOwner());
  DCHECK_GT(num_in_flight_updates_, 0);
  DCHECK(!waiting_on_db_read_);
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      std::move(*simple_url_loader_it);
  url_loaders_.erase(simple_url_loader_it);

  base::UmaHistogramMediumTimes("Ads.InterestGroup.Net.DownloadTime.Update",
                                base::TimeTicks::Now() - start_time);

  // TODO(crbug.com/40172488): Report HTTP error info to devtools.
  if (!fetch_body) {
    ReportUpdateFailed(group_key,
                       /*delay_type=*/simple_url_loader->NetError() ==
                               net::ERR_INTERNET_DISCONNECTED
                           ? UpdateDelayType::kNoInternet
                           : UpdateDelayType::kNetFailure);
    return;
  }
  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.ResponseSizeBytes.Update", fetch_body->size());

  data_decoder::DataDecoder::ParseJsonIsolated(
      *fetch_body,
      base::BindOnce(
          &InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerJsonParse,
          weak_factory_.GetWeakPtr(), std::move(group_key)));
}

void InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerJsonParse(
    blink::InterestGroupKey group_key,
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_EQ(group_key.owner, owners_to_update_.FrontOwner());
  DCHECK_GT(num_in_flight_updates_, 0);
  DCHECK(!waiting_on_db_read_);
  std::optional<InterestGroupUpdate> interest_group_update =
      ParseUpdateJson(group_key, result);
  if (!interest_group_update) {
    ReportUpdateFailed(group_key, UpdateDelayType::kParseFailure);
    return;
  }
  // All ads' allowed reporting origins must be attested. Otherwise don't update
  // the interest group.
  if (interest_group_update->ads) {
    for (auto& ad : *interest_group_update->ads) {
      if (ad.allowed_reporting_origins) {
        // Sort and de-duplicate by passing it through a flat_set.
        ad.allowed_reporting_origins =
            base::flat_set<url::Origin>(
                std::move(ad.allowed_reporting_origins.value()))
                .extract();
        if (!attestation_callback_.Run(ad.allowed_reporting_origins.value())) {
          // Treat this the same way as a parse failure.
          ReportUpdateFailed(group_key, UpdateDelayType::kParseFailure);
          return;
        }
      }
    }
  }
  UpdateInterestGroup(group_key, std::move(*interest_group_update));
}

void InterestGroupUpdateManager::UpdateInterestGroup(
    const blink::InterestGroupKey& group_key,
    InterestGroupUpdate update) {
  manager_->UpdateInterestGroup(
      group_key, std::move(update),
      base::BindOnce(
          &InterestGroupUpdateManager::OnUpdateInterestGroupCompleted,
          weak_factory_.GetWeakPtr(), group_key));
}

void InterestGroupUpdateManager::OnUpdateInterestGroupCompleted(
    const blink::InterestGroupKey& group_key,
    bool success) {
  if (!success) {
    ReportUpdateFailed(group_key, UpdateDelayType::kParseFailure);
    return;
  }
  OnOneUpdateCompleted();
}

void InterestGroupUpdateManager::OnOneUpdateCompleted() {
  DCHECK_GT(num_in_flight_updates_, 0);
  --num_in_flight_updates_;
  MaybeContinueUpdatingCurrentOwner();
}

void InterestGroupUpdateManager::ReportUpdateFailed(
    const blink::InterestGroupKey& group_key,
    UpdateDelayType delay_type) {
  if (delay_type != UpdateDelayType::kNoInternet) {
    manager_->ReportUpdateFailed(
        group_key,
        /*parse_failure=*/delay_type == UpdateDelayType::kParseFailure);
  }

  if (delay_type == UpdateDelayType::kNoInternet) {
    // If the internet is disconnected, no more updating is possible at the
    // moment. As we are now stopping update work, and it is an invariant
    // that `owners_to_update_` only stores owners that will eventually
    // be processed, we clear `owners_to_update_` to ensure that future
    // update attempts aren't blocked.
    //
    // To avoid violating the invariant that we're always updating the front of
    // the queue, only clear we encounter this error on the last in-flight
    // update.
    if (num_in_flight_updates_ == 1) {
      owners_to_update_.Clear();
    }
  }

  OnOneUpdateCompleted();
}

}  // namespace content

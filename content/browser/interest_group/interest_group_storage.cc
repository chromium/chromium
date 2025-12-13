// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_storage.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/pass_key.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/for_debugging_only_report_util.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_storage.pb.h"
#include "content/browser/interest_group/interest_group_storage_metric_types.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/ad_auction/event_record.h"
#include "services/network/public/cpp/features.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/snappy/src/snappy.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/origin.h"

namespace content {

namespace {

using PassKey = base::PassKey<InterestGroupStorage>;
using blink::mojom::BiddingBrowserSignalsPtr;
using blink::mojom::PreviousWinPtr;
using blink::mojom::ViewAndClickCountsPtr;
using blink::mojom::ViewOrClickCountsPtr;
using SellerCapabilitiesType = blink::SellerCapabilitiesType;
using network::AdAuctionEventRecord;

// Allow lookups using `std::string_view`.
struct StringViewHasher : public std::hash<std::string_view> {
  using is_transparent = void;
};

using InterestGroupsByName = std::unordered_map<std::string,
                                                StorageInterestGroup,
                                                StringViewHasher,
                                                std::equal_to<>>;

// The raw view and click data for a given (provider_origin, eligible_origin)
// tuple.
struct ViewClickCountsForProviderAndEligible {
  ListOfTimestamps uncompacted_view_events;
  ListOfTimestampAndCounts compacted_view_events;
  ListOfTimestamps uncompacted_click_events;
  ListOfTimestampAndCounts compacted_click_events;
};

// Reason why a database lookup couldn't produce a result. It could be an
// error, or just because there is nothing there.
enum class MissingReason { kNotInDb, kDbError, kDecodeError };

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("InterestGroups");

// Version number of the database.
//
// Version 1 - 2021/03 - crrev.com/c/2757425
// Version 2 - 2021/08 - crrev.com/c/3097715
// Version 3 - 2021/09 - crrev.com/c/3165576
// Version 4 - 2021/10 - crrev.com/c/3172863
// Version 5 - 2021/10 - crrev.com/c/3067804
// Version 6 - 2021/12 - crrev.com/c/3330516
// Version 7 - 2022/03 - crrev.com/c/3517534
// Version 8 - 2022/06 - crrev.com/c/3696265
// Version 9 - 2022/07 - crrev.com/c/3780305
// Version 10 - 2022/08 - crrev.com/c/3818142
// Version 13 - 2023/01 - crrev.com/c/4167800
// Version 14 - 2023/08 - crrev.com/c/4739632
// Version 15 - 2023/08 - crrev.com/c/4808727
// Version 16 - 2023/08 - crrev.com/c/4822944
// Version 17 - 2023/09 - crrev.com/c/4852051
// Version 18 - 2023/09 - crrev.com/c/4902233
// Version 19 - 2023/10 - crrev.com/c/4891458
// Version 20 - 2023/11 - crrev.com/c/5050989
// Version 21 - 2023/11 - crrev.com/c/5063314
// Version 22 - 2023/12 - crrev.com/c/5063589
// Version 23 - 2024/01 - crrev.com/c/5173733
// Version 24 - 2024/01 - crrev.com/c/5245196
// Version 25 - 2024/04 - crrev.com/c/5497898
// Version 26 - 2024/05 - crrev.com/c/5555460
// Version 27 - 2024/05 - crrev.com/c/5521957
// Version 28 - 2024/06 - crrev.com/c/5647523
// Version 29 - 2024/06 - crrev.com/c/5753049
// Version 30 - 2024/08 - crrev.com/c/5707491
// Version 31 - 2025/01 - crrev.com/c/6084483
// Version 32 - 2025/02 - crrev.com/c/6239846
// Version 33 - 2025/02 - crrev.com/c/6248184
// Version 34 - 2025/02 - crrev.com/c/6256880
// Version 35 - 2025/03 - crrev.com/c/6361984
//
// Version 1 adds a table for interest groups.
// Version 2 adds a column for rate limiting interest group updates.
// Version 3 adds a field for ad components.
// Version 4 adds joining origin and url.
// Version 5 adds k-anonymity tables and fields.
// Version 6 adds WebAssembly helper url.
// Version 7 changes an index, adds interest group priority.
// Version 8 adds the execution_mode field to interest groups.
// Version 9 changes bid_history and join_history to daily counts.
// Version 10 changes k-anonymity table so it doesn't split by type.
// Version 11 adds priority vector support and time a group was joined.
// Version 12 adds seller capabilities fields.
// Version 13 adds ad size-related fields (ad_sizes & size_groups).
// Version 14 adds auction server request flags.
// Version 15 adds an additional bid key field.
// Version 16 changes the ads and ad component columns of the interest group
// table to protobuf format.
// Version 17 adds interest group name and owner columns to the k-anonymity
// table.
// Version 18 adds a new index on IG type (regular vs negative) to support
// split caps on max interest groups per owner.
// Version 19 adds the aggregation_coordinator_origin and storage_size columns
// to the interest group table.
// Version 20 adds the lockout_debugging_only_report and
// cooldown_debugging_only_report tables.
// Version 21 adds the trusted_bidding_signals_slot_size_mode column to the
// interest group table.
// Version 22 adds id column to the debug report lockout table, and changes
// starting and duration columns to starting_time and type columns to the debug
// report cooldown table.
// Version 23 adds trusted bidding signals URL length limit.
// Version 24 adds cached B&A server keys.
// Version 25 uses hashed k-anon keys instead of the unhashed versions.
// Version 26 runs a VACUUM command.
// Version 27 stores k-anon values and update times in interest group table.
// Version 28 adds trusted bidding signals coordinator.
// Version 29 adds selectableBuyerAndSellerReportingIds field to ad object.
// Version 30 compresses the AdsProto field using Snappy compression and runs a
// VACUUM command.
// Version 31 adds creative_scanning_metadata field to ad object.
// Version 32 adds duration column to the debug report lockout table, and
//  renames its last_report_sent_time column to starting_time.
// Version 33 adds view_and_click_counts_providers interest_groups field.
// Version 34 adds view_and_click_events table.
// Version 35 adds a cache of k-anon hashes previously retrieved from the
//  k-anonymity server with a TTL of the QueryInterval.

const int kCurrentVersionNumber = 35;

// Earliest version of the code which can use a |kCurrentVersionNumber| database
// without failing. This is used to determine if an upgraded version of the
// database -- upgraded by running a newer version of the client -- can still
// work when later running on an older version of the client. Note that if the
// older version of the client isn't identified as compatible, the client will
// wipe the database entirely before proceeding. As a rule of thumb, this
// probably needs to be incremented to the `kCurrentVersionNumber` if you're
// adding or deleting columns to an existing table, but not if you're just
// adding a new table.
const int kCompatibleVersionNumber = 33;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database.
const int kDeprecatedVersionNumber = 5;

// Truncates `in` down to the hour.
base::Time TruncateToHour(base::Time in) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      in.ToDeltaSinceWindowsEpoch().FloorToMultiple(base::Hours(1)));
}

// Truncates `in` down to the day.
base::Time TruncateToDay(base::Time in) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      in.ToDeltaSinceWindowsEpoch().FloorToMultiple(base::Days(1)));
}

std::string Serialize(base::ValueView value_view) {
  std::optional<std::string> json_output = base::WriteJson(value_view);
  if (json_output.has_value()) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.JSONSerializationResult",
        InterestGroupStorageJSONSerializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.JSONSerializationResult",
        InterestGroupStorageJSONSerializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  return std::move(json_output).value_or(std::string());
}

std::optional<base::Value> DeserializeValue(std::string_view serialized_value) {
  if (serialized_value.empty()) {
    return {};
  }
  std::optional<base::Value> result = base::JSONReader::Read(
      serialized_value, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (result) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.JSONDeserializationResult",
        InterestGroupStorageJSONDeserializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.JSONDeserializationResult",
        InterestGroupStorageJSONDeserializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }

  return result;
}

std::string Serialize(const url::Origin& origin) {
  return origin.Serialize();
}
url::Origin DeserializeOrigin(std::string_view serialized_origin) {
  return url::Origin::Create(GURL(serialized_origin));
}

std::string Serialize(const std::optional<GURL>& url) {
  if (!url) {
    return std::string();
  }
  return url->spec();
}
std::optional<GURL> DeserializeURL(std::string_view serialized_url) {
  GURL result(serialized_url);
  if (result.is_empty()) {
    return std::nullopt;
  }
  return result;
}

blink::InterestGroup::Ad FromInterestGroupAdValue(const PassKey& passkey,
                                                  const base::Value::Dict& dict,
                                                  bool for_components) {
  const std::string* maybe_url = dict.FindString("url");
  if (!maybe_url) {
    return blink::InterestGroup::Ad();
  }
  blink::InterestGroup::Ad result(passkey, *maybe_url);
  const std::string* maybe_size_group = dict.FindString("size_group");
  if (maybe_size_group) {
    result.size_group = *maybe_size_group;
  }
  if (!for_components) {
    const std::string* maybe_buyer_reporting_id =
        dict.FindString("buyer_reporting_id");
    if (maybe_buyer_reporting_id) {
      result.buyer_reporting_id = *maybe_buyer_reporting_id;
    }
    const std::string* maybe_buyer_and_seller_reporting_id =
        dict.FindString("buyer_and_seller_reporting_id");
    if (maybe_buyer_and_seller_reporting_id) {
      result.buyer_and_seller_reporting_id =
          *maybe_buyer_and_seller_reporting_id;
    }
    const auto* maybe_selectable_buyer_and_seller_reporting_ids =
        dict.FindList("selectable_buyer_and_seller_reporting_ids");

    if (maybe_selectable_buyer_and_seller_reporting_ids) {
      std::vector<std::string> selectable_buyer_and_seller_reporting_ids;
      for (const auto& id : *maybe_selectable_buyer_and_seller_reporting_ids) {
        // TODO: `base::Value::GetString()` will crash if this value is not a
        // string, but this data originates from a potentially untrusted
        // database. Operations like this, including `DCHECK`s, should be
        // audited throughout this file.
        selectable_buyer_and_seller_reporting_ids.emplace_back(id.GetString());
      }
      result.selectable_buyer_and_seller_reporting_ids =
          std::move(selectable_buyer_and_seller_reporting_ids);
    }
    const auto* maybe_allowed_reporting_origins =
        dict.FindList("allowed_reporting_origins");
    if (maybe_allowed_reporting_origins) {
      std::vector<url::Origin> allowed_reporting_origins_vector;
      for (const auto& origin : *maybe_allowed_reporting_origins) {
        const std::string* origin_str = origin.GetIfString();
        DCHECK(origin_str);
        allowed_reporting_origins_vector.emplace_back(
            DeserializeOrigin(*origin_str));
      }
      result.allowed_reporting_origins =
          std::move(allowed_reporting_origins_vector);
    }
  }

  const std::string* maybe_metadata = dict.FindString("metadata");
  if (maybe_metadata) {
    result.metadata = *maybe_metadata;
  }
  const std::string* maybe_ad_render_id = dict.FindString("ad_render_id");
  if (maybe_ad_render_id) {
    result.ad_render_id = *maybe_ad_render_id;
  }
  return result;
}

std::string Serialize(
    const std::optional<base::flat_map<std::string, double>>& flat_map) {
  if (!flat_map) {
    return std::string();
  }
  base::Value::Dict dict;
  for (const auto& key_value_pair : *flat_map) {
    dict.Set(key_value_pair.first, key_value_pair.second);
  }
  return Serialize(dict);
}
std::optional<base::flat_map<std::string, double>> DeserializeStringDoubleMap(
    std::string_view serialized_flat_map) {
  std::optional<base::Value> flat_map_value =
      DeserializeValue(serialized_flat_map);
  if (!flat_map_value || !flat_map_value->is_dict()) {
    return std::nullopt;
  }

  // Extract all key/values pairs to a vector before writing to a flat_map,
  // since flat_map insertion is O(n).
  std::vector<std::pair<std::string, double>> pairs;
  for (const auto pair : flat_map_value->GetDict()) {
    if (!pair.second.is_double()) {
      return std::nullopt;
    }
    pairs.emplace_back(pair.first, pair.second.GetDouble());
  }
  return base::flat_map<std::string, double>(std::move(pairs));
}

AdProtos GetAdProtosFromAds(const std::vector<blink::InterestGroup::Ad>& ads) {
  AdProtos ad_protos;
  for (const blink::InterestGroup::Ad& ad : ads) {
    AdProtos_AdProto* ad_proto = ad_protos.add_ads();
    ad_proto->set_render_url(ad.render_url());
    if (ad.size_group.has_value()) {
      ad_proto->set_size_group(*ad.size_group);
    }
    if (ad.metadata.has_value()) {
      ad_proto->set_metadata(*ad.metadata);
    }
    if (ad.buyer_reporting_id.has_value()) {
      ad_proto->set_buyer_reporting_id(*ad.buyer_reporting_id);
    }
    if (ad.buyer_and_seller_reporting_id.has_value()) {
      ad_proto->set_buyer_and_seller_reporting_id(
          *ad.buyer_and_seller_reporting_id);
    }
    if (ad.selectable_buyer_and_seller_reporting_ids.has_value()) {
      ad_proto->mutable_selectable_buyer_and_seller_reporting_ids()->Add(
        ad.selectable_buyer_and_seller_reporting_ids->begin(),
        ad.selectable_buyer_and_seller_reporting_ids->end()
      );
    }
    if (ad.ad_render_id.has_value()) {
      ad_proto->set_ad_render_id(*ad.ad_render_id);
    }
    if (ad.allowed_reporting_origins.has_value()) {
      for (auto allowed_reporting_origin : *ad.allowed_reporting_origins) {
        ad_proto->add_allowed_reporting_origins(
            allowed_reporting_origin.Serialize());
      }
    }
    if (ad.creative_scanning_metadata.has_value()) {
      ad_proto->set_creative_scanning_metadata(*ad.creative_scanning_metadata);
    }
  }
  return ad_protos;
}

// Upgrade code needs to serialize without compression -- otherwise, the
// Serialize() method below is used.
std::string SerializeUncompressed(
    const std::optional<std::vector<blink::InterestGroup::Ad>>& ads) {
  base::TimeTicks start = base::TimeTicks::Now();
  std::string serialized_ads;
  AdProtos ad_protos =
      ads.has_value() ? GetAdProtosFromAds(ads.value()) : AdProtos();

  if (ad_protos.SerializeToString(&serialized_ads)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.AdProtos",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.AdProtos",
        InterestGroupStorageProtoSerializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  base::UmaHistogramTimes("Storage.InterestGroup.AdProtoSerializationTime",
                          base::TimeTicks::Now() - start);
  return serialized_ads;
}

std::string Serialize(
    const std::optional<std::vector<blink::InterestGroup::Ad>>& ads) {
  std::string serialized_ads = SerializeUncompressed(ads);

  std::string compressed;
  base::TimeTicks start = base::TimeTicks::Now();
  snappy::Compress(serialized_ads.data(), serialized_ads.size(), &compressed);
  base::UmaHistogramTimes("Storage.InterestGroup.AdProtoCompressionTime",
                          base::TimeTicks::Now() - start);
  if (serialized_ads.size() > 0u) {
    base::UmaHistogramPercentage(
        "Storage.InterestGroup.AdProtoCompressionRatio",
        compressed.size() * 100 / serialized_ads.size());
  }
  base::UmaHistogramCounts1M("Storage.InterestGroup.AdProtoSizeUncompressed",
                             serialized_ads.size());
  base::UmaHistogramCounts1M("Storage.InterestGroup.AdProtoSizeCompressed",
                             compressed.size());
  return compressed;
}

std::optional<std::vector<blink::InterestGroup::Ad>>
DeserializeInterestGroupAdVectorJson(const PassKey& passkey,
                                     std::string_view serialized_ads,
                                     bool for_components) {
  std::optional<base::Value> ads_value = DeserializeValue(serialized_ads);
  if (!ads_value || !ads_value->is_list()) {
    return std::nullopt;
  }
  std::vector<blink::InterestGroup::Ad> result;
  for (const auto& ad_value : ads_value->GetList()) {
    const base::Value::Dict* dict = ad_value.GetIfDict();
    if (dict) {
      result.emplace_back(
          FromInterestGroupAdValue(passkey, *dict, for_components));
    }
  }
  return result;
}

// Upgrade code needs to deserialize without decompression -- otherwise,
// DecompressAndDeserializeInterestGroupAdVectorProto() is used.
std::optional<std::vector<blink::InterestGroup::Ad>>
DeserializeInterestGroupAdVectorProto(const PassKey& passkey,
                                      std::string_view serialized_ads) {
  base::TimeTicks start = base::TimeTicks::Now();
  AdProtos ad_protos;

  bool success = ad_protos.ParseFromString(serialized_ads);

  if (success) {
    UMA_HISTOGRAM_ENUMERATION(
        "Storage.InterestGroup.ProtoDeserializationResult.AdProtos",
        InterestGroupStorageProtoDeserializationResult::kSucceeded);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Storage.InterestGroup.ProtoDeserializationResult.AdProtos",
        InterestGroupStorageProtoDeserializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }

  if (!success || ad_protos.ads().empty()) {
    return std::nullopt;
  }

  std::vector<blink::InterestGroup::Ad> out;
  out.reserve(ad_protos.ads_size());
  for (auto& ad_proto : *ad_protos.mutable_ads()) {
    blink::InterestGroup::Ad& ad =
        out.emplace_back(passkey, std::move(*ad_proto.mutable_render_url()));
    if (ad_proto.has_size_group()) {
      ad.size_group = std::move(*ad_proto.mutable_size_group());
    }
    if (ad_proto.has_metadata()) {
      ad.metadata = std::move(*ad_proto.mutable_metadata());
    }
    if (ad_proto.has_buyer_reporting_id()) {
      ad.buyer_reporting_id = std::move(*ad_proto.mutable_buyer_reporting_id());
    }
    if (ad_proto.has_buyer_and_seller_reporting_id()) {
      ad.buyer_and_seller_reporting_id =
          std::move(*ad_proto.mutable_buyer_and_seller_reporting_id());
    }
    if (base::FeatureList::IsEnabled(
            blink::features::kFledgeAuctionDealSupport) &&
        !ad_proto.selectable_buyer_and_seller_reporting_ids().empty()) {
      std::vector<std::string> selectable_buyer_and_seller_reporting_ids;
      for (const auto& id :
           ad_proto.selectable_buyer_and_seller_reporting_ids()) {
        selectable_buyer_and_seller_reporting_ids.emplace_back(id);
      }
      ad.selectable_buyer_and_seller_reporting_ids =
          std::move(selectable_buyer_and_seller_reporting_ids);
    }
    if (ad_proto.has_ad_render_id()) {
      ad.ad_render_id = std::move(*ad_proto.mutable_ad_render_id());
    }
    if (!ad_proto.allowed_reporting_origins().empty()) {
      std::vector<url::Origin> allowed_reporting_origins_vector;
      allowed_reporting_origins_vector.reserve(
          ad_proto.allowed_reporting_origins_size());
      for (const std::string& allowed_reporting_origin :
           ad_proto.allowed_reporting_origins()) {
        allowed_reporting_origins_vector.emplace_back(
            DeserializeOrigin(allowed_reporting_origin));
      }
      ad.allowed_reporting_origins =
          std::move(allowed_reporting_origins_vector);
    }
    if (ad_proto.has_creative_scanning_metadata()) {
      ad.creative_scanning_metadata =
          std::move(*ad_proto.mutable_creative_scanning_metadata());
    }
  }
  UMA_HISTOGRAM_TIMES("Storage.InterestGroup.AdProtoDeserializationTime",
                      base::TimeTicks::Now() - start);
  return out;
}

std::optional<std::vector<blink::InterestGroup::Ad>>
DecompressAndDeserializeInterestGroupAdVectorProto(
    const PassKey& passkey,
    std::string_view compressed) {
  std::string serialized_ads;
  base::TimeTicks start = base::TimeTicks::Now();
  if (!snappy::Uncompress(compressed.data(), compressed.size(),
                          &serialized_ads)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.AdProtoDecompressionOutcome",
        AdProtoDecompressionOutcome::kFailure);
    return std::nullopt;
  }
  UMA_HISTOGRAM_TIMES("Storage.InterestGroup.AdProtoDecompressionTime",
                      base::TimeTicks::Now() - start);
  UMA_HISTOGRAM_ENUMERATION("Storage.InterestGroup.AdProtoDecompressionOutcome",
                            AdProtoDecompressionOutcome::kSuccess);
  return DeserializeInterestGroupAdVectorProto(passkey, serialized_ads);
}

std::string Serialize(
    const std::optional<base::flat_map<std::string, blink::AdSize>>& ad_sizes) {
  if (!ad_sizes) {
    return std::string();
  }
  base::Value::Dict dict;
  for (const auto& key_value_pair : *ad_sizes) {
    base::Value::Dict size_dict;
    size_dict.Set("width", key_value_pair.second.width);
    size_dict.Set("width_units",
                  static_cast<int>(key_value_pair.second.width_units));
    size_dict.Set("height", key_value_pair.second.height);
    size_dict.Set("height_units",
                  static_cast<int>(key_value_pair.second.height_units));
    dict.Set(key_value_pair.first, Serialize(size_dict));
  }
  return Serialize(dict);
}
std::optional<base::flat_map<std::string, blink::AdSize>>
DeserializeStringSizeMap(std::string_view serialized_sizes) {
  std::optional<base::Value> dict = DeserializeValue(serialized_sizes);
  if (!dict || !dict->is_dict()) {
    return std::nullopt;
  }
  std::vector<std::pair<std::string, blink::AdSize>> result;
  for (std::pair<const std::string&, base::Value&> entry : dict->GetDict()) {
    std::optional<base::Value> ads_size =
        DeserializeValue(entry.second.GetString());
    const base::Value::Dict* size_dict = ads_size->GetIfDict();
    DCHECK(size_dict);
    const base::Value* width_val = size_dict->Find("width");
    const base::Value* width_units_val = size_dict->Find("width_units");
    const base::Value* height_val = size_dict->Find("height");
    const base::Value* height_units_val = size_dict->Find("height_units");
    if (!width_val || !width_units_val || !height_val || !height_units_val) {
      return std::nullopt;
    }
    result.emplace_back(entry.first,
                        blink::AdSize(width_val->GetDouble(),
                                      static_cast<blink::AdSize::LengthUnit>(
                                          width_units_val->GetInt()),
                                      height_val->GetDouble(),
                                      static_cast<blink::AdSize::LengthUnit>(
                                          height_units_val->GetInt())));
  }
  return result;
}

std::string Serialize(
    const std::optional<base::flat_map<std::string, std::vector<std::string>>>&
        size_groups) {
  if (!size_groups) {
    return std::string();
  }
  base::Value::Dict dict;
  for (const auto& key_value_pair : *size_groups) {
    base::Value::List list;
    for (const auto& s : key_value_pair.second) {
      list.Append(s);
    }
    dict.Set(key_value_pair.first, Serialize(list));
  }
  return Serialize(dict);
}
std::optional<base::flat_map<std::string, std::vector<std::string>>>
DeserializeStringStringVectorMap(std::string_view serialized_groups) {
  std::optional<base::Value> dict = DeserializeValue(serialized_groups);
  if (!dict || !dict->is_dict()) {
    return std::nullopt;
  }
  std::vector<std::pair<std::string, std::vector<std::string>>> result;
  for (std::pair<const std::string&, base::Value&> entry : dict->GetDict()) {
    std::optional<base::Value> list =
        DeserializeValue(entry.second.GetString());
    DCHECK(list && list->is_list());
    std::vector<std::string> result_sizes;
    for (base::Value& size : list->GetList()) {
      result_sizes.emplace_back(size.GetString());
    }
    result.emplace_back(entry.first, result_sizes);
  }
  return result;
}

std::string Serialize(const std::optional<std::vector<std::string>>& strings) {
  if (!strings) {
    return std::string();
  }
  base::Value::List list;
  for (const auto& s : strings.value()) {
    list.Append(s);
  }
  return Serialize(list);
}

std::optional<std::vector<std::string>> DeserializeStringVector(
    std::string_view serialized_vector) {
  std::optional<base::Value> list = DeserializeValue(serialized_vector);
  if (!list || !list->is_list()) {
    return std::nullopt;
  }
  std::vector<std::string> result;
  for (const auto& value : list->GetList()) {
    result.push_back(value.GetString());
  }
  return result;
}

std::string Serialize(const std::optional<std::vector<url::Origin>>& origins) {
  std::string serialized;
  ListOfOrigins list_of_origins;

  if (origins) {
    for (const url::Origin& o : origins.value()) {
      list_of_origins.add_origins(o.Serialize());
    }
  }
  if (list_of_origins.SerializeToString(&serialized)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.ListOfOrigins",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.ListOfOrigins",
        InterestGroupStorageProtoSerializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  return serialized;
}

std::optional<std::vector<url::Origin>> DeserializeOriginVector(
    std::string_view serialized_vector) {
  ListOfOrigins list_of_origins;

  bool success = list_of_origins.ParseFromString(serialized_vector);

  if (success) {
    UMA_HISTOGRAM_ENUMERATION(
        "Storage.InterestGroup.ProtoDeserializationResult.ListOfOrigins",
        InterestGroupStorageProtoDeserializationResult::kSucceeded);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Storage.InterestGroup.ProtoDeserializationResult.ListOfOrigins",
        InterestGroupStorageProtoDeserializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }

  if (!success || list_of_origins.origins().empty()) {
    return std::nullopt;
  }
  std::vector<url::Origin> result;
  result.reserve(list_of_origins.origins_size());
  for (const std::string& origin_string : list_of_origins.origins()) {
    result.emplace_back(DeserializeOrigin(origin_string));
  }
  return result;
}

int64_t Serialize(SellerCapabilitiesType capabilities) {
  uint64_t result = capabilities.ToEnumBitmask();
  // Supporting 64 or more seller capabilities will require a different
  // serialization format.
  DCHECK(result <= std::numeric_limits<int64_t>::max());
  return static_cast<int64_t>(result);
}
SellerCapabilitiesType DeserializeSellerCapabilities(int64_t serialized) {
  DCHECK(serialized >= 0);
  return SellerCapabilitiesType::FromEnumBitmask(serialized);
}

std::string Serialize(
    const std::optional<base::flat_map<url::Origin, SellerCapabilitiesType>>&
        flat_map) {
  if (!flat_map) {
    return std::string();
  }
  base::Value::Dict dict;
  for (const auto& key_value_pair : *flat_map) {
    dict.Set(Serialize(key_value_pair.first),
             base::NumberToString(Serialize(key_value_pair.second)));
  }
  return Serialize(dict);
}
std::optional<base::flat_map<url::Origin, SellerCapabilitiesType>>
DeserializeSellerCapabilitiesMap(std::string_view serialized) {
  std::optional<base::Value> dict = DeserializeValue(serialized);
  if (!dict || !dict->is_dict()) {
    return std::nullopt;
  }
  std::vector<std::pair<url::Origin, SellerCapabilitiesType>> result;
  for (std::pair<const std::string&, base::Value&> entry : dict->GetDict()) {
    std::string* value_string = entry.second.GetIfString();
    if (!value_string) {
      return std::nullopt;
    }
    int64_t value_bitmask;
    if (!base::StringToInt64(*value_string, &value_bitmask)) {
      return std::nullopt;
    }
    result.emplace_back(DeserializeOrigin(entry.first),
                        DeserializeSellerCapabilities(value_bitmask));
  }
  return result;
}

int64_t Serialize(blink::AuctionServerRequestFlags flags) {
  // Supporting 64 or more auction server request flags will require a different
  // serialization format. That check is done in EnumSet at compile time, so we
  // don't need to duplicate it here.
  return flags.ToEnumBitmask();
}
blink::AuctionServerRequestFlags DeserializeAuctionServerRequestFlags(
    int64_t serialized) {
  return blink::AuctionServerRequestFlags::FromEnumBitmask(serialized);
}

std::vector<uint8_t> Serialize(
    const std::optional<blink::InterestGroup::AdditionalBidKey>& key) {
  if (!key || key->empty()) {
    return std::vector<uint8_t>();
  }
  return std::vector<uint8_t>(key->begin(), key->end());
}
std::optional<blink::InterestGroup::AdditionalBidKey>
DeserializeAdditionalBidKey(base::span<const uint8_t> serialized) {
  if (serialized.size() != ED25519_PUBLIC_KEY_LEN) {
    return std::nullopt;
  }
  blink::InterestGroup::AdditionalBidKey deserialized;
  std::copy(serialized.begin(), serialized.end(), deserialized.begin());
  return deserialized;
}

// Merges new `priority_signals_overrides` received from an update with an
// existing set of overrides store with an interest group. Populates `overrides`
// if it was previously null.
void MergePrioritySignalsOverrides(
    const base::flat_map<std::string, std::optional<double>>& update_data,
    std::optional<base::flat_map<std::string, double>>&
        priority_signals_overrides) {
  if (!priority_signals_overrides) {
    priority_signals_overrides.emplace();
  }
  for (const auto& pair : update_data) {
    if (!pair.second.has_value()) {
      priority_signals_overrides->erase(pair.first);
      continue;
    }
    priority_signals_overrides->insert_or_assign(pair.first, *pair.second);
  }
}

// Same as above, but takes a map with PrioritySignalsDoublePtrs instead of
// std::optional<double>s. This isn't much more code than it takes to convert
// the flat_map of PrioritySignalsDoublePtr to one of optionals, so just
// duplicate the logic.
void MergePrioritySignalsOverrides(
    const base::flat_map<std::string,
                         auction_worklet::mojom::PrioritySignalsDoublePtr>&
        update_data,
    std::optional<base::flat_map<std::string, double>>&
        priority_signals_overrides) {
  if (!priority_signals_overrides) {
    priority_signals_overrides.emplace();
  }
  for (const auto& pair : update_data) {
    if (!pair.second) {
      priority_signals_overrides->erase(pair.first);
      continue;
    }
    priority_signals_overrides->insert_or_assign(pair.first,
                                                 pair.second->value);
  }
}

// These values are persisted to the database. Do not change.
enum KAnonKeyType {
  kAdBid = 0,
  kAdNameReporting = 1,
  kComponentBid = 2,
  kUnknown = 3
};

KAnonKeyType GetKAnonType(std::string_view unhashed_key) {
  if (unhashed_key.starts_with(blink::kKAnonKeyForAdBidPrefix)) {
    return KAnonKeyType::kAdBid;
  } else if (unhashed_key.starts_with(
                 blink::kKAnonKeyForAdComponentBidPrefix)) {
    return KAnonKeyType::kComponentBid;
  }
  return KAnonKeyType::kAdNameReporting;
}

// Adds indices to the `interest_group` table.
// Call this function after the table has been created,
// both when creating a new database in CreateCurrentSchema
// and after dropping/recreating the `interest_groups` table
// in the *latest* UpgradeVxxSchemaToVxx function to do so.
bool CreateInterestGroupIndices(sql::Database& db) {
  // Index on group expiration. Owner and Name are only here to speed up
  // queries that don't need the full group.
  DCHECK(!db.DoesIndexExist("interest_group_expiration"));
  static const char kInterestGroupExpirationIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_expiration"
      " ON interest_groups(expiration DESC, owner, name)";
  // clang-format on
  if (!db.Execute(kInterestGroupExpirationIndexSql)) {
    return false;
  }

  // Index on group expiration by owner.
  DCHECK(!db.DoesIndexExist("interest_group_owner"));
  static const char kInterestGroupOwnerIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_owner"
      " ON interest_groups(owner,expiration DESC,next_update_after ASC,name)";
  // clang-format on
  if (!db.Execute(kInterestGroupOwnerIndexSql)) {
    return false;
  }

  // Index on group expiration by owner and IG type (regular vs negative)
  DCHECK(!db.DoesIndexExist("interest_group_owner_and_type"));
  static const char kInterestGroupOwnerAndTypeIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_owner_and_type"
      " ON interest_groups("
      "LENGTH(additional_bid_key) == 0,owner,expiration DESC,name)";
  // clang-format on
  if (!db.Execute(kInterestGroupOwnerAndTypeIndexSql)) {
    return false;
  }

  // Index on storage size. Used for ClearExcessiveStorage().
  DCHECK(!db.DoesIndexExist("interest_group_owner_size"));
  static const char kInterestGroupOwnerSizeIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_owner_size"
      " ON interest_groups(owner,expiration DESC,storage_size)";
  // clang-format on
  if (!db.Execute(kInterestGroupOwnerSizeIndexSql)) {
    return false;
  }

  // Index on group expiration by joining origin. Owner and Name are only here
  // to speed up queries that don't need the full group.
  DCHECK(!db.DoesIndexExist("interest_group_joining_origin"));
  static const char kInterestGroupJoiningOriginIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_joining_origin"
      " ON interest_groups(joining_origin, expiration DESC, owner, name)";
  // clang-format on
  if (!db.Execute(kInterestGroupJoiningOriginIndexSql)) {
    return false;
  }

  return true;
}

// Adds indices to the `kanon` table.
// Call this function after the table has been created,
// both when creating a new database in CreateCurrentSchema
// and after dropping/recreating the `kanon` table
// in the *latest* UpgradeVxxSchemaToVxx function to do so.
bool CreateKAnonIndices(sql::Database& db) {
  DCHECK(!db.DoesIndexExist("k_anon_last_server_time_idx"));
  static const char kCreateKAnonServerTimeIndexSQL[] =
      // clang-format off
    "CREATE INDEX k_anon_last_server_time_idx "
    "ON joined_k_anon(last_reported_to_anon_server_time DESC);";
  // clang-format on
  return db.Execute(kCreateKAnonServerTimeIndexSQL);
}

bool MaybeCreateKAnonEntryForV17DatabaseUpgrade(
    sql::Database& db,
    const blink::InterestGroupKey& interest_group_key,
    const std::string& key,
    const base::Time& now) {
  base::Time distant_past = base::Time::Min();
  base::Time last_referenced_time = now;

  sql::Statement get_previous_kanon_val(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT is_k_anon,"
                            "last_k_anon_updated_time,"
                            "last_reported_to_anon_server_time "
                            "FROM k_anon WHERE key = ? "
                            "LIMIT 1"));
  // We can get any previously added row for a k_anon key because the same data
  // is duplicated for each row of the same key.
  if (!get_previous_kanon_val.is_valid()) {
    return false;
  }

  get_previous_kanon_val.BindString(0, key);

  bool is_kanon = false;
  base::Time last_k_anon_updated_time = distant_past;
  base::Time last_reported_to_anon_server_time = distant_past;
  if (get_previous_kanon_val.Step()) {
    is_kanon = get_previous_kanon_val.ColumnBool(0);
    last_k_anon_updated_time = get_previous_kanon_val.ColumnTime(1);
    last_reported_to_anon_server_time = get_previous_kanon_val.ColumnTime(2);
  }

  // clang-format off
  const char insert_k_anon_str[] =
      "INSERT OR REPLACE INTO k_anon_new("
              "last_referenced_time,"
              "key,"
              "owner,"
              "name,"
              "is_k_anon,"
              "last_k_anon_updated_time,"
              "last_reported_to_anon_server_time) "
            "VALUES(?,?,?,?,?,?,?)";
  // clang-format on

  sql::Statement maybe_insert_kanon(
      db.GetCachedStatement(SQL_FROM_HERE, insert_k_anon_str));

  if (!maybe_insert_kanon.is_valid()) {
    return false;
  }

  maybe_insert_kanon.Reset(true);
  maybe_insert_kanon.BindTime(0, last_referenced_time);
  maybe_insert_kanon.BindString(1, key);
  maybe_insert_kanon.BindString(2, Serialize(interest_group_key.owner));
  maybe_insert_kanon.BindString(3, interest_group_key.name);
  maybe_insert_kanon.BindBool(4, is_kanon);
  maybe_insert_kanon.BindTime(5, last_k_anon_updated_time);
  maybe_insert_kanon.BindTime(6, last_reported_to_anon_server_time);
  return maybe_insert_kanon.Run();
}

// Initializes the tables, returning true on success.
// The tables cannot exist when calling this function.
bool CreateCurrentSchema(sql::Database& db) {
  DCHECK(!db.DoesTableExist("interest_groups"));
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        // Most recent time the interest group was joined. Called
        // `exact_join_time` to differentiate it from `join_time` in the join
        // history table, which is a day, and isn't looked up on InterestGroup
        // load.
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "trusted_bidding_signals_slot_size_mode INTEGER NOT NULL,"
        "max_trusted_bidding_signals_url_length INTEGER NOT NULL,"
        "trusted_bidding_signals_coordinator TEXT,"
        "view_and_click_counts_providers TEXT,"
        "user_bidding_signals TEXT,"
        "ads_pb BLOB NOT NULL,"
        "ad_components_pb BLOB NOT NULL,"
        "ad_sizes TEXT NOT NULL,"
        "size_groups TEXT NOT NULL,"
        "auction_server_request_flags INTEGER NOT NULL,"
        "additional_bid_key BLOB NOT NULL,"
        "aggregation_coordinator_origin TEXT,"
        "storage_size INTEGER NOT NULL,"
        "last_k_anon_updated_time INTEGER NOT NULL,"
        "kanon_keys BLOB NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  if (!CreateInterestGroupIndices(db)) {
    return false;
  }

  DCHECK(!db.DoesTableExist("k_anon"));
  static const char kCreateInterestGroupKAnonTableSql[] =
      "CREATE TABLE joined_k_anon("
      "hashed_key BLOB NOT NULL,"
      "last_reported_to_anon_server_time INTEGER NOT NULL,"
      "PRIMARY KEY(hashed_key))";
  // clang-format on
  if (!db.Execute(kCreateInterestGroupKAnonTableSql)) {
    return false;
  }

  if (!CreateKAnonIndices(db)) {
    return false;
  }

  DCHECK(!db.DoesTableExist("join_history"));
  static const char kJoinHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE join_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "join_time INTEGER NOT NULL,"
        "count INTEGER NOT NULL,"
      "PRIMARY KEY(owner, name, join_time) "
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kJoinHistoryTableSql)) {
    return false;
  }

  DCHECK(!db.DoesTableExist("bid_history"));
  static const char kBidHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE bid_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "bid_time INTEGER NOT NULL,"
        "count INTEGER NOT NULL,"
      "PRIMARY KEY(owner, name, bid_time) "
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kBidHistoryTableSql)) {
    return false;
  }

  // We can't use the interest group and win time as primary keys since
  // auctions on separate pages may occur at the same time.
  DCHECK(!db.DoesTableExist("win_history"));
  static const char kWinHistoryTableSQL[] =
      // clang-format off
      "CREATE TABLE win_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "win_time INTEGER NOT NULL,"
        "ad TEXT NOT NULL,"
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kWinHistoryTableSQL)) {
    return false;
  }

  DCHECK(!db.DoesIndexExist("win_history_index"));
  static const char kWinHistoryIndexSQL[] =
      // clang-format off
      "CREATE INDEX win_history_index "
      "ON win_history(owner,name,win_time DESC)";
  // clang-format on
  if (!db.Execute(kWinHistoryIndexSQL)) {
    return false;
  }

  DCHECK(!db.DoesTableExist("lockout_debugging_only_report"));
  static const char kLockoutDebugReportTableSql[] =
      // clang-format off
      "CREATE TABLE lockout_debugging_only_report("
        "id INTEGER NOT NULL,"
        "starting_time INTEGER NOT NULL,"
        "duration INTEGER NOT NULL,"
      "PRIMARY KEY(id))";
  // clang-format on
  if (!db.Execute(kLockoutDebugReportTableSql)) {
    return false;
  }

  DCHECK(!db.DoesTableExist("cooldown_debugging_only_report"));
  // The type field stores an enum mapped to the real TimeDelta cooldown
  // duration defined by finch, so that we can easily control duration periods
  // through finch without needing to update the database.
  static const char kCooldownDebugReportTableSql[] =
      // clang-format off
      "CREATE TABLE cooldown_debugging_only_report("
        "origin TEXT NOT NULL,"
        "starting_time INTEGER NOT NULL,"
        "type INTEGER NOT NULL,"
      "PRIMARY KEY(origin))";
  // clang-format on
  if (!db.Execute(kCooldownDebugReportTableSql)) {
    return false;
  }

  DCHECK(!db.DoesTableExist("bidding_and_auction_server_keys"));
  static const char kBAKeysTableSql[] =
      // clang-format off
      "CREATE TABLE bidding_and_auction_server_keys("
        "coordinator TEXT NOT NULL,"
        "keys BLOB NOT NULL,"
        "expiration INTEGER NOT NULL,"
      "PRIMARY KEY(coordinator))";
  // clang-format on
  if (!db.Execute(kBAKeysTableSql)) {
    return false;
  }

  DCHECK(!db.DoesTableExist("view_and_click_events"));
  static const char kViewAndClickEventsSql[] =
      // clang-format off
      "CREATE TABLE view_and_click_events("
        "provider_origin TEXT NOT NULL,"
        "eligible_origin TEXT NOT NULL,"
        "uncompacted_view_events BLOB NOT NULL,"
        "compacted_view_events BLOB NOT NULL,"
        "uncompacted_click_events BLOB NOT NULL,"
        "compacted_click_events BLOB NOT NULL,"
      "PRIMARY KEY(provider_origin, eligible_origin))";
  // clang-format on
  if (!db.Execute(kViewAndClickEventsSql)) {
    DLOG(ERROR)
        << "view_and_click_events CREATE SQL statement did not compile: "
        << db.GetErrorMessage();
    return false;
  }

  DCHECK(!db.DoesTableExist("cached_k_anonymity_hashes"));
  static const char kCachedKAnonymityHashesTableSql[] =
      // clang-format off
      "CREATE TABLE cached_k_anonymity_hashes("
        "key_hash BLOB NOT NULL,"
        "is_k_anon INTEGER NOT NULL,"
        "fetch_time INTEGER NOT NULL,"
      "PRIMARY KEY(key_hash))";
  // clang-format on
  if (!db.Execute(kCachedKAnonymityHashesTableSql)) {
    return false;
  }

  // Index on fetch_time, needed to delete keys when they expire.
  DCHECK(!db.DoesIndexExist("cached_k_anonymity_hashes_expiration_index"));
  static const char kCachedKAnonymityHashesExpirationIndexSql[] =
      // clang-format off
      "CREATE INDEX cached_k_anonymity_hashes_expiration_index"
      " ON cached_k_anonymity_hashes(fetch_time)";
  // clang-format on
  if (!db.Execute(kCachedKAnonymityHashesExpirationIndexSql)) {
    return false;
  }

  return true;
}

bool VacuumDB(sql::Database& db) {
  static const char kVacuum[] = "VACUUM";
  return db.Execute(kVacuum);
}

bool UpgradeV34SchemaToV35(sql::Database& db, sql::MetaTable& meta_table) {
  // Make a new table, `cached_k_anonymity_hashes`.
  DCHECK(!db.DoesTableExist("cached_k_anonymity_hashes"));
  static const char kCachedKAnonymityHashesTableSql[] =
      // clang-format off
      "CREATE TABLE cached_k_anonymity_hashes("
        "key_hash BLOB NOT NULL,"
        "is_k_anon INTEGER NOT NULL,"
        "fetch_time INTEGER NOT NULL,"
      "PRIMARY KEY(key_hash))";
  // clang-format on
  if (!db.Execute(kCachedKAnonymityHashesTableSql)) {
    DLOG(ERROR) << "cached_k_anonymity_hashes upgrade CREATE SQL "
                   "statement did not compile: "
                << db.GetErrorMessage();
    return false;
  }

  // Index on fetch_time, needed to delete keys when they expire.
  DCHECK(!db.DoesIndexExist("cached_k_anonymity_hashes_expiration_index"));
  static const char kCachedKAnonymityHashesExpirationIndexSql[] =
      // clang-format off
      "CREATE INDEX cached_k_anonymity_hashes_expiration_index"
      " ON cached_k_anonymity_hashes(fetch_time)";
  // clang-format on
  if (!db.Execute(kCachedKAnonymityHashesExpirationIndexSql)) {
    return false;
  }

  return true;
}

bool UpgradeV33SchemaToV34(sql::Database& db, sql::MetaTable& meta_table) {
  // Make `view_and_click_events` table.
  DCHECK(!db.DoesTableExist("view_and_click_events"));
  static const char kViewAndClickEventsSql[] =
      // clang-format off
      "CREATE TABLE view_and_click_events("
        "provider_origin TEXT NOT NULL,"
        "eligible_origin TEXT NOT NULL,"
        "uncompacted_view_events BLOB NOT NULL,"
        "compacted_view_events BLOB NOT NULL,"
        "uncompacted_click_events BLOB NOT NULL,"
        "compacted_click_events BLOB NOT NULL,"
      "PRIMARY KEY(provider_origin, eligible_origin))";
  // clang-format on
  if (!db.Execute(kViewAndClickEventsSql)) {
    DLOG(ERROR) << "view_and_click_counts_providers upgrade CREATE SQL "
                   "statement did not compile: "
                << db.GetErrorMessage();
    return false;
  }

  return true;
}

bool UpgradeV32SchemaToV33(sql::Database& db, sql::MetaTable& meta_table) {
  // Make a table with new column `view_and_click_counts_providers`.
  static const char kInterestGroupTableSql[] =
      // clang-format off
    "CREATE TABLE new_interest_groups("
    "expiration INTEGER NOT NULL,"
    "last_updated INTEGER NOT NULL,"
    "next_update_after INTEGER NOT NULL,"
    "owner TEXT NOT NULL,"
    "joining_origin TEXT NOT NULL,"
    "exact_join_time INTEGER NOT NULL,"
    "name TEXT NOT NULL,"
    "priority DOUBLE NOT NULL,"
    "enable_bidding_signals_prioritization INTEGER NOT NULL,"
    "priority_vector TEXT NOT NULL,"
    "priority_signals_overrides TEXT NOT NULL,"
    "seller_capabilities TEXT NOT NULL,"
    "all_sellers_capabilities INTEGER NOT NULL,"
    "execution_mode INTEGER NOT NULL,"
    "joining_url TEXT NOT NULL,"
    "bidding_url TEXT NOT NULL,"
    "bidding_wasm_helper_url TEXT NOT NULL,"
    "update_url TEXT NOT NULL,"
    "trusted_bidding_signals_url TEXT NOT NULL,"
    "trusted_bidding_signals_keys TEXT NOT NULL,"
    "trusted_bidding_signals_slot_size_mode INTEGER NOT NULL,"
    "max_trusted_bidding_signals_url_length INTEGER NOT NULL,"
    "trusted_bidding_signals_coordinator TEXT,"
    "view_and_click_counts_providers TEXT,"
    "user_bidding_signals TEXT,"
    "ads_pb BLOB NOT NULL,"
    "ad_components_pb BLOB NOT NULL,"
    "ad_sizes TEXT NOT NULL,"
    "size_groups TEXT NOT NULL,"
    "auction_server_request_flags INTEGER NOT NULL,"
    "additional_bid_key BLOB NOT NULL,"
    "aggregation_coordinator_origin TEXT,"
    "storage_size INTEGER NOT NULL,"
    "last_k_anon_updated_time INTEGER NOT NULL, "
    "kanon_keys BLOB NOT NULL,"
    "PRIMARY KEY(owner,name))";

  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
      "last_updated,"
      "next_update_after,"
      "owner,"
      "joining_origin,"
      "exact_join_time,"
      "name,"
      "priority,"
      "enable_bidding_signals_prioritization,"
      "priority_vector,"
      "priority_signals_overrides,"
      "seller_capabilities,"
      "all_sellers_capabilities,"
      "execution_mode,"
      "joining_url,"
      "bidding_url,"
      "bidding_wasm_helper_url,"
      "update_url,"
      "trusted_bidding_signals_url,"
      "trusted_bidding_signals_keys,"
      "trusted_bidding_signals_slot_size_mode,"
      "max_trusted_bidding_signals_url_length,"
      "trusted_bidding_signals_coordinator,"
      "NULL,"  // view_and_click_counts_providers
      "user_bidding_signals,"
      "ads_pb,"
      "ad_components_pb,"
      "ad_sizes,"
      "size_groups,"
      "auction_server_request_flags,"
      "additional_bid_key,"
      "aggregation_coordinator_origin,"
      "storage_size,"
      "last_k_anon_updated_time,"
      "kanon_keys "
      "FROM interest_groups";
  // clang-format on

  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
    "ALTER TABLE new_interest_groups "
    "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return CreateInterestGroupIndices(db);
}

bool UpgradeV31SchemaToV32(sql::Database& db, sql::MetaTable& meta_table) {
  // Adds duration column to the debug report lockout table, and rename its
  // last_report_sent_time column to starting_time.
  static const char kLockoutTableSql[] =
      // clang-format off
    "CREATE TABLE new_lockout_debugging_only_report("
        "id INTEGER NOT NULL,"
        "starting_time INTEGER NOT NULL,"
        "duration INTEGER NOT NULL,"
      "PRIMARY KEY(id))";
  // clang-format on
  if (!db.Execute(kLockoutTableSql)) {
    return false;
  }

  // Copy over the existing columns, and set the new duration column's value
  // which was always kFledgeDebugReportLockout before.
  // clang-format off
  sql::Statement copy_lockout_table_sql(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO new_lockout_debugging_only_report "
      "SELECT id,"
      "last_report_sent_time,"
      "? "
      "FROM lockout_debugging_only_report"));
  // clang-format on

  copy_lockout_table_sql.BindTimeDelta(
      0, blink::features::kFledgeDebugReportLockout.Get());

  if (!copy_lockout_table_sql.Run()) {
    return false;
  }

  static const char kDropLockoutTableSql[] =
      "DROP TABLE lockout_debugging_only_report";
  if (!db.Execute(kDropLockoutTableSql)) {
    return false;
  }

  static const char kRenameLockoutTableSql[] =
      // clang-format off
    "ALTER TABLE new_lockout_debugging_only_report "
    "RENAME TO lockout_debugging_only_report";
  // clang-format on
  return db.Execute(kRenameLockoutTableSql);
}

bool UpgradeV29SchemaToV30(sql::Database& db, sql::MetaTable& meta_table) {
  // There are no new columns, but the `ads_pb` and `ad_components_pb` columns
  // get compressed with Snappy.

  // clang-format off
  sql::Statement select_prev_groups(
      db.GetCachedStatement(SQL_FROM_HERE,
      "SELECT owner,"
      "name,"
      "ads_pb,"
      "ad_components_pb "
      "FROM interest_groups"));
  // clang-format on
  if (!select_prev_groups.is_valid()) {
    return false;
  }

  // clang-format off
  sql::Statement update_group(db.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE interest_groups "
      "SET ads_pb=?,"
        "ad_components_pb=? "
      "WHERE owner=? AND name=?"));
  // clang-format on
  if (!update_group.is_valid()) {
    return false;
  }

  while (select_prev_groups.Step()) {
    update_group.Reset(/*clear_bound_vars=*/true);

    // Update the `ads_pb` and `ad_components_pb` columns with their contents
    // compressed with Snappy.
    std::string compressed_ads_pb;
    base::span<const uint8_t> ads_pb = select_prev_groups.ColumnBlob(2);
    base::TimeTicks start_ads = base::TimeTicks::Now();
    snappy::Compress(reinterpret_cast<const char*>(ads_pb.data()),
                     ads_pb.size(), &compressed_ads_pb);
    UMA_HISTOGRAM_TIMES("Storage.InterestGroup.AdProtoCompressionTime",
                        base::TimeTicks::Now() - start_ads);
    update_group.BindBlob(0, base::as_byte_span(compressed_ads_pb));
    if (ads_pb.size() > 0u) {
      base::UmaHistogramPercentage(
          "Storage.InterestGroup.AdProtoCompressionRatio",
          compressed_ads_pb.size() * 100 / ads_pb.size());
    }
    UMA_HISTOGRAM_COUNTS_1M("Storage.InterestGroup.AdProtoSizeUncompressed",
                            ads_pb.size());
    UMA_HISTOGRAM_COUNTS_1M("Storage.InterestGroup.AdProtoSizeCompressed",
                            compressed_ads_pb.size());

    std::string compressed_ad_components_pb;
    base::span<const uint8_t> ad_components_pb =
        select_prev_groups.ColumnBlob(3);
    base::TimeTicks start_ad_components = base::TimeTicks::Now();
    snappy::Compress(reinterpret_cast<const char*>(ad_components_pb.data()),
                     ad_components_pb.size(), &compressed_ad_components_pb);
    UMA_HISTOGRAM_TIMES("Storage.InterestGroup.AdProtoCompressionTime",
                        base::TimeTicks::Now() - start_ad_components);
    update_group.BindBlob(1, base::as_byte_span(compressed_ad_components_pb));
    if (ad_components_pb.size() > 0u) {
      base::UmaHistogramPercentage(
          "Storage.InterestGroup.AdProtoCompressionRatio",
          compressed_ad_components_pb.size() * 100 / ad_components_pb.size());
    }
    UMA_HISTOGRAM_COUNTS_1M("Storage.InterestGroup.AdProtoSizeUncompressed",
                            ad_components_pb.size());
    UMA_HISTOGRAM_COUNTS_1M("Storage.InterestGroup.AdProtoSizeCompressed",
                            compressed_ad_components_pb.size());

    update_group.BindString(2, select_prev_groups.ColumnStringView(0));
    update_group.BindString(3, select_prev_groups.ColumnStringView(1));

    if (!update_group.Run()) {
      return false;
    }
  }
  if (!select_prev_groups.Succeeded()) {
    return false;
  }

  return true;
}

bool UpgradeV27SchemaToV28(sql::Database& db, sql::MetaTable& meta_table) {
  // Make a table with new columns `trusted_bidding_signals_protocol_version`
  // and `trusted_bidding_signals_coordinator.`
  static const char kInterestGroupTableSql[] =
      // clang-format off
    "CREATE TABLE new_interest_groups("
    "expiration INTEGER NOT NULL,"
    "last_updated INTEGER NOT NULL,"
    "next_update_after INTEGER NOT NULL,"
    "owner TEXT NOT NULL,"
    "joining_origin TEXT NOT NULL,"
    "exact_join_time INTEGER NOT NULL,"
    "name TEXT NOT NULL,"
    "priority DOUBLE NOT NULL,"
    "enable_bidding_signals_prioritization INTEGER NOT NULL,"
    "priority_vector TEXT NOT NULL,"
    "priority_signals_overrides TEXT NOT NULL,"
    "seller_capabilities TEXT NOT NULL,"
    "all_sellers_capabilities INTEGER NOT NULL,"
    "execution_mode INTEGER NOT NULL,"
    "joining_url TEXT NOT NULL,"
    "bidding_url TEXT NOT NULL,"
    "bidding_wasm_helper_url TEXT NOT NULL,"
    "update_url TEXT NOT NULL,"
    "trusted_bidding_signals_url TEXT NOT NULL,"
    "trusted_bidding_signals_keys TEXT NOT NULL,"
    "trusted_bidding_signals_slot_size_mode INTEGER NOT NULL,"
    "max_trusted_bidding_signals_url_length INTEGER NOT NULL,"
    "trusted_bidding_signals_coordinator TEXT,"
    "user_bidding_signals TEXT,"
    "ads_pb BLOB NOT NULL,"
    "ad_components_pb BLOB NOT NULL,"
    "ad_sizes TEXT NOT NULL,"
    "size_groups TEXT NOT NULL,"
    "auction_server_request_flags INTEGER NOT NULL,"
    "additional_bid_key BLOB NOT NULL,"
    "aggregation_coordinator_origin TEXT,"
    "storage_size INTEGER NOT NULL,"
    "last_k_anon_updated_time INTEGER NOT NULL, "
    "kanon_keys BLOB NOT NULL,"
    "PRIMARY KEY(owner,name))";

  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
      "last_updated,"
      "next_update_after,"
      "owner,"
      "joining_origin,"
      "exact_join_time,"
      "name,"
      "priority,"
      "enable_bidding_signals_prioritization,"
      "priority_vector,"
      "priority_signals_overrides,"
      "seller_capabilities,"
      "all_sellers_capabilities,"
      "execution_mode,"
      "joining_url,"
      "bidding_url,"
      "bidding_wasm_helper_url,"
      "update_url,"
      "trusted_bidding_signals_url,"
      "trusted_bidding_signals_keys,"
      "trusted_bidding_signals_slot_size_mode,"
      "max_trusted_bidding_signals_url_length,"
      "NULL," // trusted_bidding_signals_coordinator
      "user_bidding_signals,"
      "ads_pb,"
      "ad_components_pb,"
      "ad_sizes,"
      "size_groups,"
      "auction_server_request_flags,"
      "additional_bid_key,"
      "aggregation_coordinator_origin,"
      "storage_size,"
      "last_k_anon_updated_time,"
      "kanon_keys "
      "FROM interest_groups";
  // clang-format on

  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
    "ALTER TABLE new_interest_groups "
    "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return CreateInterestGroupIndices(db);
}

bool UpgradeV26SchemaToV27(sql::Database& db, sql::MetaTable& meta_table) {
  // Make a table with new columns `last_k_anon_updated_time` and `kanon_keys`.
  static const char kInterestGroupTableSql[] =
      // clang-format off
    "CREATE TABLE new_interest_groups("
    "expiration INTEGER NOT NULL,"
    "last_updated INTEGER NOT NULL,"
    "next_update_after INTEGER NOT NULL,"
    "owner TEXT NOT NULL,"
    "joining_origin TEXT NOT NULL,"
    "exact_join_time INTEGER NOT NULL,"
    "name TEXT NOT NULL,"
    "priority DOUBLE NOT NULL,"
    "enable_bidding_signals_prioritization INTEGER NOT NULL,"
    "priority_vector TEXT NOT NULL,"
    "priority_signals_overrides TEXT NOT NULL,"
    "seller_capabilities TEXT NOT NULL,"
    "all_sellers_capabilities INTEGER NOT NULL,"
    "execution_mode INTEGER NOT NULL,"
    "joining_url TEXT NOT NULL,"
    "bidding_url TEXT NOT NULL,"
    "bidding_wasm_helper_url TEXT NOT NULL,"
    "update_url TEXT NOT NULL,"
    "trusted_bidding_signals_url TEXT NOT NULL,"
    "trusted_bidding_signals_keys TEXT NOT NULL,"
    "trusted_bidding_signals_slot_size_mode INTEGER NOT NULL,"
    "max_trusted_bidding_signals_url_length INTEGER NOT NULL,"
    "user_bidding_signals TEXT,"
    "ads_pb BLOB NOT NULL,"
    "ad_components_pb BLOB NOT NULL,"
    "ad_sizes TEXT NOT NULL,"
    "size_groups TEXT NOT NULL,"
    "auction_server_request_flags INTEGER NOT NULL,"
    "additional_bid_key BLOB NOT NULL,"
    "aggregation_coordinator_origin TEXT,"
    "storage_size INTEGER NOT NULL,"
    "last_k_anon_updated_time INTEGER NOT NULL, "
    "kanon_keys BLOB NOT NULL,"
    "PRIMARY KEY(owner,name))";

  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  // Copy over the values from the old `interest_groups` before
  // populating the new columns.
  sql::Statement copy_interest_groups(db.GetCachedStatement(
      SQL_FROM_HERE,
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
      "last_updated,"
      "next_update_after,"
      "owner,"
      "joining_origin,"
      "exact_join_time,"
      "name,"
      "priority,"
      "enable_bidding_signals_prioritization,"
      "priority_vector,"
      "priority_signals_overrides,"
      "seller_capabilities,"
      "all_sellers_capabilities,"
      "execution_mode,"
      "joining_url,"
      "bidding_url,"
      "bidding_wasm_helper_url,"
      "update_url,"
      "trusted_bidding_signals_url,"
      "trusted_bidding_signals_keys,"
      "trusted_bidding_signals_slot_size_mode,"
      "max_trusted_bidding_signals_url_length,"
      "user_bidding_signals,"
      "ads_pb,"
      "ad_components_pb,"
      "ad_sizes,"
      "size_groups,"
      "auction_server_request_flags,"
      "additional_bid_key,"
      "aggregation_coordinator_origin,"
      "storage_size,"
      "?," // last_k_anon_updated_time
      "? " // kanon_keys
      "FROM interest_groups"
      // clang-format on
      ));

  copy_interest_groups.BindTime(0, base::Time::Min());
  std::string kanon_key_protos_str;
  if (KAnonKeyProtos().SerializeToString(&kanon_key_protos_str)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoSerializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  copy_interest_groups.BindBlob(1, kanon_key_protos_str);

  if (!copy_interest_groups.Run()) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
    "ALTER TABLE new_interest_groups "
    "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  if (!CreateInterestGroupIndices(db)) {
    return false;
  }

  static const char kMoveKAnonTimes[] =
      // clang-format off
    "UPDATE interest_groups "
    "SET last_k_anon_updated_time = k.last_k_anon_updated_time "
    "FROM "
      "(SELECT owner, name, MIN(last_k_anon_updated_time) "
      "AS last_k_anon_updated_time FROM k_anon GROUP BY owner, name) "
    "AS k "
    "WHERE interest_groups.owner = k.owner AND interest_groups.name = k.name";
  // clang-format on
  if (!db.Execute(kMoveKAnonTimes)) {
    return false;
  }

  // Copy over positive keys to the interest groups table.
  sql::Statement get_positive_keys(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT owner, name, hashed_key "
                            "FROM k_anon WHERE is_k_anon > 0"));
  std::map<std::pair<std::string, std::string>, KAnonKeyProtos> positive_keys;
  while (get_positive_keys.Step()) {
    std::string owner = get_positive_keys.ColumnString(0);
    std::string name = get_positive_keys.ColumnString(1);
    std::string hashed_key = get_positive_keys.ColumnString(2);
    positive_keys[std::make_pair(std::move(owner), std::move(name))].add_keys(
        std::move(hashed_key));
  }
  sql::Statement move_positive_kanon_keys(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "UPDATE interest_groups "
                            "SET kanon_keys = ? "
                            "WHERE owner = ? and name = ?"));

  for (auto [owner_name_pair, keys] : positive_keys) {
    move_positive_kanon_keys.Reset(true);
    std::string keys_str;
    if (keys.SerializeToString(&keys_str)) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
          InterestGroupStorageProtoSerializationResult::kSucceeded);
    } else {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
          InterestGroupStorageProtoSerializationResult::kFailed);
      // TODO(crbug.com/355010821): Consider bubbling out the failure.
    }
    move_positive_kanon_keys.BindBlob(0, keys_str);
    move_positive_kanon_keys.BindString(1, owner_name_pair.first);
    move_positive_kanon_keys.BindString(2, owner_name_pair.second);
    if (!move_positive_kanon_keys.Run()) {
      return false;
    }
  }

  // Make a new `joined_k_anon` table and copy over the last reported
  // times.
  static const char kCreateInterestGroupKAnonTableSql[] =
      "CREATE TABLE joined_k_anon("
      "hashed_key BLOB NOT NULL,"
      "last_reported_to_anon_server_time INTEGER NOT NULL,"
      "PRIMARY KEY(hashed_key))";
  // clang-format on
  if (!db.Execute(kCreateInterestGroupKAnonTableSql)) {
    return false;
  }

  // The old schema expected all rows with the same hashed_key to have the
  // same last_reported_to_anon_server_time so we can use any
  // last_reported_to_anon_server_time from any row.
  static const char kMoveKanonLastReportedTimes[] =
      // clang-format off
    "INSERT INTO joined_k_anon "
      "SELECT hashed_key, MIN(last_reported_to_anon_server_time) "
      "FROM k_anon "
      "GROUP BY hashed_key";
  // clang-format on
  if (!db.Execute(kMoveKanonLastReportedTimes)) {
    return false;
  }

  // Drop the old k-anon table and make new indices.
  static const char kDropKAnonTableSql[] = "DROP TABLE k_anon";
  if (!db.Execute(kDropKAnonTableSql)) {
    return false;
  }

  return CreateKAnonIndices(db);
}

bool UpgradeV24SchemaToV25(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kCreateKAnonTableSql[] =
      "CREATE TABLE k_anon_new("
      "last_referenced_time INTEGER NOT NULL,"
      "hashed_key BLOB NOT NULL,"
      "owner TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "is_k_anon INTEGER NOT NULL,"
      "key_type INTEGER NOT NULL,"
      "last_k_anon_updated_time INTEGER NOT NULL,"
      "last_reported_to_anon_server_time INTEGER NOT NULL,"
      "PRIMARY KEY(owner,name,hashed_key))";
  if (!db.Execute(kCreateKAnonTableSql)) {
    return false;
  }

  sql::Statement select_previous_k_anon_values(db.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT last_referenced_time, owner, name, "
      "is_k_anon, last_k_anon_updated_time, "
      "last_reported_to_anon_server_time, key FROM k_anon"));
  if (!select_previous_k_anon_values.is_valid()) {
    return false;
  }

  // clang-format off
  sql::Statement insert_entry(
    db.GetCachedStatement(SQL_FROM_HERE,
    "INSERT INTO k_anon_new("
            "last_referenced_time,"
            "owner,"
            "name,"
            "is_k_anon,"
            "last_k_anon_updated_time,"
            "last_reported_to_anon_server_time,"
            "hashed_key,"
            "key_type) "
          "VALUES(?,?,?,?,?,?,?,?)"));
  // clang-format on
  if (!insert_entry.is_valid()) {
    return false;
  }

  while (select_previous_k_anon_values.Step()) {
    insert_entry.Reset(true);

    // Copy over the existing columns.
    insert_entry.BindTime(0, select_previous_k_anon_values.ColumnTime(0));
    insert_entry.BindString(1,
                            select_previous_k_anon_values.ColumnStringView(1));
    insert_entry.BindString(2,
                            select_previous_k_anon_values.ColumnStringView(2));
    insert_entry.BindBool(3, select_previous_k_anon_values.ColumnBool(3));
    insert_entry.BindTime(4, select_previous_k_anon_values.ColumnTime(4));
    insert_entry.BindTime(5, select_previous_k_anon_values.ColumnTime(5));

    // Create the new columns.
    std::string_view unhashed_key =
        select_previous_k_anon_values.ColumnStringView(6);
    insert_entry.BindBlob(6, crypto::SHA256HashString(unhashed_key));
    insert_entry.BindInt(7, GetKAnonType(unhashed_key));

    if (!insert_entry.Run()) {
      return false;
    }
  }
  if (!select_previous_k_anon_values.Succeeded()) {
    return false;
  }

  static const char kDropKAnonTableSql[] = "DROP TABLE k_anon";
  if (!db.Execute(kDropKAnonTableSql)) {
    return false;
  }

  static const char kRenameKAnonTableSql[] =
      // clang-format off
      "ALTER TABLE k_anon_new "
      "RENAME TO k_anon";
  // clang-format on
  return db.Execute(kRenameKAnonTableSql);
}

bool UpgradeV23SchemaToV24(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kBAKeysTableSql[] =
      // clang-format off
    "CREATE TABLE bidding_and_auction_server_keys("
      "coordinator TEXT NOT NULL,"
      "keys BLOB NOT NULL,"
      "expiration INTEGER NOT NULL,"
    "PRIMARY KEY(coordinator))";
  // clang-format on
  return db.Execute(kBAKeysTableSql);
}

bool UpgradeV22SchemaToV23(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
    "CREATE TABLE new_interest_groups("
    "expiration INTEGER NOT NULL,"
    "last_updated INTEGER NOT NULL,"
    "next_update_after INTEGER NOT NULL,"
    "owner TEXT NOT NULL,"
    "joining_origin TEXT NOT NULL,"
    "exact_join_time INTEGER NOT NULL,"
    "name TEXT NOT NULL,"
    "priority DOUBLE NOT NULL,"
    "enable_bidding_signals_prioritization INTEGER NOT NULL,"
    "priority_vector TEXT NOT NULL,"
    "priority_signals_overrides TEXT NOT NULL,"
    "seller_capabilities TEXT NOT NULL,"
    "all_sellers_capabilities INTEGER NOT NULL,"
    "execution_mode INTEGER NOT NULL,"
    "joining_url TEXT NOT NULL,"
    "bidding_url TEXT NOT NULL,"
    "bidding_wasm_helper_url TEXT NOT NULL,"
    "update_url TEXT NOT NULL,"
    "trusted_bidding_signals_url TEXT NOT NULL,"
    "trusted_bidding_signals_keys TEXT NOT NULL,"
    "trusted_bidding_signals_slot_size_mode INTEGER NOT NULL,"
    "max_trusted_bidding_signals_url_length INTEGER NOT NULL,"
    "user_bidding_signals TEXT,"
    "ads_pb BLOB NOT NULL,"
    "ad_components_pb BLOB NOT NULL,"
    "ad_sizes TEXT NOT NULL,"
    "size_groups TEXT NOT NULL,"
    "auction_server_request_flags INTEGER NOT NULL,"
    "additional_bid_key BLOB NOT NULL,"
    "aggregation_coordinator_origin TEXT,"
    "storage_size INTEGER NOT NULL, "
    "PRIMARY KEY(owner,name))";

  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
      "last_updated,"
      "next_update_after,"
      "owner,"
      "joining_origin,"
      "exact_join_time,"
      "name,"
      "priority,"
      "enable_bidding_signals_prioritization,"
      "priority_vector,"
      "priority_signals_overrides,"
      "seller_capabilities,"
      "all_sellers_capabilities,"
      "execution_mode,"
      "joining_url,"
      "bidding_url,"
      "bidding_wasm_helper_url,"
      "update_url,"
      "trusted_bidding_signals_url,"
      "trusted_bidding_signals_keys,"
      "trusted_bidding_signals_slot_size_mode,"
      "0," // max_trusted_bidding_signals_url_length
      "user_bidding_signals,"
      "ads_pb,"
      "ad_components_pb,"
      "ad_sizes,"
      "size_groups,"
      "auction_server_request_flags,"
      "additional_bid_key,"
      "aggregation_coordinator_origin,"
      "storage_size "
      "FROM interest_groups";
  // clang-format on

  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
    "ALTER TABLE new_interest_groups "
    "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return CreateInterestGroupIndices(db);
}

bool UpgradeV21SchemaToV22(sql::Database& db, sql::MetaTable& meta_table) {
  // The two changed tables had no data, so no need to copy data over.
  static const char kDropLockoutTableTableSql[] =
      "DROP TABLE lockout_debugging_only_report";
  if (!db.Execute(kDropLockoutTableTableSql)) {
    return false;
  }
  static const char kLockoutDebugReportTableSql[] =
      // clang-format off
      "CREATE TABLE lockout_debugging_only_report("
        "id INTEGER NOT NULL,"
        "last_report_sent_time INTEGER NOT NULL,"
      "PRIMARY KEY(id))";
  // clang-format on
  if (!db.Execute(kLockoutDebugReportTableSql)) {
    return false;
  }

  static const char kDropCooldownTableSql[] =
      "DROP TABLE cooldown_debugging_only_report";
  if (!db.Execute(kDropCooldownTableSql)) {
    return false;
  }
  static const char kCooldownDebugReportTableSql[] =
      // clang-format off
      "CREATE TABLE cooldown_debugging_only_report("
        "origin TEXT NOT NULL,"
        "starting_time INTEGER NOT NULL,"
        "type INTEGER NOT NULL,"
      "PRIMARY KEY(origin))";
  // clang-format on
  if (!db.Execute(kCooldownDebugReportTableSql)) {
    return false;
  }

  return true;
}

bool UpgradeV20SchemaToV21(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
    "CREATE TABLE new_interest_groups("
    "expiration INTEGER NOT NULL,"
    "last_updated INTEGER NOT NULL,"
    "next_update_after INTEGER NOT NULL,"
    "owner TEXT NOT NULL,"
    "joining_origin TEXT NOT NULL,"
    "exact_join_time INTEGER NOT NULL,"
    "name TEXT NOT NULL,"
    "priority DOUBLE NOT NULL,"
    "enable_bidding_signals_prioritization INTEGER NOT NULL,"
    "priority_vector TEXT NOT NULL,"
    "priority_signals_overrides TEXT NOT NULL,"
    "seller_capabilities TEXT NOT NULL,"
    "all_sellers_capabilities INTEGER NOT NULL,"
    "execution_mode INTEGER NOT NULL,"
    "joining_url TEXT NOT NULL,"
    "bidding_url TEXT NOT NULL,"
    "bidding_wasm_helper_url TEXT NOT NULL,"
    "update_url TEXT NOT NULL,"
    "trusted_bidding_signals_url TEXT NOT NULL,"
    "trusted_bidding_signals_keys TEXT NOT NULL,"
    "trusted_bidding_signals_slot_size_mode INTEGER NOT NULL,"
    "user_bidding_signals TEXT,"
    "ads_pb BLOB NOT NULL,"
    "ad_components_pb BLOB NOT NULL,"
    "ad_sizes TEXT NOT NULL,"
    "size_groups TEXT NOT NULL,"
    "auction_server_request_flags INTEGER NOT NULL,"
    "additional_bid_key BLOB NOT NULL,"
    "aggregation_coordinator_origin TEXT,"
    "storage_size INTEGER NOT NULL, "
    "PRIMARY KEY(owner,name))";

  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
      "last_updated,"
      "next_update_after,"
      "owner,"
      "joining_origin,"
      "exact_join_time,"
      "name,"
      "priority,"
      "enable_bidding_signals_prioritization,"
      "priority_vector,"
      "priority_signals_overrides,"
      "seller_capabilities,"
      "all_sellers_capabilities,"
      "execution_mode,"
      "joining_url,"
      "bidding_url,"
      "bidding_wasm_helper_url,"
      "update_url,"
      "trusted_bidding_signals_url,"
      "trusted_bidding_signals_keys,"
      "0," // trusted_bidding_signals_slot_size_mode
      "user_bidding_signals,"
      "ads_pb,"
      "ad_components_pb,"
      "ad_sizes,"
      "size_groups,"
      "auction_server_request_flags,"
      "additional_bid_key,"
      "aggregation_coordinator_origin,"
      "storage_size "
      "FROM interest_groups";
  // clang-format on

  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
    "ALTER TABLE new_interest_groups "
    "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return CreateInterestGroupIndices(db);
}

bool UpgradeV19SchemaToV20(sql::Database& db, sql::MetaTable& meta_table) {
  // The difference from V19 is V20 adds the following two new tables, used for
  // down sampling forDebuggingOnly reports.
  static const char kLockoutDebuggingOnlyReportTableSql[] =
      // clang-format off
      "CREATE TABLE lockout_debugging_only_report("
        "date_of_last_report_sent INTEGER NOT NULL)";
  // clang-format on
  if (!db.Execute(kLockoutDebuggingOnlyReportTableSql)) {
    return false;
  }

  static const char kCooldownDebuggingOnlyReportTableSql[] =
      // clang-format off
      "CREATE TABLE cooldown_debugging_only_report("
        "origin TEXT NOT NULL,"
        "starting_date INTEGER NOT NULL,"
        "duration INTEGER NOT NULL,"
      "PRIMARY KEY(origin))";
  // clang-format on
  if (!db.Execute(kCooldownDebuggingOnlyReportTableSql)) {
    return false;
  }

  return true;
}

bool UpgradeV18SchemaToV19(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads_pb BLOB NOT NULL,"
        "ad_components_pb BLOB NOT NULL,"
        "ad_sizes TEXT NOT NULL,"
        "size_groups TEXT NOT NULL,"
        "auction_server_request_flags INTEGER NOT NULL,"
        "additional_bid_key BLOB NOT NULL,"
        "aggregation_coordinator_origin TEXT,"
        "storage_size INTEGER NOT NULL, "
      "PRIMARY KEY(owner,name))";

  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "exact_join_time,"
             "name,"
             "priority,"
             "enable_bidding_signals_prioritization,"
             "priority_vector,"
             "priority_signals_overrides,"
             "seller_capabilities,"
             "all_sellers_capabilities,"
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads_pb,"
             "ad_components_pb,"
             "ad_sizes,"
             "size_groups,"
             "auction_server_request_flags,"
             "additional_bid_key,"
             "NULL," // aggregation_coordinator_origin
             "(LENGTH(interest_groups.owner)+"
              "LENGTH(interest_groups.joining_origin)+"
              "LENGTH(interest_groups.name)+"
              "LENGTH(interest_groups.priority_vector)+"
              "LENGTH(interest_groups.priority_signals_overrides)+"
              "LENGTH(interest_groups.seller_capabilities)+"
              "LENGTH(interest_groups.joining_url)+"
              "LENGTH(interest_groups.bidding_url)+"
              "LENGTH(interest_groups.bidding_wasm_helper_url)+"
              "LENGTH(interest_groups.update_url)+"
              "LENGTH(interest_groups.trusted_bidding_signals_url)+"
              "LENGTH(interest_groups.trusted_bidding_signals_keys)+"
              "IFNULL(LENGTH(interest_groups.user_bidding_signals),0)+"
              "LENGTH(interest_groups.ads_pb)+"
              "LENGTH(interest_groups.ad_components_pb)+"
              "LENGTH(interest_groups.ad_sizes)+"
              "LENGTH(interest_groups.size_groups)+"
              "LENGTH(interest_groups.additional_bid_key)+"
              "40) " // storage_size -- start with the old estimated size.
      "FROM interest_groups";
  // clang-format on

  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return CreateInterestGroupIndices(db);
}

bool UpgradeV17SchemaToV18(sql::Database& db, sql::MetaTable& meta_table) {
  // The only difference was the addition of the `interest_group_owner_and_type`
  // index, which would be removed and recreated in UpgradeV18SchemaToV19.
  return true;
}

bool UpgradeV16SchemaToV17(sql::Database& db,
                           sql::MetaTable& meta_table,
                           const PassKey& passkey) {
  static const char kCreateKAnonTableSql[] =
      "CREATE TABLE k_anon_new("
      "last_referenced_time INTEGER NOT NULL,"
      "key TEXT NOT NULL,"
      "owner TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "is_k_anon INTEGER NOT NULL,"
      "last_k_anon_updated_time INTEGER NOT NULL,"
      "last_reported_to_anon_server_time INTEGER NOT NULL,"
      "PRIMARY KEY(owner,name,key))";

  if (!db.Execute(kCreateKAnonTableSql)) {
    return false;
  }

  // Copy over all existing k-anon values into the new table.
  static const char kInsertPreviousKANonValues[] =
      // clang-format off
      "INSERT INTO k_anon_new("
        "last_referenced_time,"
        "key,"
        "owner,"
        "name,"
        "is_k_anon,"
        "last_k_anon_updated_time,"
        "last_reported_to_anon_server_time) "
      "SELECT last_referenced_time, key,'','',"
        "is_k_anon, last_k_anon_updated_time,"
        "last_reported_to_anon_server_time "
      "FROM k_anon";
  // clang-format on

  if (!db.Execute(kInsertPreviousKANonValues)) {
    return false;
  }

  // Make sure all k-anon keys for ads in the interest_groups table are
  // represented in the k_anon table.
  sql::Statement select_igs_with_ads_and_bidding_url(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT owner, name, ads_pb, ad_components_pb, "
                            "bidding_url FROM interest_groups"));
  base::Time now = base::Time::Now();
  while (select_igs_with_ads_and_bidding_url.Step()) {
    blink::InterestGroup ig;
    ig.owner = DeserializeOrigin(
        select_igs_with_ads_and_bidding_url.ColumnStringView(0));
    ig.name = select_igs_with_ads_and_bidding_url.ColumnString(1);
    ig.ads = DeserializeInterestGroupAdVectorProto(
        passkey, select_igs_with_ads_and_bidding_url.ColumnStringView(2));
    ig.ad_components = DeserializeInterestGroupAdVectorProto(
        passkey, select_igs_with_ads_and_bidding_url.ColumnStringView(3));
    ig.bidding_url =
        DeserializeURL(select_igs_with_ads_and_bidding_url.ColumnStringView(4));

    // Insert k-anon keys for the interest group's ads.
    blink::InterestGroupKey interest_group_key(ig.owner, ig.name);
    if (ig.ads.has_value() && ig.bidding_url.has_value()) {
      for (auto& ad : *ig.ads) {
        if (!MaybeCreateKAnonEntryForV17DatabaseUpgrade(
                db, interest_group_key,
                blink::DEPRECATED_KAnonKeyForAdNameReporting(
                    ig, ad,
                    /*selected_buyer_and_seller_reporting_id=*/std::nullopt),
                now)) {
          return false;
        }
        if (!MaybeCreateKAnonEntryForV17DatabaseUpgrade(
                db, interest_group_key,
                blink::DEPRECATED_KAnonKeyForAdBid(ig, ad.render_url()), now)) {
          return false;
        }
      }
    }
    if (ig.ad_components.has_value()) {
      for (auto& ad : *ig.ad_components) {
        if (!MaybeCreateKAnonEntryForV17DatabaseUpgrade(
                db, interest_group_key,
                blink::DEPRECATED_KAnonKeyForAdComponentBid(ad.render_url()),
                now)) {
          return false;
        }
      }
    }
  }

  static const char kDropKAnonTableSql[] = "DROP TABLE k_anon";
  if (!db.Execute(kDropKAnonTableSql)) {
    return false;
  }

  static const char kRenameKAnonTableSql[] =
      // clang-format off
      "ALTER TABLE k_anon_new "
      "RENAME TO k_anon";
  // clang-format on
  return db.Execute(kRenameKAnonTableSql);
}

bool UpgradeV15SchemaToV16(sql::Database& db,
                           sql::MetaTable& meta_table,
                           const PassKey& passkey) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads_pb BLOB NOT NULL,"
        "ad_components_pb BLOB NOT NULL,"
        "ad_sizes TEXT NOT NULL,"
        "size_groups TEXT NOT NULL,"
        "auction_server_request_flags INTEGER NOT NULL,"
        "additional_bid_key BLOB NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  // clang-format off
  sql::Statement kCopyInterestGroupTableWithEmptyAdPBsSql(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "exact_join_time,"
             "name,"
             "priority,"
             "enable_bidding_signals_prioritization,"
             "priority_vector,"
             "priority_signals_overrides,"
             "seller_capabilities,"
             "all_sellers_capabilities,"
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "?," // ads_pb
             "?," // ad_components_pb
             "ad_sizes,"
             "size_groups,"
             "auction_server_request_flags,"
             "additional_bid_key "
      "FROM interest_groups"));
  // clang-format on
  std::string empty_ad_proto_value;
  if (!AdProtos().SerializeToString(&empty_ad_proto_value)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.AdProtos",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.AdProtos",
        InterestGroupStorageProtoSerializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  kCopyInterestGroupTableWithEmptyAdPBsSql.BindBlob(0, empty_ad_proto_value);
  kCopyInterestGroupTableWithEmptyAdPBsSql.BindBlob(1, empty_ad_proto_value);

  if (!kCopyInterestGroupTableWithEmptyAdPBsSql.Run()) {
    return false;
  }

  sql::Statement kSelectIGsWithAds(db.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT owner, name, ads, ad_components from interest_groups"));

  while (kSelectIGsWithAds.Step()) {
    std::string owner = kSelectIGsWithAds.ColumnString(0);
    std::string name = kSelectIGsWithAds.ColumnString(1);
    std::optional<std::vector<blink::InterestGroup::Ad>> ads =
        DeserializeInterestGroupAdVectorJson(
            passkey, kSelectIGsWithAds.ColumnStringView(2),
            /*for_components=*/false);
    std::optional<std::vector<blink::InterestGroup::Ad>> ad_components =
        DeserializeInterestGroupAdVectorJson(
            passkey, kSelectIGsWithAds.ColumnStringView(3),
            /*for_components=*/true);

    std::string serialized_ads = SerializeUncompressed(ads);
    std::string serialized_ad_components = SerializeUncompressed(ad_components);

    sql::Statement insert_value_into_IG(db.GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE new_interest_groups SET ads_pb = ?, ad_components_pb = ? "
        "WHERE owner = ? AND name = ?"));

    insert_value_into_IG.BindBlob(0, serialized_ads);
    insert_value_into_IG.BindBlob(1, serialized_ad_components);
    insert_value_into_IG.BindString(2, owner);
    insert_value_into_IG.BindString(3, name);

    if (!insert_value_into_IG.Run()) {
      return false;
    }
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }
  return true;
}

bool UpgradeV14SchemaToV15(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
        "ad_sizes TEXT NOT NULL,"
        "size_groups TEXT NOT NULL,"
        "auction_server_request_flags INTEGER NOT NULL,"
        "additional_bid_key BLOB NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "exact_join_time,"
             "name,"
             "priority,"
             "enable_bidding_signals_prioritization,"
             "priority_vector,"
             "priority_signals_overrides,"
             "seller_capabilities,"
             "all_sellers_capabilities,"
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads,"
             "ad_components,"
             "ad_sizes,"
             "size_groups,"
             "auction_server_request_flags, "
             "X'' " // additional_bid_key
      "FROM interest_groups";
  // clang-format on
  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return true;
}

bool UpgradeV13SchemaToV14(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
        "ad_sizes TEXT NOT NULL,"
        "size_groups TEXT NOT NULL,"
        "auction_server_request_flags INTEGER NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "exact_join_time,"
             "name,"
             "priority,"
             "enable_bidding_signals_prioritization,"
             "priority_vector,"
             "priority_signals_overrides,"
             "seller_capabilities,"
             "all_sellers_capabilities,"
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads,"
             "ad_components,"
             "ad_sizes,"
             "size_groups,"
             "0 " // auction_server_request_flags
      "FROM interest_groups";
  // clang-format on
  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return true;
}

bool UpgradeV12SchemaToV13(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
        "ad_sizes TEXT NOT NULL,"
        "size_groups TEXT NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "exact_join_time,"
             "name,"
             "priority,"
             "enable_bidding_signals_prioritization,"
             "priority_vector,"
             "priority_signals_overrides,"
             "seller_capabilities,"
             "all_sellers_capabilities,"
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads,"
             "ad_components,"
             "''," // ad_sizes
             "''" // size_groups
      "FROM interest_groups";
  // clang-format on
  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return true;
}

bool UpgradeV11SchemaToV12(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "exact_join_time,"
             "name,"
             "priority,"
             "enable_bidding_signals_prioritization,"
             "priority_vector,"
             "priority_signals_overrides,"
             "'',"  // seller_capabilities
             "0,"  // all_sellers_capabilities
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads,"
             "ad_components "
      "FROM interest_groups";
  // clang-format on
  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return true;
}

bool UpgradeV10SchemaToV11(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "last_updated," // exact_join_time
             "name,"
             "priority,"
             "0," // enable_bidding_signals_prioritization
             "''," // priority_vector
             "''," // priority_signals_overrides
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads,"
             "ad_components "
      "FROM interest_groups";
  // clang-format on
  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return true;
}

bool UpgradeV9SchemaToV10(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupKAnonTableSql[] =
      // clang-format off
      "CREATE TABLE k_anon("
        "last_referenced_time INTEGER NOT NULL,"
        "key TEXT NOT NULL,"
        "is_k_anon INTEGER NOT NULL,"
        "last_k_anon_updated_time INTEGER NOT NULL,"
        "last_reported_to_anon_server_time INTEGER NOT NULL,"
        "PRIMARY KEY(key))";
  // clang-format on
  if (!db.Execute(kInterestGroupKAnonTableSql)) {
    return false;
  }

  static const char kInterestGroupKAnonLastRefIndexSql[] =
      // clang-format off
      "CREATE INDEX k_anon_last_referenced_time"
      " ON k_anon(last_referenced_time DESC)";
  // clang-format on
  if (!db.Execute(kInterestGroupKAnonLastRefIndexSql)) {
    return false;
  }

  static const char kCopyKAnonTableSql[] =
      // clang-format off
      "INSERT INTO k_anon "
        "SELECT last_referenced_time,"
        "key,"
        "k_anon_count > 50 AS is_k_anon,"
        "last_k_anon_updated_time,"
        "last_reported_to_anon_server_time "
      "FROM kanon";
  // clang-format on
  if (!db.Execute(kCopyKAnonTableSql)) {
    return false;
  }

  static const char kDropKanonTableSql[] = "DROP TABLE kanon";
  if (!db.Execute(kDropKanonTableSql)) {
    return false;
  }

  return true;
}

bool UpgradeV8SchemaToV9(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kJoinHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE join_history2("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "join_time INTEGER NOT NULL,"
        "count INTEGER NOT NULL,"
      "PRIMARY KEY(owner, name, join_time) "
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kJoinHistoryTableSql)) {
    return false;
  }

  // Consolidate old join records into a single record that
  // counts the number joins in a day (86400000000 microseconds)
  static const char kCopyJoinHistoryTableSql[] =
      // clang-format off
      "INSERT INTO join_history2 "
      "SELECT owner,"
             "name,"
             "(join_time-(join_time%86400000000)) as join_time2,"
             "COUNT() as count "
      "FROM join_history "
      "GROUP BY owner,name,join_time2";
  // clang-format on
  if (!db.Execute(kCopyJoinHistoryTableSql)) {
    return false;
  }

  static const char kDropJoinHistoryTableSql[] = "DROP TABLE join_history";
  if (!db.Execute(kDropJoinHistoryTableSql)) {
    return false;
  }

  static const char kRenameJoinHistoryTableSql[] =
      // clang-format off
      "ALTER TABLE join_history2 "
      "RENAME TO join_history";
  // clang-format on
  if (!db.Execute(kRenameJoinHistoryTableSql)) {
    return false;
  }

  static const char kBidHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE bid_history2("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "bid_time INTEGER NOT NULL,"
        "count INTEGER NOT NULL,"
      "PRIMARY KEY(owner, name, bid_time) "
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kBidHistoryTableSql)) {
    return false;
  }

  // Consolidate old bid records into a single record that
  // counts the number bids in a day (86400000000 microseconds)
  static const char kCopyBidHistoryTableSql[] =
      // clang-format off
      "INSERT INTO bid_history2 "
      "SELECT owner,"
             "name,"
             "(bid_time-(bid_time%86400000000)) as bid_time2,"
             "COUNT() as count "
      "FROM bid_history "
      "GROUP BY owner,name,bid_time2";
  // clang-format on
  if (!db.Execute(kCopyBidHistoryTableSql)) {
    return false;
  }

  static const char kDropBidHistoryTableSql[] = "DROP TABLE bid_history";
  if (!db.Execute(kDropBidHistoryTableSql)) {
    return false;
  }

  static const char kRenameBidHistoryTableSql[] =
      // clang-format off
      "ALTER TABLE bid_history2 "
      "RENAME TO bid_history";
  // clang-format on
  if (!db.Execute(kRenameBidHistoryTableSql)) {
    return false;
  }
  return true;
}

bool UpgradeV7SchemaToV8(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupsAddExecutionModeSql[] =
      "ALTER TABLE interest_groups ADD COLUMN execution_mode INTEGER DEFAULT 0";
  if (!db.Execute(kInterestGroupsAddExecutionModeSql)) {
    return false;
  }
  return true;
}

bool UpgradeV6SchemaToV7(sql::Database& db, sql::MetaTable& meta_table) {
  // Index on group expiration by owner.
  DCHECK(db.DoesIndexExist("interest_group_owner"));
  static const char kRemoveInterestGroupOwnerIndexSql[] =
      // clang-format off
      "DROP INDEX interest_group_owner";
  // clang-format on
  if (!db.Execute(kRemoveInterestGroupOwnerIndexSql)) {
    return false;
  }
  DCHECK(!db.DoesIndexExist("interest_group_owner"));
  static const char kInterestGroupOwnerIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_owner"
      " ON interest_groups(owner,expiration DESC,next_update_after ASC,name)";
  // clang-format on
  if (!db.Execute(kInterestGroupOwnerIndexSql)) {
    return false;
  }

  // Update interest_group table.
  static const char kInterestGroupsAddPrioritySql[] =
      "ALTER TABLE interest_groups ADD COLUMN priority DOUBLE DEFAULT 0";
  if (!db.Execute(kInterestGroupsAddPrioritySql)) {
    return false;
  }
  return true;
}

bool UpgradeDB(sql::Database& db,
               const int db_version,
               sql::MetaTable& meta_table,
               const PassKey& pass_key) {
  // Whether to vacuum the database after the upgrade. The vacuum must happen
  // after the transaction is committed.
  bool vacuum_db_post_upgrade = false;
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }
  switch (db_version) {
    case 6:
      if (!UpgradeV6SchemaToV7(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 7:
      if (!UpgradeV7SchemaToV8(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 8:
      if (!UpgradeV8SchemaToV9(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 9:
      if (!UpgradeV9SchemaToV10(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 10:
      if (!UpgradeV10SchemaToV11(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 11:
      if (!UpgradeV11SchemaToV12(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 12:
      if (!UpgradeV12SchemaToV13(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 13:
      if (!UpgradeV13SchemaToV14(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 14:
      if (!UpgradeV14SchemaToV15(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 15:
      if (!UpgradeV15SchemaToV16(db, meta_table, pass_key)) {
        return false;
      }
      [[fallthrough]];
    case 16:
      if (!UpgradeV16SchemaToV17(db, meta_table, pass_key)) {
        return false;
      }
      [[fallthrough]];
    case 17:
      if (!UpgradeV17SchemaToV18(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 18:
      if (!UpgradeV18SchemaToV19(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 19:
      if (!UpgradeV19SchemaToV20(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 20:
      if (!UpgradeV20SchemaToV21(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 21:
      if (!UpgradeV21SchemaToV22(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 22:
      if (!UpgradeV22SchemaToV23(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 23:
      if (!UpgradeV23SchemaToV24(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 24:
      if (!UpgradeV24SchemaToV25(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 25:
      vacuum_db_post_upgrade = true;
      [[fallthrough]];
    case 26:
      vacuum_db_post_upgrade = true;
      if (!UpgradeV26SchemaToV27(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 27:
      if (!UpgradeV27SchemaToV28(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 28:
      // v29 adds a new field in the IG.ads structure, and so doesn't require
      // any changes to the InterestGroup table. Existing data is
      // forwards-compatible because `FromInterestGroupAdValue` correctly
      // handles the lack of a value for
      // `selectable_buyer_and_seller_reporting_ids`.
      [[fallthrough]];
    case 29:
      vacuum_db_post_upgrade = true;
      if (!UpgradeV29SchemaToV30(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 30:
      // Conversion is a no-op, just bookkeeping for a proto change.
      [[fallthrough]];
    case 31:
      if (!UpgradeV31SchemaToV32(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 32:
      if (!UpgradeV32SchemaToV33(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 33:
      if (!UpgradeV33SchemaToV34(db, meta_table)) {
        return false;
      }
      [[fallthrough]];
    case 34:
      if (!UpgradeV34SchemaToV35(db, meta_table)) {
        return false;
      }
      if (!meta_table.SetVersionNumber(kCurrentVersionNumber)) {
        return false;
      }
  }
  bool committed = transaction.Commit();
  if (!committed) {
    return false;
  }
  if (vacuum_db_post_upgrade) {
    const bool vacuum_result = VacuumDB(db);
    if (vacuum_result) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.VacuumResult",
          InterestGroupStorageVacuumResult::kSucceeded);
    } else {
      DLOG(ERROR) << "Failed to vacuum: " << db.GetErrorMessage();
      base::UmaHistogramEnumeration("Storage.InterestGroup.VacuumResult",
                                    InterestGroupStorageVacuumResult::kFailed);
    }
  }

  return true;
}

bool RemoveJoinHistory(sql::Database& db,
                       const blink::InterestGroupKey& group_key) {
  sql::Statement remove_join_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM join_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_join_history.is_valid()) {
    return false;
  }

  remove_join_history.Reset(true);
  remove_join_history.BindString(0, Serialize(group_key.owner));
  remove_join_history.BindString(1, group_key.name);
  return remove_join_history.Run();
}

bool RemoveBidHistory(sql::Database& db,
                      const blink::InterestGroupKey& group_key) {
  sql::Statement remove_bid_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM bid_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_bid_history.is_valid()) {
    return false;
  }

  remove_bid_history.Reset(true);
  remove_bid_history.BindString(0, Serialize(group_key.owner));
  remove_bid_history.BindString(1, group_key.name);
  return remove_bid_history.Run();
}

bool RemoveWinHistory(sql::Database& db,
                      const blink::InterestGroupKey& group_key) {
  sql::Statement remove_win_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM win_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_win_history.is_valid()) {
    return false;
  }

  remove_win_history.Reset(true);
  remove_win_history.BindString(0, Serialize(group_key.owner));
  remove_win_history.BindString(1, group_key.name);
  return remove_win_history.Run();
}

bool DoRemoveInterestGroup(sql::Database& db,
                           const blink::InterestGroupKey& group_key) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  // These tables have foreign keys that reference the interest group table.
  if (!RemoveJoinHistory(db, group_key)) {
    return false;
  }
  if (!RemoveBidHistory(db, group_key)) {
    return false;
  }
  if (!RemoveWinHistory(db, group_key)) {
    return false;
  }

  sql::Statement remove_group(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM interest_groups "
                            "WHERE owner=? AND name=?"));
  if (!remove_group.is_valid()) {
    return false;
  }

  remove_group.Reset(true);
  remove_group.BindString(0, Serialize(group_key.owner));
  remove_group.BindString(1, group_key.name);
  return remove_group.Run() && transaction.Commit();
}

bool DoClearClusteredBiddingGroups(sql::Database& db,
                                   const url::Origin& owner,
                                   const url::Origin& main_frame) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  // clang-format off
  sql::Statement same_cluster_groups(
      db.GetCachedStatement(SQL_FROM_HERE,
        "SELECT name "
        "FROM interest_groups "
        "WHERE owner = ? AND joining_origin = ? AND execution_mode = ?"));
  // clang-format on

  if (!same_cluster_groups.is_valid()) {
    return false;
  }

  same_cluster_groups.Reset(true);
  same_cluster_groups.BindString(0, Serialize(owner));
  same_cluster_groups.BindString(1, Serialize(main_frame));
  same_cluster_groups.BindInt(
      2, static_cast<int>(
             blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));

  while (same_cluster_groups.Step()) {
    if (!DoRemoveInterestGroup(
            db, blink::InterestGroupKey(owner,
                                        same_cluster_groups.ColumnString(0)))) {
      return false;
    }
  }
  return transaction.Commit();
}

// Leaves all the interest groups joined on `joining_origin` except
// `interest_groups_to_keep`. Returns std::nullopt on error, and a (possibly
// empty) list of left interest groups on success.
std::optional<std::vector<std::string>> DoClearOriginJoinedInterestGroups(
    sql::Database& db,
    const url::Origin& owner,
    const std::set<std::string>& interest_groups_to_keep,
    const url::Origin& joining_origin) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return std::nullopt;
  }

  // Have to select interest groups and then use DoRemoveInterestGroup() in
  // order to remove data in other tables about any removed groups. Can't just
  // do a single delete.

  // clang-format off
  sql::Statement same_cluster_groups(
    db.GetCachedStatement(SQL_FROM_HERE,
      "SELECT name "
      "FROM interest_groups "
      "WHERE owner = ? AND joining_origin = ?"));
  // clang-format on

  if (!same_cluster_groups.is_valid()) {
    return std::nullopt;
  }

  same_cluster_groups.Reset(true);
  same_cluster_groups.BindString(0, Serialize(owner));
  same_cluster_groups.BindString(1, Serialize(joining_origin));

  std::vector<std::string> cleared_interest_groups;

  while (same_cluster_groups.Step()) {
    std::string name = same_cluster_groups.ColumnString(0);
    if (interest_groups_to_keep.contains(name)) {
      continue;
    }
    if (!DoRemoveInterestGroup(db, blink::InterestGroupKey(owner, name))) {
      return std::nullopt;
    }
    cleared_interest_groups.emplace_back(std::move(name));
  }
  if (!transaction.Commit()) {
    return std::nullopt;
  }
  return cleared_interest_groups;
}

#define COMMON_INTEREST_GROUPS_QUERY_FIELDS \
  "expiration,"                             \
  "joining_origin,"                         \
  "exact_join_time,"                        \
  "last_updated,"                           \
  "next_update_after,"                      \
  "priority,"                               \
  "enable_bidding_signals_prioritization,"  \
  "priority_vector,"                        \
  "priority_signals_overrides,"             \
  "seller_capabilities,"                    \
  "all_sellers_capabilities,"               \
  "execution_mode,"                         \
  "bidding_url,"                            \
  "bidding_wasm_helper_url,"                \
  "update_url,"                             \
  "trusted_bidding_signals_url,"            \
  "trusted_bidding_signals_keys,"           \
  "trusted_bidding_signals_slot_size_mode," \
  "max_trusted_bidding_signals_url_length," \
  "trusted_bidding_signals_coordinator,"    \
  "view_and_click_counts_providers,"        \
  "user_bidding_signals," /* opaque data */ \
  "ads_pb,"                                 \
  "ad_components_pb,"                       \
  "ad_sizes,"                               \
  "size_groups,"                            \
  "auction_server_request_flags,"           \
  "additional_bid_key,"                     \
  "aggregation_coordinator_origin,"         \
  "last_k_anon_updated_time,"               \
  "kanon_keys"

// Populate `group` with the current `load` outcome. Prerequisite:
// `load` is an interest group query with the initial fields being
// `COMMON_INTEREST_GROUPS_QUERY_FIELDS`, and there is a row of data returned.
void PopulateInterestGroupFromQueryResult(sql::Statement& load,
                                          const PassKey& passkey,
                                          StorageInterestGroup& group) {
  group.interest_group.expiry = load.ColumnTime(0);
  group.joining_origin = DeserializeOrigin(load.ColumnStringView(1));

  group.join_time = load.ColumnTime(2);
  group.last_updated = load.ColumnTime(3);
  group.next_update_after = load.ColumnTime(4);
  group.interest_group.priority = load.ColumnDouble(5);
  group.interest_group.enable_bidding_signals_prioritization =
      load.ColumnBool(6);
  group.interest_group.priority_vector =
      DeserializeStringDoubleMap(load.ColumnStringView(7));
  group.interest_group.priority_signals_overrides =
      DeserializeStringDoubleMap(load.ColumnStringView(8));
  group.interest_group.seller_capabilities =
      DeserializeSellerCapabilitiesMap(load.ColumnStringView(9));
  group.interest_group.all_sellers_capabilities =
      DeserializeSellerCapabilities(load.ColumnInt64(10));
  group.interest_group.execution_mode =
      static_cast<blink::InterestGroup::ExecutionMode>(load.ColumnInt(11));
  group.interest_group.bidding_url = DeserializeURL(load.ColumnStringView(12));
  group.interest_group.bidding_wasm_helper_url =
      DeserializeURL(load.ColumnStringView(13));
  group.interest_group.update_url = DeserializeURL(load.ColumnStringView(14));
  group.interest_group.trusted_bidding_signals_url =
      DeserializeURL(load.ColumnStringView(15));
  group.interest_group.trusted_bidding_signals_keys =
      DeserializeStringVector(load.ColumnStringView(16));
  group.interest_group.trusted_bidding_signals_slot_size_mode =
      static_cast<blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode>(
          load.ColumnInt(17));
  group.interest_group.max_trusted_bidding_signals_url_length =
      load.ColumnInt(18);
  if (load.GetColumnType(19) != sql::ColumnType::kNull) {
    group.interest_group.trusted_bidding_signals_coordinator =
        DeserializeOrigin(load.ColumnStringView(19));
  }
  if (load.GetColumnType(20) != sql::ColumnType::kNull) {
    group.interest_group.view_and_click_counts_providers =
        DeserializeOriginVector(load.ColumnStringView(20));
  }
  if (load.GetColumnType(21) != sql::ColumnType::kNull) {
    group.interest_group.user_bidding_signals = load.ColumnString(21);
  }
  group.interest_group.ads = DecompressAndDeserializeInterestGroupAdVectorProto(
      passkey, load.ColumnStringView(22));
  group.interest_group.ad_components =
      DecompressAndDeserializeInterestGroupAdVectorProto(
          passkey, load.ColumnStringView(23));
  group.interest_group.ad_sizes =
      DeserializeStringSizeMap(load.ColumnStringView(24));
  group.interest_group.size_groups =
      DeserializeStringStringVectorMap(load.ColumnStringView(25));
  group.interest_group.auction_server_request_flags =
      DeserializeAuctionServerRequestFlags(load.ColumnInt64(26));
  group.interest_group.additional_bid_key =
      DeserializeAdditionalBidKey(load.ColumnBlob(27));
  if (load.GetColumnType(28) != sql::ColumnType::kNull) {
    group.interest_group.aggregation_coordinator_origin =
        DeserializeOrigin(load.ColumnStringView(28));
  }
  group.last_k_anon_updated = load.ColumnTime(29);
  KAnonKeyProtos keys_proto;
  if (keys_proto.ParseFromString(load.ColumnStringView(30))) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoDeserializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoDeserializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoDeserializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoDeserializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  group.hashed_kanon_keys =
      std::vector(keys_proto.keys().begin(), keys_proto.keys().end());
}

bool DoLoadInterestGroup(sql::Database& db,
                         const PassKey& passkey,
                         const blink::InterestGroupKey& group_key,
                         StorageInterestGroup& group) {
  // clang-format off
  sql::Statement load(
      db.GetCachedStatement(SQL_FROM_HERE,
        "SELECT " COMMON_INTEREST_GROUPS_QUERY_FIELDS " "
        "FROM interest_groups "
        "WHERE owner = ? AND name = ? "));
  // clang-format on

  if (!load.is_valid()) {
    return false;
  }

  load.Reset(true);
  load.BindString(0, Serialize(group_key.owner));
  load.BindString(1, group_key.name);

  if (!load.Step() || !load.Succeeded()) {
    return false;
  }
  group.interest_group.owner = group_key.owner;
  group.interest_group.name = group_key.name;

  PopulateInterestGroupFromQueryResult(load, passkey, group);

  return true;
}

bool DoRecordInterestGroupJoin(sql::Database& db,
                               const url::Origin& owner,
                               const std::string& name,
                               base::Time join_time) {
  // This flow basically emulates SQLite's UPSERT feature which is disabled in
  // Chrome. Although there are two statements executed, we don't need to
  // enclose them in a transaction since only one will actually modify the
  // database.

  // NOTE: Join and bid history can expire up to a few hours before the interest
  // group, since join and bid history are floored to UTC days, but the
  // interest group doesn't necessarily expire at UTC midnight.
  //
  // The reason for this is to reduce the number of join / bid rows stored.
  //
  // For joins, this can result in a join count of 0.
  int64_t join_day = join_time.ToDeltaSinceWindowsEpoch()
                         .FloorToMultiple(base::Days(1))
                         .InMicroseconds();

  // clang-format off
  sql::Statement insert_join_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR IGNORE INTO join_history(owner,name,join_time,count) "
      "VALUES(?,?,?,1)"));
  // clang-format on
  if (!insert_join_hist.is_valid()) {
    return false;
  }

  insert_join_hist.Reset(true);
  insert_join_hist.BindString(0, Serialize(owner));
  insert_join_hist.BindString(1, name);
  insert_join_hist.BindInt64(2, join_day);
  if (!insert_join_hist.Run()) {
    return false;
  }

  // If the insert changed the database return early.
  if (db.GetLastChangeCount() > 0) {
    return true;
  }

  // clang-format off
  sql::Statement update_join_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE join_history "
          "SET count=count+1 "
          "WHERE owner=? AND name=? AND join_time=?"));
  // clang-format on
  if (!update_join_hist.is_valid()) {
    return false;
  }

  update_join_hist.Reset(true);
  update_join_hist.BindString(0, Serialize(owner));
  update_join_hist.BindString(1, name);
  update_join_hist.BindInt64(2, join_day);

  return update_join_hist.Run();
}

std::optional<InterestGroupKanonUpdateParameter> DoJoinInterestGroup(
    sql::Database& db,
    const PassKey& passkey,
    const blink::InterestGroup& data,
    const GURL& joining_url,
    base::Time exact_join_time,
    base::Time last_updated,
    base::Time next_update_after) {
  DCHECK(data.IsValid() && data.IsValidForJoinAndUpdate());
  url::Origin joining_origin = url::Origin::Create(joining_url);
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return std::nullopt;
  }

  StorageInterestGroup old_group;
  base::Time last_k_anon_updated = base::Time::Min();
  base::flat_set<std::string> positive_kanon_keys;
  base::flat_set<std::string> all_old_kanon_keys;
  blink::InterestGroupKey interest_group_key(data.owner, data.name);
  if (DoLoadInterestGroup(db, passkey, interest_group_key, old_group)) {
    if (old_group.interest_group.expiry <= base::Time::Now()) {
      // If there's a matching old interest group that's expired but that
      // hasn't yet been cleaned up, delete it. This removes its associated
      // tables, which should expire at the same time as the old interest
      // group.
      if (!DoRemoveInterestGroup(db, interest_group_key)) {
        return std::nullopt;
      }
    } else if (old_group.interest_group.execution_mode ==
                   blink::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
               joining_origin != old_group.joining_origin) {
      // Clear all interest groups with same owner and mode
      // GroupedByOriginMode and same `old_group.joining_origin`.
      if (!DoClearClusteredBiddingGroups(db, data.owner,
                                         old_group.joining_origin)) {
        return std::nullopt;
      }
    } else {
      last_k_anon_updated = old_group.last_k_anon_updated;
      positive_kanon_keys = std::move(old_group.hashed_kanon_keys);
      all_old_kanon_keys = old_group.interest_group.GetAllKAnonKeys();
    }
  }

  InterestGroupKanonUpdateParameter kanon_update(last_k_anon_updated);
  base::flat_set<std::string> all_new_kanon_keys = data.GetAllKAnonKeys();
  std::set_difference(all_new_kanon_keys.begin(), all_new_kanon_keys.end(),
                      all_old_kanon_keys.begin(), all_old_kanon_keys.end(),
                      std::back_inserter(kanon_update.newly_added_hashed_keys));
  positive_kanon_keys.erase(
      std::remove_if(positive_kanon_keys.begin(), positive_kanon_keys.end(),
                     [&](const std::string& key) -> bool {
                       return !all_new_kanon_keys.contains(key);
                     }),
      positive_kanon_keys.end());
  kanon_update.hashed_keys.insert(kanon_update.hashed_keys.end(),
                                  all_new_kanon_keys.begin(),
                                  all_new_kanon_keys.end());

  // clang-format off
  sql::Statement join_group(
      db.GetCachedStatement(SQL_FROM_HERE,
        "INSERT OR REPLACE INTO interest_groups("
          "expiration,"
          "last_updated,"
          "next_update_after,"
          "owner,"
          "joining_origin,"
          "exact_join_time,"
          "name,"
          "priority,"
          "enable_bidding_signals_prioritization,"
          "priority_vector,"
          "priority_signals_overrides,"
          "seller_capabilities,"
          "all_sellers_capabilities,"
          "execution_mode,"
          "joining_url,"
          "bidding_url,"
          "bidding_wasm_helper_url,"
          "update_url,"
          "trusted_bidding_signals_url,"
          "trusted_bidding_signals_keys,"
          "trusted_bidding_signals_slot_size_mode,"
          "max_trusted_bidding_signals_url_length,"
          "trusted_bidding_signals_coordinator,"
          "view_and_click_counts_providers,"
          "user_bidding_signals,"  // opaque data
          "ads_pb,"
          "ad_components_pb,"
          "ad_sizes,"
          "size_groups,"
          "auction_server_request_flags,"
          "additional_bid_key,"
          "aggregation_coordinator_origin,"
          "storage_size,"
          "last_k_anon_updated_time,"
          "kanon_keys) "
        "VALUES("
          "?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"
        ));

  // clang-format on
  if (!join_group.is_valid()) {
    return std::nullopt;
  }
  join_group.Reset(true);
  join_group.BindTime(0, data.expiry);
  join_group.BindTime(1, last_updated);
  join_group.BindTime(2, next_update_after);
  join_group.BindString(3, Serialize(data.owner));
  join_group.BindString(4, Serialize(joining_origin));
  join_group.BindTime(5, exact_join_time);
  join_group.BindString(6, data.name);
  join_group.BindDouble(7, data.priority);
  join_group.BindBool(8, data.enable_bidding_signals_prioritization);
  join_group.BindString(9, Serialize(data.priority_vector));
  join_group.BindString(10, Serialize(data.priority_signals_overrides));
  join_group.BindString(11, Serialize(data.seller_capabilities));
  join_group.BindInt64(12, Serialize(data.all_sellers_capabilities));
  join_group.BindInt(13, static_cast<int>(data.execution_mode));
  join_group.BindString(14, Serialize(joining_url));
  join_group.BindString(15, Serialize(data.bidding_url));
  join_group.BindString(16, Serialize(data.bidding_wasm_helper_url));
  join_group.BindString(17, Serialize(data.update_url));
  join_group.BindString(18, Serialize(data.trusted_bidding_signals_url));
  join_group.BindString(19, Serialize(data.trusted_bidding_signals_keys));
  join_group.BindInt(
      20, static_cast<int>(data.trusted_bidding_signals_slot_size_mode));
  join_group.BindInt(21, data.max_trusted_bidding_signals_url_length);
  if (data.trusted_bidding_signals_coordinator) {
    join_group.BindString(22,
                          Serialize(*data.trusted_bidding_signals_coordinator));
  } else {
    join_group.BindNull(22);
  }
  if (data.view_and_click_counts_providers) {
    join_group.BindString(23, Serialize(*data.view_and_click_counts_providers));
  } else {
    join_group.BindNull(23);
  }
  if (data.user_bidding_signals) {
    join_group.BindString(24, data.user_bidding_signals.value());
  } else {
    join_group.BindNull(24);
  }
  join_group.BindBlob(25, Serialize(data.ads));
  join_group.BindBlob(26, Serialize(data.ad_components));
  join_group.BindString(27, Serialize(data.ad_sizes));
  join_group.BindString(28, Serialize(data.size_groups));
  join_group.BindInt64(29, Serialize(data.auction_server_request_flags));
  join_group.BindBlob(30, Serialize(data.additional_bid_key));
  if (data.aggregation_coordinator_origin) {
    join_group.BindString(31, Serialize(*data.aggregation_coordinator_origin));
  } else {
    join_group.BindNull(31);
  }
  join_group.BindInt64(32, data.EstimateSize());
  join_group.BindTime(33, last_k_anon_updated);
  KAnonKeyProtos key_proto;
  *key_proto.mutable_keys() = {positive_kanon_keys.begin(),
                               positive_kanon_keys.end()};
  std::string key_proto_str;
  if (key_proto.SerializeToString(&key_proto_str)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoSerializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  join_group.BindBlob(34, key_proto_str);

  if (!join_group.Run()) {
    return std::nullopt;
  }

  if (!DoRecordInterestGroupJoin(db, data.owner, data.name, last_updated)) {
    return std::nullopt;
  }

  if (!transaction.Commit()) {
    return std::nullopt;
  }

  if (data.ads) {
    base::UmaHistogramCounts1000(
        "Storage.InterestGroup.PerInterestGroup.NumAds", data.ads->size());
    for (blink::InterestGroup::Ad ad : *data.ads) {
      base::UmaHistogramCounts10000("Storage.InterestGroup.AdRenderURLSize",
                                    ad.render_url().size());
    }
  }
  if (data.ad_components) {
    base::UmaHistogramCounts1000(
        "Storage.InterestGroup.PerInterestGroup.NumAdComponents",
        data.ad_components->size());
    for (blink::InterestGroup::Ad ad_component : *data.ad_components) {
      base::UmaHistogramCounts10000(
          "Storage.InterestGroup.AdComponentRenderURLSize",
          ad_component.render_url().size());
    }
  }

  return std::move(kanon_update);
}

bool DoStoreInterestGroupUpdate(
    sql::Database& db,
    const blink::InterestGroup& group,
    base::Time now,
    base::flat_set<std::string>& positive_kanon_keys) {
  // clang-format off
  sql::Statement store_group(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE interest_groups "
          "SET last_updated=?,"
            "next_update_after=?,"
            "priority=?,"
            "enable_bidding_signals_prioritization=?,"
            "priority_vector=?,"
            "priority_signals_overrides=?,"
            "seller_capabilities=?,"
            "all_sellers_capabilities=?,"
            "execution_mode=?,"
            "bidding_url=?,"
            "bidding_wasm_helper_url=?,"
            "update_url=?,"
            "trusted_bidding_signals_url=?,"
            "trusted_bidding_signals_keys=?,"
            "trusted_bidding_signals_slot_size_mode=?,"
            "max_trusted_bidding_signals_url_length=?,"
            "trusted_bidding_signals_coordinator=?,"
            "view_and_click_counts_providers=?,"
            "user_bidding_signals=?,"
            "ads_pb=?,"
            "ad_components_pb=?,"
            "ad_sizes=?,"
            "size_groups=?,"
            "auction_server_request_flags=?,"
            "additional_bid_key=?,"
            "aggregation_coordinator_origin=?,"
            "storage_size=?,"
            "kanon_keys=? "
          "WHERE owner=? AND name=?"));

  // clang-format on
  if (!store_group.is_valid()) {
    return false;
  }

  store_group.Reset(true);
  store_group.BindTime(0, now);
  store_group.BindTime(
      1, now + InterestGroupStorage::kUpdateSucceededBackoffPeriod);
  store_group.BindDouble(2, group.priority);
  store_group.BindBool(3, group.enable_bidding_signals_prioritization);
  store_group.BindString(4, Serialize(group.priority_vector));
  store_group.BindString(5, Serialize(group.priority_signals_overrides));
  store_group.BindString(6, Serialize(group.seller_capabilities));
  store_group.BindInt64(7, Serialize(group.all_sellers_capabilities));
  store_group.BindInt(8, static_cast<int>(group.execution_mode));
  store_group.BindString(9, Serialize(group.bidding_url));
  store_group.BindString(10, Serialize(group.bidding_wasm_helper_url));
  store_group.BindString(11, Serialize(group.update_url));
  store_group.BindString(12, Serialize(group.trusted_bidding_signals_url));
  store_group.BindString(13, Serialize(group.trusted_bidding_signals_keys));
  store_group.BindInt(
      14, static_cast<int>(group.trusted_bidding_signals_slot_size_mode));
  store_group.BindInt(15, group.max_trusted_bidding_signals_url_length);
  if (group.trusted_bidding_signals_coordinator) {
    store_group.BindString(
        16, Serialize(*group.trusted_bidding_signals_coordinator));
  } else {
    store_group.BindNull(16);
  }
  if (group.view_and_click_counts_providers) {
    store_group.BindString(17,
                           Serialize(*group.view_and_click_counts_providers));
  } else {
    store_group.BindNull(17);
  }
  if (group.user_bidding_signals) {
    store_group.BindString(18, group.user_bidding_signals.value());
  } else {
    store_group.BindNull(18);
  }
  store_group.BindBlob(19, Serialize(group.ads));
  store_group.BindBlob(20, Serialize(group.ad_components));
  store_group.BindString(21, Serialize(group.ad_sizes));
  store_group.BindString(22, Serialize(group.size_groups));
  store_group.BindInt64(23, Serialize(group.auction_server_request_flags));
  store_group.BindBlob(24, Serialize(group.additional_bid_key));
  if (group.aggregation_coordinator_origin) {
    store_group.BindString(25,
                           Serialize(*group.aggregation_coordinator_origin));
  } else {
    store_group.BindNull(25);
  }
  store_group.BindInt64(26, group.EstimateSize());

  KAnonKeyProtos key_proto;
  *key_proto.mutable_keys() = {positive_kanon_keys.begin(),
                               positive_kanon_keys.end()};
  std::string key_proto_str;
  if (key_proto.SerializeToString(&key_proto_str)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoSerializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  store_group.BindBlob(27, key_proto_str);

  store_group.BindString(28, Serialize(group.owner));
  store_group.BindString(29, group.name);

  return store_group.Run();
}

std::optional<InterestGroupKanonUpdateParameter> DoUpdateInterestGroup(
    sql::Database& db,
    const PassKey& passkey,
    const blink::InterestGroupKey& group_key,
    InterestGroupUpdate update,
    base::Time now) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return std::nullopt;
  }

  // Unlike Join() operations, for Update() operations, values that aren't
  // specified in the JSON returned by servers (Serialize()'d below as empty
  // strings) aren't modified in the database -- in this sense, new data is
  // merged with old data.
  //
  // Since we need to verify this results in a valid interest group, we have
  // to first read the interest group from the DB, apply the changes and then
  // verify the interest group is valid before writing it to the database.

  StorageInterestGroup storage_interest_group;
  if (!DoLoadInterestGroup(db, passkey, group_key, storage_interest_group)) {
    return std::nullopt;
  }

  blink::InterestGroup& updated_group = storage_interest_group.interest_group;
  base::flat_set<std::string> pre_existing_k_anon_keys =
      updated_group.GetAllKAnonKeys();
  base::flat_set<std::string> positive_kanon_keys =
      std::move(storage_interest_group.hashed_kanon_keys);
  bool updated_kanon_keys = false;

  // (Optimization) Don't do anything for expired interest groups.
  if (updated_group.expiry <= now) {
    return std::nullopt;
  }
  if (update.priority) {
    updated_group.priority = *update.priority;
  }
  if (update.enable_bidding_signals_prioritization) {
    updated_group.enable_bidding_signals_prioritization =
        *update.enable_bidding_signals_prioritization;
  }
  if (update.priority_vector) {
    updated_group.priority_vector = update.priority_vector;
  }
  if (update.priority_signals_overrides) {
    MergePrioritySignalsOverrides(*update.priority_signals_overrides,
                                  updated_group.priority_signals_overrides);
  }
  if (update.seller_capabilities) {
    updated_group.seller_capabilities = update.seller_capabilities;
  }
  if (update.all_sellers_capabilities) {
    updated_group.all_sellers_capabilities = *update.all_sellers_capabilities;
  }
  if (update.execution_mode) {
    updated_group.execution_mode = *update.execution_mode;
  }
  if (update.daily_update_url) {
    updated_group.update_url = std::move(update.daily_update_url);
  }
  if (update.bidding_url) {
    // The bidding URL is part of k-anon keys.
    updated_kanon_keys = true;
    updated_group.bidding_url = std::move(update.bidding_url);
  }
  if (update.bidding_wasm_helper_url) {
    updated_group.bidding_wasm_helper_url =
        std::move(update.bidding_wasm_helper_url);
  }
  if (update.trusted_bidding_signals_url) {
    updated_group.trusted_bidding_signals_url =
        std::move(update.trusted_bidding_signals_url);
  }
  if (update.trusted_bidding_signals_keys) {
    updated_group.trusted_bidding_signals_keys =
        std::move(update.trusted_bidding_signals_keys);
  }
  if (update.trusted_bidding_signals_slot_size_mode) {
    updated_group.trusted_bidding_signals_slot_size_mode =
        *update.trusted_bidding_signals_slot_size_mode;
  }
  if (update.max_trusted_bidding_signals_url_length) {
    updated_group.max_trusted_bidding_signals_url_length =
        *update.max_trusted_bidding_signals_url_length;
  }
  if (update.trusted_bidding_signals_coordinator.has_value()) {
    updated_group.trusted_bidding_signals_coordinator =
        std::move(update.trusted_bidding_signals_coordinator.value());
  }
  if (update.view_and_click_counts_providers.has_value()) {
    updated_group.view_and_click_counts_providers =
        std::move(update.view_and_click_counts_providers.value());
  }
  if (update.user_bidding_signals) {
    updated_group.user_bidding_signals = std::move(update.user_bidding_signals);
  }
  if (update.ads) {
    updated_kanon_keys = true;
    updated_group.ads = std::move(update.ads);
  }
  if (update.ad_components) {
    updated_kanon_keys = true;
    updated_group.ad_components = std::move(update.ad_components);
  }
  if (update.ad_sizes) {
    updated_group.ad_sizes = std::move(update.ad_sizes);
  }
  if (update.size_groups) {
    updated_group.size_groups = std::move(update.size_groups);
  }
  if (update.auction_server_request_flags) {
    updated_group.auction_server_request_flags =
        *update.auction_server_request_flags;
  }

  if (update.aggregation_coordinator_origin) {
    updated_group.aggregation_coordinator_origin =
        std::move(update.aggregation_coordinator_origin);
  }

  if (!updated_group.IsValid() || !updated_group.IsValidForJoinAndUpdate()) {
    // TODO(behamilton): Report errors to devtools.
    return std::nullopt;
  }

  InterestGroupKanonUpdateParameter kanon_update(
      storage_interest_group.last_k_anon_updated);
  if (updated_kanon_keys) {
    base::flat_set<std::string> new_keys = updated_group.GetAllKAnonKeys();
    positive_kanon_keys.erase(
        std::remove_if(positive_kanon_keys.begin(), positive_kanon_keys.end(),
                       [&](const std::string& key) -> bool {
                         return !new_keys.contains(key);
                       }),
        positive_kanon_keys.end());
    kanon_update.hashed_keys.insert(kanon_update.hashed_keys.end(),
                                    new_keys.begin(), new_keys.end());
    std::set_difference(
        kanon_update.hashed_keys.begin(), kanon_update.hashed_keys.end(),
        pre_existing_k_anon_keys.begin(), pre_existing_k_anon_keys.end(),
        std::back_inserter(kanon_update.newly_added_hashed_keys));
  } else {
    kanon_update.hashed_keys.insert(kanon_update.hashed_keys.end(),
                                    pre_existing_k_anon_keys.begin(),
                                    pre_existing_k_anon_keys.end());
  }

  if (!DoStoreInterestGroupUpdate(db, updated_group, now,
                                  positive_kanon_keys) ||
      !transaction.Commit()) {
    return std::nullopt;
  }

  return std::move(kanon_update);
}

bool DoAllowUpdateIfOlderThan(sql::Database& db,
                              const PassKey& passkey,
                              const blink::InterestGroupKey& group_key,
                              base::TimeDelta update_if_older_than,
                              base::Time now) {
  sql::Statement allow_update_if_older_than(
      db.GetCachedStatement(SQL_FROM_HERE, R"(
UPDATE interest_groups SET
  next_update_after=?
WHERE owner=? AND name=? AND ? - last_updated >= ?)"));

  if (!allow_update_if_older_than.is_valid()) {
    return false;
  }

  allow_update_if_older_than.Reset(true);
  allow_update_if_older_than.BindTime(0, now);
  allow_update_if_older_than.BindString(1, Serialize(group_key.owner));
  allow_update_if_older_than.BindString(2, group_key.name);
  allow_update_if_older_than.BindTime(3, now);
  allow_update_if_older_than.BindTimeDelta(4, update_if_older_than);

  return allow_update_if_older_than.Run();
}

bool DoReportUpdateFailed(sql::Database& db,
                          const blink::InterestGroupKey& group_key,
                          bool parse_failure,
                          base::Time now) {
  sql::Statement update_group(db.GetCachedStatement(SQL_FROM_HERE, R"(
UPDATE interest_groups SET
  next_update_after=?
WHERE owner=? AND name=?)"));

  if (!update_group.is_valid()) {
    return false;
  }

  update_group.Reset(true);
  if (parse_failure) {
    // Non-network failures delay the same amount of time as successful
    // updates.
    update_group.BindTime(
        0, now + InterestGroupStorage::kUpdateSucceededBackoffPeriod);
  } else {
    update_group.BindTime(
        0, now + InterestGroupStorage::kUpdateFailedBackoffPeriod);
  }
  update_group.BindString(1, Serialize(group_key.owner));
  update_group.BindString(2, group_key.name);

  return update_group.Run();
}

bool DoRecordInterestGroupBid(sql::Database& db,
                              const blink::InterestGroupKey& group_key,
                              base::Time bid_time) {
  // This flow basically emulates SQLite's UPSERT feature which is disabled in
  // Chrome. Although there are two statements executed, we don't need to
  // enclose them in a transaction since only one will actually modify the
  // database.

  // NOTE: Join and bid history can expire up to a few hours before the interest
  // group, since join and bid history are floored to UTC days, but the
  // interest group doesn't necessarily expire at UTC midnight.
  //
  // The reason for this is to reduce the number of join / bid rows stored.
  int64_t bid_day = bid_time.ToDeltaSinceWindowsEpoch()
                        .FloorToMultiple(base::Days(1))
                        .InMicroseconds();

  // clang-format off
  sql::Statement insert_bid_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR IGNORE INTO bid_history(owner,name,bid_time,count) "
      "VALUES(?,?,?,1)"));
  // clang-format on
  if (!insert_bid_hist.is_valid()) {
    return false;
  }

  insert_bid_hist.Reset(true);
  insert_bid_hist.BindString(0, Serialize(group_key.owner));
  insert_bid_hist.BindString(1, group_key.name);
  insert_bid_hist.BindInt64(2, bid_day);
  if (!insert_bid_hist.Run()) {
    return false;
  }

  // If the insert changed the database return early.
  if (db.GetLastChangeCount() > 0) {
    return true;
  }

  // clang-format off
  sql::Statement update_bid_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE bid_history "
          "SET count=count+1 "
          "WHERE owner=? AND name=? AND bid_time=?"));
  // clang-format on
  if (!update_bid_hist.is_valid()) {
    return false;
  }

  update_bid_hist.Reset(true);
  update_bid_hist.BindString(0, Serialize(group_key.owner));
  update_bid_hist.BindString(1, group_key.name);
  update_bid_hist.BindInt64(2, bid_day);

  return update_bid_hist.Run();
}

bool DoRecordInterestGroupBids(sql::Database& db,
                               const blink::InterestGroupSet& group_keys,
                               base::Time bid_time) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  for (const auto& group_key : group_keys) {
    if (!DoRecordInterestGroupBid(db, group_key, bid_time)) {
      return false;
    }
  }
  return transaction.Commit();
}

bool DoRecordInterestGroupWin(sql::Database& db,
                              const blink::InterestGroupKey& group_key,
                              const std::string& ad_json,
                              base::Time win_time) {
  // Record the win. It should be unique since auctions should be serialized.
  // If it is not unique we should just keep the first one.
  // clang-format off
  sql::Statement win_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO win_history(owner,name,win_time,ad) "
      "VALUES(?,?,?,?)"));
  // clang-format on
  if (!win_hist.is_valid()) {
    return false;
  }

  win_hist.Reset(true);
  win_hist.BindString(0, Serialize(group_key.owner));
  win_hist.BindString(1, group_key.name);
  win_hist.BindTime(2, win_time);
  win_hist.BindString(3, ad_json);
  return win_hist.Run();
}

bool DoRecordDebugReportLockout(sql::Database& db,
                                base::Time starting_time,
                                base::TimeDelta duration) {
  sql::Statement debug_lockout(db.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE "
      "INTO lockout_debugging_only_report(id, starting_time, duration) "
      "VALUES(1, ?, ?)"));
  if (!debug_lockout.is_valid()) {
    return false;
  }

  debug_lockout.Reset(true);
  // Ceil to nearest hour to be stored in DB.
  debug_lockout.BindInt64(0, starting_time.ToDeltaSinceWindowsEpoch()
                                 .CeilToMultiple(base::Hours(1))
                                 .InMicroseconds());
  debug_lockout.BindTimeDelta(1, duration);
  return debug_lockout.Run();
}

std::optional<base::Time> DoGetMostDistantInterestGroupExpiration(
    sql::Database& db,
    base::Time now) {
  base::Time result;
  sql::Statement get_expiration(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT expiration "
                            "FROM interest_groups "
                            "WHERE expiration>? "
                            "ORDER BY expiration DESC "
                            "LIMIT 1"));
  if (!get_expiration.is_valid()) {
    DLOG(ERROR) << "GetMostDistantInterestGroupExpiration SQL statement did "
                   "not compile: "
                << db.GetErrorMessage();
    return std::nullopt;
  }
  get_expiration.Reset(true);
  get_expiration.BindTime(0, now);
  if (!get_expiration.Step()) {
    return std::nullopt;
  }
  return result = get_expiration.ColumnTime(0);
}

bool DoSetDebugReportLockoutUntilIGExpires(sql::Database& db, base::Time now) {
  sql::Transaction transaction(&db);

  if (!transaction.Begin()) {
    return false;
  }

  std::optional<base::Time> maybe_expiration =
      DoGetMostDistantInterestGroupExpiration(db, now);
  // If all interest groups joined before now already expired, then no need to
  // lockout.
  if (!maybe_expiration.has_value()) {
    sql::Statement clear_lockout(db.GetCachedStatement(
        SQL_FROM_HERE, "DELETE FROM lockout_debugging_only_report"));
    return clear_lockout.Run() && transaction.Commit();
  }

  sql::Statement debug_lockout(db.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE "
      "INTO lockout_debugging_only_report(id, starting_time, duration) "
      "VALUES(1, ?, ?)"));
  if (!debug_lockout.is_valid()) {
    return false;
  }

  int64_t starting_time_nearest_next_hour = now.ToDeltaSinceWindowsEpoch()
                                                .CeilToMultiple(base::Hours(1))
                                                .InMicroseconds();
  int64_t duration =
      maybe_expiration->ToDeltaSinceWindowsEpoch().InMicroseconds() -
      starting_time_nearest_next_hour;
  if (duration < 0) {
    duration = 0;
  }

  debug_lockout.Reset(true);
  // Ceil to nearest hour to be stored in DB.
  debug_lockout.BindInt64(0, starting_time_nearest_next_hour);
  debug_lockout.BindInt64(1, duration);

  return debug_lockout.Run() && transaction.Commit();
}

bool DoRecordDebugReportCooldown(sql::Database& db,
                                 const url::Origin& origin,
                                 base::Time cooldown_start,
                                 DebugReportCooldownType cooldown_type) {
  sql::Statement debug_cooldown(db.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE "
      "INTO cooldown_debugging_only_report(origin, starting_time, type) "
      "VALUES(?, ?, ?)"));
  if (!debug_cooldown.is_valid()) {
    return false;
  }

  debug_cooldown.Reset(true);
  debug_cooldown.BindString(0, Serialize(origin));
  // Ceil to nearest hour to be stored in DB.
  debug_cooldown.BindInt64(1, cooldown_start.ToDeltaSinceWindowsEpoch()
                                  .CeilToMultiple(base::Hours(1))
                                  .InMicroseconds());
  debug_cooldown.BindInt(2, static_cast<int>(cooldown_type));

  return debug_cooldown.Run();
}

bool DoUpdateKAnonymity(sql::Database& db,
                        const blink::InterestGroupKey& interest_group_key,
                        const std::vector<std::string>& positive_hashed_keys,
                        const base::Time update_time,
                        bool replace_existing_values,
                        base::Time now) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  KAnonKeyProtos keys_to_insert;
  base::Time update_time_to_insert = update_time;
  std::set<std::string> existing_keys;
  if (!replace_existing_values) {
    sql::Statement get_existing_keys(db.GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT last_k_anon_updated_time, kanon_keys FROM interest_groups "
        "WHERE owner = ? AND name = ? "));
    get_existing_keys.BindString(0, Serialize(interest_group_key.owner));
    get_existing_keys.BindString(1, interest_group_key.name);
    if (!get_existing_keys.Step()) {
      return false;
    }
    update_time_to_insert = get_existing_keys.ColumnTime(0);
    if (update_time_to_insert > update_time) {
      // DO NOT perform an update if the existing keys are newer.
      return false;
    }
    if (keys_to_insert.ParseFromString(get_existing_keys.ColumnStringView(1))) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.ProtoDeserializationResult.KAnonKeyProtos",
          InterestGroupStorageProtoDeserializationResult::kSucceeded);
    } else {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.ProtoDeserializationResult.KAnonKeyProtos",
          InterestGroupStorageProtoDeserializationResult::kFailed);
      return false;
    }
    existing_keys.insert(keys_to_insert.keys().begin(),
                         keys_to_insert.keys().end());
  }
  for (const std::string& new_key : positive_hashed_keys) {
    if (!existing_keys.contains(new_key)) {
      keys_to_insert.add_keys(new_key);
    }
  }

  sql::Statement set_values(db.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE interest_groups "
      "SET last_k_anon_updated_time=?, kanon_keys=? "
      "WHERE owner=? AND name=? AND last_k_anon_updated_time<=?"));

  set_values.Reset(true);
  set_values.BindTime(0, update_time_to_insert);
  std::string keys_to_insert_str;
  if (keys_to_insert.SerializeToString(&keys_to_insert_str)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.KAnonKeyProtos",
        InterestGroupStorageProtoSerializationResult::kFailed);
    // TODO(crbug.com/355010821): Consider bubbling out the failure.
  }
  set_values.BindBlob(1, keys_to_insert_str);
  set_values.BindString(2, Serialize(interest_group_key.owner));
  set_values.BindString(3, interest_group_key.name);
  set_values.BindTime(4, update_time);
  if (!set_values.Run()) {
    return false;
  }

  return transaction.Commit();
}

std::optional<base::Time> DoGetLastKAnonymityReported(
    sql::Database& db,
    const std::string& hashed_key) {
  const base::Time distant_past = base::Time::Min();

  sql::Statement get_reported(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT last_reported_to_anon_server_time FROM "
                            "joined_k_anon WHERE hashed_key=?"));
  if (!get_reported.is_valid()) {
    DLOG(ERROR) << "GetLastKAnonymityReported SQL statement did not compile: "
                << db.GetErrorMessage();
    return std::nullopt;
  }
  get_reported.Reset(true);
  get_reported.BindBlob(0, hashed_key);
  if (!get_reported.Step()) {
    return distant_past;
  }
  if (!get_reported.Succeeded()) {
    return std::nullopt;
  }
  return get_reported.ColumnTime(0);
}

void DoUpdateLastKAnonymityReported(sql::Database& db,
                                    const std::string& hashed_key,
                                    base::Time now) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return;
  }

  // clang-format off
    sql::Statement set_reported(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR REPLACE INTO joined_k_anon("
              "hashed_key,"
              "last_reported_to_anon_server_time) "
            "VALUES(?,?)"));
  // clang-format on

  if (!set_reported.is_valid()) {
    return;
  }

  set_reported.Reset(true);
  set_reported.BindBlob(0, hashed_key);
  set_reported.BindTime(1, now);

  if (!set_reported.Run()) {
    return;
  }

  transaction.Commit();
}

// Loads the view and click data for `provider_origin`, `eligible_origin`.
// Returns data if available, or reason if there isn't otherwise.
base::expected<ViewClickCountsForProviderAndEligible, MissingReason>
DoGetViewClickCountsForProviderAndEligible(sql::Database& db,
                                           const url::Origin& provider_origin,
                                           const url::Origin& eligible_origin) {
  sql::Statement get_counts(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT uncompacted_view_events,"
                            "compacted_view_events,"
                            "uncompacted_click_events,"
                            "compacted_click_events "
                            "FROM view_and_click_events "
                            "WHERE provider_origin=? AND eligible_origin=?"));
  if (!get_counts.is_valid()) {
    return base::unexpected(MissingReason::kDbError);
  }
  get_counts.Reset(true);
  get_counts.BindString(0, provider_origin.Serialize());
  get_counts.BindString(1, eligible_origin.Serialize());
  if (!get_counts.Step()) {
    // No counts stored.
    return base::unexpected(MissingReason::kNotInDb);
  }
  if (!get_counts.Succeeded()) {
    return base::unexpected(MissingReason::kDbError);
  }
  ViewClickCountsForProviderAndEligible result;
  if (!result.uncompacted_view_events.ParseFromString(
          get_counts.ColumnStringView(0)) ||
      !result.compacted_view_events.ParseFromString(
          get_counts.ColumnStringView(1)) ||
      !result.uncompacted_click_events.ParseFromString(
          get_counts.ColumnStringView(2)) ||
      !result.compacted_click_events.ParseFromString(
          get_counts.ColumnStringView(3))) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoDeserializationResult.ListOfTimestamps",
        InterestGroupStorageProtoSerializationResult::kFailed);
    return base::unexpected(MissingReason::kDecodeError);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoDeserializationResult.ListOfTimestamps",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
  }
  return result;
}

void DoIncrementViewClickCounts(base::Time now,
                                ViewOrClickCountsPtr& view_or_click_counts,
                                base::TimeDelta max_window,
                                int64_t int_timestamp,
                                int32_t count) {
  base::TimeDelta event_age = now - base::Time::FromDeltaSinceWindowsEpoch(
                                        base::Microseconds(int_timestamp));
  if (event_age > max_window) {
    return;
  }

  if (event_age <= base::Hours(1)) {
    view_or_click_counts->past_hour += count;
  }
  if (event_age <= base::Days(1)) {
    view_or_click_counts->past_day += count;
  }
  if (event_age <= base::Days(7)) {
    view_or_click_counts->past_week += count;
  }
  if (event_age <= base::Days(30)) {
    view_or_click_counts->past_30_days += count;
  }
  if (event_age <= base::Days(90)) {
    view_or_click_counts->past_90_days += count;
  }
  // Older expired events may exist -- maintenance will eventually remove them.
}

// Reads in view and click counts from database, and converts them to an
// aggregated form, just including counts for each window and category,
// rather than the stored time info.
//
// If successful, the value will not be null; entries not in database
// are represented as unexpected(kNotInDb) instead.
[[nodiscard]] base::expected<ViewAndClickCountsPtr, MissingReason>
DoGetViewAndClickCountsSummarized(sql::Database& db,
                                  base::Time now,
                                  const url::Origin& provider_origin,
                                  const url::Origin& eligible_origin) {
  const base::TimeDelta max_window = blink::MaxInterestGroupLifetime();

  base::expected<ViewClickCountsForProviderAndEligible, MissingReason>
      raw_counts = DoGetViewClickCountsForProviderAndEligible(
          /*db=*/db,
          /*provider_origin=*/provider_origin,
          /*eligible_origin=*/eligible_origin);
  if (!raw_counts.has_value()) {
    return base::unexpected(raw_counts.error());
  }

  ViewAndClickCountsPtr view_and_click_counts =
      blink::mojom::ViewAndClickCounts::New(
          /*view_counts=*/blink::mojom::ViewOrClickCounts::New(),
          /*click_counts=*/blink::mojom::ViewOrClickCounts::New());

  for (int64_t timestamp : raw_counts->uncompacted_view_events.timestamps()) {
    DoIncrementViewClickCounts(
        /*now=*/now,
        /*view_or_click_counts=*/view_and_click_counts->view_counts,
        /*max_window=*/max_window,
        /*int_timestamp=*/timestamp, /*count=*/1);
  }
  for (ListOfTimestampAndCounts_Entry timestamp_and_count :
       raw_counts->compacted_view_events.timestamp_and_counts()) {
    DoIncrementViewClickCounts(
        /*now=*/now,
        /*view_or_click_counts=*/view_and_click_counts->view_counts,
        /*max_window=*/max_window,
        /*int_timestamp=*/timestamp_and_count.timestamp(),
        /*count=*/timestamp_and_count.count());
  }
  for (int64_t timestamp : raw_counts->uncompacted_click_events.timestamps()) {
    DoIncrementViewClickCounts(
        /*now=*/now,
        /*view_or_click_counts=*/view_and_click_counts->click_counts,
        /*max_window=*/max_window,
        /*int_timestamp=*/timestamp, /*count=*/1);
  }
  for (ListOfTimestampAndCounts_Entry timestamp_and_count :
       raw_counts->compacted_click_events.timestamp_and_counts()) {
    DoIncrementViewClickCounts(
        /*now=*/now,
        /*view_or_click_counts=*/view_and_click_counts->click_counts,
        /*max_window=*/max_window,
        /*int_timestamp=*/timestamp_and_count.timestamp(),
        /*count=*/timestamp_and_count.count());
  }
  return view_and_click_counts;
}

void AggregateViewOrClickCounts(const blink::mojom::ViewOrClickCounts& in,
                                blink::mojom::ViewOrClickCounts& out) {
  out.past_hour += in.past_hour;
  out.past_day += in.past_day;
  out.past_week += in.past_week;
  out.past_30_days += in.past_30_days;
  out.past_90_days += in.past_90_days;
}

void AggregateViewAndClickCounts(const blink::mojom::ViewAndClickCounts& in,
                                 blink::mojom::ViewAndClickCounts& out) {
  AggregateViewOrClickCounts(*in.view_counts, *out.view_counts);
  AggregateViewOrClickCounts(*in.click_counts, *out.click_counts);
}

// Mutates browser signals for each entry in `interest_groups`, filling
// in loaded view and click counts.
//
// Returns true on success, and false on failure.
[[nodiscard]] bool DoGetViewAndClickCountsForGroups(
    sql::Database& db,
    base::Time now,
    const url::Origin& owner,
    InterestGroupsByName& interest_groups_by_name) {
  std::vector<url::Origin> default_providers = {owner};

  // Figure out which click tables we need.
  std::set<url::Origin> clickiness_providers;
  for (auto& [unused_name, storage_group] : interest_groups_by_name) {
    if (storage_group.interest_group.IsNegativeInterestGroup()) {
      continue;
    }
    if (!storage_group.interest_group.view_and_click_counts_providers ||
        storage_group.interest_group.view_and_click_counts_providers->empty()) {
      clickiness_providers.insert(owner);
      continue;
    }

    for (const url::Origin& provider_origin :
         *storage_group.interest_group.view_and_click_counts_providers) {
      clickiness_providers.insert(provider_origin);
    }
  }

  // Read all the needed tables into memory.
  std::map<url::Origin, ViewAndClickCountsPtr> clickiness_summaries;
  for (auto& provider_origin : clickiness_providers) {
    base::expected<ViewAndClickCountsPtr, MissingReason> summary =
        DoGetViewAndClickCountsSummarized(db, now,
                                          /*provider_origin=*/provider_origin,
                                          /*eligible_origin=*/owner);
    if (!summary.has_value()) {
      switch (summary.error()) {
        case MissingReason::kNotInDb:
          continue;
        case MissingReason::kDbError:
        case MissingReason::kDecodeError:
          return false;
      }
    }
    clickiness_summaries[provider_origin] = std::move(*summary);
  }

  // Now compose the info for each group.
  for (const auto& [unused_name, storage_group] : interest_groups_by_name) {
    storage_group.bidding_browser_signals->view_and_click_counts =
        blink::mojom::ViewAndClickCounts::New(
            /*view_counts=*/blink::mojom::ViewOrClickCounts::New(),
            /*click_counts=*/blink::mojom::ViewOrClickCounts::New());
    if (storage_group.interest_group.IsNegativeInterestGroup()) {
      continue;
    }

    for (const url::Origin& provider_origin :
         storage_group.interest_group.view_and_click_counts_providers &&
                 !storage_group.interest_group.view_and_click_counts_providers
                      ->empty()
             ? *storage_group.interest_group.view_and_click_counts_providers
             : default_providers) {
      auto it = clickiness_summaries.find(provider_origin);
      if (it == clickiness_summaries.end()) {
        continue;
      }

      AggregateViewAndClickCounts(
          *(it->second),
          *storage_group.bidding_browser_signals->view_and_click_counts);
    }
  }
  return true;
}

// Returns true if the rate limit (10 events in the last 20 seconds) has been
// exceeded.
bool IsClickinessRateLimited(base::Time now,
                             const ListOfTimestamps& uncompacted_events) {
  constexpr int kMaxEvents = 10;
  constexpr base::TimeDelta kRateLimitWindow = base::Seconds(20);
  int event_count = 0;
  // Iterate backwards -- the newest events are at the end.
  for (int64_t timestamp : base::Reversed(uncompacted_events.timestamps())) {
    base::TimeDelta event_age = now - base::Time::FromDeltaSinceWindowsEpoch(
                                          base::Microseconds(timestamp));
    if (event_age > kRateLimitWindow) {
      return false;
    }
    if (++event_count >= kMaxEvents) {
      return true;
    }
  }
  return false;
}

void DoRecordViewClick(sql::Database& db,
                       const network::AdAuctionEventRecord& record,
                       base::Time now) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return;
  }

  const std::vector<url::Origin>* eligible_origins = nullptr;
  std::vector<url::Origin> default_eligible_origin;
  if (record.eligible_origins.empty()) {
    default_eligible_origin.emplace_back(record.providing_origin);
    eligible_origins = &default_eligible_origin;
  } else {
    eligible_origins = &record.eligible_origins;
  }

  for (const url::Origin& eligible_origin : *eligible_origins) {
    sql::Statement get_counts(
        db.GetCachedStatement(SQL_FROM_HERE,
                              "SELECT uncompacted_view_events,"
                              "uncompacted_click_events "
                              "FROM view_and_click_events "
                              "WHERE provider_origin=? AND eligible_origin=?"));
    if (!get_counts.is_valid()) {
      DLOG(ERROR) << "DoRecordViewClick SELECT SQL statement did not compile: "
                  << db.GetErrorMessage();
      return;
    }
    get_counts.Reset(true);
    get_counts.BindString(0, record.providing_origin.Serialize());
    get_counts.BindString(1, eligible_origin.Serialize());

    bool row_exists = false;
    ListOfTimestamps uncompacted_view_events;
    ListOfTimestamps uncompacted_click_events;
    if (get_counts.Step()) {
      row_exists = true;
      if (!uncompacted_view_events.ParseFromString(
              get_counts.ColumnStringView(0)) ||
          !uncompacted_click_events.ParseFromString(
              get_counts.ColumnStringView(1))) {
        // TODO(crbug.com/355010821): Consider bubbling out the failure.
        base::UmaHistogramEnumeration(
            "Storage.InterestGroup.ProtoDeserializationResult.ListOfTimestamps",
            InterestGroupStorageProtoSerializationResult::kFailed);
        return;
      } else {
        base::UmaHistogramEnumeration(
            "Storage.InterestGroup.ProtoDeserializationResult.ListOfTimestamps",
            InterestGroupStorageProtoSerializationResult::kSucceeded);
      }
    } else if (!get_counts.Succeeded()) {
      return;
    }

    const int64_t int_now = now.ToDeltaSinceWindowsEpoch().InMicroseconds();
    switch (record.type) {
      case AdAuctionEventRecord::Type::kUninitialized:
        NOTREACHED();
      case AdAuctionEventRecord::Type::kView:
        if (IsClickinessRateLimited(now, uncompacted_view_events)) {
          return;
        }
        uncompacted_view_events.add_timestamps(int_now);
        break;
      case AdAuctionEventRecord::Type::kClick:
        if (IsClickinessRateLimited(now, uncompacted_click_events)) {
          return;
        }
        uncompacted_click_events.add_timestamps(int_now);
        break;
    }

    std::string uncompacted_view_events_str;
    std::string uncompacted_click_events_str;
    if (!uncompacted_view_events.SerializeToString(
            &uncompacted_view_events_str) ||
        !uncompacted_click_events.SerializeToString(
            &uncompacted_click_events_str)) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.ProtoSerializationResult.ListOfTimestamps",
          InterestGroupStorageProtoSerializationResult::kFailed);
      // TODO(crbug.com/355010821): Consider bubbling out the failure.
      return;
    } else {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.ProtoSerializationResult.ListOfTimestamps",
          InterestGroupStorageProtoSerializationResult::kSucceeded);
    }

    if (!row_exists) {
      // clang-format off
      sql::Statement insert_counts(
        db.GetCachedStatement(SQL_FROM_HERE,
          "INSERT INTO view_and_click_events("
          "provider_origin,"
          "eligible_origin,"
          "uncompacted_view_events,"
          "compacted_view_events,"
          "uncompacted_click_events,"
          "compacted_click_events) "
          "VALUES(?,?,?,?,?,?)"));
      // clang-format on

      if (!insert_counts.is_valid()) {
        DLOG(ERROR)
            << "DoRecordViewClick INSERT SQL statement did not compile: "
            << db.GetErrorMessage();
        return;
      }

      insert_counts.Reset(true);
      insert_counts.BindString(0, record.providing_origin.Serialize());
      insert_counts.BindString(1, eligible_origin.Serialize());
      insert_counts.BindString(2, uncompacted_view_events_str);
      insert_counts.BindString(3, "");
      insert_counts.BindString(4, uncompacted_click_events_str);
      insert_counts.BindString(5, "");

      if (!insert_counts.Run()) {
        return;
      }
    } else {
      // clang-format off
      sql::Statement update_counts(
        db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE view_and_click_events "
          "SET uncompacted_view_events=?,"
          "uncompacted_click_events=? "
          "WHERE provider_origin=? AND eligible_origin=?"));
      // clang-format on

      if (!update_counts.is_valid()) {
        DLOG(ERROR)
            << "DoRecordViewClick UPDATE SQL statement did not compile: "
            << db.GetErrorMessage();
        return;
      }

      update_counts.Reset(true);
      update_counts.BindString(0, uncompacted_view_events_str);
      update_counts.BindString(1, uncompacted_click_events_str);
      update_counts.BindString(2, record.providing_origin.Serialize());
      update_counts.BindString(3, eligible_origin.Serialize());

      if (!update_counts.Run()) {
        return;
      }
    }
  }

  transaction.Commit();
}

std::optional<std::vector<url::Origin>> DoGetAllInterestGroupOwners(
    sql::Database& db,
    base::Time expiring_after) {
  std::vector<url::Origin> result;
  sql::Statement load(db.GetCachedStatement(SQL_FROM_HERE,
                                            "SELECT DISTINCT owner "
                                            "FROM interest_groups "
                                            "WHERE expiration>? "
                                            "ORDER BY expiration DESC"));
  if (!load.is_valid()) {
    DLOG(ERROR) << "LoadAllInterestGroups SQL statement did not compile: "
                << db.GetErrorMessage();
    return std::nullopt;
  }
  load.Reset(true);
  load.BindTime(0, expiring_after);
  while (load.Step()) {
    result.push_back(DeserializeOrigin(load.ColumnStringView(0)));
  }
  if (!load.Succeeded()) {
    return std::nullopt;
  }
  return result;
}

std::optional<std::vector<url::Origin>> DoGetAllInterestGroupJoiningOrigins(
    sql::Database& db,
    base::Time expiring_after) {
  std::vector<url::Origin> result;
  sql::Statement load(db.GetCachedStatement(SQL_FROM_HERE,
                                            "SELECT DISTINCT joining_origin "
                                            "FROM interest_groups "
                                            "WHERE expiration>?"));
  if (!load.is_valid()) {
    DLOG(ERROR) << "LoadAllInterestGroupJoiningOrigins SQL statement did not "
                   "compile: "
                << db.GetErrorMessage();
    return std::nullopt;
  }
  load.Reset(true);
  load.BindTime(0, expiring_after);
  while (load.Step()) {
    result.push_back(DeserializeOrigin(load.ColumnStringView(0)));
  }
  if (!load.Succeeded()) {
    return std::nullopt;
  }
  return result;
}

bool DoRemoveInterestGroupsMatchingOwnerAndJoiner(
    sql::Database& db,
    const url::Origin& owner,
    const url::Origin& joining_origin,
    base::Time expiring_after) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  std::vector<std::string> owner_joiner_names;
  sql::Statement load(db.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT name "
      "FROM interest_groups "
      "WHERE owner=? AND joining_origin=? AND expiration>?"));

  if (!load.is_valid()) {
    return false;
  }

  load.Reset(true);
  load.BindString(0, owner.Serialize());
  load.BindString(1, joining_origin.Serialize());
  load.BindTime(2, expiring_after);

  while (load.Step()) {
    owner_joiner_names.emplace_back(load.ColumnString(0));
  }

  for (auto& name : owner_joiner_names) {
    if (!DoRemoveInterestGroup(
            db, blink::InterestGroupKey{owner, std::move(name)})) {
      return false;
    }
  }

  return transaction.Commit();
}

std::optional<std::vector<std::pair<url::Origin, url::Origin>>>
DoGetAllInterestGroupOwnerJoinerPairs(sql::Database& db,
                                      base::Time expiring_after) {
  std::vector<std::pair<url::Origin, url::Origin>> result;
  sql::Statement load(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT DISTINCT owner,joining_origin "
                            "FROM interest_groups "
                            "WHERE expiration>?"));
  if (!load.is_valid()) {
    DLOG(ERROR) << "LoadAllInterestGroupOwnerJoinerPairs SQL statement did not "
                   "compile: "
                << db.GetErrorMessage();
    return std::nullopt;
  }
  load.Reset(true);
  load.BindTime(0, expiring_after);
  while (load.Step()) {
    result.emplace_back(DeserializeOrigin(load.ColumnStringView(0)),
                        DeserializeOrigin(load.ColumnStringView(1)));
  }
  if (!load.Succeeded()) {
    return std::nullopt;
  }
  return result;
}

bool GetPreviousWins(sql::Database& db,
                     const blink::InterestGroupKey& group_key,
                     base::Time win_time_after,
                     BiddingBrowserSignalsPtr& output) {
  // clang-format off
  sql::Statement prev_wins(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT win_time, ad "
                            "FROM win_history "
                            "WHERE owner = ? AND name = ? AND win_time >= ? "
                            "ORDER BY win_time DESC"));
  // clang-format on
  if (!prev_wins.is_valid()) {
    DLOG(ERROR) << "GetInterestGroupsForOwner win_history SQL statement did "
                   "not compile: "
                << db.GetErrorMessage();
    return false;
  }
  prev_wins.Reset(true);
  prev_wins.BindString(0, Serialize(group_key.owner));
  prev_wins.BindString(1, group_key.name);
  prev_wins.BindTime(2, win_time_after);
  while (prev_wins.Step()) {
    PreviousWinPtr prev_win = blink::mojom::PreviousWin::New(
        /*time=*/prev_wins.ColumnTime(0),
        /*ad_json=*/prev_wins.ColumnString(1));
    output->prev_wins.push_back(std::move(prev_win));
  }
  bool succeeded = prev_wins.Succeeded();
  if (succeeded) {
    UMA_HISTOGRAM_COUNTS_100000("Storage.InterestGroup.PrevWinsNumEntries",
                                output->prev_wins.size());
  }
  return succeeded;
}

// NOTE: Join and bid history can expire up to a few hours before the interest
// group, since join and bid history are floored to UTC days, but the
// interest group doesn't necessarily expire at UTC midnight.
//
// The reason for this is to reduce the number of join / bid rows stored.
//
// For joins, this can result in a join count of 0.
bool GetJoinCount(sql::Database& db,
                  const blink::InterestGroupKey& group_key,
                  base::Time joined_after,
                  BiddingBrowserSignalsPtr& output) {
  // clang-format off
  sql::Statement join_count(
      db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT SUM(count) "
    "FROM join_history "
    "WHERE owner = ? AND name = ? AND join_time >=?"));
  // clang-format on
  if (!join_count.is_valid()) {
    DLOG(ERROR) << "GetJoinCount SQL statement did not compile: "
                << db.GetErrorMessage();
    return false;
  }
  join_count.Reset(true);
  join_count.BindString(0, Serialize(group_key.owner));
  join_count.BindString(1, group_key.name);
  join_count.BindTime(2, joined_after);
  while (join_count.Step()) {
    output->join_count = join_count.ColumnInt64(0);
  }
  return join_count.Succeeded();
}

// NOTE: Join and bid history can expire up to a few hours before the interest
// group, since join and bid history are floored to UTC days, but the
// interest group doesn't necessarily expire at UTC midnight.
//
// The reason for this is to reduce the number of join / bid rows stored.
bool GetBidCount(sql::Database& db,
                 const blink::InterestGroupKey& group_key,
                 base::Time now,
                 BiddingBrowserSignalsPtr& output) {
  // clang-format off
  sql::Statement bid_count(
      db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT SUM(count) "
    "FROM bid_history "
    "WHERE owner = ? AND name = ? AND bid_time >= ?"));
  // clang-format on
  if (!bid_count.is_valid()) {
    DLOG(ERROR) << "GetBidCount SQL statement did not compile: "
                << db.GetErrorMessage();
    return false;
  }
  bid_count.Reset(true);
  bid_count.BindString(0, Serialize(group_key.owner));
  bid_count.BindString(1, group_key.name);
  bid_count.BindTime(2, now - blink::MaxInterestGroupLifetimeForMetadata());
  while (bid_count.Step()) {
    output->bid_count = bid_count.ColumnInt64(0);
  }
  return bid_count.Succeeded();
}

std::optional<DebugReportLockout> DoGetDebugReportLockout(
    sql::Database& db,
    std::optional<base::Time> ignore_before) {
  sql::Statement lockout(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT starting_time, duration "
                            "FROM lockout_debugging_only_report "
                            "WHERE starting_time > ?"));
  if (!lockout.is_valid()) {
    DLOG(ERROR) << "GetDebugReportLockout SQL statement did not compile: "
                << db.GetErrorMessage();
    return std::nullopt;
  }
  lockout.BindTime(0, ignore_before.value_or(base::Time::Min()));

  if (lockout.Step()) {
    return DebugReportLockout(lockout.ColumnTime(0),
                              lockout.ColumnTimeDelta(1));
  }

  if (!lockout.Succeeded()) {
    // When reading the table fails, treat it as there is an unexpired lockout,
    // to cautiously avoid sending more fDO reports than allowed in case there
    // is a lockout in the table.
    return DebugReportLockout(base::Time::Now(), base::Days(365));
  }
  // Reading the table succeeded but there was no row.
  return std::nullopt;
}

void DoGetAllDebugReportCooldowns(
    sql::Database& db,
    std::optional<base::Time> ignore_before,
    DebugReportLockoutAndCooldowns& debug_report_lockout_and_cooldowns) {
  sql::Statement cooldowns(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT origin, starting_time, type "
                            "FROM cooldown_debugging_only_report "
                            "WHERE starting_time > ?"));
  if (!cooldowns.is_valid()) {
    DLOG(ERROR) << "GetAllDebugReportCooldowns SQL statement did not compile: "
                << db.GetErrorMessage();
    return;
  }
  cooldowns.BindTime(0, ignore_before.value_or(base::Time::Min()));

  while (cooldowns.Step()) {
    url::Origin origin = DeserializeOrigin(cooldowns.ColumnStringView(0));
    debug_report_lockout_and_cooldowns
        .debug_report_cooldown_map[std::move(origin)] = DebugReportCooldown(
        cooldowns.ColumnTime(1),
        static_cast<DebugReportCooldownType>(cooldowns.ColumnInt(2)));
  }

  // TODO(qingxinwu): When reading the table fails, treat it as there is an
  // unexpired cooldown.
}

std::optional<DebugReportCooldown> DoGetDebugReportCooldownForOrigin(
    sql::Database& db,
    const url::Origin& origin,
    std::optional<base::Time> ignore_before) {
  sql::Statement cooldown_debugging_only_report(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT starting_time, type "
                            "FROM cooldown_debugging_only_report "
                            "WHERE origin = ? AND starting_time > ?"));
  if (!cooldown_debugging_only_report.is_valid()) {
    DLOG(ERROR) << "GetDebugReportCooldown SQL statement did not compile: "
                << db.GetErrorMessage();
    return std::nullopt;
  }
  cooldown_debugging_only_report.BindString(0, Serialize(origin));
  cooldown_debugging_only_report.BindTime(
      1, ignore_before.value_or(base::Time::Min()));

  if (cooldown_debugging_only_report.Step()) {
    return DebugReportCooldown(
        cooldown_debugging_only_report.ColumnTime(0),
        static_cast<DebugReportCooldownType>(
            cooldown_debugging_only_report.ColumnInt(1)));
  }

  if (!cooldown_debugging_only_report.Succeeded()) {
    // When reading the table fails, treat it as there is an unexpired cooldown,
    // to cautiously avoid sending more fDO reports than allowed in case there
    // is a cooldown in the table.
    return DebugReportCooldown(base::Time::Now(),
                               DebugReportCooldownType::kRestrictedCooldown);
  }
  // Reading the table succeeded but there was no row.
  return std::nullopt;
}

void DoGetDebugReportCooldowns(
    sql::Database& db,
    const base::flat_set<url::Origin>& origins,
    std::optional<base::Time> ignore_before,
    DebugReportLockoutAndCooldowns& debug_report_lockout_and_cooldowns) {
  for (const url::Origin& origin : origins) {
    std::optional<DebugReportCooldown> cooldown =
        DoGetDebugReportCooldownForOrigin(db, origin, ignore_before);
    if (cooldown.has_value()) {
      debug_report_lockout_and_cooldowns.debug_report_cooldown_map[origin] =
          *cooldown;
    }
  }
}

std::optional<std::vector<std::string>> DoGetInterestGroupNamesForOwner(
    sql::Database& db,
    const url::Origin& owner,
    base::Time now) {
  // clang-format off
  sql::Statement get_names(
    db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT name "
    "FROM interest_groups "
    "WHERE owner=? AND expiration>? "
    "ORDER BY expiration DESC"));
  // clang-format on

  if (!get_names.is_valid()) {
    return std::nullopt;
  }

  get_names.Reset(true);
  get_names.BindString(0, Serialize(owner));
  get_names.BindTime(1, now);

  std::vector<std::string> result;
  while (get_names.Step()) {
    result.push_back(get_names.ColumnString(0));
  }
  if (!get_names.Succeeded()) {
    return std::nullopt;
  }

  return result;
}

std::optional<std::vector<std::string>>
DoGetAllRegularInterestGroupNamesForOwner(sql::Database& db,
                                          const url::Origin& owner) {
  // clang-format off
  sql::Statement get_names(
    db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT name "
    "FROM interest_groups "
    "WHERE LENGTH(additional_bid_key) == 0 AND owner=? "
    "ORDER BY expiration DESC"));
  // clang-format on

  if (!get_names.is_valid()) {
    return std::nullopt;
  }

  get_names.Reset(true);
  get_names.BindString(0, Serialize(owner));

  std::vector<std::string> result;
  while (get_names.Step()) {
    result.push_back(get_names.ColumnString(0));
  }
  if (!get_names.Succeeded()) {
    return std::nullopt;
  }

  return result;
}

std::optional<std::vector<std::string>>
DoGetAllNegativeInterestGroupNamesForOwner(sql::Database& db,
                                           const url::Origin& owner) {
  // clang-format off
  sql::Statement get_names(
    db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT name "
    "FROM interest_groups "
    "WHERE NOT LENGTH(additional_bid_key) == 0 AND owner=? "
    "ORDER BY expiration DESC"));
  // clang-format on

  if (!get_names.is_valid()) {
    return std::nullopt;
  }

  get_names.Reset(true);
  get_names.BindString(0, Serialize(owner));

  std::vector<std::string> result;
  while (get_names.Step()) {
    result.push_back(get_names.ColumnString(0));
  }
  if (!get_names.Succeeded()) {
    return std::nullopt;
  }

  return result;
}

std::optional<StorageInterestGroup> DoGetStoredInterestGroup(
    sql::Database& db,
    const PassKey& passkey,
    const blink::InterestGroupKey& group_key,
    base::Time now) {
  // We need an InterestGroupsByName for DoGetViewAndClickCountsForGroups;
  // but that doesn't actually look at names.
  InterestGroupsByName interest_groups_by_name;
  StorageInterestGroup& db_interest_group = interest_groups_by_name["0"];

  if (!DoLoadInterestGroup(db, passkey, group_key, db_interest_group)) {
    return std::nullopt;
  }

  db_interest_group.bidding_browser_signals =
      blink::mojom::BiddingBrowserSignals::New();
  if (!GetJoinCount(db, group_key,
                    now - blink::MaxInterestGroupLifetimeForMetadata(),
                    db_interest_group.bidding_browser_signals)) {
    return std::nullopt;
  }
  if (!GetBidCount(db, group_key,
                   now - blink::MaxInterestGroupLifetimeForMetadata(),
                   db_interest_group.bidding_browser_signals)) {
    return std::nullopt;
  }
  if (!GetPreviousWins(db, group_key,
                       now - blink::MaxInterestGroupLifetimeForMetadata(),
                       db_interest_group.bidding_browser_signals)) {
    return std::nullopt;
  }

  if (!DoGetViewAndClickCountsForGroups(db, now,
                                        db_interest_group.interest_group.owner,
                                        interest_groups_by_name)) {
    return std::nullopt;
  }
  return std::move(db_interest_group);
}

std::optional<std::vector<InterestGroupUpdateParameter>>
DoGetInterestGroupsForUpdate(sql::Database& db,
                             const url::Origin& owner,
                             base::Time now,
                             size_t groups_limit) {
  std::vector<InterestGroupUpdateParameter> result;

  // To maximize the chance of reusing open sockets from a previous batch, sort
  // the storage groups by joining origin.
  //
  // To mitigate potential side-channel vulnerabilities and prevent updates from
  // occurring in a predictable sequence, such as the order of expiration, data
  // is shuffled when reading from the database.
  sql::Statement get_interest_group_update_parameters(db.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT name, update_url, joining_origin "
      "FROM interest_groups "
      "WHERE owner=? AND expiration>? AND ?>=next_update_after "
      "ORDER BY joining_origin, RANDOM() "
      "LIMIT ?"));

  if (!get_interest_group_update_parameters.is_valid()) {
    return std::nullopt;
  }

  get_interest_group_update_parameters.Reset(true);
  get_interest_group_update_parameters.BindString(0, Serialize(owner));
  get_interest_group_update_parameters.BindTime(1, now);
  get_interest_group_update_parameters.BindTime(2, now);
  get_interest_group_update_parameters.BindInt64(3, groups_limit);

  while (get_interest_group_update_parameters.Step()) {
    std::optional<GURL> update_url = DeserializeURL(
        get_interest_group_update_parameters.ColumnStringView(1));
    if (!update_url.has_value()) {
      continue;
    }

    result.emplace_back(
        blink::InterestGroupKey(
            owner, get_interest_group_update_parameters.ColumnString(0)),
        update_url.value(),
        DeserializeOrigin(
            get_interest_group_update_parameters.ColumnStringView(2)));
  }
  if (!get_interest_group_update_parameters.Succeeded()) {
    return std::nullopt;
  }
  return result;
}

std::optional<std::vector<StorageInterestGroup>> DoGetInterestGroupsForOwner(
    sql::Database& db,
    const PassKey& passkey,
    const url::Origin& owner,
    base::Time now) {
  TRACE_EVENT("fledge", "DoGetInterestGroupsForOwner");
  sql::Transaction transaction(&db);

  if (!transaction.Begin()) {
    return std::nullopt;
  }

  InterestGroupsByName interest_group_by_name;
  {
    TRACE_EVENT("fledge", "load_from_interest_groups_table");

    // clang-format off
    sql::Statement load(
        db.GetCachedStatement(SQL_FROM_HERE,
          "SELECT " COMMON_INTEREST_GROUPS_QUERY_FIELDS ","
            "name "
          "FROM interest_groups "
          "WHERE owner=? AND expiration>?"));
    // clang-format on

    if (!load.is_valid()) {
      return std::nullopt;
    }

    load.BindString(0, Serialize(owner));
    load.BindTime(1, now);

    while (load.Step()) {
      std::string name = load.ColumnString(31);
      StorageInterestGroup& db_interest_group = interest_group_by_name[name];
      db_interest_group.bidding_browser_signals =
          blink::mojom::BiddingBrowserSignals::New();

      db_interest_group.interest_group.owner = owner;
      db_interest_group.interest_group.name = std::move(name);

      PopulateInterestGroupFromQueryResult(load, passkey, db_interest_group);
    }

    if (!load.Succeeded()) {
      return std::nullopt;
    }
  }
  {
    TRACE_EVENT("fledge", "load_from_join_history_table");

    // clang-format off
    sql::Statement join_count(
        db.GetCachedStatement(SQL_FROM_HERE,
      "SELECT name, SUM(count) "
      "FROM join_history "
      "WHERE owner=? AND join_time>=? "
      "GROUP BY name"));
    // clang-format on

    join_count.BindString(0, Serialize(owner));
    join_count.BindTime(1, now - blink::MaxInterestGroupLifetimeForMetadata());

    while (join_count.Step()) {
      auto it = interest_group_by_name.find(join_count.ColumnStringView(0));
      if (it == interest_group_by_name.end()) {
        // TODO(yaoxia): Return std::nullopt?
        continue;
      }

      StorageInterestGroup& db_interest_group = it->second;

      db_interest_group.bidding_browser_signals->join_count =
          join_count.ColumnInt64(1);
    }

    if (!join_count.Succeeded()) {
      return std::nullopt;
    }
  }
  {
    TRACE_EVENT("fledge", "load_from_bid_history_table");

    // clang-format off
    sql::Statement bid_count(
        db.GetCachedStatement(SQL_FROM_HERE,
      "SELECT name, SUM(count) "
      "FROM bid_history "
      "WHERE owner=? AND bid_time>=? "
      "GROUP BY name"));
    // clang-format on

    bid_count.BindString(0, Serialize(owner));
    bid_count.BindTime(1, now - blink::MaxInterestGroupLifetimeForMetadata());

    while (bid_count.Step()) {
      auto it = interest_group_by_name.find(bid_count.ColumnStringView(0));
      if (it == interest_group_by_name.end()) {
        // TODO(yaoxia): Return std::nullopt?
        continue;
      }

      StorageInterestGroup& db_interest_group = it->second;

      db_interest_group.bidding_browser_signals->bid_count =
          bid_count.ColumnInt64(1);
    }

    if (!bid_count.Succeeded()) {
      return std::nullopt;
    }
  }
  {
    TRACE_EVENT("fledge", "load_from_prev_wins_table");

    // clang-format off
    sql::Statement prev_wins(
        db.GetCachedStatement(SQL_FROM_HERE,
                              "SELECT name, win_time, ad "
                              "FROM win_history "
                              "WHERE owner=? AND win_time>=? "
                              "ORDER BY name, win_time DESC "));
    // clang-format on

    prev_wins.BindString(0, Serialize(owner));
    prev_wins.BindTime(1, now - blink::MaxInterestGroupLifetimeForMetadata());

    while (prev_wins.Step()) {
      auto it = interest_group_by_name.find(prev_wins.ColumnStringView(0));
      if (it == interest_group_by_name.end()) {
        // TODO(yaoxia): Return std::nullopt?
        continue;
      }

      StorageInterestGroup& db_interest_group = it->second;

      PreviousWinPtr prev_win = blink::mojom::PreviousWin::New(
          /*time=*/prev_wins.ColumnTime(1),
          /*ad_json=*/prev_wins.ColumnString(2));
      db_interest_group.bidding_browser_signals->prev_wins.push_back(
          std::move(prev_win));
    }

    if (!prev_wins.Succeeded()) {
      return std::nullopt;
    }
  }
  {
    TRACE_EVENT("fledge", "load_from_clicks_views_table");

    if (!DoGetViewAndClickCountsForGroups(db, now, owner,
                                          interest_group_by_name)) {
      return std::nullopt;
    }
  }
  if (!transaction.Commit()) {
    return std::nullopt;
  }

  std::vector<StorageInterestGroup> result;
  for (auto& [key, value] : interest_group_by_name) {
    result.push_back(std::move(value));
  }

  // Sort `result` by decreasing expiration time.
  std::sort(result.begin(), result.end(),
            [](const StorageInterestGroup& a, const StorageInterestGroup& b) {
              return a.interest_group.expiry > b.interest_group.expiry;
            });

  return result;
}

std::optional<std::vector<blink::InterestGroupKey>>
DoGetInterestGroupNamesForJoiningOrigin(sql::Database& db,
                                        const url::Origin& joining_origin,
                                        base::Time now) {
  std::vector<blink::InterestGroupKey> result;

  // clang-format off
  sql::Statement load(
      db.GetCachedStatement(SQL_FROM_HERE,
        "SELECT owner,name "
        "FROM interest_groups "
        "WHERE joining_origin=? AND expiration>?"));
  // clang-format on

  if (!load.is_valid()) {
    DLOG(ERROR) << "GetInterestGroupNamesForJoiningOrigin SQL statement did "
                   "not compile: "
                << db.GetErrorMessage();
    return std::nullopt;
  }

  load.Reset(true);
  load.BindString(0, Serialize(joining_origin));
  load.BindTime(1, now);

  while (load.Step()) {
    result.emplace_back(DeserializeOrigin(load.ColumnStringView(0)),
                        load.ColumnString(1));
  }
  if (!load.Succeeded()) {
    return std::nullopt;
  }
  return result;
}

bool DoDeleteViewClickCounts(sql::Database& db) {
  sql::Statement remove_view_clicks(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM view_and_click_events"));
  if (!remove_view_clicks.is_valid()) {
    return false;
  }

  remove_view_clicks.Reset(true);
  return remove_view_clicks.Run();
}

bool DoDeleteViewClickCountsForProvider(
    sql::Database& db,
    const StoragePartition::StorageKeyMatcherFunction& storage_key_matcher) {
  std::vector<url::Origin> providers_to_delete;

  sql::Statement load(db.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT DISTINCT provider_origin FROM view_and_click_events"));
  if (!load.is_valid()) {
    return false;
  }
  load.Reset(true);
  while (load.Step()) {
    url::Origin origin = DeserializeOrigin(load.ColumnStringView(0));
    if (storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(origin))) {
      providers_to_delete.push_back(std::move(origin));
    }
  }

  sql::Statement del(db.GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM view_and_click_events WHERE provider_origin=?"));
  if (!del.is_valid()) {
    return false;
  }
  for (const url::Origin& origin : providers_to_delete) {
    del.Reset(true);
    del.BindString(0, origin.Serialize());
    if (!del.Run()) {
      return false;
    }
  }
  return true;
}

bool DoDeleteInterestGroupData(
    sql::Database& db,
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    bool user_initiated_deletion) {
  const base::Time distant_past = base::Time::Min();
  sql::Transaction transaction(&db);

  if (!transaction.Begin()) {
    return false;
  }

  // For view & click events, we generally delete everything even when the user
  // asked for a subset of sites, since we do not know what top-level site the
  // events are associated with, so we have to be conservative to make sure to
  // match everything that may be expected. Doing this for Clear-Site-Data,
  // however, would let sites hostilely delete clickiness data of others,
  // so if `user_initiated_deletion` is false, only things provided by that
  // origin are deleted.
  if (user_initiated_deletion || storage_key_matcher.is_null()) {
    if (!DoDeleteViewClickCounts(db)) {
      return false;
    }
  } else {
    if (!DoDeleteViewClickCountsForProvider(db, storage_key_matcher)) {
      return false;
    }
  }

  std::vector<url::Origin> affected_origins;
  std::optional<std::vector<url::Origin>> maybe_all_origins =
      DoGetAllInterestGroupOwners(db, distant_past);

  if (!maybe_all_origins) {
    return false;
  }
  for (const url::Origin& origin : maybe_all_origins.value()) {
    if (storage_key_matcher.is_null() ||
        storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(origin))) {
      affected_origins.push_back(origin);
    }
  }

  for (const auto& affected_origin : affected_origins) {
    std::optional<std::vector<std::string>> maybe_group_names =
        DoGetInterestGroupNamesForOwner(db, affected_origin, distant_past);
    if (!maybe_group_names) {
      return false;
    }
    for (const auto& group_name : maybe_group_names.value()) {
      if (!DoRemoveInterestGroup(
              db, blink::InterestGroupKey(affected_origin, group_name))) {
        return false;
      }
    }
  }

  affected_origins.clear();
  maybe_all_origins = DoGetAllInterestGroupJoiningOrigins(db, distant_past);
  if (!maybe_all_origins) {
    return false;
  }
  for (const url::Origin& origin : maybe_all_origins.value()) {
    if (storage_key_matcher.is_null() ||
        storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(origin))) {
      affected_origins.push_back(origin);
    }
  }
  for (const auto& affected_origin : affected_origins) {
    std::optional<std::vector<blink::InterestGroupKey>> maybe_group_names =
        DoGetInterestGroupNamesForJoiningOrigin(db, affected_origin,
                                                distant_past);
    if (!maybe_group_names) {
      return false;
    }
    for (const auto& interest_group_key : maybe_group_names.value()) {
      if (!DoRemoveInterestGroup(db, interest_group_key)) {
        return false;
      }
    }
  }

  return transaction.Commit();
}

bool DoSetInterestGroupPriority(sql::Database& db,
                                const blink::InterestGroupKey& group_key,
                                double priority) {
  // clang-format off
  sql::Statement set_priority_sql(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE interest_groups "
          "SET priority=? "
          "WHERE owner=? AND name=?"));
  // clang-format on
  if (!set_priority_sql.is_valid()) {
    DLOG(ERROR) << "SetPriority SQL statement did not compile.";
    return false;
  }
  set_priority_sql.Reset(true);
  set_priority_sql.BindDouble(0, priority);
  set_priority_sql.BindString(1, Serialize(group_key.owner));
  set_priority_sql.BindString(2, group_key.name);
  return set_priority_sql.Run();
}

bool DoSetInterestGroupPrioritySignalsOverrides(
    sql::Database& db,
    const blink::InterestGroupKey& group_key,
    const std::optional<base::flat_map<std::string, double>>&
        priority_signals_overrides) {
  // clang-format off
  sql::Statement update_priority_signals_overrides_sql(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE interest_groups "
          "SET priority_signals_overrides=? "
          "WHERE owner=? AND name=?"));
  // clang-format on
  if (!update_priority_signals_overrides_sql.is_valid()) {
    DLOG(ERROR) << "SetPrioritySignalsOverrides SQL statement did not compile.";
    return false;
  }
  update_priority_signals_overrides_sql.Reset(true);
  update_priority_signals_overrides_sql.BindString(
      0, Serialize(priority_signals_overrides));
  update_priority_signals_overrides_sql.BindString(1,
                                                   Serialize(group_key.owner));
  update_priority_signals_overrides_sql.BindString(2, group_key.name);
  return update_priority_signals_overrides_sql.Run();
}

bool DeleteOldJoins(sql::Database& db, base::Time cutoff) {
  sql::Statement del_join_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM join_history WHERE join_time <= ?"));
  if (!del_join_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldJoins SQL statement did not compile.";
    return false;
  }
  del_join_history.Reset(true);
  del_join_history.BindTime(0, cutoff);
  if (!del_join_history.Run()) {
    DLOG(ERROR) << "Could not delete old join_history.";
    return false;
  }
  return true;
}

bool DeleteOldBids(sql::Database& db, base::Time cutoff) {
  sql::Statement del_bid_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM bid_history WHERE bid_time <= ?"));
  if (!del_bid_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldBids SQL statement did not compile.";
    return false;
  }
  del_bid_history.Reset(true);
  del_bid_history.BindTime(0, cutoff);
  if (!del_bid_history.Run()) {
    DLOG(ERROR) << "Could not delete old bid_history.";
    return false;
  }
  return true;
}

bool DeleteOldWins(sql::Database& db, base::Time cutoff) {
  sql::Statement del_win_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM win_history WHERE win_time <= ?"));
  if (!del_win_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldWins SQL statement did not compile.";
    return false;
  }
  del_win_history.Reset(true);
  del_win_history.BindTime(0, cutoff);
  if (!del_win_history.Run()) {
    DLOG(ERROR) << "Could not delete old win_history.";
    return false;
  }
  return true;
}

bool DoClearExcessInterestGroups(
    sql::Database& db,
    const url::Origin& affected_origin,
    const std::optional<std::vector<std::string>> maybe_interest_groups,
    size_t max_owner_interest_groups) {
  if (!maybe_interest_groups) {
    return false;
  }
  for (size_t group_idx = max_owner_interest_groups;
       group_idx < maybe_interest_groups.value().size(); group_idx++) {
    if (!DoRemoveInterestGroup(
            db,
            blink::InterestGroupKey(
                affected_origin, maybe_interest_groups.value()[group_idx]))) {
      return false;
    }
  }
  return true;
}

bool ClearExcessInterestGroups(sql::Database& db,
                               size_t max_owners,
                               size_t max_owner_regular_interest_groups,
                               size_t max_owner_negative_interest_groups) {
  const base::Time distant_past = base::Time::Min();
  const std::optional<std::vector<url::Origin>> maybe_all_origins =
      DoGetAllInterestGroupOwners(db, distant_past);
  if (!maybe_all_origins) {
    return false;
  }
  for (size_t owner_idx = 0; owner_idx < maybe_all_origins.value().size();
       owner_idx++) {
    const url::Origin& affected_origin = maybe_all_origins.value()[owner_idx];
    if (!DoClearExcessInterestGroups(
            db, affected_origin,
            DoGetAllRegularInterestGroupNamesForOwner(db, affected_origin),
            owner_idx < max_owners ? max_owner_regular_interest_groups : 0)) {
      return false;
    }
    if (!DoClearExcessInterestGroups(
            db, affected_origin,
            DoGetAllNegativeInterestGroupNamesForOwner(db, affected_origin),
            owner_idx < max_owners ? max_owner_negative_interest_groups : 0)) {
      return false;
    }
  }
  return true;
}

bool ClearExpiredInterestGroups(sql::Database& db,
                                base::Time expiration_before) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement expired_interest_group(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT owner, name "
                            "FROM interest_groups "
                            "WHERE expiration<=?"));
  if (!expired_interest_group.is_valid()) {
    DLOG(ERROR) << "ClearExpiredInterestGroups SQL statement did not compile.";
    return false;
  }

  expired_interest_group.Reset(true);
  expired_interest_group.BindTime(0, expiration_before);
  std::vector<blink::InterestGroupKey> expired_groups;
  while (expired_interest_group.Step()) {
    expired_groups.emplace_back(
        DeserializeOrigin(expired_interest_group.ColumnStringView(0)),
        expired_interest_group.ColumnString(1));
  }
  if (!expired_interest_group.Succeeded()) {
    DLOG(ERROR) << "ClearExpiredInterestGroups could not get expired groups.";
    // Keep going so we can clear any groups that we did get.
  }
  for (const auto& interest_group : expired_groups) {
    if (!DoRemoveInterestGroup(db, interest_group)) {
      return false;
    }
  }
  return transaction.Commit();
}

// Removes interest groups so that per-owner limit is respected.
bool ClearExcessiveStorage(sql::Database& db, size_t max_owner_storage_size) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  // We go through groups for each owner, starting with the ones that expire
  // later, accumulating the stored size. If the accumulated size is too much,
  // we start marking groups for deletion. This means that the interest groups
  // expiring soonest will be deleted if we need to free up space.
  // clang-format off
  sql::Statement excessive_storage_groups(db.GetCachedStatement(
      SQL_FROM_HERE,
        "SELECT owner, name, storage_size "
        "FROM interest_groups "
        "ORDER BY owner, expiration DESC"
      ));
  // clang-format on
  if (!excessive_storage_groups.is_valid()) {
    return false;
  }

  excessive_storage_groups.Reset(true);
  std::vector<blink::InterestGroupKey> groups_to_remove;
  std::optional<url::Origin> previous;
  size_t cum_size;
  while (excessive_storage_groups.Step()) {
    url::Origin group_owner =
        DeserializeOrigin(excessive_storage_groups.ColumnStringView(0));
    std::string group_name = excessive_storage_groups.ColumnString(1);
    size_t group_size = excessive_storage_groups.ColumnInt64(2);

    if (!previous || *previous != group_owner) {
      previous = group_owner;
      cum_size = 0;
    }
    if (cum_size + group_size > max_owner_storage_size) {
      groups_to_remove.emplace_back(std::move(group_owner),
                                    std::move(group_name));
    } else {
      cum_size += group_size;
    }
  }
  if (!excessive_storage_groups.Succeeded()) {
    DLOG(ERROR) << "ClearExcessiveStorage could not get expired groups.";
    // Keep going so we can clear any groups that we did get.
  }
  for (const auto& interest_group : groups_to_remove) {
    if (!DoRemoveInterestGroup(db, interest_group)) {
      return false;
    }
  }
  return transaction.Commit();
}

bool ClearExpiredKAnon(sql::Database& db, base::Time cutoff) {
  // clang-format off
  sql::Statement expired_k_anon(
      db.GetCachedStatement(SQL_FROM_HERE,
                "DELETE FROM joined_k_anon "
                "WHERE last_reported_to_anon_server_time < ?"));
  // clang-format on
  if (!expired_k_anon.is_valid()) {
    DLOG(ERROR) << "ClearExpiredKAnon SQL statement did not compile.";
    return false;
  }

  expired_k_anon.Reset(true);
  expired_k_anon.BindTime(0, cutoff);
  return expired_k_anon.Run();
}

bool DeleteExpiredDebugReportCooldown(sql::Database& db, base::Time now) {
  // clang-format off
  sql::Statement delete_cooldown(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM cooldown_debugging_only_report "
                            "WHERE (type==? AND starting_time<?) OR "
                                  "(type==? AND starting_time<?)"));
  // clang-format on
  if (!delete_cooldown.is_valid()) {
    DLOG(ERROR)
        << "DeleteExpiredDebugReportCooldown SQL statement did not compile.";
    return false;
  }

  delete_cooldown.Reset(true);
  std::optional<base::TimeDelta> short_duration =
      ConvertDebugReportCooldownTypeToDuration(
          DebugReportCooldownType::kShortCooldown);
  std::optional<base::TimeDelta> restricted_duration =
      ConvertDebugReportCooldownTypeToDuration(
          DebugReportCooldownType::kRestrictedCooldown);
  CHECK(short_duration.has_value());
  CHECK(restricted_duration.has_value());

  delete_cooldown.BindInt(
      0, static_cast<int>(DebugReportCooldownType::kShortCooldown));
  delete_cooldown.BindTime(1, now - *short_duration);
  delete_cooldown.BindInt(
      2, static_cast<int>(DebugReportCooldownType::kRestrictedCooldown));
  delete_cooldown.BindTime(3, now - *restricted_duration);

  if (!delete_cooldown.Run()) {
    DLOG(ERROR) << "Could not delete old debug report cooldown.";
    return false;
  }
  return true;
}

bool DoDeleteAllDebugReportCooldowns(sql::Database& db) {
  sql::Statement clear_cooldown(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM cooldown_debugging_only_report"));
  return clear_cooldown.Run();
}

bool ClearExpiredBiddingAndAuctionKeys(sql::Database& db, base::Time now) {
  sql::Statement clear_expired_keys(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM bidding_and_auction_server_keys "
                            "WHERE expiration<?"));
  clear_expired_keys.BindTime(0, now);
  return clear_expired_keys.Run();
}

bool ClearExpiredCachedKAnonymityHashes(sql::Database& db, base::Time now) {
  const base::TimeDelta ttl = features::kFledgeCacheKAnonHashedKeysTtl.Get();
  sql::Statement clear_expired_cache_entries(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM cached_k_anonymity_hashes "
                            "WHERE fetch_time<?"));
  if (!clear_expired_cache_entries.is_valid()) {
    DLOG(ERROR)
        << "ClearExpiredCachedKAnonymityHashes SQL statement did not compile.";
    return {};
  }
  clear_expired_cache_entries.BindTime(0, now - ttl);
  return clear_expired_cache_entries.Run();
}

// Used by CompactClickiness(). The number of events that occurred at the
// base::Time in CompactionMap.
using ClickinessCount = int64_t;
// Used by CompactClickiness(). The raw integer timestamp stored in the database
// for clickiness events. Stored as microseconds since the Windows epoch.
using ClickinessIntTimestamp = int64_t;
// Used by CompactClickiness(). Temporary storage used during the compaction
// process.
using CompactionMap = std::map<base::Time, ClickinessCount>;

// A helper function for ClickinessCompactionRawToMap(), which is itself a
// helper for CompactClickiness(). For a given raw `int_timestamp` loaded from
// the database (which is in microseconds since the Windows epoch) and `count`
// of events at that timestamp, update the count at the compacted time.
//
// Events older than 1 hour (determined using `now`) will be grouped by hour,
// and events older than 1 day will be grouped by day.
void ClickinessCompactionAddTimestampToMap(base::Time now,
                                           ClickinessIntTimestamp int_timestamp,
                                           ClickinessCount count,
                                           CompactionMap& map) {
  base::Time timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(int_timestamp));
  base::TimeDelta event_age = now - timestamp;
  if (event_age <= base::Hours(1)) {
    // Count can be > 1 here if the clock is rolled back.
    map[timestamp] += count;
  } else if (event_age <= base::Days(1)) {
    map[TruncateToHour(timestamp)] += count;
  } else if (event_age <= base::Days(90)) {
    map[TruncateToDay(timestamp)] += count;
  }
}

// A helper function for CompactClickiness(). For each compacted and uncompacted
// events protobuf loaded from the database (`raw`), compact those events into
// the returned map.
//
// Events older than 1 hour (determined using `now`) will be grouped by hour,
// and events older than 1 day will be grouped by day.
//
// ClickinessCompactionMapToRaw() will then convert the returned map back into
// raw protobuf form that can be written to the database, splitting events into
// compacted and uncompacted events, with events younger than an hour going into
// uncompacted events.
//
// Returns std::nullopt on error.
std::optional<CompactionMap> ClickinessCompactionRawToMap(
    base::Time now,
    const InterestGroupStorage::ClickinessCompactionRawEvents& raw) {
  CompactionMap map;

  {
    // Write raw uncompacted events.
    ListOfTimestamps uncompacted_events;
    if (!uncompacted_events.ParseFromString(raw.uncompacted_events)) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.ProtoDeserializationResult.ListOfTimestamps",
          InterestGroupStorageProtoSerializationResult::kFailed);
      return std::nullopt;
    }
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoDeserializationResult.ListOfTimestamps",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
    for (ClickinessIntTimestamp int_timestamp :
         uncompacted_events.timestamps()) {
      ClickinessCompactionAddTimestampToMap(now, int_timestamp, /*count=*/1,
                                            map);
    }
  }

  {
    // Add raw compacted events.
    ListOfTimestampAndCounts compacted_events;
    if (!compacted_events.ParseFromString(raw.compacted_events)) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.ProtoDeserializationResult.ListOfTimestamps",
          InterestGroupStorageProtoSerializationResult::kFailed);
      return std::nullopt;
    }
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoDeserializationResult.ListOfTimestamps",
        InterestGroupStorageProtoSerializationResult::kSucceeded);
    for (ListOfTimestampAndCounts_Entry entry :
         compacted_events.timestamp_and_counts()) {
      ClickinessCompactionAddTimestampToMap(now, entry.timestamp(),
                                            /*count=*/entry.count(), map);
    }
  }

  return map;
}

// A helper function for CompactClickiness(). Converts `map` (produced by
// ClickinessCompactionRawToMap()) back to the raw protobuf form that can be
// written to the database, splitting events into compacted and uncompacted
// events, with events younger than an hour going into
// uncompacted events.
//
// Returns std::nullopt on error.
std::optional<InterestGroupStorage::ClickinessCompactionRawEvents>
ClickinessCompactionMapToRaw(base::Time now, const CompactionMap& map) {
  InterestGroupStorage::ClickinessCompactionRawEvents raw;
  ListOfTimestamps uncompacted_events;
  ListOfTimestampAndCounts compacted_events;

  // Populate proto objects.
  for (const auto& [timestamp, count] : map) {
    base::TimeDelta event_age = now - timestamp;

    if (event_age <= base::Hours(1)) {
      for (int i = 0; i < count; i++) {
        // While (possibly) rare, it's possible for more than one of the same
        // event to be recorded at the same time.
        uncompacted_events.add_timestamps(
            timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
      }
    } else {
      ListOfTimestampAndCounts_Entry* entry =
          compacted_events.add_timestamp_and_counts();
      entry->set_timestamp(
          timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
      entry->set_count(count);
    }
  }

  // Serialize proto objects to raw strings.
  if (!uncompacted_events.SerializeToString(&raw.uncompacted_events) ||
      !compacted_events.SerializeToString(&raw.compacted_events)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.ProtoSerializationResult.ListOfTimestamps",
        InterestGroupStorageProtoSerializationResult::kFailed);
    return std::nullopt;
  }

  base::UmaHistogramEnumeration(
      "Storage.InterestGroup.ProtoSerializationResult.ListOfTimestamps",
      InterestGroupStorageProtoSerializationResult::kSucceeded);

  return raw;
}

// Computation portion of compaction --- takes in raw protobufs from database,
// and returns compacted portion, unless it fails.
std::optional<InterestGroupStorage::ClickinessCompactionRawEvents>
ComputeCompactClickiness(
    base::Time now,
    const InterestGroupStorage::ClickinessCompactionRawEvents& raw) {
  // Perform the compaction in a std::map. This avoids n^2 performance of
  // in-place insertions. The raw -> map and map -> raw phases are separate for
  // better structuring. The map -> raw phase will write events back
  // to the uncompacted list that are less than an hour old.
  std::optional<CompactionMap> map = ClickinessCompactionRawToMap(now, raw);
  if (!map) {
    return std::nullopt;
  }
  return ClickinessCompactionMapToRaw(now, *map);
}

// To reduce storage utilization for view and click events, compact events older
// than an hour to store only a per-hour count, and events older than a day to
// store only a per-day count.
bool CompactClickiness(sql::Database& db, base::Time now) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }
  // For each primary key in view_and_click_events (which is the
  // (provider_origin, eligible_origin) tuple), we load all uncompacted and
  // compacted view and click events, compact them in memory, then write the
  // results back to that (provider_origin, eligible_origin).

  sql::Statement get_all_rows_sql(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT provider_origin,"
                            "eligible_origin,"
                            "uncompacted_view_events,"
                            "compacted_view_events,"
                            "uncompacted_click_events,"
                            "compacted_click_events "
                            "FROM view_and_click_events"));
  if (!get_all_rows_sql.is_valid()) {
    return false;
  }

  sql::Statement update_row(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "UPDATE view_and_click_events "
                            "SET uncompacted_view_events=?,"
                            "compacted_view_events=?,"
                            "uncompacted_click_events=?,"
                            "compacted_click_events=? "
                            "WHERE provider_origin=? AND eligible_origin=?"));
  if (!update_row.is_valid()) {
    return false;
  }

  sql::Statement delete_row(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM view_and_click_events "
                            "WHERE provider_origin=? AND eligible_origin=?"));
  if (!delete_row.is_valid()) {
    return false;
  }

  // For each (provider_origin, eligible_origin) in view_and_click_events,
  // compact that (provider_origin, eligible_origin), and write the updated
  // results:
  while (get_all_rows_sql.Step()) {
    update_row.Reset(/*clear_bound_vars=*/true);

    // Read raw compacted and uncompacted events into `raw`, and compact them
    // for both view and clicks, clearing memory we don't need anymore as
    // we go to reduce peak memory consumption, avoiding keeping excess copies
    // of all events in memory.
    std::string provider_origin(get_all_rows_sql.ColumnStringView(0));
    std::string eligible_origin(get_all_rows_sql.ColumnStringView(1));
    // Views
    std::optional<InterestGroupStorage::ClickinessCompactionRawEvents>
        raw_views;
    {
      InterestGroupStorage::ClickinessCompactionRawEvents raw;
      raw.uncompacted_events = get_all_rows_sql.ColumnStringView(2);
      raw.compacted_events = get_all_rows_sql.ColumnStringView(3);
      raw_views = ComputeCompactClickiness(now, raw);
    }
    if (!raw_views) {
      return false;
    }

    // Clicks
    std::optional<InterestGroupStorage::ClickinessCompactionRawEvents>
        raw_clicks;
    {
      InterestGroupStorage::ClickinessCompactionRawEvents raw;
      raw.uncompacted_events = get_all_rows_sql.ColumnStringView(4);
      raw.compacted_events = get_all_rows_sql.ColumnStringView(5);
      raw_clicks = ComputeCompactClickiness(now, raw);
    }
    if (!raw_clicks) {
      return false;
    }

    // Done compacting this (provider_origin, eligible_origin) row.
    // If something is left, write the results back to the database,
    // otherwise delete it.
    if (raw_views->uncompacted_events.empty() &&
        raw_views->compacted_events.empty() &&
        raw_clicks->uncompacted_events.empty() &&
        raw_clicks->compacted_events.empty()) {
      delete_row.Reset(/*clear_bound_vars=*/true);
      delete_row.BindString(0, provider_origin);
      delete_row.BindString(1, eligible_origin);
      if (!delete_row.Run()) {
        return false;
      }
    } else {
      update_row.BindString(0, raw_views->uncompacted_events);
      update_row.BindString(1, raw_views->compacted_events);
      update_row.BindString(2, raw_clicks->uncompacted_events);
      update_row.BindString(3, raw_clicks->compacted_events);
      update_row.BindString(4, provider_origin);
      update_row.BindString(5, eligible_origin);

      if (!update_row.Run()) {
        return false;
      }
    }
  }
  if (!get_all_rows_sql.Succeeded()) {
    return false;
  }

  return transaction.Commit();
}

bool DoSetBiddingAndAuctionServerKeys(sql::Database& db,
                                      const url::Origin& coordinator,
                                      std::string serialized_keys,
                                      base::Time expiration) {
  sql::Statement insert_keys_statement(db.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO "
      "bidding_and_auction_server_keys(coordinator,keys,expiration) "
      "VALUES  (?,?,?)"));
  if (!insert_keys_statement.is_valid()) {
    DLOG(ERROR)
        << "DoSetBiddingAndAuctionServerKeys SQL statement did not compile.";
    return false;
  }

  insert_keys_statement.Reset(true);
  insert_keys_statement.BindString(0, Serialize(coordinator));

  insert_keys_statement.BindBlob(1, std::move(serialized_keys));
  insert_keys_statement.BindTime(2, expiration);
  return insert_keys_statement.Run();
}

std::pair<base::Time, std::string> DoGetBiddingAndAuctionServerKeys(
    sql::Database& db,
    const url::Origin& coordinator) {
  sql::Statement keys_statement(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT expiration, keys "
                            "FROM bidding_and_auction_server_keys "
                            "WHERE coordinator = ? AND expiration>?"));
  if (!keys_statement.is_valid()) {
    DLOG(ERROR)
        << "DoGetBiddingAndAuctionServerKeys SQL statement did not compile.";
    return {};
  }

  keys_statement.Reset(true);
  keys_statement.BindString(0, Serialize(coordinator));
  keys_statement.BindTime(1, base::Time::Now());

  if (keys_statement.Step()) {
    base::Time expiration = keys_statement.ColumnTime(0);
    std::string key_blob = keys_statement.ColumnString(1);
    return {expiration, std::move(key_blob)};
  }
  return {base::Time::Min(), {}};
}

bool DoWriteHashedKAnonymityKeysToCache(
    sql::Database& db,
    const std::vector<std::string>& positive_hashed_keys,
    const std::vector<std::string>& negative_hashed_keys,
    base::Time fetch_time) {
  sql::Statement insert_keys_statement(db.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO "
      "cached_k_anonymity_hashes(key_hash,is_k_anon,fetch_time) "
      "VALUES  (?,?,?)"));
  if (!insert_keys_statement.is_valid()) {
    DLOG(ERROR)
        << "DoWriteHashedKAnonymityKeysToCache SQL statement did not compile.";
    return {};
  }

  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }
  for (const std::string& key : positive_hashed_keys) {
    insert_keys_statement.Reset(true);
    insert_keys_statement.BindBlob(0, key);
    insert_keys_statement.BindBool(1, true);
    insert_keys_statement.BindTime(2, fetch_time);
    if (!insert_keys_statement.Run()) {
      return false;
    }
  }
  for (const std::string& key : negative_hashed_keys) {
    insert_keys_statement.Reset(true);
    insert_keys_statement.BindBlob(0, key);
    insert_keys_statement.BindBool(1, false);
    insert_keys_statement.BindTime(2, fetch_time);
    if (!insert_keys_statement.Run()) {
      return false;
    }
  }
  return transaction.Commit();
}

bool AppendKAnonCacheQueryStatement(
    sql::Database& db,
    sql::StatementID statement_id,
    size_t number_of_keys,
    std::vector<std::pair<size_t, std::unique_ptr<sql::Statement>>>* levels) {
  if (number_of_keys < 1) {
    return false;
  }

  std::string query_string =
      "SELECT key_hash, is_k_anon "
      "FROM cached_k_anonymity_hashes "
      "WHERE fetch_time >= ? AND key_hash ";
  if (number_of_keys > 1) {
    base::StrAppend(&query_string, {"IN (?"});
    base::StrAppend(&query_string,
                    std::vector<std::string>(number_of_keys - 1, ",?"));
    base::StrAppend(&query_string, {")"});
  } else {
    base::StrAppend(&query_string, {"= ?"});
  }

  std::unique_ptr<sql::Statement> statement = std::make_unique<sql::Statement>(
      db.GetCachedStatement(statement_id, query_string));
  if (!statement->is_valid()) {
    DLOG(ERROR) << "CreateKAnonCacheQueryStatement "
                << "for number_of_keys=" << number_of_keys
                << " SQL statement did not compile.";
    return false;
  }

  levels->emplace_back(number_of_keys, std::move(statement));
  return true;
}

InterestGroupStorage::KAnonymityCacheResponse
DoLoadPositiveHashedKAnonymityKeysFromCache(
    sql::Database& db,
    const std::vector<std::string>& keys,
    const base::Time fetch_time) {
  const base::TimeDelta ttl = features::kFledgeCacheKAnonHashedKeysTtl.Get();
  const base::Time min_valid_time = fetch_time - ttl;

  // These should always be in descending order of number_of_keys, and the
  // last one should always be number_of_keys = 1.
  std::vector<std::pair<size_t, std::unique_ptr<sql::Statement>>> levels;
  if (!AppendKAnonCacheQueryStatement(db, SQL_FROM_HERE, 100, &levels)) {
    return {{}, keys};
  }
  if (!AppendKAnonCacheQueryStatement(db, SQL_FROM_HERE, 10, &levels)) {
    return {{}, keys};
  }
  if (!AppendKAnonCacheQueryStatement(db, SQL_FROM_HERE, 1, &levels)) {
    return {{}, keys};
  }

  base::flat_map<std::string, bool> lookup_results;
  size_t key_index = 0;
  while (key_index < keys.size()) {
    for (auto& level : levels) {
      const size_t number_of_keys = level.first;
      if (keys.size() - key_index >= number_of_keys) {
        sql::Statement* statement = level.second.get();
        statement->Reset(true);
        statement->BindTime(0, min_valid_time);
        for (size_t param_index = 1; param_index <= number_of_keys;
             ++param_index) {
          statement->BindBlob(param_index, keys[key_index++]);
        }
        while (statement->Step()) {
          lookup_results.emplace(statement->ColumnString(0),
                                 statement->ColumnBool(1));
        }
        if (!statement->Succeeded()) {
          DLOG(ERROR) << "DoLoadPositiveHashedKAnonymityKeysFromCache "
                      << "Encountered error while stepping through results";
          return {{}, keys};
        }
        break;
      }
    }
  }

  std::vector<std::string> positive_hashed_keys;
  std::vector<std::string> keys_to_fetch;
  for (const std::string& key : keys) {
    auto is_k_anon_iterator = lookup_results.find(key);
    if (is_k_anon_iterator == lookup_results.end()) {
      keys_to_fetch.emplace_back(key);
    } else if (is_k_anon_iterator->second) {
      positive_hashed_keys.emplace_back(key);
    }
  }

  base::UmaHistogramPercentage(
      "Storage.InterestGroup.KAnonymityKeysCacheHitRate",
      (keys.size() - keys_to_fetch.size()) * 100 / keys.size());

  return {positive_hashed_keys, keys_to_fetch};
}

bool DoPerformDatabaseMaintenance(sql::Database& db,
                                  base::Time now,
                                  size_t max_owners,
                                  size_t max_owner_storage_size,
                                  size_t max_owner_regular_interest_groups,
                                  size_t max_owner_negative_interest_groups) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Storage.InterestGroup.DBMaintenanceTime");
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }
  if (!ClearExcessInterestGroups(db, max_owners,
                                 max_owner_regular_interest_groups,
                                 max_owner_negative_interest_groups)) {
    return false;
  }
  if (!ClearExpiredInterestGroups(db, now)) {
    return false;
  }
  if (!ClearExcessiveStorage(db, max_owner_storage_size)) {
    return false;
  }
  if (!DeleteOldJoins(db, now - blink::MaxInterestGroupLifetimeForMetadata())) {
    return false;
  }
  if (!DeleteOldBids(db, now - blink::MaxInterestGroupLifetimeForMetadata())) {
    return false;
  }
  if (!DeleteOldWins(db, now - blink::MaxInterestGroupLifetimeForMetadata())) {
    return false;
  }
  if (!ClearExpiredKAnon(
          db, now - InterestGroupStorage::kAdditionalKAnonStoragePeriod)) {
    return false;
  }
  if (!DeleteExpiredDebugReportCooldown(db, now)) {
    return false;
  }
  if (!ClearExpiredBiddingAndAuctionKeys(db, now)) {
    return false;
  }
  if (!ClearExpiredCachedKAnonymityHashes(db, now)) {
    return false;
  }
  if (!CompactClickiness(db, now)) {
    return false;
  }
  return transaction.Commit();
}

base::FilePath DBPath(const base::FilePath& base) {
  if (base.empty()) {
    return base;
  }
  return base.Append(kDatabasePath);
}

sql::DatabaseOptions GetDatabaseOptions() {
  return sql::DatabaseOptions().set_wal_mode(base::FeatureList::IsEnabled(
      features::kFledgeEnableWALForInterestGroupStorage));
}

void ReportCreateSchemaResult(
    bool create_schema_result,
    sql::RazeIfIncompatibleResult raze_if_incompatible_result,
    bool missing_metatable_razed) {
  if (create_schema_result) {
    if (raze_if_incompatible_result ==
        sql::RazeIfIncompatibleResult::kRazedSuccessfully) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::
              kSuccessCreateSchemaAfterIncompatibleRaze);
    } else if (missing_metatable_razed) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::
              kSuccessCreateSchemaAfterNoMetaTableRaze);
    } else {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::kSuccessCreateSchema);
    }
  } else {
    if (raze_if_incompatible_result ==
        sql::RazeIfIncompatibleResult::kRazedSuccessfully) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::
              kFailedCreateSchemaAfterIncompatibleRaze);
    } else if (missing_metatable_razed) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::
              kFailedCreateSchemaAfterNoMetaTableRaze);
    } else {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::kFailedCreateSchema);
    }
  }
}

void ReportUpgradeDBResult(bool upgrade_succeeded, int db_version) {
  if (upgrade_succeeded) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.InitializationResult",
        InterestGroupStorageInitializationResult::kSuccessUpgraded);
    static_assert(kCurrentVersionNumber <= 100,
                  "UmaHistogramExactLinear() only supports 100 buckets -- a "
                  "new histogram is needed for larger versions.");
    base::UmaHistogramExactLinear(
        "Storage.InterestGroup.UpgradeSucceededStartVersion", db_version,
        /*exclusive_max=*/101);
    base::UmaHistogramExactLinear(
        "Storage.InterestGroup.UpgradeSucceededEndVersion",
        kCurrentVersionNumber,
        /*exclusive_max=*/101);
  } else {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.InitializationResult",
        InterestGroupStorageInitializationResult::kFailedUpgradeDB);
    base::UmaHistogramExactLinear(
        "Storage.InterestGroup.UpgradeFailedStartVersion", db_version,
        /*exclusive_max=*/101);
    base::UmaHistogramExactLinear(
        "Storage.InterestGroup.UpgradeFailedEndVersion", kCurrentVersionNumber,
        /*exclusive_max=*/101);
  }
}

}  // namespace

constexpr base::TimeDelta InterestGroupStorage::kMaintenanceInterval;
constexpr base::TimeDelta InterestGroupStorage::kDefaultIdlePeriod;
constexpr base::TimeDelta InterestGroupStorage::kUpdateSucceededBackoffPeriod;
constexpr base::TimeDelta InterestGroupStorage::kUpdateFailedBackoffPeriod;

InterestGroupStorage::InterestGroupStorage(const base::FilePath& path)
    : InterestGroupStorage(std::move(path),
                           /*idle_period=*/kDefaultIdlePeriod) {}

InterestGroupStorage::InterestGroupStorage(const base::FilePath& path,
                                           base::TimeDelta idle_period)
    : path_to_database_(DBPath(path)),
      max_owners_(network::features::kInterestGroupStorageMaxOwners.Get()),
      max_owner_regular_interest_groups_(MaxOwnerRegularInterestGroups()),
      max_owner_negative_interest_groups_(MaxOwnerNegativeInterestGroups()),
      max_owner_storage_size_(MaxOwnerStorageSize()),
      max_ops_before_maintenance_(
          network::features::kInterestGroupStorageMaxOpsBeforeMaintenance
              .Get()),
      db_maintenance_timer_(FROM_HERE,
                            idle_period,
                            this,
                            &InterestGroupStorage::PerformDBMaintenance) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InterestGroupStorage::~InterestGroupStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool InterestGroupStorage::EnsureDBInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time now = base::Time::Now();
  if (now > last_maintenance_time_ + kMaintenanceInterval) {
    // Schedule maintenance for next idle period. If maintenance already
    // scheduled this delays it further (we're not idle).
    db_maintenance_timer_.Reset();
  }
  // Force maintenance even if we're busy if the database may have changed too
  // much.
  if (ops_since_last_maintenance_++ > max_ops_before_maintenance_) {
    PerformDBMaintenance();
  }

  last_access_time_ = now;
  if (db_ && db_->is_open()) {
    return true;
  }
  return InitializeDB();
}

bool InterestGroupStorage::InitializeDB() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_ = std::make_unique<sql::Database>(GetDatabaseOptions(),
                                        sql::Database::Tag("InterestGroups"));
  db_->set_error_callback(base::BindRepeating(
      &InterestGroupStorage::DatabaseErrorCallback, base::Unretained(this)));

  if (path_to_database_.empty()) {
    if (!db_->OpenInMemory()) {
      DLOG(ERROR) << "Failed to create in-memory interest group database: "
                  << db_->GetErrorMessage();
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::kFailedCreateInMemory);
      return false;
    }
  } else {
    const base::FilePath dir = path_to_database_.DirName();

    if (!base::CreateDirectory(dir)) {
      DLOG(ERROR) << "Failed to create directory for interest group database";
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::kFailedCreateDirectory);
      return false;
    }
    if (db_->Open(path_to_database_) == false) {
      DLOG(ERROR) << "Failed to open interest group database: "
                  << db_->GetErrorMessage();
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::kFailedCreateFile);
      return false;
    }
  }

  if (!InitializeSchema()) {
    db_->Close();
    return false;
  }

  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  DCHECK(db_->DoesTableExist("interest_groups"));
  DCHECK(db_->DoesTableExist("join_history"));
  DCHECK(db_->DoesTableExist("bid_history"));
  DCHECK(db_->DoesTableExist("win_history"));
  DCHECK(db_->DoesTableExist("joined_k_anon"));
  DCHECK(db_->DoesTableExist("lockout_debugging_only_report"));
  DCHECK(db_->DoesTableExist("cooldown_debugging_only_report"));
  return true;
}

bool InterestGroupStorage::InitializeSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return false;
  }

  sql::RazeIfIncompatibleResult raze_if_incompatible_result =
      sql::MetaTable::RazeIfIncompatible(
          db_.get(), /*lowest_supported_version=*/kDeprecatedVersionNumber + 1,
          kCurrentVersionNumber);
  if (raze_if_incompatible_result == sql::RazeIfIncompatibleResult::kFailed) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.InitializationResult",
        InterestGroupStorageInitializationResult::kFailedToRazeIncompatible);
    return false;
  }

  sql::MetaTable meta_table;
  bool missing_metatable_razed = false;
  bool has_metatable = meta_table.DoesTableExist(db_.get());
  if (!has_metatable && db_->DoesTableExist("interest_groups")) {
    // Existing DB with no meta table. We have no idea what version the schema
    // is so we should remove it and start fresh.
    missing_metatable_razed = true;
    // If the incompatible version raze happened and succeeded, it should have
    // removed the interest_groups table.
    CHECK_NE(raze_if_incompatible_result,
             sql::RazeIfIncompatibleResult::kRazedSuccessfully);
    if (!db_->Raze()) {
      base::UmaHistogramEnumeration(
          "Storage.InterestGroup.InitializationResult",
          InterestGroupStorageInitializationResult::kFailedToRazeNoMetaTable);
      return false;
    }
  }
  const bool new_db = !has_metatable;
  if (!meta_table.Init(db_.get(), kCurrentVersionNumber,
                       kCompatibleVersionNumber)) {
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.InitializationResult",
        InterestGroupStorageInitializationResult::kFailedMetaTableInit);
    return false;
  }

  if (new_db) {
    bool create_schema_result = CreateCurrentSchema(*db_);
    ReportCreateSchemaResult(
        /*create_schema_result=*/create_schema_result,
        /*raze_if_incompatible_result=*/raze_if_incompatible_result,
        /*missing_metatable_razed=*/missing_metatable_razed);
    return create_schema_result;
  }

  const int db_version = meta_table.GetVersionNumber();

  if (db_version >= kCurrentVersionNumber) {
    // Getting past RazeIfIncompatible implies that
    // kCurrentVersionNumber >= meta_table.GetCompatibleVersionNumber
    // So DB is either the current database version or a future version that is
    // back-compatible with this version of Chrome.
    base::UmaHistogramEnumeration(
        "Storage.InterestGroup.InitializationResult",
        InterestGroupStorageInitializationResult::kSuccessAlreadyCurrent);
    return true;
  }

  // Older versions - should be migrated.
  // db_version < kCurrentVersionNumber
  // db_version > kDeprecatedVersionNumber
  bool upgrade_succeeded = UpgradeDB(*db_, db_version, meta_table, PassKey());
  ReportUpgradeDBResult(/*upgrade_succeeded=*/upgrade_succeeded,
                        /*db_version=*/db_version);
  return upgrade_succeeded;
}

std::optional<InterestGroupKanonUpdateParameter>
InterestGroupStorage::JoinInterestGroup(const blink::InterestGroup& group,
                                        const GURL& main_frame_joining_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::nullopt;
  }
  base::Time now = base::Time::Now();
  std::optional<InterestGroupKanonUpdateParameter> kanon_update =
      DoJoinInterestGroup(*db_, PassKey(), group, main_frame_joining_url,
                          /*exact_join_time=*/now,
                          /*last_updated=*/now,
                          /*next_update_after=*/base::Time::Min());
  if (!kanon_update) {
    DLOG(ERROR) << "Could not join interest group: " << db_->GetErrorMessage();
  }
  return kanon_update;
}

void InterestGroupStorage::LeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const url::Origin& main_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  StorageInterestGroup old_group;
  if (DoLoadInterestGroup(*db_, PassKey(), group_key, old_group) &&
      old_group.interest_group.execution_mode ==
          blink::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
      main_frame != old_group.joining_origin) {
    // Clear all interest groups with same owner and mode GroupedByOriginMode
    // and same old_group.joining_origin.
    if (!DoClearClusteredBiddingGroups(*db_, group_key.owner,
                                       old_group.joining_origin)) {
      DLOG(ERROR) << "Could not leave interest group: "
                  << db_->GetErrorMessage();
    }
    return;
  }

  if (!DoRemoveInterestGroup(*db_, group_key)) {
    DLOG(ERROR) << "Could not leave interest group: " << db_->GetErrorMessage();
  }
}

std::vector<std::string> InterestGroupStorage::ClearOriginJoinedInterestGroups(
    const url::Origin& owner,
    const std::set<std::string>& interest_groups_to_keep,
    const url::Origin& main_frame_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::vector<std::string>();
  }

  std::optional<std::vector<std::string>> left_interest_groups =
      DoClearOriginJoinedInterestGroups(*db_, owner, interest_groups_to_keep,
                                        main_frame_origin);
  if (!left_interest_groups) {
    DLOG(ERROR) << "Could not leave interest group: " << db_->GetErrorMessage();
    return std::vector<std::string>();
  }
  return std::move(left_interest_groups.value());
}

std::optional<DebugReportLockoutAndCooldowns>
InterestGroupStorage::GetDebugReportLockoutAndCooldowns(
    const base::flat_set<url::Origin>& origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::nullopt;
  }
  DebugReportLockoutAndCooldowns debug_report_lockout_and_cooldowns;

  // Ignore lockout and cooldowns whose start time is before
  // kFledgeEnableFilteringDebugReportStartingFrom.
  std::optional<base::Time> ignore_before = GetSampleDebugReportStartingFrom();
  debug_report_lockout_and_cooldowns.lockout =
      DoGetDebugReportLockout(*db_, ignore_before);
  DoGetDebugReportCooldowns(*db_, origins, ignore_before,
                            debug_report_lockout_and_cooldowns);
  return debug_report_lockout_and_cooldowns;
}

std::optional<DebugReportLockoutAndCooldowns>
InterestGroupStorage::GetDebugReportLockoutAndAllCooldowns() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::nullopt;
  }
  DebugReportLockoutAndCooldowns debug_report_lockout_and_cooldowns;

  // Ignore lockout and cooldowns whose start time is before
  // kFledgeEnableFilteringDebugReportStartingFrom.
  std::optional<base::Time> ignore_before = GetSampleDebugReportStartingFrom();
  debug_report_lockout_and_cooldowns.lockout =
      DoGetDebugReportLockout(*db_, ignore_before);
  DoGetAllDebugReportCooldowns(*db_, ignore_before,
                               debug_report_lockout_and_cooldowns);
  return debug_report_lockout_and_cooldowns;
}

std::optional<InterestGroupKanonUpdateParameter>
InterestGroupStorage::UpdateInterestGroup(
    const blink::InterestGroupKey& group_key,
    InterestGroupUpdate update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::nullopt;
  }

  std::optional<InterestGroupKanonUpdateParameter> kanon_update =
      DoUpdateInterestGroup(*db_, PassKey(), group_key, update,
                            base::Time::Now());
  if (!kanon_update) {
    DLOG(ERROR) << "Could not update interest group: "
                << db_->GetErrorMessage();
  }
  return kanon_update;
}

void InterestGroupStorage::AllowUpdateIfOlderThan(
    blink::InterestGroupKey group_key,
    base::TimeDelta update_if_older_than) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  bool success = DoAllowUpdateIfOlderThan(
      *db_, PassKey(), group_key, update_if_older_than, base::Time::Now());
  if (!success) {
    DLOG(ERROR) << "Could not process update_if_older_than: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::ReportUpdateFailed(
    const blink::InterestGroupKey& group_key,
    bool parse_failure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    NOTREACHED();  // We already fetched interest groups to update...
  }

  if (!DoReportUpdateFailed(*db_, group_key, parse_failure,
                            base::Time::Now())) {
    DLOG(ERROR) << "Couldn't update next_update_after: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RecordInterestGroupBids(
    const blink::InterestGroupSet& group_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoRecordInterestGroupBids(*db_, group_keys, base::Time::Now())) {
    DLOG(ERROR) << "Could not record win for interest group: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RecordInterestGroupWin(
    const blink::InterestGroupKey& group_key,
    const std::string& ad_json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoRecordInterestGroupWin(*db_, group_key, ad_json, base::Time::Now())) {
    DLOG(ERROR) << "Could not record bid for interest group: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RecordDebugReportLockout(base::Time starting_time,
                                                    base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoRecordDebugReportLockout(*db_, starting_time, duration)) {
    DLOG(ERROR) << "Could not record debugging only report lockout: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RecordDebugReportCooldown(
    const url::Origin& origin,
    base::Time cooldown_start,
    DebugReportCooldownType cooldown_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }
  if (!DoRecordDebugReportCooldown(*db_, origin, cooldown_start,
                                   cooldown_type)) {
    DLOG(ERROR) << "Could not record debugging only report cooldown: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::DeleteAllDebugReportCooldowns() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoDeleteAllDebugReportCooldowns(*db_)) {
    DLOG(ERROR) << "Could not delete all debug report cooldowns: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::UpdateKAnonymity(
    const blink::InterestGroupKey& interest_group_key,
    const std::vector<std::string>& positive_hashed_keys,
    const base::Time update_time,
    bool replace_existing_values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoUpdateKAnonymity(*db_, interest_group_key, positive_hashed_keys,
                          update_time, replace_existing_values,
                          base::Time::Now())) {
    DLOG(ERROR) << "Could not update k-anonymity: " << db_->GetErrorMessage();
  }
}

std::optional<base::Time> InterestGroupStorage::GetLastKAnonymityReported(
    const std::string& hashed_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }

  return DoGetLastKAnonymityReported(*db_, hashed_key);
}

void InterestGroupStorage::UpdateLastKAnonymityReported(
    const std::string& hashed_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  DoUpdateLastKAnonymityReported(*db_, hashed_key, base::Time::Now());
}

void InterestGroupStorage::RecordViewClick(
    const network::AdAuctionEventRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  DoRecordViewClick(*db_, record, base::Time::Now());
}

std::optional<StorageInterestGroup> InterestGroupStorage::GetInterestGroup(
    const blink::InterestGroupKey& group_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::nullopt;
  }

  return DoGetStoredInterestGroup(*db_, PassKey(), group_key,
                                  base::Time::Now());
}

std::vector<url::Origin> InterestGroupStorage::GetAllInterestGroupOwners() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }

  std::optional<std::vector<url::Origin>> maybe_result =
      DoGetAllInterestGroupOwners(*db_, base::Time::Now());
  if (!maybe_result) {
    return {};
  }
  return std::move(maybe_result.value());
}

std::vector<StorageInterestGroup>
InterestGroupStorage::GetInterestGroupsForOwner(const url::Origin& owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }

  std::optional<std::vector<StorageInterestGroup>> maybe_result =
      DoGetInterestGroupsForOwner(*db_, PassKey(), owner, base::Time::Now());
  if (!maybe_result) {
    return {};
  }
  base::UmaHistogramCounts1000("Storage.InterestGroup.PerSiteCount",
                               maybe_result->size());
  return std::move(maybe_result.value());
}

std::vector<InterestGroupUpdateParameter>
InterestGroupStorage::GetInterestGroupsForUpdate(const url::Origin& owner,
                                                 size_t groups_limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }

  std::optional<std::vector<InterestGroupUpdateParameter>> maybe_result =
      DoGetInterestGroupsForUpdate(*db_, owner, base::Time::Now(),
                                   groups_limit);
  if (!maybe_result) {
    return {};
  }
  return std::move(maybe_result.value());
}

std::vector<url::Origin>
InterestGroupStorage::GetAllInterestGroupJoiningOrigins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }
  std::optional<std::vector<url::Origin>> maybe_result =
      DoGetAllInterestGroupJoiningOrigins(*db_, base::Time::Now());
  if (!maybe_result) {
    return {};
  }
  return std::move(maybe_result.value());
}

std::vector<std::pair<url::Origin, url::Origin>>
InterestGroupStorage::GetAllInterestGroupOwnerJoinerPairs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }
  std::optional<std::vector<std::pair<url::Origin, url::Origin>>> maybe_result =
      DoGetAllInterestGroupOwnerJoinerPairs(*db_, base::Time::Now());
  if (!maybe_result) {
    return {};
  }
  return std::move(maybe_result.value());
}

void InterestGroupStorage::SetDebugReportLockoutUntilIGExpires() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoSetDebugReportLockoutUntilIGExpires(*db_, base::Time::Now())) {
    DLOG(ERROR)
        << "Could not set debug report lockout until interest groups expire: "
        << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RemoveInterestGroupsMatchingOwnerAndJoiner(
    const url::Origin& owner,
    const url::Origin& joining_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoRemoveInterestGroupsMatchingOwnerAndJoiner(*db_, owner, joining_origin,
                                                    base::Time::Now())) {
    DLOG(ERROR)
        << "Could not remove interest groups matching owner and joiner: "
        << db_->GetErrorMessage();
  }

  return;
}

void InterestGroupStorage::DeleteInterestGroupData(
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    bool user_initiated_deletion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoDeleteInterestGroupData(*db_, std::move(storage_key_matcher),
                                 user_initiated_deletion)) {
    DLOG(ERROR) << "Could not delete interest group data: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::DeleteAllInterestGroupData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  db_->RazeAndPoison();
  db_.reset();
}

void InterestGroupStorage::SetInterestGroupPriority(
    const blink::InterestGroupKey& group_key,
    double priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  if (!DoSetInterestGroupPriority(*db_, group_key, priority)) {
    DLOG(ERROR) << "Could not set interest group priority: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::UpdateInterestGroupPriorityOverrides(
    const blink::InterestGroupKey& group_key,
    base::flat_map<std::string,
                   auction_worklet::mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }

  StorageInterestGroup group;
  if (!DoLoadInterestGroup(*db_, PassKey(), group_key, group)) {
    return;
  }

  MergePrioritySignalsOverrides(
      update_priority_signals_overrides,
      group.interest_group.priority_signals_overrides);
  if (!group.interest_group.IsValid()) {
    // TODO(mmenke): Report errors to devtools.
    return;
  }

  if (!DoSetInterestGroupPrioritySignalsOverrides(
          *db_, group_key, group.interest_group.priority_signals_overrides)) {
    DLOG(ERROR) << "Could not set interest group priority signals overrides: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::PerformDBMaintenance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_maintenance_time_ = base::Time::Now();
  ops_since_last_maintenance_ = 0;
  std::optional<int64_t> db_size = base::GetFileSize(path_to_database_);
  if (db_size.has_value()) {
    UMA_HISTOGRAM_MEMORY_KB("Storage.InterestGroup.DBSize",
                            db_size.value() / 1024);
  }
  if (EnsureDBInitialized()) {
    DoPerformDatabaseMaintenance(
        *db_, last_maintenance_time_, /*max_owners=*/max_owners_,
        /*max_owner_storage_size=*/max_owner_storage_size_,
        /*max_owner_regular_interest_groups=*/
        max_owner_regular_interest_groups_,
        /*max_owner_negative_interest_groups=*/
        max_owner_negative_interest_groups_);
  }
}

std::vector<StorageInterestGroup>
InterestGroupStorage::GetAllInterestGroupsUnfilteredForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {};
  }
  const base::Time distant_past = base::Time::Min();
  std::vector<StorageInterestGroup> result;
  std::optional<std::vector<url::Origin>> maybe_owners =
      DoGetAllInterestGroupOwners(*db_, distant_past);
  if (!maybe_owners) {
    return {};
  }
  for (const auto& owner : *maybe_owners) {
    std::optional<std::vector<StorageInterestGroup>> maybe_owner_results =
        DoGetInterestGroupsForOwner(*db_, PassKey(), owner, distant_past);
    DCHECK(maybe_owner_results) << owner;
    std::move(maybe_owner_results->begin(), maybe_owner_results->end(),
              std::back_inserter(result));
  }
  return result;
}

void InterestGroupStorage::SetBiddingAndAuctionServerKeys(
    const url::Origin& coordinator,
    std::string serialized_keys,
    base::Time expiration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return;
  }
  DoSetBiddingAndAuctionServerKeys(*db_, coordinator,
                                   std::move(serialized_keys), expiration);
}

std::pair<base::Time, std::string>
InterestGroupStorage::GetBiddingAndAuctionServerKeys(
    const url::Origin& coordinator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {base::Time::Min(), {}};
  }
  return DoGetBiddingAndAuctionServerKeys(*db_, coordinator);
}

bool InterestGroupStorage::WriteHashedKAnonymityKeysToCache(
    const std::vector<std::string>& positive_hashed_keys,
    const std::vector<std::string>& negative_hashed_keys,
    base::Time fetch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return false;
  }
  return DoWriteHashedKAnonymityKeysToCache(*db_, positive_hashed_keys,
                                            negative_hashed_keys, fetch_time);
}

InterestGroupStorage::KAnonymityCacheResponse::KAnonymityCacheResponse(
    std::vector<std::string> _positive_hashed_keys_from_cache,
    std::vector<std::string> _ids_to_query_from_server)
    : positive_hashed_keys_from_cache(
          std::move(_positive_hashed_keys_from_cache)),
      ids_to_query_from_server(std::move(_ids_to_query_from_server)) {}

InterestGroupStorage::KAnonymityCacheResponse::KAnonymityCacheResponse(
    const KAnonymityCacheResponse& other) = default;

InterestGroupStorage::KAnonymityCacheResponse&
InterestGroupStorage::KAnonymityCacheResponse::operator=(
    const KAnonymityCacheResponse& other) = default;

InterestGroupStorage::KAnonymityCacheResponse::~KAnonymityCacheResponse() =
    default;

InterestGroupStorage::KAnonymityCacheResponse
InterestGroupStorage::LoadPositiveHashedKAnonymityKeysFromCache(
    const std::vector<std::string>& keys,
    base::Time check_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return {{}, keys};
  }
  return DoLoadPositiveHashedKAnonymityKeysFromCache(*db_, keys, check_time);
}

// static
size_t InterestGroupStorage::MaxOwnerRegularInterestGroups() {
  return network::features::kInterestGroupStorageMaxGroupsPerOwner.Get();
}

// static
size_t InterestGroupStorage::MaxOwnerNegativeInterestGroups() {
  return network::features::kInterestGroupStorageMaxNegativeGroupsPerOwner
      .Get();
}

// static
size_t InterestGroupStorage::MaxOwnerStorageSize() {
  return network::features::kInterestGroupStorageMaxStoragePerOwner.Get();
}

base::Time InterestGroupStorage::GetLastMaintenanceTimeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_maintenance_time_;
}

/* static */ int InterestGroupStorage::GetCurrentVersionNumberForTesting() {
  return kCurrentVersionNumber;
}

/*static */ std::unique_ptr<InterestGroupStorage>
InterestGroupStorage::CreateWithIdlePeriodForTesting(
    const base::FilePath& path,
    base::TimeDelta idle_period) {
  return base::WrapUnique(new InterestGroupStorage(path, idle_period));
}

void InterestGroupStorage::ResetIdleTimerForTesting() {
  EnsureDBInitialized();
}

// static
std::optional<InterestGroupStorage::ClickinessCompactionRawEvents>
InterestGroupStorage::ComputeCompactClickinessForTesting(
    base::Time now,
    const InterestGroupStorage::ClickinessCompactionRawEvents& raw) {
  return ComputeCompactClickiness(now, raw);
}

std::optional<bool>
InterestGroupStorage::CheckViewClickCountsForProviderAndEligibleInDbForTesting(
    const url::Origin& provider_origin,
    const url::Origin& eligible_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    return std::nullopt;
  }

  auto status = DoGetViewClickCountsForProviderAndEligible(
      *db_, provider_origin, eligible_origin);
  if (status.has_value()) {
    return true;
  }
  switch (status.error()) {
    case MissingReason::kNotInDb:
      return false;
    case MissingReason::kDbError:
    case MissingReason::kDecodeError:
      return std::nullopt;
  }
}

void InterestGroupStorage::DatabaseErrorCallback(int extended_error,
                                                 sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only save the basic error code (not extended) to UMA.
  base::UmaHistogramExactLinear("Storage.InterestGroup.DBErrors",
                                extended_error & 0xFF,
                                /*sqlite error max+1*/ SQLITE_WARNING + 1);

  if (sql::IsErrorCatastrophic(extended_error)) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndPoison() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    db_->RazeAndPoison();
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error)) {
    DLOG(FATAL) << db_->GetErrorMessage();
  }
}

}  // namespace content

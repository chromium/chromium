// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_serializer.h"

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/public/common/content_switches.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/numeric/bits.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/zlib/google/compression_utils.h"

namespace content {

namespace {

const size_t kFramingHeaderSize = 5;       // bytes
const size_t kOhttpEncIdSize = 7;          // bytes
const size_t kOhttpSharedSecretSize = 48;  // bytes
const size_t kOhttpHeaderSize = kOhttpEncIdSize + kOhttpSharedSecretSize;

const uint8_t kRequestVersion = 0;
const uint8_t kRequestVersionBitOffset = 5;
const uint8_t kGzipCompression = 2;
const uint8_t kCompressionBitOffset = 0;

// The 7 sizes we are allowed to use when the request size isn't explicitly
// specified.
const std::array<uint32_t, 7> kBinSizes = {
    {0, 5 * 1024, 10 * 1024, 20 * 1024, 30 * 1024, 40 * 1024, 55 * 1024}};

struct ValueAndSize {
  cbor::Value value;
  base::CheckedNumeric<size_t> size;
};

struct CompressedInterestGroups {
  // `data` is the compressed, serialized interest groups.
  std::string data;
  // `group_names` is a list of interest group names for this bidder. This will
  // be needed to reconstruct the bidding interest groups from the server
  // response.
  std::vector<std::string> group_names;
  // `uncompressed_size` is the uncompressed size of the `data` in bytes.
  size_t uncompressed_size;
  // `num_groups` is the number of interest groups included in the `data`.
  size_t num_groups;
  // `group_pagg_coordinators` maps from interest group key to an aggregation
  // coordinator origin, if the interest group has a not null coordinator.
  base::flat_map<blink::InterestGroupKey, url::Origin> group_pagg_coordinators;
};

struct SerializedBiddersMap {
  // `bidders` maps from bidder origin to compressed, serialized interest groups
  // for this bidder.
  cbor::Value::MapValue bidders;
  // `group_names` maps from bidder origin to a list of interest group names for
  // that bidder. This will be needed to reconstruct the bidding interest groups
  // for each bidder from the server response.
  base::flat_map<url::Origin, std::vector<std::string>> group_names;
  // `uncompressed_size` is total uncompressed size of the values in the
  // `bidders` map in bytes.
  size_t uncompressed_size;
  // `compressed_size` is total compressed size of the values in the `bidders`
  // map in bytes (excluding the keys).
  size_t compressed_size;
  // `num_groups` is the total number of interest groups included in all of the
  // values in the `bidders` map.
  size_t num_groups;
  // `bidders_elements_size` is the running size estimate for serializing the
  // `bidders` map.
  base::CheckedNumeric<size_t> bidders_elements_size;  // bytes
  // `group_pagg_coordinators` maps from interest group key to an aggregation
  // coordinator origin, if the interest group has a not null coordinator.
  base::flat_map<blink::InterestGroupKey, url::Origin> group_pagg_coordinators;
};

constexpr std::size_t constexpr_strlen(const char* s) {
  return std::char_traits<char>::length(s);
}

// Length of the CBOR encoded length of a CBOR value.
constexpr size_t LengthOfLength(uint64_t length) {
  if (length < 24) {
    return 0;
  }
  if (length <= 0xFF) {
    return 1;
  }
  if (length <= 0xFFFF) {
    return 2;
  }
  if (length <= 0xFFFFFFFF) {
    return 4;
  }
  return 8;
}

// Finds the number of bytes from `length` that need to be used to store the
// size of the largest CBOR value that fits.
// Solves `length = 1 + x + LengthOfLength(x)` for `LengthOfLength(x)`.
size_t MaxLengthOfTaggedData(uint64_t length) {
  size_t lol_x = 0;
  if (length <= 23 + 1) {
    // Length and tag stored in a single byte.
    lol_x = 0;
  } else if (length <= 0xFF + 1 + 1) {
    // Length and tag stored in separate bytes.
    lol_x = 1;
  } else if (length <= 0xFFFF + 1 + 2) {
    // 1 byte tag, 2 bytes length.
    lol_x = 2;
  } else if (length <= 0xFFFFFFFF + 1 + 4) {
    // 1 byte tag, 4 bytes length.
    lol_x = 4;
  } else {
    // 1 byte tag, 8 bytes length.
    lol_x = 8;
  }
  DCHECK_EQ(LengthOfLength(length - 1 - lol_x), lol_x);
  return lol_x;
}

// CBOR encoded length of a string with the given length.
constexpr base::CheckedNumeric<size_t> TaggedStringLength(size_t length) {
  return 1 + LengthOfLength(length) + length;
}

constexpr base::CheckedNumeric<size_t> TaggedUIntLength(uint64_t value) {
  return 1 + LengthOfLength(value);
}

constexpr base::CheckedNumeric<size_t> TaggedSIntLength(int64_t value) {
  if (value < 0) {
    return TaggedUIntLength(-value - 1);
  }
  return TaggedUIntLength(value);
}

// Array is serialized with a tag then the number of elements in the array.
base::CheckedNumeric<size_t> TaggedArrayLength(
    const cbor::Value::ArrayValue& array,
    base::CheckedNumeric<size_t> elements_size) {
  return 1 + LengthOfLength(array.size()) + elements_size;
}

// A map is serialized with a tag then twice the number of elements in the map.
base::CheckedNumeric<size_t> TaggedMapLength(
    const cbor::Value::MapValue& map,
    base::CheckedNumeric<size_t> elements_size) {
  return 1 + LengthOfLength(2 * map.size()) + elements_size;
}

ValueAndSize SerializeAds(const std::vector<blink::InterestGroup::Ad>& ads,
                          bool include_full_ads) {
  cbor::Value::ArrayValue result;
  base::CheckedNumeric<size_t> array_elements_size = 0;
  for (const auto& ad : ads) {
    if (include_full_ads) {
      cbor::Value::MapValue obj;
      base::CheckedNumeric<size_t> map_elements_size = 0;
      obj[cbor::Value("renderURL")] = cbor::Value(ad.render_url());
      map_elements_size += TaggedStringLength(constexpr_strlen("renderURL")) +
                           TaggedStringLength(ad.render_url().size());
      if (ad.metadata) {
        obj[cbor::Value("metadata")] = cbor::Value(ad.metadata.value());
        map_elements_size += TaggedStringLength(constexpr_strlen("metadata")) +
                             TaggedStringLength(ad.metadata->size());
      }
      if (ad.size_group) {
        obj[cbor::Value("sizeGroup")] = cbor::Value(ad.size_group.value());
        map_elements_size += TaggedStringLength(constexpr_strlen("sizeGroup")) +
                             TaggedStringLength(ad.size_group->size());
      }
      if (ad.ad_render_id) {
        obj[cbor::Value("adRenderId")] = cbor::Value(ad.ad_render_id.value());
        map_elements_size +=
            TaggedStringLength(constexpr_strlen("adRenderId")) +
            TaggedStringLength(ad.ad_render_id->size());
      }

      array_elements_size += TaggedMapLength(obj, map_elements_size);
      result.emplace_back(std::move(obj));
    } else {
      if (ad.ad_render_id) {
        result.emplace_back(ad.ad_render_id.value());
        array_elements_size += TaggedStringLength(ad.ad_render_id->size());
      }
    }
  }

  base::CheckedNumeric<size_t> total_size =
      TaggedArrayLength(result, array_elements_size);
  return {cbor::Value(std::move(result)), total_size};
}

// This serialization is sent to the B&A server, so the format is standardized.
// We can't add fields to this format without coordinating with the B&A team.
ValueAndSize SerializeInterestGroup(base::Time start_time,
                                    const SingleStorageInterestGroup& group) {
  cbor::Value::MapValue group_obj;
  base::CheckedNumeric<size_t> group_elements_size = 0;

  group_obj[cbor::Value("name")] = cbor::Value(group->interest_group.name);
  group_elements_size += TaggedStringLength(constexpr_strlen("name")) +
                         TaggedStringLength(group->interest_group.name.size());
  if (group->interest_group.trusted_bidding_signals_keys) {
    cbor::Value::ArrayValue bidding_signal_keys;
    base::CheckedNumeric<size_t> array_elements_size = 0;
    bidding_signal_keys.reserve(
        group->interest_group.trusted_bidding_signals_keys->size());
    for (const auto& key :
         *group->interest_group.trusted_bidding_signals_keys) {
      bidding_signal_keys.emplace_back(key);
      array_elements_size += TaggedStringLength(key.size());
    }
    group_elements_size +=
        TaggedStringLength(constexpr_strlen("biddingSignalsKeys")) +
        TaggedArrayLength(bidding_signal_keys, array_elements_size);
    group_obj[cbor::Value("biddingSignalsKeys")] =
        cbor::Value(std::move(bidding_signal_keys));
  }
  if (!group->interest_group.auction_server_request_flags.Has(
          blink::AuctionServerRequestFlagsEnum::kOmitUserBiddingSignals) &&
      group->interest_group.user_bidding_signals) {
    group_obj[cbor::Value("userBiddingSignals")] =
        cbor::Value(*group->interest_group.user_bidding_signals);
    group_elements_size +=
        TaggedStringLength(constexpr_strlen("userBiddingSignals")) +
        TaggedStringLength(group->interest_group.user_bidding_signals->size());
  }
  if (!group->interest_group.auction_server_request_flags.Has(
          blink::AuctionServerRequestFlagsEnum::kOmitAds)) {
    if (group->interest_group.ads) {
      ValueAndSize ads = SerializeAds(
          *group->interest_group.ads,
          group->interest_group.auction_server_request_flags.Has(
              blink::AuctionServerRequestFlagsEnum::kIncludeFullAds));
      group_obj[cbor::Value("ads")] = std::move(ads.value);
      group_elements_size +=
          TaggedStringLength(constexpr_strlen("ads")) + ads.size;
    }
    if (group->interest_group.ad_components) {
      ValueAndSize components = SerializeAds(
          *group->interest_group.ad_components,
          group->interest_group.auction_server_request_flags.Has(
              blink::AuctionServerRequestFlagsEnum::kIncludeFullAds));
      group_obj[cbor::Value("components")] = std::move(components.value);
      group_elements_size +=
          TaggedStringLength(constexpr_strlen("components")) + components.size;
    }
  }
  cbor::Value::MapValue browser_signals;
  base::CheckedNumeric<size_t> browser_signals_elements_size = 0;
  browser_signals[cbor::Value("bidCount")] =
      cbor::Value(group->bidding_browser_signals->bid_count);
  browser_signals_elements_size +=
      TaggedStringLength(constexpr_strlen("bidCount")) +
      TaggedSIntLength(group->bidding_browser_signals->bid_count);
  // joinCount and recency are noised and binned on the server.
  browser_signals[cbor::Value("joinCount")] =
      cbor::Value(group->bidding_browser_signals->join_count);
  browser_signals_elements_size +=
      TaggedStringLength(constexpr_strlen("joinCount")) +
      TaggedSIntLength(group->bidding_browser_signals->join_count);
  int32_t recency = (start_time - group->join_time).InSeconds();
  if (recency < 0) {
    // It doesn't make sense to say that the browser joined the interest group
    // in the future, so just truncate to the present.
    recency = 0;
  }
  browser_signals[cbor::Value("recency")] = cbor::Value(recency);
  browser_signals_elements_size +=
      TaggedStringLength(constexpr_strlen("recency")) +
      TaggedSIntLength(recency);

  cbor::Value::ArrayValue prev_wins;
  base::CheckedNumeric<size_t> prev_wins_elements_size = 0;
  for (const auto& prev_win : group->bidding_browser_signals->prev_wins) {
    cbor::Value::ArrayValue tuple;
    base::CheckedNumeric<size_t> tuple_elements_size = 0;
    int32_t prev_win_time = (start_time - prev_win->time).InSeconds();
    if (prev_win_time < 0) {
      // It doesn't make sense to say that the interest group won an auction
      // in the future, so just truncate to the present.
      prev_win_time = 0;
    }
    tuple_elements_size += TaggedSIntLength(prev_win_time);
    tuple.emplace_back(prev_win_time);
    // We trust this ad_json because we wrote it ourselves.
    // Currently it's probably not worth it to deserialize this at the same time
    // we load the interest group from the database. We will want to revisit
    // this in the future.
    JSONStringValueDeserializer deserializer(prev_win->ad_json);
    std::string error_msg;
    std::unique_ptr<base::Value> ad = deserializer.Deserialize(
        /*error_code=*/nullptr,
        /*error_message=*/&error_msg);
    if (!ad) {
      // This should not happen unless the DB is corrupted.
      // Just do our best regardless.
      continue;
    }
    if (group->interest_group.auction_server_request_flags.Has(
            blink::AuctionServerRequestFlagsEnum::kIncludeFullAds)) {
      cbor::Value::MapValue obj;
      base::CheckedNumeric<size_t> obj_elements_size = 0;
      for (const auto kv : ad->GetDict()) {
        switch (kv.second.type()) {
          case base::Value::Type::BOOLEAN:
            obj[cbor::Value(kv.first)] = cbor::Value(kv.second.GetBool());
            obj_elements_size += TaggedStringLength(kv.first.size()) + 1;
            break;
          case base::Value::Type::INTEGER:
            obj[cbor::Value(kv.first)] = cbor::Value(kv.second.GetInt());
            obj_elements_size += TaggedStringLength(kv.first.size()) +
                                 TaggedSIntLength(kv.second.GetInt());
            break;
          case base::Value::Type::STRING:
            obj[cbor::Value(kv.first)] = cbor::Value(kv.second.GetString());
            obj_elements_size +=
                TaggedStringLength(kv.first.size()) +
                TaggedStringLength(kv.second.GetString().size());
            break;
          default:
            LOG(ERROR) << "Unsupported type in prevWins.ad for key '"
                       << kv.first << "': " << kv.second.DebugString();
        }
      }
      tuple_elements_size += TaggedMapLength(obj, obj_elements_size);
      tuple.emplace_back(std::move(obj));
    } else {
      std::string* ad_render_id = ad->GetDict().FindString("adRenderId");
      if (ad_render_id) {
        tuple.emplace_back(*ad_render_id);
        tuple_elements_size += TaggedStringLength(ad_render_id->size());
      } else {
        // If there's no adRenderId we still can send the time.
        tuple.emplace_back("");
        tuple_elements_size += TaggedStringLength(0);
      }
    }
    prev_wins_elements_size += TaggedArrayLength(tuple, tuple_elements_size);
    prev_wins.emplace_back(std::move(tuple));
  }
  browser_signals_elements_size +=
      TaggedStringLength(constexpr_strlen("prevWins")) +
      TaggedArrayLength(prev_wins, prev_wins_elements_size);
  browser_signals[cbor::Value("prevWins")] = cbor::Value(std::move(prev_wins));

  group_elements_size +=
      TaggedStringLength(constexpr_strlen("browserSignals")) +
      TaggedMapLength(browser_signals, browser_signals_elements_size);
  group_obj[cbor::Value("browserSignals")] =
      cbor::Value(std::move(browser_signals));

  base::CheckedNumeric<size_t> total_size =
      TaggedMapLength(group_obj, group_elements_size);
  return {cbor::Value(std::move(group_obj)), total_size};
}

CompressedInterestGroups CompressInterestGroups(
    const url::Origin& owner,
    const std::vector<SingleStorageInterestGroup>& groups,
    base::Time start_time,
    std::optional<uint32_t> target_uncompressed_size) {
  CompressedInterestGroups result{{}, {}, 0, 0};
  cbor::Value::ArrayValue groups_array;
  base::CheckedNumeric<size_t> groups_elements_size = 0;
  for (const SingleStorageInterestGroup& group : groups) {
    ValueAndSize serialized_group = SerializeInterestGroup(start_time, group);
    base::CheckedNumeric<size_t> uncompressed_size =
        serialized_group.size + groups_elements_size;
    if (!uncompressed_size.IsValid()) {
      DLOG(ERROR) << "Invalid uncompressed size.";
      return {};
    }
    if (target_uncompressed_size &&
        uncompressed_size.ValueOrDie() > *target_uncompressed_size) {
      break;
    }
    groups_array.emplace_back(std::move(serialized_group.value));
    result.group_names.push_back(group->interest_group.name);
    std::optional<url::Origin> maybe_coordinator =
        group->interest_group.aggregation_coordinator_origin;
    if (maybe_coordinator.has_value()) {
      result.group_pagg_coordinators[blink::InterestGroupKey(
          owner, group->interest_group.name)] = *maybe_coordinator;
    }
    groups_elements_size += serialized_group.size;
    result.num_groups++;
  }

  if (groups_array.empty()) {
    return result;
  }

  base::CheckedNumeric<size_t> inner_size =
      TaggedArrayLength(groups_array, groups_elements_size);
  cbor::Value groups_obj(std::move(groups_array));
  std::optional<std::vector<uint8_t>> maybe_sub_message =
      cbor::Writer::Write(groups_obj);
  DCHECK(maybe_sub_message);
  DCHECK_EQ(static_cast<size_t>(inner_size.ValueOrDie()),
            maybe_sub_message->size());
  std::string compressed_groups;
  bool success =
      compression::GzipCompress(maybe_sub_message.value(), &compressed_groups);
  CHECK(success);

  result.uncompressed_size += maybe_sub_message->size();
  result.data = std::move(compressed_groups);
  return result;
}

SerializedBiddersMap SerializeBidderGroupsWithConfig(
    const std::vector<
        std::pair<url::Origin, std::vector<SingleStorageInterestGroup>>>&
        bidders_and_groups,
    const blink::mojom::AuctionDataConfig& config,
    size_t total_size_before_groups,
    base::Time start_time) {
  BiddingAndAuctionSerializer::TargetSizeEstimator estimator(
      total_size_before_groups, &config);

  // First serialize all of the buyers' groups to determine which buyers will
  // not use all of their space. If they fit without applying limits then we
  // will use this result for the final serialization. This also allows us to
  // estimate the compression ratio for the groups.
  std::vector<CompressedInterestGroups> all_bidders_full_compressed_groups;
  all_bidders_full_compressed_groups.reserve(bidders_and_groups.size());
  for (size_t idx = 0; idx < bidders_and_groups.size(); ++idx) {
    const auto& bidder_groups = bidders_and_groups[idx];
    all_bidders_full_compressed_groups.emplace_back(CompressInterestGroups(
        bidder_groups.first, bidder_groups.second, start_time, std::nullopt));
    estimator.UpdatePerBuyerMaxSize(
        bidder_groups.first,
        all_bidders_full_compressed_groups[idx].data.size());
  }

  SerializedBiddersMap result{{}, {}, 0, 0, 0, 0, {}};
  result.bidders.reserve(bidders_and_groups.size());
  result.group_names.reserve(bidders_and_groups.size());
  for (size_t idx = 0; idx < bidders_and_groups.size(); ++idx) {
    const auto& bidder_groups = bidders_and_groups[idx];
    std::string bidder_origin = bidder_groups.first.Serialize();

    std::optional<uint64_t> target_compressed_size =
        estimator.EstimateTargetSize(bidder_groups.first,
                                     result.bidders_elements_size);

    if (target_compressed_size && target_compressed_size.value() <= 0) {
      // No space for this bidder.
      continue;
    }

    CompressedInterestGroups compressed_groups =
        std::move(all_bidders_full_compressed_groups[idx]);

    if (target_compressed_size) {
      int num_iterations = 0;
      while (compressed_groups.data.size() > *target_compressed_size) {
        num_iterations++;
        if (num_iterations > 20) {
          // Give up after too many iterations. Don't send anything for this
          // bidder.
          compressed_groups = {};
          break;
        }

        // Calculate the target uncompressed size based on the current
        // compression ratio. This will always produce a smaller uncompressed
        // target size than the previous iteration of the loop.
        //
        // Proof:
        // Let
        // C = `compressed_groups.data.size()`
        // U = `compressed_groups.uncompressed_size`
        // T = `*target_compressed_size`
        // D = `current_uncompressed_target_size`
        // D' is the next uncompressed target size
        //
        // The loop condition implies that C > T. Further, we know that
        // CompressInterestGroups will always satisfy U <= D. So we calculate
        // the next target uncompressed size as D'= T*U/C. Rearranging and
        // substituting C > T into D' = T * U / C, we get D' * C / U < C, or
        // D' < U (for positive U and C). This gives us D' < U <= D, implying
        // that the next uncompressed target size is strictly less than the
        // current uncompressed target size. Therefore, the loop will
        // terminate. Further, if the previous compression ratio is the same
        // as the new compression ratio then it will take only a single
        // iteration to converge.
        size_t current_uncompressed_target_size =
            (*target_compressed_size * compressed_groups.uncompressed_size) /
            compressed_groups.data.size();
        // Shrink this a little because smaller things don't compress as well.
        // 15/16 is a bit of a fudge factor that seemed to work well when
        // working with some simulated data. This is not necessary for
        // correctness, but makes things converge faster at the (unlikely)
        // cost of excluding some groups that could have been included.
        current_uncompressed_target_size =
            (current_uncompressed_target_size * 15) / 16;

        compressed_groups = CompressInterestGroups(
            bidder_groups.first, bidder_groups.second, start_time,
            current_uncompressed_target_size);
      }

      // Only record iteration count if we were trying to fit within a
      // specific size.
      base::UmaHistogramCounts100(
          "Ads.InterestGroup.ServerAuction.Request.NumIterations",
          num_iterations);
    }

    if (compressed_groups.num_groups == 0) {
      // Don't include bidder if we couldn't fit any groups.
      continue;
    }

    result.num_groups += compressed_groups.num_groups;
    result.compressed_size += compressed_groups.data.size();
    result.uncompressed_size += compressed_groups.uncompressed_size;
    result.bidders_elements_size +=
        TaggedStringLength(bidder_origin.size()) +
        TaggedStringLength(compressed_groups.data.size());
    result.group_names.emplace(bidder_groups.first,
                               std::move(compressed_groups.group_names));
    result.group_pagg_coordinators.insert(
        std::make_move_iterator(
            compressed_groups.group_pagg_coordinators.begin()),
        std::make_move_iterator(
            compressed_groups.group_pagg_coordinators.end()));
    result.bidders[cbor::Value(bidder_origin)] = cbor::Value(
        std::move(compressed_groups.data), cbor::Value::Type::BYTE_STRING);
  }
  return result;
}

}  // namespace

BiddingAndAuctionData::BiddingAndAuctionData() = default;
BiddingAndAuctionData::BiddingAndAuctionData(BiddingAndAuctionData&& other) =
    default;
BiddingAndAuctionData::~BiddingAndAuctionData() = default;

BiddingAndAuctionData& BiddingAndAuctionData::operator=(
    BiddingAndAuctionData&& other) = default;

BiddingAndAuctionSerializer::TargetSizeEstimator::TargetSizeEstimator(
    size_t total_size_before_groups,
    const blink::mojom::AuctionDataConfig* config)
    : total_size_before_groups_(total_size_before_groups), config_(config) {
  DCHECK(config_);
  DCHECK(config_->request_size || config_->per_buyer_configs.empty());
}

BiddingAndAuctionSerializer::TargetSizeEstimator::~TargetSizeEstimator() =
    default;

void BiddingAndAuctionSerializer::TargetSizeEstimator::UpdatePerBuyerMaxSize(
    const url::Origin& bidder,
    size_t max_size) {
  base::CheckedNumeric<size_t> overhead =
      TaggedStringLength(bidder.Serialize().size());
  overhead += 1 + LengthOfLength(max_size);

  size_t new_size = (overhead + max_size).ValueOrDie();
  per_buyer_size_[bidder] = new_size;

  auto it = config_->per_buyer_configs.find(bidder);
  if (it != config_->per_buyer_configs.end() &&
      it->second->target_size.has_value()) {
    // Update size estimates for sized buyers.
    per_buyer_total_allowed_size_ += it->second->target_size.value();
  } else {
    // Update estimates for unsized buyers.
    total_unsized_buyers_++;
  }
}

std::optional<uint64_t>
BiddingAndAuctionSerializer::TargetSizeEstimator::EstimateTargetSize(
    const url::Origin& bidder,
    base::CheckedNumeric<size_t> bidders_elements_size) {
  if (!config_->request_size) {
    return std::nullopt;
  }
  base::CheckedNumeric<uint64_t> target_compressed_size;
  base::CheckedNumeric<size_t> current_size =
      total_size_before_groups_ + bidders_elements_size;
  if (!current_size.IsValid() ||
      current_size.ValueOrDie() >= *config_->request_size) {
    return 0;
  }
  base::CheckedNumeric<size_t> remaining_size =
      *config_->request_size - current_size;
  DCHECK_LE(static_cast<uint64_t>(per_buyer_current_allowed_size_.ValueOrDie()),
            static_cast<uint64_t>(per_buyer_total_allowed_size_.ValueOrDie()));

  auto it = config_->per_buyer_configs.find(bidder);
  if (it != config_->per_buyer_configs.end() &&
      it->second->target_size.has_value()) {
    size_t buyer_size = it->second->target_size.value();
    if (total_unsized_buyers_ > 0) {
      // If there are groups without specific sizes then just use the target
      // size ("fixed" mode). If we run short then use the remaining space.
      target_compressed_size = remaining_size.Min(buyer_size);
      per_buyer_current_allowed_size_ += buyer_size;
    } else {
      if (per_buyer_current_allowed_size_.ValueOrDie() == 0) {
        // We haven't processed any proportionally-sized buyers yet, so we
        // need to perform our global size estimation to determine how we
        // allocate the entire remaining space.
        UpdateSizedGroupSizes(remaining_size.ValueOrDie());
      }
      base::CheckedNumeric<uint64_t> remaining_per_buyer_size =
          per_buyer_total_allowed_size_ - per_buyer_current_allowed_size_;

      // Although we performed global size assignment, there may be extra
      // space available if a previous buyer didn't use their entire
      // allocation. We expand the allocation proportionally based on the
      // remaining size. Note we cast up to uint64_t to avoid overflow from
      // the multiply.
      target_compressed_size =
          (base::CheckedNumeric<uint64_t>(per_buyer_size_[bidder]) *
           remaining_size) /
          remaining_per_buyer_size;
      per_buyer_current_allowed_size_ += per_buyer_size_[bidder];
    }
  } else {
    // No target size for this bidder. Note that we require all specifically
    // sized buyers to be handled first (order set by
    // InterestGroupManagerImpl::GetInterestGroupAdAuctionData), so if we're
    // here then we must have gone through all of the
    // `per_buyer_total_allowed_size_`.
    DCHECK_EQ(
        static_cast<uint64_t>(per_buyer_current_allowed_size_.ValueOrDie()),
        static_cast<uint64_t>(per_buyer_total_allowed_size_.ValueOrDie()));
    if (!unsized_buyer_size_) {
      // We haven't processed any unsized buyers yet, so we need to perform
      // our global size estimation to determine how we allocate the entire
      // remaining space equally across all unsized buyers.
      UpdateUnsizedGroupSizes(remaining_size.ValueOrDie());
    }
    if (per_buyer_size_[bidder] > unsized_buyer_size_.value()) {
      DCHECK_GT(remaining_unallocated_unsized_buyers_, 0u);
      // Although we performed global size assignment, there may be extra
      // space available if a previous buyer didn't use their entire
      // allocation. We expand the allocation equally among groups that
      // filled up their initial allocation (unallocated groups). This may
      // actually be less than `unsized_buyer_size_` because that was
      // calculated using the ceiling of the average size.
      DCHECK_GE(static_cast<size_t>(remaining_size.ValueOrDie()),
                static_cast<size_t>(
                    remaining_allocated_unsized_buyer_size_.ValueOrDie()));
      target_compressed_size = (base::CheckedNumeric<uint64_t>(remaining_size) -
                                remaining_allocated_unsized_buyer_size_) /
                               remaining_unallocated_unsized_buyers_;
      remaining_unallocated_unsized_buyers_--;
    } else {
      // This buyer could put all of their data in so just set the size to
      // what was required (or remaining size if it ended up being less - this
      // can happen since we took the ceiling of the size in the global
      // allocation).
      target_compressed_size = remaining_size.Min(per_buyer_size_[bidder]);
      remaining_allocated_unsized_buyer_size_ -= per_buyer_size_[bidder];
    }
  }

  // Calculate the overhead for the bidder origin and field length.

  // Size of encoding the bidder origin.
  base::CheckedNumeric<size_t> bidder_origin_overhead =
      TaggedStringLength(bidder.Serialize().size());

  if (!bidder_origin_overhead.IsValid() ||
      target_compressed_size.ValueOrDie() <
          bidder_origin_overhead.ValueOrDie()) {
    // If we don't have enough space for even the bidder origin, then just
    // skip this bidder. We may be able to fit a bidder with a shorter origin
    // though.
    return 0;
  }

  base::CheckedNumeric<size_t> overhead = bidder_origin_overhead;

  // Add the size of encoding the tag and remaining length.
  overhead += 1 + MaxLengthOfTaggedData(
                      (target_compressed_size - overhead).ValueOrDie());

  if (!overhead.IsValid() ||
      overhead.ValueOrDie() > target_compressed_size.ValueOrDie()) {
    // If we don't have enough space for even the overhead, then just skip
    // this bidder. We may be able to fit a bidder with a shorter origin
    // though.
    return 0;
  }

  // Set the target size to the remaining space after considering the
  // overhead.
  target_compressed_size -= overhead;
  return target_compressed_size.ValueOrDie();
}

void BiddingAndAuctionSerializer::TargetSizeEstimator::UpdateSizedGroupSizes(
    size_t remaining_size) {
  std::map<url::Origin, size_t> allocated_sizes;
  base::CheckedNumeric<uint64_t> unallocated_target_size =
      per_buyer_total_allowed_size_;
  base::CheckedNumeric<size_t> unallocated_size = remaining_size;
  std::set<url::Origin> allocated_buyers;

  // Iteratively refine the size estimates by pulling out buyers that are
  // definitely going to get more space than they can use. We stop when either
  // all of the buyers have been removed or no more buyers can be removed.
  for (size_t iteration = 0; iteration < per_buyer_size_.size(); iteration++) {
    base::CheckedNumeric<uint64_t> new_total_per_buyer_size = 0;
    // For each buyer determine if it needs the space it would be allocated.
    // Reallocates unused space from previous iterations.
    for (const auto& [bidder, bidder_config] : config_->per_buyer_configs) {
      // We only use proportional allocation if all buyers have a targetSize.
      DCHECK(bidder_config->target_size.has_value());

      if (allocated_buyers.contains(bidder)) {
        // Already removed from the pool.
        new_total_per_buyer_size += per_buyer_size_[bidder];
        continue;
      }

      // Use the `target_size`s as a weight to allocate the space. The total
      // weight of groups contending for the remaining space is
      // `unallocated_target_size` so the weight for this buyer is
      // `target_size`/`unallocated_target_size`. Note we cast up to
      // uint64_t to avoid overflow from the multiply.
      base::CheckedNumeric<uint64_t> allocated_size =
          (base::CheckedNumeric<uint64_t>(bidder_config->target_size.value()) *
           unallocated_size) /
          unallocated_target_size;

      if (per_buyer_size_[bidder] <= allocated_size.ValueOrDie()) {
        // New bidder that doesn't need any more space. Reserve it for exactly
        // as much space as it needs.
        allocated_sizes[bidder] = per_buyer_size_[bidder];
        allocated_buyers.insert(bidder);
        unallocated_size -= per_buyer_size_[bidder];
        unallocated_target_size -= bidder_config->target_size.value();
        new_total_per_buyer_size += per_buyer_size_[bidder];
        continue;
      }

      allocated_sizes[bidder] = allocated_size.ValueOrDie<size_t>();
      new_total_per_buyer_size += allocated_size;
    }
    // If we've converged to a new total size or we can fit all of the groups
    // for all of the bidders, then we're done.
    if (new_total_per_buyer_size.ValueOrDie() ==
            per_buyer_total_allowed_size_.ValueOrDie() ||
        unallocated_target_size.ValueOrDie() == 0) {
      per_buyer_total_allowed_size_ = new_total_per_buyer_size;
      break;
    }
    per_buyer_total_allowed_size_ = new_total_per_buyer_size;
  }

  for (const auto& [bidder, size] : allocated_sizes) {
    per_buyer_size_[bidder] = size;
  }
}

void BiddingAndAuctionSerializer::TargetSizeEstimator::UpdateUnsizedGroupSizes(
    size_t remaining_size) {
  DCHECK_GT(total_unsized_buyers_, 0u);
  remaining_unallocated_unsized_buyers_ = total_unsized_buyers_;
  base::CheckedNumeric<size_t> unallocated_size = remaining_size;
  std::set<url::Origin> allocated_buyers;
  size_t previous_size_allocation = 0;
  // Iteratively refine the size estimates by pulling out buyers that are
  // definitely going to get more space than they can use. We stop when either
  // all of the buyers have been removed or no more buyers can be removed.
  for (size_t iteration = 0; iteration < per_buyer_size_.size(); iteration++) {
    if (remaining_unallocated_unsized_buyers_ == 0) {
      // All buyers removed.
      break;
    }
    // Set the size allocation to the ceiling of the remaining available space
    // divided by the number of remaining groups. Ceiling is fine because we
    // avoid allocating too much space when we do the final allocation in
    // `EstimateTargetSize`, and in the common case groups will not exactly
    // fit in an allocation. Cast up to uint64_t to avoid any addition
    // overflow before the divide.
    size_t equal_size_allocation =
        ((base::CheckedNumeric<uint64_t>(unallocated_size) +
          remaining_unallocated_unsized_buyers_ - 1) /
         remaining_unallocated_unsized_buyers_)
            .ValueOrDie<size_t>();
    if (equal_size_allocation == previous_size_allocation) {
      // No changes mean no more buyers can be removed, so we're done.
      break;
    }
    unsized_buyer_size_ = equal_size_allocation;

    // For each buyer determine if it needs the space it would be allocated.
    // Reallocates unused space from previous iterations.
    for (const auto& [bidder, buyer_size] : per_buyer_size_) {
      // Skip sized groups.
      auto it = config_->per_buyer_configs.find(bidder);
      if (it != config_->per_buyer_configs.end() &&
          it->second->target_size.has_value()) {
        continue;
      }

      if (allocated_buyers.contains(bidder)) {
        // This was already removed from the pool, so just skip it.
        continue;
      }

      if (buyer_size <= equal_size_allocation) {
        // New bidder that doesn't need any more space. Reserve it for exactly
        // as much space as it needs.
        unallocated_size -= buyer_size;
        allocated_buyers.insert(bidder);
        DCHECK_GT(remaining_unallocated_unsized_buyers_, 0u);
        remaining_unallocated_unsized_buyers_--;
        remaining_allocated_unsized_buyer_size_ += buyer_size;
        continue;
      }
    }
    previous_size_allocation = equal_size_allocation;
  }
}

BiddingAndAuctionSerializer::BiddingAndAuctionSerializer() = default;
BiddingAndAuctionSerializer::BiddingAndAuctionSerializer(
    BiddingAndAuctionSerializer&& other) = default;
BiddingAndAuctionSerializer::~BiddingAndAuctionSerializer() = default;

void BiddingAndAuctionSerializer::AddGroups(
    const url::Origin& owner,
    scoped_refptr<StorageInterestGroups> groups) {
  std::vector<SingleStorageInterestGroup> groups_to_add =
      groups->GetInterestGroups();
  std::erase_if(groups_to_add, [](const SingleStorageInterestGroup& group) {
    return (!group->interest_group.ads) ||
           (group->interest_group.ads->size() == 0);
  });
  // We don't remove owners with no groups here because if the config specified
  // a size for that owner it would mess up the size accounting we do during
  // serialization.

  // Randomize then order, then sort by priority. This insures fairness
  // between groups with the same priority.
  base::RandomShuffle(groups_to_add.begin(), groups_to_add.end());
  base::ranges::stable_sort(
      groups_to_add, [](const SingleStorageInterestGroup& a,
                        const SingleStorageInterestGroup& b) {
        return a->interest_group.priority > b->interest_group.priority;
      });
  accumulated_groups_.emplace_back(std::move(owner), std::move(groups_to_add));
}

BiddingAndAuctionData BiddingAndAuctionSerializer::Build() {
  DCHECK(config_);
  // If we are serializing all groups then we can return an empty list.
  // Otherwise we still need to return a fixed size request (all padding).
  if (config_->per_buyer_configs.empty() && accumulated_groups_.empty()) {
    return {};
  }

  BiddingAndAuctionData data;

  cbor::Value::MapValue message_obj;
  base::CheckedNumeric<size_t> message_elements_size = 0;
  message_obj[cbor::Value("version")] = cbor::Value(0);
  message_elements_size +=
      TaggedStringLength(constexpr_strlen("version")) + TaggedUIntLength(0);
  // "gzip" is the default so we don't need to specify the compression.
  // message_obj[cbor::Value("compression")] = cbor::Value("gzip");
  DCHECK(generation_id_.is_valid());
  std::string generation_id_str = generation_id_.AsLowercaseString();
  message_elements_size +=
      TaggedStringLength(constexpr_strlen("generationId")) +
      TaggedStringLength(generation_id_str.size());
  message_obj[cbor::Value("generationId")] =
      cbor::Value(std::move(generation_id_str));

  message_obj[cbor::Value("publisher")] = cbor::Value(publisher_);
  message_elements_size += TaggedStringLength(constexpr_strlen("publisher")) +
                           TaggedStringLength(publisher_.size());

  message_obj[cbor::Value("enableDebugReporting")] =
      cbor::Value(base::FeatureList::IsEnabled(
                      blink::features::kBiddingAndScoringDebugReportingAPI) &&
                  !debug_report_in_lockout_);
  message_elements_size +=
      TaggedStringLength(constexpr_strlen("enableDebugReporting")) + 1;

  std::string debug_key =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProtectedAudiencesConsentedDebugToken);
  if (!debug_key.empty()) {
    cbor::Value::MapValue debug_map;
    base::CheckedNumeric<size_t> debug_map_elements_size = 0;
    debug_map[cbor::Value("isConsented")] = cbor::Value(true);
    debug_map_elements_size +=
        TaggedStringLength(constexpr_strlen("isConsented")) + 1;
    debug_map[cbor::Value("token")] = cbor::Value(debug_key);
    debug_map_elements_size += TaggedStringLength(constexpr_strlen("token")) +
                               TaggedStringLength(debug_key.size());

    message_elements_size +=
        TaggedStringLength(constexpr_strlen("consentedDebugConfig")) +
        TaggedMapLength(debug_map, debug_map_elements_size);
    message_obj[cbor::Value("consentedDebugConfig")] =
        cbor::Value(std::move(debug_map));
  }
  int64_t timestamp = (timestamp_ - base::Time::UnixEpoch()).InMilliseconds();
  message_obj[cbor::Value("requestTimestampMs")] = cbor::Value(timestamp);
  message_elements_size +=
      TaggedStringLength(constexpr_strlen("requestTimestampMs")) +
      TaggedSIntLength(timestamp);

  // Add a dummy element that we will overwrite later to help us estimate the
  // size of the message.
  message_obj[cbor::Value("interestGroups")] = cbor::Value();
  message_elements_size +=
      TaggedStringLength(constexpr_strlen("interestGroups"));

  const size_t framing_size =
      kFramingHeaderSize + kOhttpHeaderSize +
      (base::FeatureList::IsEnabled(kBiddingAndAuctionEncryptionMediaType) ? 1
                                                                           : 0);
  const base::CheckedNumeric<size_t> total_size_before_groups =
      TaggedMapLength(message_obj,
                      message_elements_size + 1 +
                          LengthOfLength(2 * accumulated_groups_.size())) +
      framing_size;

  if (!total_size_before_groups.IsValid()) {
    DLOG(ERROR) << "total_size_before_groups is invalid";
    return {};
  }

  // If we don't fit in the desired size, don't send anything.
  if (total_size_before_groups.ValueOrDie() >
      config_->request_size.value_or(kBinSizes.back())) {
    return {};
  }

  blink::mojom::AuctionDataConfigPtr config = config_->Clone();
  if (!config->request_size) {
    // If size isn't specified, then we need to fit in the biggest bin.
    config->request_size = kBinSizes.back();
  }

  SerializedBiddersMap groups = SerializeBidderGroupsWithConfig(
      accumulated_groups_, *config, total_size_before_groups.ValueOrDie(),
      timestamp_);

  // If we have no groups and the buyers weren't specified, don't send anything.
  // We still need to provide a non-empty request if the buyers are specified in
  // order to avoid leaking interest groups state.
  if (config->per_buyer_configs.empty() && groups.bidders.empty()) {
    return {};
  }

  message_elements_size +=
      TaggedMapLength(groups.bidders, groups.bidders_elements_size);
  message_obj[cbor::Value("interestGroups")] =
      cbor::Value(std::move(groups.bidders));

  // UMA requires integers, so we scale the relative compressed size by 100.
  if (groups.uncompressed_size > 0) {
    int relative_compressed_size =
        (100 * groups.compressed_size) / groups.uncompressed_size;
    base::UmaHistogramPercentage(
        "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize",
        relative_compressed_size);
  }
  base::UmaHistogramCounts1000(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", groups.num_groups);

  base::CheckedNumeric<size_t> total_size =
      TaggedMapLength(message_obj, message_elements_size);
  cbor::Value message(std::move(message_obj));
  std::optional<std::vector<uint8_t>> maybe_msg = cbor::Writer::Write(message);
  DCHECK(maybe_msg);
  DCHECK_EQ(static_cast<size_t>(total_size.ValueOrDie()), maybe_msg->size());

  base::CheckedNumeric<uint32_t> desired_size;
  if (config->per_buyer_configs.empty()) {
    // If we didn't set a list of buyers then use the requested size as the
    // maximum size bucket.
    const size_t size_before_padding =
        base::CheckAdd(framing_size, maybe_msg->size()).ValueOrDie();
    DCHECK_GE(config->request_size.value(), size_before_padding);

    auto size_iter = std::lower_bound(kBinSizes.begin(), kBinSizes.end(),
                                      size_before_padding);
    if (size_iter != kBinSizes.end()) {
      desired_size = std::min(*size_iter, config->request_size.value());
    } else {
      desired_size = config->request_size.value();
    }
  } else {
    // For customized requests we *MUST* always use the requested size.
    // Since the page can specify which buyers are included in the request, the
    // request size could leak interest group state for a specific buyer if the
    // size was allowed to vary.
    desired_size = config->request_size.value();
  }
  base::CheckedNumeric<size_t> padded_size =
      desired_size - framing_size + kFramingHeaderSize;
  if (!padded_size.IsValid()) {
    DLOG(ERROR) << "padded_size is invalid";
    return {};
  }
  CHECK_GE(static_cast<size_t>(padded_size.ValueOrDie()),
           maybe_msg->size() + kFramingHeaderSize);

  std::vector<uint8_t> request(padded_size.ValueOrDie());
  // first byte is version and compression
  request[0] = (kRequestVersion << kRequestVersionBitOffset) |
               (kGzipCompression << kCompressionBitOffset);
  uint32_t request_size = maybe_msg->size();
  request[1] = (request_size >> 24) & 0xff;
  request[2] = (request_size >> 16) & 0xff;
  request[3] = (request_size >> 8) & 0xff;
  request[4] = (request_size >> 0) & 0xff;

  memcpy(&request[kFramingHeaderSize], maybe_msg->data(), maybe_msg->size());

  data.request = std::move(request);
  data.group_names = std::move(groups.group_names);
  data.group_pagg_coordinators = std::move(groups.group_pagg_coordinators);
  return data;
}

}  // namespace content

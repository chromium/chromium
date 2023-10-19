// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_serializer.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/public/common/content_switches.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/numeric/bits.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/zlib/google/compression_utils.h"

namespace content {

namespace {

const size_t kFramingHeaderSize = 5;  // bytes

const uint8_t kRequestVersion = 0;
const uint8_t kRequestVersionBitOffset = 5;
const uint8_t kGzipCompression = 2;
const uint8_t kCompressionBitOffset = 0;

cbor::Value SerializeAds(const std::vector<blink::InterestGroup::Ad>& ads,
                         bool include_full_ads) {
  cbor::Value::ArrayValue result;
  for (const auto& ad : ads) {
    if (include_full_ads) {
      cbor::Value::MapValue obj;
      obj[cbor::Value("renderURL")] = cbor::Value(ad.render_url.spec());
      if (ad.metadata) {
        obj[cbor::Value("metadata")] = cbor::Value(ad.metadata.value());
      }
      if (ad.size_group) {
        obj[cbor::Value("sizeGroup")] = cbor::Value(ad.size_group.value());
      }
      if (ad.ad_render_id) {
        obj[cbor::Value("adRenderId")] = cbor::Value(ad.ad_render_id.value());
      }
      result.emplace_back(std::move(obj));
    } else {
      if (ad.ad_render_id) {
        result.emplace_back(ad.ad_render_id.value());
      }
    }
  }
  return cbor::Value(std::move(result));
}

// This serialization is sent to the B&A server, so the format is standardized.
// We can't add fields to this format without coordinating with the B&A team.
cbor::Value SerializeInterestGroup(base::Time start_time,
                                   const SingleStorageInterestGroup& group) {
  cbor::Value::MapValue group_obj;
  group_obj[cbor::Value("name")] = cbor::Value(group->interest_group.name);
  if (group->interest_group.trusted_bidding_signals_keys) {
    cbor::Value::ArrayValue bidding_signal_keys;
    bidding_signal_keys.reserve(
        group->interest_group.trusted_bidding_signals_keys->size());
    for (const auto& key :
         *group->interest_group.trusted_bidding_signals_keys) {
      bidding_signal_keys.emplace_back(key);
    }
    group_obj[cbor::Value("biddingSignalsKeys")] =
        cbor::Value(std::move(bidding_signal_keys));
  }
  if (group->interest_group.user_bidding_signals) {
    group_obj[cbor::Value("userBiddingSignals")] =
        cbor::Value(*group->interest_group.user_bidding_signals);
  }
  if (!group->interest_group.auction_server_request_flags.Has(
          blink::AuctionServerRequestFlagsEnum::kOmitAds)) {
    if (group->interest_group.ads) {
      group_obj[cbor::Value("ads")] = SerializeAds(
          *group->interest_group.ads,
          group->interest_group.auction_server_request_flags.Has(
              blink::AuctionServerRequestFlagsEnum::kIncludeFullAds));
    }
    if (group->interest_group.ad_components) {
      group_obj[cbor::Value("components")] = SerializeAds(
          *group->interest_group.ad_components,
          group->interest_group.auction_server_request_flags.Has(
              blink::AuctionServerRequestFlagsEnum::kIncludeFullAds));
    }
  }
  cbor::Value::MapValue browser_signals;
  browser_signals[cbor::Value("bidCount")] =
      cbor::Value(group->bidding_browser_signals->bid_count);
  // joinCount and recency are noised and binned on the server.
  browser_signals[cbor::Value("joinCount")] =
      cbor::Value(group->bidding_browser_signals->join_count);
  browser_signals[cbor::Value("recency")] =
      cbor::Value((start_time - group->join_time).InSeconds());
  cbor::Value::ArrayValue prev_wins;
  for (const auto& prev_win : group->bidding_browser_signals->prev_wins) {
    cbor::Value::ArrayValue tuple;
    tuple.emplace_back((start_time - prev_win->time).InSeconds());
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
      for (const auto kv : ad->GetDict()) {
        switch (kv.second.type()) {
          case base::Value::Type::BOOLEAN:
            obj[cbor::Value(kv.first)] = cbor::Value(kv.second.GetBool());
            break;
          case base::Value::Type::INTEGER:
            obj[cbor::Value(kv.first)] = cbor::Value(kv.second.GetInt());
            break;
          case base::Value::Type::DOUBLE:
            obj[cbor::Value(kv.first)] = cbor::Value(kv.second.GetDouble());
            break;
          case base::Value::Type::STRING:
            obj[cbor::Value(kv.first)] = cbor::Value(kv.second.GetString());
            break;
          default:
            LOG(ERROR) << "Unsupported type in prevWins.ad for key '"
                       << kv.first << "': " << kv.second.DebugString();
        }
      }
      tuple.emplace_back(std::move(obj));
    } else {
      std::string* ad_render_id = ad->GetDict().FindString("adRenderId");
      if (ad_render_id) {
        tuple.emplace_back(*ad_render_id);
      } else {
        // If there's no adRenderId we still can send the time.
        tuple.emplace_back("");
      }
    }
    prev_wins.emplace_back(std::move(tuple));
  }
  browser_signals[cbor::Value("prevWins")] = cbor::Value(std::move(prev_wins));
  group_obj[cbor::Value("browserSignals")] =
      cbor::Value(std::move(browser_signals));
  return cbor::Value(std::move(group_obj));
}

}  // namespace

BiddingAndAuctionData::BiddingAndAuctionData() = default;
BiddingAndAuctionData::BiddingAndAuctionData(BiddingAndAuctionData&& other) =
    default;
BiddingAndAuctionData::~BiddingAndAuctionData() = default;

BiddingAndAuctionData& BiddingAndAuctionData::operator=(
    BiddingAndAuctionData&& other) = default;

BiddingAndAuctionSerializer::BiddingAndAuctionSerializer() {
  start_time_ = base::Time::Now();
}
BiddingAndAuctionSerializer::BiddingAndAuctionSerializer(
    BiddingAndAuctionSerializer&& other) = default;
BiddingAndAuctionSerializer::~BiddingAndAuctionSerializer() = default;

void BiddingAndAuctionSerializer::AddGroups(
    const url::Origin& owner,
    scoped_refptr<StorageInterestGroups> groups) {
  std::vector<SingleStorageInterestGroup> groups_to_add =
      groups->GetInterestGroups();
  base::EraseIf(groups_to_add, [](const SingleStorageInterestGroup& group) {
    return (!group->interest_group.ads) ||
           (group->interest_group.ads->size() == 0);
  });
  if (groups_to_add.size() > 0) {
    accumulated_groups_.emplace_back(std::move(owner),
                                     std::move(groups_to_add));
  }
}

BiddingAndAuctionData BiddingAndAuctionSerializer::Build() {
  if (accumulated_groups_.empty()) {
    return {};
  }
  BiddingAndAuctionData data;

  cbor::Value::MapValue message_obj;
  message_obj[cbor::Value("version")] = cbor::Value(0);
  // "gzip" is the default so we don't need to specify the compression.
  // message_obj[cbor::Value("compression")] = cbor::Value("gzip");
  DCHECK(generation_id_.is_valid());
  message_obj[cbor::Value("generationId")] =
      cbor::Value(generation_id_.AsLowercaseString());
  message_obj[cbor::Value("publisher")] = cbor::Value(publisher_);

  cbor::Value::MapValue groups_map;
  groups_map.reserve(accumulated_groups_.size());
  for (const auto& bidder_groups : accumulated_groups_) {
    cbor::Value::ArrayValue groups;
    std::vector<std::string> names;
    for (const SingleStorageInterestGroup& group : bidder_groups.second) {
      cbor::Value group_obj = SerializeInterestGroup(start_time_, group);
      groups.emplace_back(std::move(group_obj));
      names.push_back(group->interest_group.name);
    }
    cbor::Value groups_obj(std::move(groups));
    absl::optional<std::vector<uint8_t>> maybe_sub_message =
        cbor::Writer::Write(groups_obj);
    DCHECK(maybe_sub_message);
    std::string compressed_groups;
    bool success = compression::GzipCompress(maybe_sub_message.value(),
                                             &compressed_groups);
    DCHECK(success);
    groups_map[cbor::Value(bidder_groups.first.Serialize())] =
        cbor::Value(compressed_groups, cbor::Value::Type::BYTE_STRING);
    data.group_names.emplace(bidder_groups.first, std::move(names));
  }

  message_obj[cbor::Value("enableDebugReporting")] =
      cbor::Value(base::FeatureList::IsEnabled(
          blink::features::kBiddingAndScoringDebugReportingAPI));

  message_obj[cbor::Value("interestGroups")] =
      cbor::Value(std::move(groups_map));

  std::string debug_key =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProtectedAudiencesConsentedDebugToken);
  if (!debug_key.empty()) {
    cbor::Value::MapValue debug_map;
    debug_map[cbor::Value("isConsented")] = cbor::Value(true);
    debug_map[cbor::Value("token")] = cbor::Value(debug_key);

    message_obj[cbor::Value("consentedDebugConfig")] =
        cbor::Value(std::move(debug_map));
  }

  cbor::Value message(std::move(message_obj));
  absl::optional<std::vector<uint8_t>> maybe_msg = cbor::Writer::Write(message);
  DCHECK(maybe_msg);

  size_t size_before_padding = kFramingHeaderSize + maybe_msg->size();
  size_t desired_size = absl::bit_ceil(size_before_padding);

  std::vector<uint8_t> request(desired_size);
  // first byte is version and compression
  request[0] = (kRequestVersion << kRequestVersionBitOffset) |
               (kGzipCompression << kCompressionBitOffset);
  uint32_t request_size = maybe_msg->size();
  request[1] = (request_size >> 24) & 0xff;
  request[2] = (request_size >> 16) & 0xff;
  request[3] = (request_size >> 8) & 0xff;
  request[4] = (request_size >> 0) & 0xff;
  DCHECK_GE(request.size(), kFramingHeaderSize + maybe_msg->size());
  memcpy(&request[kFramingHeaderSize], maybe_msg->data(), maybe_msg->size());

  data.request = std::move(request);
  return data;
}

}  // namespace content

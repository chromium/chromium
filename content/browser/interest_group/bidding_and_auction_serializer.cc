// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_serializer.h"

#include "base/containers/cxx20_erase.h"
#include "base/json/json_string_value_serializer.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/writer.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/zlib/google/compression_utils.h"

namespace content {

namespace {

cbor::Value SerializeAds(const std::vector<blink::InterestGroup::Ad>& ads) {
  cbor::Value::ArrayValue result;
  for (const auto& ad : ads) {
    if (ad.ad_render_id) {
      result.emplace_back(ad.ad_render_id.value());
    }
  }
  return cbor::Value(std::move(result));
}

// This serialization is sent to the B&A server, so the format is standardized.
// We can't add fields to this format without coordinating with the B&A team.
cbor::Value SerializeInterestGroup(base::Time start_time,
                                   const StorageInterestGroup& group) {
  cbor::Value::MapValue group_obj;
  group_obj[cbor::Value("name")] = cbor::Value(group.interest_group.name);
  if (group.interest_group.trusted_bidding_signals_keys) {
    cbor::Value::ArrayValue bidding_signal_keys;
    bidding_signal_keys.reserve(
        group.interest_group.trusted_bidding_signals_keys->size());
    for (const auto& key : *group.interest_group.trusted_bidding_signals_keys) {
      bidding_signal_keys.emplace_back(key);
    }
    group_obj[cbor::Value("biddingSignalsKeys")] =
        cbor::Value(std::move(bidding_signal_keys));
  }
  if (group.interest_group.user_bidding_signals) {
    group_obj[cbor::Value("userBiddingSignals")] =
        cbor::Value(*group.interest_group.user_bidding_signals);
  }
  if (group.interest_group.ads) {
    group_obj[cbor::Value("ads")] = SerializeAds(*group.interest_group.ads);
  }
  if (group.interest_group.ad_components) {
    group_obj[cbor::Value("adComponents")] =
        SerializeAds(*group.interest_group.ad_components);
  }
  cbor::Value::MapValue browser_signals;
  browser_signals[cbor::Value("bidCount")] =
      cbor::Value(group.bidding_browser_signals->bid_count);
  // joinCount and recency are noised and binned on the server.
  browser_signals[cbor::Value("joinCount")] =
      cbor::Value(group.bidding_browser_signals->join_count);
  browser_signals[cbor::Value("recency")] =
      cbor::Value((start_time - group.join_time).InSeconds());
  cbor::Value::ArrayValue prev_wins;
  for (const auto& prev_win : group.bidding_browser_signals->prev_wins) {
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
    std::string* ad_render_id = ad->GetDict().FindString("adRenderId");
    if (ad_render_id) {
      tuple.emplace_back(*ad_render_id);
    } else {
      // If there's no adRenderId we still can send the time.
      tuple.emplace_back("");
    }
    prev_wins.emplace_back(std::move(tuple));
  }
  browser_signals[cbor::Value("prevWins")] = cbor::Value(std::move(prev_wins));
  group_obj[cbor::Value("browserSignals")] =
      cbor::Value(std::move(browser_signals));
  return cbor::Value(std::move(group_obj));
}

}  // namespace

BiddingAndAuctionSerializer::BiddingAndAuctionSerializer() {
  start_time_ = base::Time::Now();
}
BiddingAndAuctionSerializer::BiddingAndAuctionSerializer(
    BiddingAndAuctionSerializer&& state) = default;
BiddingAndAuctionSerializer::~BiddingAndAuctionSerializer() = default;

void BiddingAndAuctionSerializer::AddGroups(
    std::string owner,
    std::vector<StorageInterestGroup> groups) {
  base::EraseIf(groups, [](const StorageInterestGroup& group) {
    return (!group.interest_group.ads) ||
           (group.interest_group.ads->size() == 0);
  });
  if (groups.size() > 0) {
    accumulated_groups_.emplace_back(owner, std::move(groups));
  }
}

std::vector<uint8_t> BiddingAndAuctionSerializer::Build() {
  if (accumulated_groups_.empty()) {
    return {};
  }
  cbor::Value::MapValue groups_map;
  groups_map.reserve(accumulated_groups_.size());
  for (const auto& bidder_groups : accumulated_groups_) {
    cbor::Value::ArrayValue groups;
    for (const auto& group : bidder_groups.second) {
      cbor::Value group_obj = SerializeInterestGroup(start_time_, group);
      groups.emplace_back(std::move(group_obj));
    }
    cbor::Value groups_obj(std::move(groups));
    absl::optional<std::vector<uint8_t>> maybe_sub_message =
        cbor::Writer::Write(groups_obj);
    DCHECK(maybe_sub_message);
    std::string compressed_groups;
    bool success = compression::GzipCompress(maybe_sub_message.value(),
                                             &compressed_groups);
    DCHECK(success);
    groups_map[cbor::Value(bidder_groups.first)] =
        cbor::Value(compressed_groups, cbor::Value::Type::BYTE_STRING);
  }
  cbor::Value::MapValue message_obj;
  message_obj[cbor::Value("version")] = cbor::Value(0);
  // "gzip" is the default so we don't need to specify the compression.
  // message_obj[cbor::Value("compression")] = cbor::Value("gzip");
  message_obj[cbor::Value("publisher")] = cbor::Value(publisher_);
  message_obj[cbor::Value("interestGroups")] =
      cbor::Value(std::move(groups_map));
  cbor::Value message(std::move(message_obj));

  absl::optional<std::vector<uint8_t>> maybe_msg = cbor::Writer::Write(message);
  DCHECK(maybe_msg);
  return *maybe_msg;
}

}  // namespace content

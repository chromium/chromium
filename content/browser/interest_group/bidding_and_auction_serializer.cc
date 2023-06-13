// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_serializer.h"

#include <algorithm>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/json/json_string_value_serializer.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/numeric/bits.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/zlib/google/compression_utils.h"

namespace content {

namespace {

// This function calculates the number of bytes needed by CBOR to encode the
// positive integer `value`. Note that values below 24 can be stored inside the
// tag field for a size of 0.
size_t GetNumUintBytes(uint64_t value) {
  if (value < 24) {
    return 0;
  } else if (value <= 0xFF) {
    return 1;
  } else if (value <= 0xFFFF) {
    return 2;
  } else if (value <= 0xFFFFFFFF) {
    return 4;
  }
  return 8;
}

size_t SerializedLength(const cbor::Value& value);
size_t SerializedLength(const cbor::Value::MapValue& map) {
  size_t elementsSize = 0;
  for (const auto& kv : map) {
    elementsSize += SerializedLength(kv.first);
    elementsSize += SerializedLength(kv.second);
  }
  return 1 + GetNumUintBytes(map.size()) + elementsSize;
}

size_t SerializedLength(const cbor::Value& value) {
  switch (value.type()) {
    case cbor::Value::Type::UNSIGNED:
      return 1 + GetNumUintBytes(value.GetUnsigned());
    case cbor::Value::Type::BYTE_STRING: {
      size_t len = value.GetBytestring().size();
      return 1 + GetNumUintBytes(len) + len;
    }
    case cbor::Value::Type::STRING: {
      size_t len = value.GetString().size();
      return 1 + GetNumUintBytes(len) + len;
    }
    case cbor::Value::Type::ARRAY: {
      size_t elementsSize = 0;
      const cbor::Value::ArrayValue& array = value.GetArray();
      for (const auto& sub_value : array) {
        elementsSize += SerializedLength(sub_value);
      }
      return 1 + GetNumUintBytes(array.size()) + elementsSize;
    }
    case cbor::Value::Type::MAP: {
      return SerializedLength(value.GetMap());
    }
    case cbor::Value::Type::SIMPLE_VALUE:
      return 1;
    default:
      NOTREACHED();
      return 0;
  }
}

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
    url::Origin owner,
    std::vector<StorageInterestGroup> groups) {
  base::EraseIf(groups, [](const StorageInterestGroup& group) {
    return (!group.interest_group.ads) ||
           (group.interest_group.ads->size() == 0);
  });
  if (groups.size() > 0) {
    accumulated_groups_.emplace_back(std::move(owner), std::move(groups));
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
    for (const auto& group : bidder_groups.second) {
      cbor::Value group_obj = SerializeInterestGroup(start_time_, group);
      groups.emplace_back(std::move(group_obj));
      names.push_back(group.interest_group.name);
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

  message_obj[cbor::Value("interestGroups")] =
      cbor::Value(std::move(groups_map));

  size_t size_before_padding = SerializedLength(message_obj);
  size_t desired_size = absl::bit_ceil(size_before_padding);

  // CBOR doesn't support trailing data so we need to pad inside the CBOR
  // object.
  // We add some "dummy" keys (the numbers 0 and 1) with bytestrings containing
  // '\0's to pad the message up to the correct size. Due to the way size is
  // encoded in cbor we can't always generate a message with the right size by
  // adding a single field, so sometimes we add two fields.
  if (desired_size != size_before_padding) {
    // The smallest amount of padding we can add is 2 bytes.
    if (desired_size < size_before_padding + 2) {
      desired_size *= 2;
    }
    // 1st guess (minimum overhead)
    // Because the length of fields in CBOR are variable lengths it takes us a
    // couple of tries to calculate the exact amount of padding we need. First
    // we guess with the minimum amount of overhead for the length.
    size_t size_to_pad = desired_size - size_before_padding - 2;
    size_t overhead = /*key*/ 1 + /*value*/ 1 + GetNumUintBytes(size_to_pad);

    // 2nd guess
    // Then we guess assuming the overhead we calculated previously. This is the
    // right number most of the time, but there is an edge case when the encoded
    // size of the length of the field changes.
    size_to_pad = desired_size - size_before_padding - overhead;
    overhead = /*key*/ 1 + /*value*/ 1 + GetNumUintBytes(size_to_pad);

    // 3rd_guess
    // Now calculate the final size. This *should* match the size we estimated
    // previously unless we're at one of the sizes that we can't pad with a
    // single field.
    size_t third_size_to_pad = desired_size - size_before_padding - overhead;

    if (third_size_to_pad != size_to_pad) {
      // There are some sizes we can't do because of the way CBOR encodes
      // lengths. We get around this by adding an additional 2 byte field.
      message_obj[cbor::Value(1)] = cbor::Value(0);

      // Now we recalculate the padding size accounting for the additional two
      // bytes of padding. Since we're avoiding the increase in the encoded size
      // for the length of the padding by using an additional field, the encoded
      // size for the length of the padding will be stable around our estimate
      // so we can calculate the padding size in just two more iterations.
      size_to_pad = desired_size - size_before_padding - overhead - 2;
      overhead = /*key*/ 1 + /*value*/ 1 + GetNumUintBytes(size_to_pad);
      size_to_pad = desired_size - size_before_padding - overhead - 2;
    }

    message_obj[cbor::Value(0)] =
        cbor::Value(std::vector<uint8_t>(size_to_pad, '\0'));
  }

  cbor::Value message(std::move(message_obj));
  absl::optional<std::vector<uint8_t>> maybe_msg = cbor::Writer::Write(message);
  DCHECK(maybe_msg);
  data.request = std::move(*maybe_msg);
  return data;
}

}  // namespace content

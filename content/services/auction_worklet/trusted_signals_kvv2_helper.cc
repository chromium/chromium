// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/common/features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "url/origin.h"

namespace auction_worklet {

namespace {
// Constants for POST request body.
constexpr std::array<std::string_view, 2> kAcceptCompression = {"none", "gzip"};
constexpr size_t kFramingHeaderSize = 5;  // bytes

// Add hardcoded `acceptCompression` to request body.
void AddPostRequestConstants(cbor::Value::MapValue& request_map_value) {
  // acceptCompression
  cbor::Value::ArrayValue accept_compression(kAcceptCompression.begin(),
                                             kAcceptCompression.end());
  request_map_value.try_emplace(cbor::Value("acceptCompression"),
                                cbor::Value(std::move(accept_compression)));

  return;
}

std::string CreateRequestBody(cbor::Value::MapValue request_map_value) {
  cbor::Value message(std::move(request_map_value));
  std::optional<std::vector<uint8_t>> maybe_msg = cbor::Writer::Write(message);
  CHECK(maybe_msg.has_value());

  // TODO(crbug.com/337917489): Skip padding for now, and will add padding after
  // end to end tests.
  std::string request_body;
  request_body.resize(kFramingHeaderSize + maybe_msg->size());
  base::SpanWriter writer(
      base::as_writable_bytes(base::make_span(request_body)));
  // First byte includes version and compression format. Always set first bytes
  // to 0x00 because request body is not compressed.
  writer.WriteU8BigEndian(0x00);
  writer.WriteU32BigEndian(base::checked_cast<int>(maybe_msg->size()));
  writer.Write(base::as_bytes(base::make_span(*maybe_msg)));

  return request_body;
}

// Creates a single entry for the "arguments" array of a partition, with a
// single tag and a variable number of data values.
cbor::Value MakeArgument(std::string_view tag,
                         const std::set<std::string>& data) {
  cbor::Value::MapValue argument;

  cbor::Value::ArrayValue tags;
  tags.emplace_back(tag);

  cbor::Value::ArrayValue cbor_data;
  for (const auto& element : data) {
    cbor_data.emplace_back(element);
  }

  argument.emplace(cbor::Value("tags"), cbor::Value(std::move(tags)));
  argument.emplace(cbor::Value("data"), cbor::Value(std::move(cbor_data)));
  return cbor::Value(std::move(argument));
}
}  // namespace

TrustedSignalsKVv2RequestHelper::TrustedSignalsKVv2RequestHelper(
    std::string post_request_body)
    : post_request_body_(std::move(post_request_body)) {}

TrustedSignalsKVv2RequestHelper::TrustedSignalsKVv2RequestHelper(
    TrustedSignalsKVv2RequestHelper&&) = default;

TrustedSignalsKVv2RequestHelper& TrustedSignalsKVv2RequestHelper::operator=(
    TrustedSignalsKVv2RequestHelper&&) = default;

TrustedSignalsKVv2RequestHelper::~TrustedSignalsKVv2RequestHelper() = default;

std::string TrustedSignalsKVv2RequestHelper::TakePostRequestBody() {
  return std::move(post_request_body_);
}

TrustedSignalsKVv2RequestHelperBuilder ::
    ~TrustedSignalsKVv2RequestHelperBuilder() = default;

TrustedSignalsKVv2RequestHelperBuilder::TrustedSignalsKVv2RequestHelperBuilder(
    std::string hostname,
    GURL trusted_signals_url,
    std::optional<int> experiment_group_id)
    : hostname_(std::move(hostname)),
      trusted_signals_url_(std::move(trusted_signals_url)),
      experiment_group_id_(experiment_group_id) {}

TrustedSignalsKVv2RequestHelperBuilder::Partition::Partition() = default;

TrustedSignalsKVv2RequestHelperBuilder::Partition::Partition(
    int partition_id,
    const std::string& interest_group_name,
    const std::set<std::string>& bidding_keys,
    const std::string& hostname,
    const std::optional<int>& experiment_group_id,
    std::pair<std::string, std::string> trusted_bidding_signals_slot_size_param)
    : partition_id(partition_id),
      interest_group_names({interest_group_name}),
      bidding_signals_keys(bidding_keys) {
  additional_params.Set("hostname", hostname);
  if (experiment_group_id.has_value()) {
    additional_params.Set("experimentGroupId",
                          base::NumberToString(experiment_group_id.value()));
  }
  additional_params.Set(trusted_bidding_signals_slot_size_param.first,
                        trusted_bidding_signals_slot_size_param.second);
}

TrustedSignalsKVv2RequestHelperBuilder::Partition::Partition(Partition&&) =
    default;

TrustedSignalsKVv2RequestHelperBuilder::Partition::~Partition() = default;

TrustedSignalsKVv2RequestHelperBuilder::Partition&
TrustedSignalsKVv2RequestHelperBuilder::Partition::operator=(Partition&&) =
    default;

TrustedBiddingSignalsKVv2RequestHelperBuilder::
    TrustedBiddingSignalsKVv2RequestHelperBuilder(
        const std::string& hostname,
        const GURL& trusted_signals_url,
        std::optional<int> experiment_group_id,
        const std::string& trusted_bidding_signals_slot_size_param)
    : TrustedSignalsKVv2RequestHelperBuilder(hostname,
                                             trusted_signals_url,
                                             experiment_group_id) {
  // Parse trusted bidding signals slot size parameter to a pair, which
  // parameter key is first and value is second.
  if (!trusted_bidding_signals_slot_size_param.empty()) {
    size_t pos = trusted_bidding_signals_slot_size_param.find('=');
    CHECK_NE(pos, std::string::npos);
    std::string key = trusted_bidding_signals_slot_size_param.substr(0, pos);
    std::string value = trusted_bidding_signals_slot_size_param.substr(pos + 1);
    CHECK(key == "slotSize" || key == "allSlotsRequestedSizes");
    trusted_bidding_signals_slot_size_param_ = {std::move(key),
                                                std::move(value)};
  }
}

TrustedBiddingSignalsKVv2RequestHelperBuilder::
    ~TrustedBiddingSignalsKVv2RequestHelperBuilder() = default;

TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex
TrustedBiddingSignalsKVv2RequestHelperBuilder::AddTrustedSignalsRequest(
    base::optional_ref<const std::string> interest_group_name,
    base::optional_ref<const std::set<std::string>> bidding_keys,
    base::optional_ref<const url::Origin> interest_group_join_origin,
    std::optional<blink::mojom::InterestGroup::ExecutionMode> execution_mode) {
  DCHECK(interest_group_name.has_value());
  DCHECK(bidding_keys.has_value());
  DCHECK(interest_group_join_origin.has_value());
  DCHECK(execution_mode.has_value());

  int partition_id;
  int compression_group_id;

  // Find or create a compression group.
  auto join_origin_compression_id_it =
      join_origin_compression_id_map().find(interest_group_join_origin.value());
  CompressionGroup* compression_group_ptr;
  if (join_origin_compression_id_it == join_origin_compression_id_map().end()) {
    // Create a new compression group keyed by joining origin.
    compression_group_id = next_compression_group_id();
    join_origin_compression_id_map().emplace(interest_group_join_origin.value(),
                                             compression_group_id);
    compression_group_ptr =
        &compression_groups().try_emplace(compression_group_id).first->second;
  } else {
    // Found existing compression group.
    compression_group_id = join_origin_compression_id_it->second;
    DCHECK_EQ(1u, compression_groups().count(compression_group_id));
    compression_group_ptr = &compression_groups()[compression_group_id];
  }

  // Get partition id based on execution mode.
  auto partition_it = compression_group_ptr->end();
  if (execution_mode ==
      blink::InterestGroup::ExecutionMode::kGroupedByOriginMode) {
    // Put current interest group to the existing "group-by-origin partition
    // which is always index 0.
    partition_id = 0;
    partition_it = compression_group_ptr->find(partition_id);
  } else {
    // If execution mode is not `kGroupedByOriginMode`, we assign new partition
    // start from index 1. To make partition id consecutive, we use compression
    // group size if there is a "group-by-origin" partition, otherwise use size
    // plus 1.
    if (compression_group_ptr->contains(0)) {
      partition_id = compression_group_ptr->size();
    } else {
      partition_id = compression_group_ptr->size() + 1;
    }
    DCHECK_EQ(0u, compression_group_ptr->count(partition_id));
  }

  // Find or create partition.
  if (partition_it == compression_group_ptr->end()) {
    Partition new_partition(partition_id, interest_group_name.value(),
                            bidding_keys.value(), hostname(),
                            experiment_group_id(),
                            trusted_bidding_signals_slot_size_param_);
    compression_group_ptr->emplace(partition_id, std::move(new_partition));
  } else {
    // We only reuse the group-by-origin partition.
    DCHECK_EQ(0, partition_id);
    DCHECK_EQ(blink::InterestGroup::ExecutionMode::kGroupedByOriginMode,
              execution_mode.value());
    partition_it->second.interest_group_names.insert(
        interest_group_name.value());
    partition_it->second.bidding_signals_keys.insert(bidding_keys->begin(),
                                                     bidding_keys->end());
  }

  return IsolationIndex(compression_group_id, partition_id);
}

TrustedSignalsKVv2RequestHelper
TrustedBiddingSignalsKVv2RequestHelperBuilder::Build() {
  cbor::Value::MapValue request_map_value;
  AddPostRequestConstants(request_map_value);

  cbor::Value::ArrayValue partition_array;

  for (const auto& group_pair : compression_groups()) {
    int compression_group_id = group_pair.first;
    const CompressionGroup& partition_map = group_pair.second;

    for (const auto& partition_pair : partition_map) {
      const Partition& partition = partition_pair.second;
      cbor::Value::MapValue partition_cbor_map = BuildMapForPartition(
          partition, partition.partition_id, compression_group_id);
      partition_array.emplace_back(partition_cbor_map);
    }
  }

  request_map_value.try_emplace(cbor::Value("partitions"),
                                cbor::Value(std::move(partition_array)));
  std::string request_body = CreateRequestBody(std::move(request_map_value));

  return TrustedSignalsKVv2RequestHelper(std::move(request_body));
}

cbor::Value::MapValue
TrustedBiddingSignalsKVv2RequestHelperBuilder::BuildMapForPartition(
    const Partition& partition,
    int partition_id,
    int compression_group_id) {
  cbor::Value::MapValue partition_cbor_map;

  partition_cbor_map.try_emplace(cbor::Value("id"), cbor::Value(partition_id));
  partition_cbor_map.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(compression_group_id));

  // metadata
  cbor::Value::MapValue metadata;
  for (const auto param : partition.additional_params) {
    CHECK(param.second.is_string());
    // TODO(xtlsheep): The slot size param probably will be changed to a new
    // format in the future. Check if these are still the right types if the
    // spec is changed.
    metadata.try_emplace(cbor::Value(param.first),
                         cbor::Value(param.second.GetString()));
  }
  partition_cbor_map.try_emplace(cbor::Value("metadata"),
                                 cbor::Value(std::move(metadata)));

  cbor::Value::ArrayValue arguments;
  arguments.emplace_back(
      MakeArgument("interestGroupNames", partition.interest_group_names));
  arguments.emplace_back(MakeArgument("keys", partition.bidding_signals_keys));

  partition_cbor_map.try_emplace(cbor::Value("arguments"),
                                 cbor::Value(std::move(arguments)));
  return partition_cbor_map;
}

}  // namespace auction_worklet

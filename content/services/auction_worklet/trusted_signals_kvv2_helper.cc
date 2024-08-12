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
#include "base/containers/span_reader.h"
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
#include "third_party/zlib/google/compression_utils.h"
#include "url/origin.h"

namespace auction_worklet {

namespace {

// Constants for POST request body.
constexpr std::array<std::string_view, 2> kAcceptCompression = {"none", "gzip"};
constexpr size_t kCompressionFormatSize = 1;  // bytes
constexpr size_t kCborStringLengthSize = 4;   // bytes
constexpr size_t kOhttpHeaderSize = 55;       // bytes
constexpr char kTagInterestGroupName[] = "interestGroupNames";
constexpr char kTagKey[] = "keys";

// Add hardcoded `acceptCompression` to request body.
void AddPostRequestConstants(cbor::Value::MapValue& request_map_value) {
  // acceptCompression
  cbor::Value::ArrayValue accept_compression(kAcceptCompression.begin(),
                                             kAcceptCompression.end());
  request_map_value.try_emplace(cbor::Value("acceptCompression"),
                                cbor::Value(std::move(accept_compression)));

  return;
}

quiche::ObliviousHttpRequest CreateOHttpRequest(
    mojom::TrustedSignalsPublicKeyPtr public_key,
    cbor::Value::MapValue request_map_value) {
  cbor::Value cbor_value(request_map_value);
  std::optional<std::vector<uint8_t>> maybe_cbor_bytes =
      cbor::Writer::Write(cbor_value);
  CHECK(maybe_cbor_bytes.has_value());

  std::string request_body;
  size_t size_before_padding = kOhttpHeaderSize + kCompressionFormatSize +
                               kCborStringLengthSize + maybe_cbor_bytes->size();
  size_t desired_size = std::bit_ceil(size_before_padding);
  size_t request_body_size = desired_size - kOhttpHeaderSize;
  request_body.resize(request_body_size, 0x00);

  base::SpanWriter writer(
      base::as_writable_bytes(base::make_span(request_body)));

  // TODO(crbug.com/337917489): Add encryption here for compression scheme, CBOR
  // string length and CBOR string later.
  //
  // Add framing header. First byte includes version and compression format.
  // Always set first byte to 0x00 because request body is uncompressed.
  writer.WriteU8BigEndian(0x00);
  writer.WriteU32BigEndian(
      base::checked_cast<uint32_t>(maybe_cbor_bytes->size()));

  // Add CBOR string.
  writer.Write(base::as_bytes(base::make_span(*maybe_cbor_bytes)));

  // Add encryption for request body.
  auto maybe_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      public_key->id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  CHECK(maybe_key_config.ok()) << maybe_key_config.status();

  auto maybe_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          std::move(request_body), public_key->key, maybe_key_config.value(),
          kTrustedSignalsKVv2EncryptionRequestMediaType);
  CHECK(maybe_request.ok()) << maybe_request.status();

  return std::move(maybe_request).value();
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

// Parse CBOR value to a CompressionGroupResult and a compression group ID.
// Or return ErrorInfo in case of any failure and `compression_group_id_out`
// will not be changed.
base::expected<CompressionGroupResult,
               TrustedSignalsKVv2ResponseParser::ErrorInfo>
ParseCompressionGroup(
    const cbor::Value& group,
    auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme,
    int& compression_group_id_out) {
  if (!group.is_map()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Compression group is not type of Map."));
  }
  const cbor::Value::MapValue& group_map = group.GetMap();
  auto compression_group_id_it =
      group_map.find(cbor::Value("compressionGroupId"));
  auto content_it = group_map.find(cbor::Value("content"));
  if (compression_group_id_it == group_map.end()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Key \"compressionGroupId\" is missing in compressionGroups map."));
  }
  if (content_it == group_map.end()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Key \"content\" is missing in compressionGroups map."));
  }

  // Get compression group id.
  const cbor::Value& compression_group_id_value =
      compression_group_id_it->second;
  if (!compression_group_id_value.is_integer()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Compression group id is not type of Integer."));
  }
  // Compression group id must be a valid 32-bit integer.
  if (!base::IsValueInRangeForNumericType<int>(
          compression_group_id_value.GetInteger())) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Compression group id is out of range for int."));
  }

  // Get ttl if the field is set.
  base::TimeDelta ttl;
  auto ttl_ms_it = group_map.find(cbor::Value("ttlMs"));
  if (ttl_ms_it != group_map.end()) {
    const cbor::Value& ttl_ms_value = ttl_ms_it->second;
    if (!ttl_ms_value.is_integer()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "Compression group ttl is not type of Integer."));
    }
    ttl = base::Milliseconds(ttl_ms_value.GetInteger());
  }

  // Get content
  const cbor::Value& content_value = content_it->second;
  if (!content_value.is_bytestring()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Compression group content is not type of Byte String."));
  }

  compression_group_id_out =
      static_cast<int>(compression_group_id_value.GetInteger());
  return CompressionGroupResult(compression_scheme,
                                content_value.GetBytestring(), ttl);
}

// Extract compression schema and cbor string from response body base on
// `kCompressionFormatSize` and `kCborStringLengthSize`. Return ErrorInfo
// in case of any failure.
base::expected<
    std::pair<auction_worklet::mojom::TrustedSignalsCompressionScheme,
              std::vector<uint8_t>>,
    TrustedSignalsKVv2ResponseParser::ErrorInfo>
ExtractCompressionSchemaAndCborStringFromResponseBody(
    base::span<const uint8_t> response_body) {
  if (response_body.size() <= kCompressionFormatSize + kCborStringLengthSize) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Response shorter than framing header."));
  }
  base::SpanReader reader(response_body);

  // TODO(crbug.com/337917489): Add decryption here for compression scheme, CBOR
  // string length and CBOR string later.
  //
  // Get compression scheme.
  auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme;
  // The higher bits are reserved and may be non-zero.
  uint8_t compression_format;
  if (!reader.ReadU8BigEndian(compression_format)) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Failed to read compression format byte."));
  }

  // Only the first two LSBs are used for compression format in the whole byte.
  compression_format &= 0x03;
  if (compression_format == 0x00) {
    compression_scheme =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone;
  } else if (compression_format == 0x02) {
    compression_scheme =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip;
  } else {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Unsupported compression scheme."));
  }

  // Get CBOR bytes length.
  uint32_t length;
  if (!reader.ReadU32BigEndian(length)) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Failed to read CBOR string length."));
  }

  // Get CBOR string.
  std::vector<uint8_t> cbor_bytes(
      response_body.begin() + kCompressionFormatSize + kCborStringLengthSize,
      response_body.begin() + kCompressionFormatSize + kCborStringLengthSize +
          length);

  return std::pair(compression_scheme, cbor_bytes);
}

// Parse a CBOR ArrayValue to a map. `key_group_outputs` should be the value of
// the `keyGroupOutput` field in the partition. Each entry of the array is
// expected to have the following form:
//
// {
//   "tags": [ <tag> ],
//   "keyValues: {<keyValueMap>}
// }
//
// The returned map has keys of <tag> with values of {<keyValueMap>}.
//
// If any value in the array is not in the expected format, including
// cases with multiple tags or keyValueMaps, or if any item is of the
// wrong type, the call fails with an error.
base::expected<std::map<std::string, const cbor::Value::MapValue*>,
               TrustedSignalsKVv2ResponseParser::ErrorInfo>
ParseKeyGroupOutputsToMap(const cbor::Value::ArrayValue& key_group_outputs) {
  std::map<std::string, const cbor::Value::MapValue*> key_group_outputs_map;
  cbor::Value tags_key("tags");
  cbor::Value key_values_key("keyValues");

  for (const auto& output_value : key_group_outputs) {
    // Parse each entry of `key_group_outputs` array to map.
    if (!output_value.is_map()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "KeyGroupOutput value is not type of Map."));
    }

    const cbor::Value::MapValue& key_group_output = output_value.GetMap();
    auto tags_it = key_group_output.find(tags_key);
    auto key_values_it = key_group_output.find(key_values_key);
    if (tags_it == key_group_output.end()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "Key \"tags\" is missing in keyGroupOutputs map."));
    }
    if (key_values_it == key_group_output.end()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "Key \"keyValues\" is missing in keyGroupOutputs map."));
    }

    // Get tags array.
    const cbor::Value& tags_value = tags_it->second;
    if (!tags_value.is_array()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "Tags value in keyGroupOutputs map is not type of Array."));
    }
    const cbor::Value::ArrayValue& tags = tags_value.GetArray();

    if (tags.size() != 1) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "Tags array must only have one tag."));
    }
    if (!tags[0].is_string()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "Tag value in tags array of keyGroupOutputs map is not type of "
          "String."));
    }
    const std::string& tag_string = tags[0].GetString();

    // Get keyValues map.
    const cbor::Value& key_values_value = key_values_it->second;
    if (!key_values_value.is_map()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "KeyValue value in keyGroupOutputs map is not type of Map."));
    }

    // Try to emplace tag to `key_group_outputs_map`. Return an error if
    // it already exists.
    auto tags_vector_emplace_result = key_group_outputs_map.try_emplace(
        tag_string, &key_values_value.GetMap());
    if (!tags_vector_emplace_result.second) {
      return base::unexpected(
          TrustedSignalsKVv2ResponseParser::ErrorInfo(base::StringPrintf(
              "Duplicate tag \"%s\" detected in keyGroupOutputs.",
              tag_string.c_str())));
    }
  }

  return key_group_outputs_map;
}

// When we have a <tag> - <keyValue map> in `keyGroupOutputs` map, the <keyValue
// map> is like this:
//
// {
//   "keyA" : {"value" : "<JSON valueA>"},
//   "keyB" : {"value" : "<JSON valueB>"}
// }
//
// The input pair is the result of using a key, such as "keyA," to find an
// entry in the above map. This method aims to use the iterator to retrieve the
// found value map and get the JSON-format string, such as ""valueForA"" or
// "["value1ForB","value2ForB"]".
base::expected<std::string_view, TrustedSignalsKVv2ResponseParser::ErrorInfo>
GetKeyValueDataString(
    const std::pair<cbor::Value, cbor::Value>& key_value_pair) {
  const cbor::Value& cbor_value = key_value_pair.second;
  if (!cbor_value.is_map()) {
    // It is up to the caller to guarantee this.
    CHECK(key_value_pair.first.is_string());
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        base::StringPrintf("Value of \"%s\" is not type of Map.",
                           key_value_pair.first.GetString().c_str())));
  }
  const cbor::Value::MapValue& cbor_value_map = cbor_value.GetMap();
  auto value_it = cbor_value_map.find(cbor::Value("value"));
  if (value_it == cbor_value_map.end()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Failed to find key \"value\" in the map."));
  }
  const cbor::Value& value_data = value_it->second;
  if (!value_data.is_string()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Failed to read value of key \"value\" as type String."));
  }
  return value_data.GetString();
}

// Retrieve the data string corresponding to each `key` from `keys` in
// `key_group_output_map` and serialize it to `AuctionV8Helper::SerializedValue`
// as the value. Insert this into a map with the `key` as the key. Return
// `ErrorInfo` in case of any failure.
base::expected<std::map<std::string, AuctionV8Helper::SerializedValue>,
               TrustedSignalsKVv2ResponseParser::ErrorInfo>
SerializeKeyGroupOutputsMap(AuctionV8Helper* v8_helper,
                            const cbor::Value::MapValue& key_group_output_map,
                            const std::set<std::string>& keys) {
  std::map<std::string, AuctionV8Helper::SerializedValue> serialized_value_map;

  for (const auto& key : keys) {
    auto cbor_it = key_group_output_map.find(cbor::Value(key));
    if (cbor_it != key_group_output_map.end()) {
      base::expected<std::string_view,
                     TrustedSignalsKVv2ResponseParser::ErrorInfo>
          data_string = GetKeyValueDataString(*cbor_it);

      if (!data_string.has_value()) {
        return base::unexpected(std::move(data_string).error());
      }

      v8::Local<v8::Value> data_v8_value;
      if (!v8_helper
               ->CreateValueFromJson(v8_helper->scratch_context(),
                                     std::move(data_string).value())
               .ToLocal(&data_v8_value)) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Failed to parse key-value string to JSON."));
      }

      AuctionV8Helper::SerializedValue serialized_value =
          v8_helper->Serialize(v8_helper->scratch_context(), data_v8_value);
      if (!serialized_value.IsOK()) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Failed to serialize data value."));
      }
      serialized_value_map.emplace(key, std::move(serialized_value));
    }
  }

  return serialized_value_map;
}

}  // namespace

TrustedSignalsKVv2RequestHelper::TrustedSignalsKVv2RequestHelper(
    std::string post_request_body,
    quiche::ObliviousHttpRequest::Context context)
    : post_request_body_(std::move(post_request_body)),
      context_(std::move(context)) {}

TrustedSignalsKVv2RequestHelper::TrustedSignalsKVv2RequestHelper(
    TrustedSignalsKVv2RequestHelper&&) = default;

TrustedSignalsKVv2RequestHelper& TrustedSignalsKVv2RequestHelper::operator=(
    TrustedSignalsKVv2RequestHelper&&) = default;

TrustedSignalsKVv2RequestHelper::~TrustedSignalsKVv2RequestHelper() = default;

std::string TrustedSignalsKVv2RequestHelper::TakePostRequestBody() {
  return std::move(post_request_body_);
}

quiche::ObliviousHttpRequest::Context
TrustedSignalsKVv2RequestHelper::TakeOHttpRequestContext() {
  return std::move(context_);
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

std::unique_ptr<TrustedSignalsKVv2RequestHelper>
TrustedBiddingSignalsKVv2RequestHelperBuilder::Build(
    mojom::TrustedSignalsPublicKeyPtr public_key) {
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
  quiche::ObliviousHttpRequest request =
      CreateOHttpRequest(std::move(public_key), std::move(request_map_value));

  std::string encrypted_request = request.EncapsulateAndSerialize();
  return std::make_unique<TrustedSignalsKVv2RequestHelper>(
      std::move(encrypted_request), std::move(request).ReleaseContext());
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

CompressionGroupResult::CompressionGroupResult() = default;

CompressionGroupResult::CompressionGroupResult(
    auction_worklet::mojom::TrustedSignalsCompressionScheme scheme,
    std::vector<uint8_t> content,
    base::TimeDelta ttl)
    : compression_scheme(scheme), content(std::move(content)), ttl(ttl) {}

CompressionGroupResult::CompressionGroupResult(CompressionGroupResult&&) =
    default;

CompressionGroupResult& CompressionGroupResult::operator=(
    CompressionGroupResult&&) = default;

CompressionGroupResult::~CompressionGroupResult() = default;

TrustedSignalsKVv2ResponseParser::SignalsFetchResult
TrustedSignalsKVv2ResponseParser::ParseResponseToSignalsFetchResult(
    const std::string& body_string,
    quiche::ObliviousHttpRequest::Context& context) {
  // Decrypt response body with saved context from request encryption process.
  auto maybe_body =
      quiche::ObliviousHttpResponse::CreateClientObliviousResponse(
          body_string, context, kTrustedSignalsKVv2EncryptionResponseMediaType);

  if (!maybe_body.ok()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Failed to decrypt response body."));
  }

  base::span<const uint8_t> body_span =
      base::as_byte_span(maybe_body->GetPlaintextData());
  auto extract_result =
      ExtractCompressionSchemaAndCborStringFromResponseBody(body_span);

  if (!extract_result.has_value()) {
    return base::unexpected(std::move(extract_result).error());
  }

  auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme =
      extract_result->first;
  std::vector<uint8_t> cbor_bytes = extract_result->second;

  // Parse CBOR bytes.
  TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap result_map;
  std::optional<cbor::Value> body = cbor::Reader::Read(base::span(cbor_bytes));

  if (!body.has_value()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Failed to parse response body as CBOR."));
  }
  const cbor::Value& body_value = std::move(body).value();
  if (!body->is_map()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Response body is not type of Map."));
  }
  const cbor::Value::MapValue& body_map = body_value.GetMap();

  // Get compression groups.
  auto compression_groups_it = body_map.find(cbor::Value("compressionGroups"));
  if (compression_groups_it == body_map.end()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Failed to find compression groups in response."));
  }
  const cbor::Value& compression_groups_value = compression_groups_it->second;
  if (!compression_groups_value.is_array()) {
    return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
        "Compression groups is not type of Array."));
  }
  const cbor::Value::ArrayValue& compression_groups =
      compression_groups_value.GetArray();

  for (const auto& group : compression_groups) {
    int compression_group_id;
    base::expected<CompressionGroupResult,
                   TrustedSignalsKVv2ResponseParser::ErrorInfo>
        compression_group = ParseCompressionGroup(group, compression_scheme,
                                                  compression_group_id);

    if (!compression_group.has_value()) {
      return base::unexpected(std::move(compression_group).error());
    }

    if (!result_map
             .emplace(compression_group_id,
                      std::move(compression_group).value())
             .second) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          base::StringPrintf("Compression group id \"%d\" is already in used.",
                             compression_group_id)));
    }
  }

  return result_map;
}

TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap
TrustedSignalsKVv2ResponseParser::ParseBiddingSignalsFetchResultToResultMap(
    AuctionV8Helper* v8_helper,
    const std::optional<std::set<std::string>>& interest_group_names,
    const std::optional<std::set<std::string>>& keys,
    const TrustedSignalsKVv2ResponseParser::CompressionGroupResultMap&
        compression_group_result_map) {
  TrustedSignalsResultMap result_map;

  for (const auto& group : compression_group_result_map) {
    // Get content from each compression group.
    base::span<const uint8_t> content_bytes;
    // Buffer for holding the data if we need to decompress.
    std::string decompressed_string;

    if (group.second.compression_scheme ==
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone) {
      content_bytes = base::as_byte_span(group.second.content);
    } else if (group.second.compression_scheme ==
               auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip) {
      bool is_decompressed = compression::GzipUncompress(group.second.content,
                                                         &decompressed_string);
      if (!is_decompressed) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Failed to decompress content string with Gzip."));
      }
      content_bytes = base::as_byte_span(decompressed_string);
    }

    std::optional<cbor::Value> maybe_content =
        cbor::Reader::Read(content_bytes);
    if (!maybe_content.has_value()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "Failed to parse content to CBOR."));
    }
    cbor::Value& content_value = maybe_content.value();
    if (!content_value.is_array()) {
      return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
          "Content is not type of Array."));
    }

    for (const auto& partition_value : content_value.GetArray()) {
      scoped_refptr<TrustedSignals::Result> result;
      TrustedSignals::Result::PerInterestGroupDataMap
          per_interest_group_data_map;
      std::map<std::string, AuctionV8Helper::SerializedValue> bidding_data_map;

      if (!partition_value.is_map()) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Partition is not type of Map."));
      }
      const cbor::Value::MapValue& partition = partition_value.GetMap();
      auto id_it = partition.find(cbor::Value("id"));
      auto key_group_outputs_it =
          partition.find(cbor::Value("keyGroupOutputs"));
      if (id_it == partition.end()) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Key \"id\" is missing in partition map."));
      }
      if (key_group_outputs_it == partition.end()) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Key \"keyGroupOutputs\" is missing in partition map."));
      }

      // Build each partition to a `TrustedSignals::Result`.
      const cbor::Value& id_value = id_it->second;
      if (!id_value.is_integer()) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Partition id is not type of Integer."));
      }

      // Partition id must be a valid 32-bit integer.
      if (!base::IsValueInRangeForNumericType<int>(id_value.GetInteger())) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Partition id is out of range for int."));
      }
      int id = static_cast<int>(id_value.GetInteger());

      // Try to find "dataVersion".
      std::optional<uint32_t> data_version;
      auto data_version_it = partition.find(cbor::Value("dataVersion"));
      if (data_version_it != partition.end()) {
        const cbor::Value& data_version_value = data_version_it->second;
        if (!data_version_value.is_integer()) {
          return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
              "DataVersion is not type of Integer."));
        }

        // "dataVersion" field must be a valid 32-bit unsigned integer.
        if (!base::IsValueInRangeForNumericType<uint32_t>(
                data_version_value.GetInteger())) {
          return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
              "DataVersion field is out of range for uint32."));
        }
        data_version = static_cast<uint32_t>(data_version_value.GetInteger());
      }

      // Parse keyGroupOutputs to a map.
      const cbor::Value& key_group_outputs_value = key_group_outputs_it->second;
      if (!key_group_outputs_value.is_array()) {
        return base::unexpected(TrustedSignalsKVv2ResponseParser::ErrorInfo(
            "Partition key group outputs is not type of Array."));
      }
      const cbor::Value::ArrayValue& key_group_outputs =
          key_group_outputs_value.GetArray();

      base::expected<std::map<std::string, const cbor::Value::MapValue*>,
                     TrustedSignalsKVv2ResponseParser::ErrorInfo>
          key_group_outputs_map = ParseKeyGroupOutputsToMap(key_group_outputs);

      if (!key_group_outputs_map.has_value()) {
        return base::unexpected(std::move(key_group_outputs_map).error());
      }

      // Try to find `kTagInterestGroupName` tag and parse the map.
      auto tag_interest_group_name_it =
          key_group_outputs_map->find(kTagInterestGroupName);
      if (tag_interest_group_name_it != key_group_outputs_map->end()) {
        DCHECK(interest_group_names.has_value());
        const cbor::Value::MapValue& key_values =
            *tag_interest_group_name_it->second;

        for (auto& name : interest_group_names.value()) {
          auto name_it = key_values.find(cbor::Value(name));
          if (name_it != key_values.end()) {
            base::expected<std::string_view,
                           TrustedSignalsKVv2ResponseParser::ErrorInfo>
                data_string = GetKeyValueDataString(*name_it);

            if (!data_string.has_value()) {
              return base::unexpected(std::move(data_string).error());
            }

            v8::Local<v8::Value> per_interest_group_data_v8_value;
            if (!v8_helper
                     ->CreateValueFromJson(v8_helper->scratch_context(),
                                           std::move(data_string).value())
                     .ToLocal(&per_interest_group_data_v8_value) ||
                !per_interest_group_data_v8_value->IsObject() ||
                // V8 considers arrays a subtype of object, but the
                // response body must be a JSON object, not a JSON
                // array,
                // so need to explicitly check if it's an array.
                per_interest_group_data_v8_value->IsArray()) {
              return base::unexpected(
                  TrustedSignalsKVv2ResponseParser::ErrorInfo(
                      "Failed to create V8 value from key group output "
                      "data."));
            }

            v8::Local<v8::Object> per_interest_group_data_v8_object =
                per_interest_group_data_v8_value.As<v8::Object>();
            std::optional<TrustedSignals::Result::PriorityVector>
                priority_vector = TrustedSignals::ParsePriorityVector(
                    v8_helper, per_interest_group_data_v8_object);

            std::optional<base::TimeDelta> update_if_older_than;
            if (base::FeatureList::IsEnabled(
                    features::kInterestGroupUpdateIfOlderThan)) {
              update_if_older_than = TrustedSignals::ParseUpdateIfOlderThan(
                  v8_helper, per_interest_group_data_v8_object);
            }

            if (priority_vector || update_if_older_than) {
              per_interest_group_data_map.emplace(
                  name, TrustedSignals::Result::PerGroupData(
                            std::move(priority_vector),
                            std::move(update_if_older_than)));
            }
          }
        }
      }

      // Try to find `kTagKey` tag and parse the map.
      auto tag_key_it = key_group_outputs_map->find(kTagKey);
      if (tag_key_it != key_group_outputs_map->end()) {
        DCHECK(keys.has_value());
        base::expected<std::map<std::string, AuctionV8Helper::SerializedValue>,
                       TrustedSignalsKVv2ResponseParser::ErrorInfo>
            key_group = SerializeKeyGroupOutputsMap(
                v8_helper, *tag_key_it->second, keys.value());

        if (!key_group.has_value()) {
          return base::unexpected(std::move(key_group).error());
        }

        bidding_data_map = std::move(key_group).value();
      }

      result = base::MakeRefCounted<TrustedSignals::Result>(
          std::move(per_interest_group_data_map), std::move(bidding_data_map),
          data_version);
      result_map->try_emplace(
          TrustedSignalsKVv2RequestHelperBuilder::IsolationIndex(group.first,
                                                                 id),
          result);
    }
  }
  return result_map;
}

}  // namespace auction_worklet

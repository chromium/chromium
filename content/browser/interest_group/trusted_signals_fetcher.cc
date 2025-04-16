// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_signals_fetcher.h"

#include <stdint.h>

#include <algorithm>
#include <bit>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "bidding_and_auction_server_key_fetcher.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/interest_group/auction_downloader_delegate.h"
#include "content/browser/interest_group/data_decoder_manager.h"
#include "content/browser/interest_group/devtools_enums.h"
#include "content/browser/renderer_host/private_network_access_util.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/buffers/oblivious_http_response.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Supported compression formats.
constexpr std::array<std::string_view, 2> kAcceptCompression = {"none", "gzip"};

// Lengths of various components of request and response header components.
constexpr size_t kCompressionFormatSize = 1;  // bytes
constexpr size_t kCborStringLengthSize = 4;   // bytes
constexpr size_t kOhttpHeaderSize = 55;       // bytes

struct IsolationIndex {
  int compression_group_id;
  int partition_id;
};

// Creates a single entry for the "arguments" array of a partition, with a
// single tag and an array of values.
cbor::Value MakeArgument(std::string_view tag, cbor::Value::ArrayValue data) {
  cbor::Value::MapValue argument;

  cbor::Value::ArrayValue tags;
  tags.emplace_back(tag);
  argument.try_emplace(cbor::Value("tags"), cbor::Value(std::move(tags)));
  argument.try_emplace(cbor::Value("data"), std::move(data));

  return cbor::Value(std::move(argument));
}

// Creates a single entry for the "arguments" array of a partition, with a
// single tag and an array that contains the single passed-in `data` value.
cbor::Value MakeArgument(std::string_view tag, cbor::Value data) {
  cbor::Value::ArrayValue cbor_array;
  cbor_array.emplace_back(std::move(data));
  return MakeArgument(tag, std::move(cbor_array));
}

// Creates a single entry for the "arguments" array of a partition, with a
// single tag and a variable number of string data values, from a set of
// strings.
cbor::Value MakeArgument(std::string_view tag,
                         const std::set<std::string>& data) {
  cbor::Value::ArrayValue cbor_data;
  for (const auto& element : data) {
    cbor_data.emplace_back(element);
  }
  return MakeArgument(tag, std::move(cbor_data));
}

// BiddingPartition overload of BuildMapForPartition().
cbor::Value::MapValue BuildMapForPartition(
    int compression_group_id,
    const TrustedSignalsFetcher::BiddingPartition& bidding_partition) {
  cbor::Value::MapValue partition_cbor_map;

  partition_cbor_map.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(compression_group_id));
  partition_cbor_map.try_emplace(cbor::Value("id"),
                                 cbor::Value(bidding_partition.partition_id));

  if (!bidding_partition.additional_params->empty()) {
    cbor::Value::MapValue metadata;
    for (const auto param : *bidding_partition.additional_params) {
      // TODO(crbug.com/333445540): Consider switching to taking
      // `additional_params` as a cbor::Value, for greater flexibility. The
      // `slotSizes` parameter, in particular, might be best represented as an
      // array. cbor::Value doesn't have operator<, having a Less comparator
      // instead, so would need to add that.
      //
      // Alternatively, could split this up into the data used to construct it.
      CHECK(param.second.is_string());
      metadata.try_emplace(cbor::Value(param.first),
                           cbor::Value(param.second.GetString()));
    }
    partition_cbor_map.try_emplace(cbor::Value("metadata"),
                                   cbor::Value(std::move(metadata)));
  }

  cbor::Value::ArrayValue arguments;
  arguments.emplace_back(MakeArgument("interestGroupNames",
                                      *bidding_partition.interest_group_names));
  arguments.emplace_back(MakeArgument("keys", *bidding_partition.keys));
  partition_cbor_map.try_emplace(cbor::Value("arguments"),
                                 cbor::Value(std::move(arguments)));

  return partition_cbor_map;
}

// ScoringPartition overload of BuildMapForPartition().
cbor::Value::MapValue BuildMapForPartition(
    int compression_group_id,
    const TrustedSignalsFetcher::ScoringPartition& scoring_partition) {
  cbor::Value::MapValue partition_cbor_map;

  partition_cbor_map.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(compression_group_id));
  partition_cbor_map.try_emplace(cbor::Value("id"),
                                 cbor::Value(scoring_partition.partition_id));

  if (!scoring_partition.additional_params->empty()) {
    cbor::Value::MapValue metadata;
    for (const auto param : *scoring_partition.additional_params) {
      // TODO(crbug.com/333445540): Consider switching to taking
      // `additional_params` as a cbor::Value, for greater flexibility.
      //
      // Alternatively, could split this up into the data used to construct it.
      CHECK(param.second.is_string());
      metadata.try_emplace(cbor::Value(param.first),
                           cbor::Value(param.second.GetString()));
    }
    partition_cbor_map.try_emplace(cbor::Value("metadata"),
                                   cbor::Value(std::move(metadata)));
  }

  cbor::Value::ArrayValue arguments;
  arguments.emplace_back(MakeArgument(
      "renderURLs", cbor::Value(scoring_partition.render_url->spec())));

  if (!scoring_partition.component_render_urls->empty()) {
    cbor::Value::ArrayValue component_urls;
    for (const GURL& component_render_urls :
         *scoring_partition.component_render_urls) {
      component_urls.emplace_back(component_render_urls.spec());
    }
    arguments.emplace_back(
        MakeArgument("adComponentRenderURLs", std::move(component_urls)));
  }

  partition_cbor_map.try_emplace(cbor::Value("arguments"),
                                 cbor::Value(std::move(arguments)));

  return partition_cbor_map;
}

// BiddingPartition overload of CollectContextualData().
void CollectContextualData(
    int compression_group_id,
    const TrustedSignalsFetcher::BiddingPartition& bidding_partition,
    std::map<std::string, std::vector<IsolationIndex>>&
        contextual_data_ids_map) {
  if (bidding_partition.buyer_tkv_signals != nullptr) {
    contextual_data_ids_map[*bidding_partition.buyer_tkv_signals].emplace_back(
        compression_group_id, bidding_partition.partition_id);
  }
}

// ScoringPartition overload of CollectContextualData().
void CollectContextualData(
    int compression_group_id,
    const TrustedSignalsFetcher::ScoringPartition& scoring_partition,
    std::map<std::string, std::vector<IsolationIndex>>&
        contextual_data_ids_map) {
  if (scoring_partition.seller_tkv_signals != nullptr) {
    contextual_data_ids_map[*scoring_partition.seller_tkv_signals].emplace_back(
        compression_group_id, scoring_partition.partition_id);
  }
}

void AddPerPartitionMetadata(
    cbor::Value::MapValue& request_map_value,
    size_t partition_count,
    const std::map<std::string, std::vector<IsolationIndex>>&
        contextual_data_ids_map) {
  if (contextual_data_ids_map.empty()) {
    return;
  }

  cbor::Value::MapValue partitioned_metadata_map;
  cbor::Value::ArrayValue contextual_data_array;

  for (const auto& signal_index_pair : contextual_data_ids_map) {
    cbor::Value::MapValue contextual_data_map;

    // Add signal string to `contextualData`.
    contextual_data_map.try_emplace(cbor::Value("value"),
                                    cbor::Value(signal_index_pair.first));

    // The `ids` list is omitted if all partitions share the same contextual
    // signal value. In other words, if a signal corresponds to a number of
    // partitions less than the total, its entry in `contextualData` must
    // include the `ids` list.
    if (signal_index_pair.second.size() != partition_count) {
      cbor::Value::ArrayValue ids_array;

      for (const auto& index : signal_index_pair.second) {
        cbor::Value::ArrayValue id_pair;
        // Emplace compression group id and partition id in order.
        id_pair.emplace_back(index.compression_group_id);
        id_pair.emplace_back(index.partition_id);
        ids_array.emplace_back(std::move(id_pair));
      }

      contextual_data_map.try_emplace(cbor::Value("ids"),
                                      cbor::Value(std::move(ids_array)));
    }

    contextual_data_array.emplace_back(std::move(contextual_data_map));
  }

  partitioned_metadata_map.try_emplace(
      cbor::Value("contextualData"),
      cbor::Value(std::move(contextual_data_array)));
  request_map_value.try_emplace(
      cbor::Value("perPartitionMetadata"),
      cbor::Value(std::move(partitioned_metadata_map)));
}

std::string CreateRequestBodyFromCbor(cbor::Value cbor_value) {
  std::optional<std::vector<uint8_t>> maybe_cbor_bytes =
      cbor::Writer::Write(cbor_value);
  CHECK(maybe_cbor_bytes.has_value());

  std::string request_body;
  size_t size_before_padding = kOhttpHeaderSize + kCompressionFormatSize +
                               kCborStringLengthSize + maybe_cbor_bytes->size();
  size_t size_with_padding = std::bit_ceil(size_before_padding);
  size_t request_body_size = size_with_padding - kOhttpHeaderSize;
  request_body.resize(request_body_size, 0x00);

  base::SpanWriter writer(base::as_writable_byte_span(request_body));

  // Add framing header. First byte includes version and compression format.
  // Always set first byte to 0x00 because request body is uncompressed.
  writer.WriteU8BigEndian(0x00);
  writer.WriteU32BigEndian(
      base::checked_cast<uint32_t>(maybe_cbor_bytes->size()));

  // Add CBOR string.
  writer.Write(base::as_byte_span(*maybe_cbor_bytes));

  DCHECK_EQ(writer.num_written(), size_before_padding - kOhttpHeaderSize);

  // TODO(crbug.com/333445540): Add encryption.

  return request_body;
}

// Builds the request body for bidding and scoring requests. The outer body is
// the same, only the data in the partitions is different, so a template works
// well for this. PartitionType is either BiddingPartition or ScoringPartition.
template <typename PartitionType>
std::string BuildSignalsRequestBody(
    std::string_view hostname,
    const std::map<int, std::vector<PartitionType>>& compression_groups) {
  cbor::Value::MapValue request_map_value;
  cbor::Value::ArrayValue accept_compression(kAcceptCompression.begin(),
                                             kAcceptCompression.end());
  request_map_value.emplace(cbor::Value("acceptCompression"),
                            cbor::Value(std::move(accept_compression)));

  cbor::Value::MapValue metadata;
  metadata.try_emplace(cbor::Value("hostname"), cbor::Value(hostname));
  request_map_value.try_emplace(cbor::Value("metadata"),
                                cbor::Value(std::move(metadata)));

  cbor::Value::ArrayValue partition_array;
  size_t partition_count = 0;
  // A map of `contextual_data` to indices for each partition, where
  // the keys are signal strings and the values are lists of compression group
  // id and partition id pair.
  std::map<std::string, std::vector<IsolationIndex>> contextual_data_ids_map;

  for (const auto& group_pair : compression_groups) {
    int compression_group_id = group_pair.first;
    for (const auto& partition : group_pair.second) {
      cbor::Value::MapValue partition_cbor_map =
          BuildMapForPartition(compression_group_id, partition);
      partition_array.emplace_back(partition_cbor_map);
      ++partition_count;

      CollectContextualData(compression_group_id, partition,
                            contextual_data_ids_map);
    }
  }

  AddPerPartitionMetadata(request_map_value, partition_count,
                          contextual_data_ids_map);
  request_map_value.emplace(cbor::Value("partitions"),
                            cbor::Value(std::move(partition_array)));

  return CreateRequestBodyFromCbor(cbor::Value(std::move(request_map_value)));
}

}  // namespace

TrustedSignalsFetcher::BiddingPartition::BiddingPartition(
    int partition_id,
    const std::set<std::string>* interest_group_names,
    const std::set<std::string>* keys,
    const base::Value::Dict* additional_params,
    const std::string* buyer_tkv_signals)
    : partition_id(partition_id),
      interest_group_names(*interest_group_names),
      keys(*keys),
      additional_params(*additional_params),
      buyer_tkv_signals(buyer_tkv_signals) {}

TrustedSignalsFetcher::BiddingPartition::BiddingPartition(BiddingPartition&&) =
    default;

TrustedSignalsFetcher::BiddingPartition::~BiddingPartition() = default;

TrustedSignalsFetcher::BiddingPartition&
TrustedSignalsFetcher::BiddingPartition::operator=(BiddingPartition&&) =
    default;

TrustedSignalsFetcher::ScoringPartition::ScoringPartition(
    int partition_id,
    const GURL* render_url,
    const std::set<GURL>* component_render_urls,
    const base::Value::Dict* additional_params,
    const std::string* seller_tkv_signals)
    : partition_id(partition_id),
      render_url(*render_url),
      component_render_urls(*component_render_urls),
      additional_params(*additional_params),
      seller_tkv_signals(seller_tkv_signals) {}

TrustedSignalsFetcher::ScoringPartition::ScoringPartition(ScoringPartition&&) =
    default;

TrustedSignalsFetcher::ScoringPartition::~ScoringPartition() = default;

TrustedSignalsFetcher::ScoringPartition&
TrustedSignalsFetcher::ScoringPartition::operator=(ScoringPartition&&) =
    default;

TrustedSignalsFetcher::CompressionGroupResult::CompressionGroupResult() =
    default;
TrustedSignalsFetcher::CompressionGroupResult::CompressionGroupResult(
    CompressionGroupResult&&) = default;

TrustedSignalsFetcher::CompressionGroupResult::~CompressionGroupResult() =
    default;

TrustedSignalsFetcher::CompressionGroupResult&
TrustedSignalsFetcher::CompressionGroupResult::operator=(
    CompressionGroupResult&&) = default;

TrustedSignalsFetcher::TrustedSignalsFetcher() = default;

TrustedSignalsFetcher::~TrustedSignalsFetcher() = default;

void TrustedSignalsFetcher::FetchBiddingSignals(
    DataDecoderManager& data_decoder_manager,
    network::mojom::URLLoaderFactory* url_loader_factory,
    FrameTreeNodeId frame_tree_node_id,
    base::flat_set<std::string> devtools_auction_ids,
    const url::Origin& main_frame_origin,
    network::mojom::IPAddressSpace ip_address_space,
    base::UnguessableToken network_partition_nonce,
    const url::Origin& script_origin,
    const GURL& trusted_bidding_signals_url,
    const BiddingAndAuctionServerKey& bidding_and_auction_key,
    const std::map<int, std::vector<BiddingPartition>>& compression_groups,
    Callback callback) {
  EncryptRequestBodyAndStart(
      data_decoder_manager, url_loader_factory,
      InterestGroupAuctionFetchType::kBidderTrustedSignals, frame_tree_node_id,
      std::move(devtools_auction_ids), main_frame_origin, ip_address_space,
      network_partition_nonce, script_origin, trusted_bidding_signals_url,
      bidding_and_auction_key,
      BuildSignalsRequestBody(main_frame_origin.host(), compression_groups),
      std::move(callback));
}

void TrustedSignalsFetcher::FetchScoringSignals(
    DataDecoderManager& data_decoder_manager,
    network::mojom::URLLoaderFactory* url_loader_factory,
    FrameTreeNodeId frame_tree_node_id,
    base::flat_set<std::string> devtools_auction_ids,
    const url::Origin& main_frame_origin,
    network::mojom::IPAddressSpace ip_address_space,
    base::UnguessableToken network_partition_nonce,
    const url::Origin& script_origin,
    const GURL& trusted_scoring_signals_url,
    const BiddingAndAuctionServerKey& bidding_and_auction_key,
    const std::map<int, std::vector<ScoringPartition>>& compression_groups,
    Callback callback) {
  EncryptRequestBodyAndStart(
      data_decoder_manager, url_loader_factory,
      InterestGroupAuctionFetchType::kSellerTrustedSignals, frame_tree_node_id,
      std::move(devtools_auction_ids), main_frame_origin, ip_address_space,
      network_partition_nonce, script_origin, trusted_scoring_signals_url,
      bidding_and_auction_key,
      BuildSignalsRequestBody(main_frame_origin.host(), compression_groups),
      std::move(callback));
}

void TrustedSignalsFetcher::EncryptRequestBodyAndStart(
    DataDecoderManager& data_decoder_manager,
    network::mojom::URLLoaderFactory* url_loader_factory,
    InterestGroupAuctionFetchType fetch_type,
    FrameTreeNodeId frame_tree_node_id,
    base::flat_set<std::string> devtools_auction_ids,
    const url::Origin& main_frame_origin,
    network::mojom::IPAddressSpace ip_address_space,
    base::UnguessableToken network_partition_nonce,
    const url::Origin& script_origin,
    const GURL& trusted_signals_url,
    const BiddingAndAuctionServerKey& bidding_and_auction_key,
    std::string plaintext_request_body,
    Callback callback) {
  DCHECK(!auction_downloader_);
  DCHECK(!callback_);
  trusted_signals_url_ = trusted_signals_url;

  // Request a DataDecoder now to pre-warm it for when data is received.
  decoder_handle_ =
      data_decoder_manager.GetHandle(main_frame_origin, script_origin);
  // Need to call GetService() to actually trigger creation of the underlying
  // service.
  decoder_handle_->data_decoder().GetService();

  callback_ = std::move(callback);

  uint32_t key_id = 0;
  bool success = base::HexStringToUInt(
      std::string_view(bidding_and_auction_key.id).substr(0, 2), &key_id);
  DCHECK(success);

  // Add encryption for request body.
  auto maybe_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  CHECK(maybe_key_config.ok());

  auto maybe_ciphertext_request_body =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          std::move(plaintext_request_body), bidding_and_auction_key.key,
          *maybe_key_config, kRequestMediaType);
  CHECK(maybe_ciphertext_request_body.ok());

  network::ResourceRequest::TrustedParams trusted_params;

  // IsolationInfos usually use main frame origin and frame origin, to separate
  // the disk cache, and prevent frames from spying on each other's cache
  // entries. These requests aren't cached (due to being POSTs), and use their
  // own nonce, so no frames can pull the responses from the cache, even the
  // main frame.
  //
  // The frame origin is also used to populate a cross-origin bit in the
  // NetworkIsolationKey, to separate out other network resources for connection
  // spying. The nonce similarly makes that sort of spying not useful
  // - the only leak is the run time of entire auction, which leaks minimal
  // information about whether there's a pre-existing connection. There's no way
  // for frames to probe in depth connection info more directly, since they
  // can't make network requests directly using the `network_partition_nonce`.
  trusted_params.isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/main_frame_origin,
      /*frame_origin=*/main_frame_origin, net::SiteForCookies(),
      network_partition_nonce);

  auto client_security_state = network::mojom::ClientSecurityState::New();
  client_security_state->ip_address_space = ip_address_space;
  client_security_state->is_web_secure_context = true;
  client_security_state->private_network_request_policy =
      network::mojom::PrivateNetworkRequestPolicy::kBlock;
  trusted_params.client_security_state = std::move(client_security_state);

  auction_downloader_ = std::make_unique<auction_worklet::AuctionDownloader>(
      url_loader_factory, trusted_signals_url,
      auction_worklet::AuctionDownloader::DownloadMode::kActualDownload,
      auction_worklet::AuctionDownloader::MimeType::kAdAuctionTrustedSignals,
      maybe_ciphertext_request_body->EncapsulateAndSerialize(),
      std::string(kRequestMediaType),
      /*request_initiator=*/script_origin, std::move(trusted_params),
      base::BindOnce(&TrustedSignalsFetcher::OnRequestComplete,
                     base::Unretained(this)),
      AuctionDownloaderDelegate::MaybeCreate(frame_tree_node_id));
  ohttp_context_ = std::make_unique<quiche::ObliviousHttpRequest::Context>(
      std::move(maybe_ciphertext_request_body).value().ReleaseContext());
  if (frame_tree_node_id &&
      devtools_instrumentation::NeedInterestGroupAuctionEvents(
          frame_tree_node_id)) {
    devtools_instrumentation::OnInterestGroupAuctionNetworkRequestCreated(
        frame_tree_node_id, fetch_type, auction_downloader_->request_id(),
        std::move(devtools_auction_ids).extract());
  }
}

void TrustedSignalsFetcher::OnRequestComplete(
    std::unique_ptr<std::string> response_body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::optional<std::string> error) {
  // `auction_downloader_` is no longer needed.
  auction_downloader_.reset();

  if (!response_body) {
    std::move(callback_).Run(base::unexpected(std::move(error).value()));
    return;
  }

  // The oblivious HTTP code returns an error on empty response bodies, so only
  // try and decrypt if the body is not empty, to give an error about size
  // rather than OHTTP failing in that case.
  std::string plaintext_response_body;
  if (response_body->size() > 0u) {
    auto maybe_plaintext_response_body =
        quiche::ObliviousHttpResponse::CreateClientObliviousResponse(
            std::move(*response_body), *ohttp_context_, kResponseMediaType);
    // `ohttp_context_` is no longer needed.
    ohttp_context_.reset();

    if (!maybe_plaintext_response_body.ok()) {
      // Don't output OHTTP error strings, directly, as they're often not very
      // user-friendly.
      std::move(callback_).Run(
          base::unexpected(CreateError("OHTTP decryption failed")));
      return;
    }
    plaintext_response_body =
        std::move(maybe_plaintext_response_body).value().ConsumePlaintextData();
  }

  base::SpanReader reader(base::as_byte_span(plaintext_response_body));
  uint8_t compression_scheme_bytes;
  uint32_t cbor_length;
  if (!reader.ReadU8BigEndian(compression_scheme_bytes) ||
      !reader.ReadU32BigEndian(cbor_length)) {
    std::move(callback_).Run(base::unexpected(CreateError(
        base::StringPrintf("Response body is shorter than a %s header",
                           kResponseMediaType.data()))));
    return;
  }

  // Only the first to bits are used for compression format in the whole byte.
  compression_scheme_bytes &= 0x03;
  if (compression_scheme_bytes == 0x00) {
    compression_scheme_ =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kNone;
  } else if (compression_scheme_bytes == 0x02) {
    compression_scheme_ =
        auction_worklet::mojom::TrustedSignalsCompressionScheme::kGzip;
  } else {
    std::move(callback_).Run(base::unexpected(CreateError(base::StringPrintf(
        "Unsupported compression scheme: %u", compression_scheme_bytes))));
    return;
  }

  base::span<const uint8_t> remaining_span = reader.remaining_span();
  if (remaining_span.size() < cbor_length) {
    std::move(callback_).Run(
        base::unexpected(CreateError("Length header exceeds body size")));
    return;
  }

  base::span<const uint8_t> cbor = remaining_span.first(cbor_length);
  decoder_handle_->data_decoder().ParseCbor(
      cbor, base::BindOnce(&TrustedSignalsFetcher::OnCborParsed,
                           weak_ptr_factory_.GetWeakPtr()));
}

void TrustedSignalsFetcher::OnCborParsed(
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  std::move(callback_).Run(ParseDataDecoderResult(std::move(value_or_error)));
}

TrustedSignalsFetcher::SignalsFetchResult
TrustedSignalsFetcher::ParseDataDecoderResult(
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  if (!value_or_error.has_value()) {
    return base::unexpected(CreateError("Failed to parse response as CBOR"));
  }

  if (!value_or_error->is_dict()) {
    // Since the data is CBOR, use CBOR type names in error messages ("map"
    // instead of JSON "object" or Value "dict").
    return base::unexpected(CreateError("Response body is not a map"));
  }

  base::Value::Dict& dict = value_or_error->GetDict();

  // Get compression groups.
  base::Value::List* compression_groups = dict.FindList("compressionGroups");
  if (!compression_groups) {
    return base::unexpected(
        CreateError("Response is missing compressionGroups array"));
  }

  CompressionGroupResultMap compression_groups_out;
  for (auto& compression_group : *compression_groups) {
    int compression_group_id;
    // This consumes each value of the list, to avoid having to copy the
    // contents of each compression group.
    auto compression_group_result = ParseCompressionGroup(
        std::move(compression_group), compression_group_id);

    if (!compression_group_result.has_value()) {
      return base::unexpected(std::move(compression_group_result).error());
    }

    if (!compression_groups_out
             .try_emplace(compression_group_id,
                          std::move(compression_group_result).value())
             .second) {
      return base::unexpected(CreateError(base::StringPrintf(
          "Response contains two compression groups with id %i",
          compression_group_id)));
    }
  }

  return compression_groups_out;
}

base::expected<TrustedSignalsFetcher::CompressionGroupResult, std::string>
TrustedSignalsFetcher::ParseCompressionGroup(
    base::Value compression_group_value,
    int& compression_group_id) {
  if (!compression_group_value.is_dict()) {
    return base::unexpected(CreateError(
        base::StringPrintf("Compression group is not of type map")));
  }

  base::Value::Dict& compression_group_dict = compression_group_value.GetDict();
  std::optional<int> compression_group_id_opt =
      compression_group_dict.FindInt("compressionGroupId");
  if (!compression_group_id_opt.has_value() || *compression_group_id_opt < 0) {
    return base::unexpected(CreateError(
        base::StringPrintf("Compression group must have a non-negative integer "
                           "compressionGroupId")));
  }

  const base::Value* ttl_ms_value = compression_group_dict.Find("ttlMs");
  // Default TTL is 0.
  base::TimeDelta ttl;
  if (ttl_ms_value) {
    if (!ttl_ms_value->is_int()) {
      return base::unexpected(CreateError(base::StringPrintf(
          "Compression group %i ttlMs value is not an integer",
          *compression_group_id_opt)));
    }
    // Treat negative values as 0. Using zero is more robust if these values are
    // ever used to set a timer.
    ttl = base::Milliseconds(std::max(0, ttl_ms_value->GetInt()));
  }

  auto* content = compression_group_dict.FindBlob("content");
  if (!content) {
    return base::unexpected(CreateError(base::StringPrintf(
        "Compression group %i missing binary string \"content\"",
        *compression_group_id_opt)));
  }

  compression_group_id = *compression_group_id_opt;

  CompressionGroupResult result;
  result.compression_scheme = compression_scheme_;
  result.compression_group_data = std::move(*content);
  result.ttl = ttl;
  return result;
}

std::string TrustedSignalsFetcher::CreateError(
    const std::string& error_message) {
  return base::StringPrintf("Failed to load %s: %s.",
                            trusted_signals_url_.spec().c_str(),
                            error_message.c_str());
}

}  // namespace content

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_signals_fetcher.h"

#include <stdint.h>

#include <bit>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("trusted_signals_fetcher", R"(
        semantics {
          sender: "TrustedSignalsFetcher"
          description:
            "Requests FLEDGE encrypted trusted signals for running an ad "
            "auction."
          trigger:
            "Requested when a website runs a Protected Audiences auction. "
            "The Protected Audience API allows sites to select content (such "
            "as personalized ads) to display based on cross-site data in a "
            "privacy preserving way."
          data:
            "HTTPS URL and POST body associated with an interest group or "
            "seller. POST data has an additional layer of encryption, and "
            "all data other than the URL is end-to-end encrypted and only "
            "accessible in a Trusted Execution Environment."
          destination: WEBSITE
          user_data: {
            type: SENSITIVE_URL
          }
          internal {
            contacts {
              email: "privacy-sandbox-dev@chromium.org"
            }
          }
          last_reviewed: "2024-06-08"
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

// Supported compression formats.
constexpr std::array<std::string_view, 2> kAcceptCompression = {"none", "gzip"};

// Lengths of various components of request and response header components.
constexpr size_t kCompressionFormatSize = 1;  // bytes
constexpr size_t kCborStringLengthSize = 4;   // bytes
constexpr size_t kOhttpHeaderSize = 55;       // bytes

// Creates a single entry for the "arguments" array of a partition, with a
// single tag and a variable number of string data values, from a set of
// strings.
cbor::Value MakeArgument(std::string_view tag,
                         const std::set<std::string>& data) {
  cbor::Value::MapValue argument;

  cbor::Value::ArrayValue tags;
  tags.emplace_back(cbor::Value(tag));
  argument.try_emplace(cbor::Value("tags"), cbor::Value(std::move(tags)));

  cbor::Value::ArrayValue cbor_data;
  for (const auto& element : data) {
    cbor_data.emplace_back(cbor::Value(element));
  }
  argument.try_emplace(cbor::Value("data"), cbor::Value(std::move(cbor_data)));

  return cbor::Value(std::move(argument));
}

cbor::Value::MapValue BuildMapForBiddingPartition(
    int compression_group_id,
    const TrustedSignalsFetcher::BiddingPartition& bidding_partition) {
  cbor::Value::MapValue partition_cbor_map;

  partition_cbor_map.try_emplace(cbor::Value("compressionGroupId"),
                                 cbor::Value(compression_group_id));
  partition_cbor_map.try_emplace(cbor::Value("id"),
                                 cbor::Value(bidding_partition.partition_id));

  cbor::Value::MapValue metadata;
  // Hostname isn't in `additional_params` since it's used by the caller to
  // partition fetches.
  metadata.try_emplace(cbor::Value("hostname"),
                       cbor::Value(bidding_partition.hostname));
  for (const auto param : bidding_partition.additional_params) {
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

  cbor::Value::ArrayValue arguments;
  arguments.emplace_back(MakeArgument("interestGroupNames",
                                      bidding_partition.interest_group_names));
  arguments.emplace_back(MakeArgument("keys", bidding_partition.keys));
  partition_cbor_map.try_emplace(cbor::Value("arguments"),
                                 cbor::Value(std::move(arguments)));

  return partition_cbor_map;
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

  base::SpanWriter writer(
      base::as_writable_bytes(base::make_span(request_body)));

  // Add framing header. First byte includes version and compression format.
  // Always set first byte to 0x00 because request body is uncompressed.
  writer.WriteU8BigEndian(0x00);
  writer.WriteU32BigEndian(
      base::checked_cast<uint32_t>(maybe_cbor_bytes->size()));

  // Add CBOR string.
  writer.Write(base::as_bytes(base::make_span(*maybe_cbor_bytes)));

  DCHECK_EQ(writer.num_written(), size_before_padding - kOhttpHeaderSize);

  // TODO(crbug.com/333445540): Add encryption.

  return request_body;
}

std::string BuildBiddingSignalsRequestBody(
    const std::map<int, std::vector<TrustedSignalsFetcher::BiddingPartition>>&
        compression_groups) {
  cbor::Value::MapValue request_map_value;
  cbor::Value::ArrayValue accept_compression(kAcceptCompression.begin(),
                                             kAcceptCompression.end());
  request_map_value.emplace(cbor::Value("acceptCompression"),
                            cbor::Value(std::move(accept_compression)));

  cbor::Value::ArrayValue partition_array;
  for (const auto& group_pair : compression_groups) {
    int compression_group_id = group_pair.first;
    for (const auto& bidding_partition : group_pair.second) {
      cbor::Value::MapValue partition_cbor_map =
          BuildMapForBiddingPartition(compression_group_id, bidding_partition);
      partition_array.emplace_back(partition_cbor_map);
    }
  }
  request_map_value.emplace(cbor::Value("partitions"),
                            cbor::Value(std::move(partition_array)));

  return CreateRequestBodyFromCbor(cbor::Value(std::move(request_map_value)));
}

}  // namespace

TrustedSignalsFetcher::BiddingPartition::BiddingPartition() = default;

TrustedSignalsFetcher::BiddingPartition::BiddingPartition(BiddingPartition&&) =
    default;

TrustedSignalsFetcher::BiddingPartition::~BiddingPartition() = default;

TrustedSignalsFetcher::BiddingPartition&
TrustedSignalsFetcher::BiddingPartition::operator=(BiddingPartition&&) =
    default;

TrustedSignalsFetcher::ScoringPartition::ScoringPartition() = default;

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
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& trusted_bidding_signals_url,
    const std::map<int, std::vector<BiddingPartition>>& compression_groups,
    Callback callback) {
  StartRequest(url_loader_factory, trusted_bidding_signals_url,
               BuildBiddingSignalsRequestBody(compression_groups),
               std::move(callback));
}

void TrustedSignalsFetcher::FetchScoringSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& trusted_scoring_signals_url,
    const std::map<int, std::vector<ScoringPartition>>& compression_groups,
    Callback callback) {
  NOTIMPLEMENTED();
}

void TrustedSignalsFetcher::StartRequest(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& trusted_signals_url,
    std::string request_body,
    Callback callback) {
  DCHECK(!simple_url_loader_);
  DCHECK(!callback_);
  trusted_signals_url_ = trusted_signals_url;
  callback_ = std::move(callback);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->url = trusted_signals_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->mode = network::mojom::RequestMode::kNoCors;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->headers.SetHeader("Accept", kResponseMediaType);

  // TODO(crbug.com/333445540): Set reasonable initiator, isolation info, client
  // security state, and credentials mode, and select reasonable maximum body
  // size. Also hook up to devtools.

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  simple_url_loader_->AttachStringForUpload(std::move(request_body),
                                            kRequestMediaType);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&TrustedSignalsFetcher::OnRequestComplete,
                     base::Unretained(this)));
}

void TrustedSignalsFetcher::OnRequestComplete(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    std::move(callback_).Run(base::unexpected(ErrorInfo(base::StringPrintf(
        "Failed to load %s error = %s.", trusted_signals_url_.spec().c_str(),
        net::ErrorToString(simple_url_loader_->NetError()).c_str()))));
    return;
  }

  if (simple_url_loader_->ResponseInfo()->mime_type != kResponseMediaType) {
    std::move(callback_).Run(base::unexpected(ErrorInfo(
        base::StringPrintf("Rejecting load of %s due to unexpected MIME type.",
                           trusted_signals_url_.spec().c_str()))));
    return;
  }

  // TODO(crbug.com/333445540): Parse the response.
  std::move(callback_).Run(
      base::unexpected(ErrorInfo("Response parsing not yet implemented")));
}

}  // namespace content

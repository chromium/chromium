// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_network_fetcher_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/public_key_parsing_utils.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace content {

AggregationServiceNetworkFetcherImpl::AggregationServiceNetworkFetcherImpl(
    const base::Clock* clock,
    StoragePartition* storage_partition)
    : clock_(*clock), storage_partition_(storage_partition) {
  CHECK(clock);
  CHECK(storage_partition_);
}

AggregationServiceNetworkFetcherImpl::AggregationServiceNetworkFetcherImpl(
    const base::Clock* clock,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool enable_debug_logging)
    : clock_(*clock),
      url_loader_factory_(std::move(url_loader_factory)),
      enable_debug_logging_(enable_debug_logging) {
  CHECK(clock);
  CHECK(url_loader_factory_);
}

AggregationServiceNetworkFetcherImpl::~AggregationServiceNetworkFetcherImpl() =
    default;

// static
std::unique_ptr<AggregationServiceNetworkFetcherImpl>
AggregationServiceNetworkFetcherImpl::CreateForTesting(
    const base::Clock* clock,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool enable_debug_logging) {
  return base::WrapUnique(new AggregationServiceNetworkFetcherImpl(
      clock, std::move(url_loader_factory), enable_debug_logging));
}

void AggregationServiceNetworkFetcherImpl::FetchPublicKeys(
    const GURL& url,
    NetworkFetchCallback callback) {
  CHECK(storage_partition_ || url_loader_factory_);

  // The browser process URLLoaderFactory is not created by default, so don't
  // create it until it is directly needed.
  if (!url_loader_factory_) {
    url_loader_factory_ =
        storage_partition_->GetURLLoaderFactoryForBrowserProcess();
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // No cache read, always download from the network.
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("aggregation_service_helper_keys", R"(
        semantics {
          sender: "Aggregation Service"
          description:
            "Downloads public keys for helper servers requested by APIs that "
            "rely on private, secure aggregation (i.e. the Attribution "
            "Reporting and Private Aggregation APIs, see "
            "https://github.com/WICG/attribution-reporting-api and "
            "https://github.com/patcg-individual-drafts/private-aggregation-api"
            "). Keys are requested prior to aggregate reports being sent and "
            "are used to encrypt payloads for the helper servers."
          trigger:
            "When an aggregatable report is about to be assembled and sent."
          data:
            "JSON data comprising public key information."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be controlled via the 'Ad measurement' setting "
            "in the 'Ad privacy' section of 'Privacy and Security'."
          chrome_policy {
            PrivacySandboxAdMeasurementEnabled {
              PrivacySandboxAdMeasurementEnabled: false
            }
          }
        })");

  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

  auto it = loaders_in_progress_.insert(loaders_in_progress_.begin(),
                                        std::move(simple_url_loader));
  simple_url_loader_ptr->SetTimeoutDuration(base::Seconds(30));

  const int kMaxRetries = 1;

  // Retry on a network change. A network change during DNS resolution
  // results in a DNS error rather than a network change error, so retry in
  // those cases as well.
  int retry_mode = network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                   network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;
  simple_url_loader_ptr->SetRetryOptions(kMaxRetries, retry_mode);

  // This is an arbitrary choice and should generally be large enough to contain
  // keys encoded in the format.
  const int kMaxJsonSize = 1000000;  // 1Mb

  // Unretained is safe because the URLLoader is owned by `this` and will be
  // deleted before `this`.
  simple_url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          &AggregationServiceNetworkFetcherImpl::OnSimpleLoaderComplete,
          base::Unretained(this), std::move(it), url, std::move(callback)),
      kMaxJsonSize);
}

void AggregationServiceNetworkFetcherImpl::OnSimpleLoaderComplete(
    UrlLoaderList::iterator it,
    const GURL& url,
    NetworkFetchCallback callback,
    std::unique_ptr<std::string> response_body) {
  std::unique_ptr<network::SimpleURLLoader> loader = std::move(*it);
  loaders_in_progress_.erase(it);

  std::optional<int> http_response_code;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers)
    http_response_code = loader->ResponseInfo()->headers->response_code();

  // Since net errors are always negative and HTTP errors are always positive,
  // it is fine to combine these in a single histogram.
  bool net_ok = loader->NetError() == net::OK ||
                loader->NetError() == net::ERR_HTTP_RESPONSE_CODE_FAILURE;
  base::UmaHistogramSparse(
      "PrivacySandbox.AggregationService.KeyFetcher.HttpResponseOrNetErrorCode",
      net_ok ? http_response_code.value_or(1) : loader->NetError());

  if (!response_body) {
    if (enable_debug_logging_) {
      LOG(ERROR) << "Key fetching failed, net error: "
                 << net::ErrorToShortString(loader->NetError())
                 << ", HTTP response code: "
                 << (http_response_code
                         ? base::NumberToString(*http_response_code)
                         : "N/A");
    }

    OnError(url, std::move(callback), FetchStatus::kDownloadError,
            /*error_msg=*/"Public key network request failed.");
    return;
  }

  base::Time response_time;
  // `expiry_time` will be null if the freshness lifetime is zero.
  base::Time expiry_time;
  base::Time current_time = clock_->Now();
  if (const network::mojom::URLResponseHead* response_info =
          loader->ResponseInfo()) {
    response_time = response_info->response_time;

    if (const net::HttpResponseHeaders* headers =
            response_info->headers.get()) {
      base::TimeDelta freshness =
          headers->GetFreshnessLifetimes(response_time).freshness;
      if (!freshness.is_zero()) {
        CHECK(freshness.is_positive());
        // Uses `response_time` as current time to get the age at response time
        // as an offset. `expiry_time` is calculated in the same way as
        // `HttpResponseHeaders::RequiresValidation`.
        expiry_time =
            response_time + freshness -
            headers->GetCurrentAge(response_info->request_time, response_time,
                                   /*current_time=*/response_time);
        if (expiry_time <= current_time) {
          OnError(url, std::move(callback), FetchStatus::kExpiredKeyError,
                  /*error_msg=*/"Public key has already expired.");
          return;
        }
      }
    }
  } else {
    response_time = current_time;
  }

  // Since DataDecoder parses untrusted data in a separate process if necessary,
  // we obey the rule of 2.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&AggregationServiceNetworkFetcherImpl::OnJsonParse,
                     weak_factory_.GetWeakPtr(), url, std::move(callback),
                     std::move(response_time), std::move(expiry_time)));
}

void AggregationServiceNetworkFetcherImpl::OnJsonParse(
    const GURL& url,
    NetworkFetchCallback callback,
    base::Time fetch_time,
    base::Time expiry_time,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    OnError(url, std::move(callback), FetchStatus::kJsonParseError,
            /*error_msg=*/result.error());
    return;
  }

  std::vector<PublicKey> keys = aggregation_service::GetPublicKeys(*result);
  if (keys.empty()) {
    OnError(url, std::move(callback), FetchStatus::kInvalidKeyError,
            /*error_msg=*/"Public key parsing failed");
    return;
  }

  RecordFetchStatus(FetchStatus::kSuccess);

  std::move(callback).Run(PublicKeyset(std::move(keys), std::move(fetch_time),
                                       std::move(expiry_time)));
}

void AggregationServiceNetworkFetcherImpl::OnError(
    const GURL& url,
    NetworkFetchCallback callback,
    FetchStatus error,
    std::string_view error_msg) {
  CHECK_NE(error, FetchStatus::kSuccess);
  RecordFetchStatus(error);

  // TODO(crbug.com/40191195): Look into better backoff logic for fetching and
  // parsing error.

  if (enable_debug_logging_) {
    LOG(ERROR) << error_msg;
  }

  std::move(callback).Run(std::nullopt);
}

void AggregationServiceNetworkFetcherImpl::RecordFetchStatus(
    FetchStatus status) const {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.AggregationService.KeyFetcher.Status2", status);
}

}  // namespace content

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_network_fetcher_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"

namespace content {

namespace {

GURL GetPublicKeyUrl(const url::Origin& origin) {
  url::Replacements<char> replacements;
  static constexpr char kEndpointPath[] =
      ".well-known/aggregation-service/keys.json";
  replacements.SetPath(kEndpointPath, url::Component(0, strlen(kEndpointPath)));
  return origin.GetURL().ReplaceComponents(replacements);
}

}  // namespace

AggregationServiceNetworkFetcherImpl::AggregationServiceNetworkFetcherImpl(
    const base::Clock* clock,
    StoragePartition* storage_partition)
    : clock_(*clock), storage_partition_(*storage_partition) {}

AggregationServiceNetworkFetcherImpl::~AggregationServiceNetworkFetcherImpl() =
    default;

void AggregationServiceNetworkFetcherImpl::FetchPublicKeys(
    const url::Origin& origin,
    NetworkFetchCallback callback) {
  // The browser process URLLoaderFactory is not created by default, so don't
  // create it until it is directly needed.
  if (!url_loader_factory_) {
    url_loader_factory_ =
        storage_partition_.GetURLLoaderFactoryForBrowserProcess();
  }

  GURL public_key_url = GetPublicKeyUrl(origin);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = public_key_url;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // No cache read, always download from the network.
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;

  // TODO(crbug.com/1238343): Update the "policy" field in the traffic
  // annotation when a setting to disable the API is properly
  // surfaced/implemented.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("aggregation_service_helper_keys", R"(
        semantics {
          sender: "Aggregation Service"
          description:
            "Downloads public keys for helper servers requested by APIs that "
            "rely on private, secure aggregation (e.g. Attribution Reporting "
            "API, see https://github.com/WICG/conversion-measurement-api). "
            "Keys are requested prior to aggregate reports being sent and are "
            "used to encrypt payloads for the helper servers."
          trigger:
            "When an aggregatable report is about to be assembled and sent."
          data:
            "JSON data comprising public key information."
          destination: OTHER
        }
        policy {
            cookies_allowed: NO
            setting:
              "This feature cannot be disabled by settings."
            policy_exception_justification: "Not implemented yet."
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
          base::Unretained(this), std::move(it), origin, std::move(callback)),
      kMaxJsonSize);
}

void AggregationServiceNetworkFetcherImpl::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void AggregationServiceNetworkFetcherImpl::OnSimpleLoaderComplete(
    UrlLoaderList::iterator it,
    const url::Origin& origin,
    NetworkFetchCallback callback,
    std::unique_ptr<std::string> response_body) {
  std::unique_ptr<network::SimpleURLLoader> loader = std::move(*it);
  loaders_in_progress_.erase(it);

  if (!response_body) {
    OnError(origin, std::move(callback), FetchError::kDownload,
            /*error_msg=*/"Public key network request failed.");
    return;
  }

  base::Time response_time;
  // `expiry_time` will be null if the freshness lifetime is zero.
  base::Time expiry_time;
  if (const network::mojom::URLResponseHead* response_info =
          loader->ResponseInfo()) {
    response_time = response_info->response_time.is_null()
                        ? clock_.Now()
                        : response_info->response_time;

    if (const net::HttpResponseHeaders* headers =
            response_info->headers.get()) {
      base::TimeDelta freshness =
          headers->GetFreshnessLifetimes(response_time).freshness;
      if (!freshness.is_zero())
        expiry_time = response_time + freshness;
    }
  } else {
    response_time = clock_.Now();
  }

  // Since DataDecoder parses untrusted data in a separate process if necessary,
  // we obey the rule of 2.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&AggregationServiceNetworkFetcherImpl::OnJsonParse,
                     weak_factory_.GetWeakPtr(), origin, std::move(callback),
                     std::move(response_time), std::move(expiry_time)));

  // TODO(crbug.com/1232599): Add performance metrics for key fetching.
}

void AggregationServiceNetworkFetcherImpl::OnJsonParse(
    const url::Origin& origin,
    NetworkFetchCallback callback,
    base::Time fetch_time,
    base::Time expiry_time,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    OnError(origin, std::move(callback), FetchError::kJsonParse, *result.error);
    return;
  }

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(result.value.value());
  if (keys.empty()) {
    OnError(origin, std::move(callback), FetchError::kJsonParse,
            /*error_msg=*/"Public key parsing failed");
    return;
  }

  std::move(callback).Run(PublicKeyset(std::move(keys), std::move(fetch_time),
                                       std::move(expiry_time)));
}

void AggregationServiceNetworkFetcherImpl::OnError(
    const url::Origin& origin,
    NetworkFetchCallback callback,
    FetchError error,
    const std::string& error_msg) {
  // TODO(crbug.com/1232601): Look into better backoff logic for fetching and
  // parsing error.

  std::move(callback).Run(absl::nullopt);
}

}  // namespace content

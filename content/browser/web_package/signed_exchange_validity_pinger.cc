// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_validity_pinger.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_type.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

namespace {

constexpr char kSXGValidityPingResult[] = "SignedExchange.ValidityPingResult";
constexpr char kSXGValidityPingDuration[] =
    "SignedExchange.ValidityPingDuration";

enum class SXGValidityPingResult {
  kSuccess,
  kFailure,
  kUnexpectedRedirect,
  kCount  // For histogram function.
};

const net::NetworkTrafficAnnotationTag kValidityPingerTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("sigined_exchange_validity_pinger", R"(
    semantics {
      sender: "Validity Pinger for Signed HTTP Exchanges"
      description:
        "Pings the validity URL of the Signed HTTP Exchanges."
      trigger:
        "Navigating Chrome (ex: clicking on a link) to an URL and the server "
        "returns a Signed HTTP Exchange."
      data: "Arbitrary site-controlled data can be included in the URL."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature cannot be disabled by settings. This feature is not "
        "enabled by default and will only be enabled via finch."
      policy_exception_justification: "Not implemented."
    })");

}  // namespace

// static
std::unique_ptr<SignedExchangeValidityPinger>
SignedExchangeValidityPinger::CreateAndStart(
    const GURL& validity_url,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
    const base::Optional<base::UnguessableToken>& throttling_profile_id,
    base::OnceClosure callback) {
  auto pinger = base::WrapUnique<SignedExchangeValidityPinger>(
      new SignedExchangeValidityPinger(std::move(callback)));
  pinger->Start(validity_url, std::move(loader_factory), std::move(throttles),
                throttling_profile_id);
  return pinger;
}

SignedExchangeValidityPinger::SignedExchangeValidityPinger(
    base::OnceClosure callback)
    : callback_(std::move(callback)) {}

void SignedExchangeValidityPinger::Start(
    const GURL& validity_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
    const base::Optional<base::UnguessableToken>& throttling_profile_id) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kSignedHTTPExchangePingValidity));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = validity_url;
  resource_request->method = "HEAD";
  resource_request->resource_type =
      static_cast<int>(ResourceType::kSubResource);
  // Set empty origin as the initiator and attach no cookies.
  resource_request->request_initiator = url::Origin();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Always hit the network as it's meant to be a liveliness check.
  // (While we don't check the result yet)
  resource_request->load_flags |=
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  resource_request->render_frame_id = MSG_ROUTING_NONE;
  resource_request->throttling_profile_id = throttling_profile_id;

  url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      std::move(url_loader_factory), std::move(throttles), 0 /* routing_id */,
      signed_exchange_utils::MakeRequestID() /* request_id */,
      network::mojom::kURLLoadOptionNone, resource_request.get(), this,
      kValidityPingerTrafficAnnotation, base::ThreadTaskRunnerHandle::Get());
}

SignedExchangeValidityPinger::~SignedExchangeValidityPinger() = default;

void SignedExchangeValidityPinger::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {}

void SignedExchangeValidityPinger::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(callback_);
  // Currently it doesn't support redirects, so just bail out.
  url_loader_.reset();
  base::UmaHistogramEnumeration(kSXGValidityPingResult,
                                SXGValidityPingResult::kUnexpectedRedirect,
                                SXGValidityPingResult::kCount);
  std::move(callback_).Run();
}

void SignedExchangeValidityPinger::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  NOTREACHED();
}

void SignedExchangeValidityPinger::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  NOTREACHED();
}

void SignedExchangeValidityPinger::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTREACHED();
}

void SignedExchangeValidityPinger::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(!pipe_drainer_);
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
}

void SignedExchangeValidityPinger::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(callback_);
  url_loader_.reset();
  SXGValidityPingResult result = (status.error_code == net::OK)
                                     ? SXGValidityPingResult::kSuccess
                                     : SXGValidityPingResult::kFailure;
  base::UmaHistogramEnumeration(kSXGValidityPingResult, result,
                                SXGValidityPingResult::kCount);
  UMA_HISTOGRAM_MEDIUM_TIMES(kSXGValidityPingDuration,
                             base::TimeTicks::Now() - start_time_);
  std::move(callback_).Run();
}

}  // namespace content

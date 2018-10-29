// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {

namespace cors {

namespace {

base::Optional<std::string> GetHeaderString(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const std::string& header_name) {
  std::string header_value;
  if (!headers->GetNormalizedHeader(header_name, &header_value))
    return base::nullopt;
  return header_value;
}

// Algorithm step 3 of the CORS-preflight fetch,
// https://fetch.spec.whatwg.org/#cors-preflight-fetch-0, that requires
//  - CORS-safelisted request-headers excluded
//  - duplicates excluded
//  - sorted lexicographically
//  - byte-lowercased
std::string CreateAccessControlRequestHeadersHeader(
    const net::HttpRequestHeaders& headers,
    bool is_revalidating) {
  // Exclude the forbidden headers because they may be added by the user
  // agent. They must be checked separately and rejected for
  // JavaScript-initiated requests.
  std::vector<std::string> filtered_headers =
      CORSUnsafeNotForbiddenRequestHeaderNames(headers.GetHeaderVector(),
                                               is_revalidating);
  if (filtered_headers.empty())
    return std::string();

  // Sort header names lexicographically.
  std::sort(filtered_headers.begin(), filtered_headers.end());

  return base::JoinString(filtered_headers, ",");
}

std::unique_ptr<ResourceRequest> CreatePreflightRequest(
    const ResourceRequest& request,
    bool tainted) {
  DCHECK(!request.url.has_username());
  DCHECK(!request.url.has_password());

  std::unique_ptr<ResourceRequest> preflight_request =
      std::make_unique<ResourceRequest>();

  // Algorithm step 1 through 4 of the CORS-preflight fetch,
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch-0.
  preflight_request->url = request.url;
  preflight_request->method = "OPTIONS";
  preflight_request->priority = request.priority;
  preflight_request->fetch_request_context_type =
      request.fetch_request_context_type;
  preflight_request->referrer = request.referrer;
  preflight_request->referrer_policy = request.referrer_policy;

  preflight_request->fetch_credentials_mode =
      mojom::FetchCredentialsMode::kOmit;
  preflight_request->load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  preflight_request->load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  preflight_request->load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;

  preflight_request->headers.SetHeader(
      header_names::kAccessControlRequestMethod, request.method);

  std::string request_headers = CreateAccessControlRequestHeadersHeader(
      request.headers, request.is_revalidating);
  if (!request_headers.empty()) {
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestHeaders, request_headers);
  }

  if (request.is_external_request) {
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestExternal, "true");
  }

  DCHECK(request.request_initiator);
  preflight_request->request_initiator = request.request_initiator;
  preflight_request->headers.SetHeader(
      net::HttpRequestHeaders::kOrigin,
      (tainted ? url::Origin() : *request.request_initiator).Serialize());

  // TODO(toyoshim): Remove the following line once the network service is
  // enabled by default.
  preflight_request->skip_service_worker = true;

  // TODO(toyoshim): Should not matter, but at this moment, it hits a sanity
  // check in ResourceDispatcherHostImpl if |resource_type| isn't set.
  preflight_request->resource_type = request.resource_type;

  return preflight_request;
}

std::unique_ptr<PreflightResult> CreatePreflightResult(
    const GURL& final_url,
    const ResourceResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    base::Optional<CORSErrorStatus>* detected_error_status) {
  DCHECK(detected_error_status);

  *detected_error_status = CheckPreflightAccess(
      final_url, head.headers->response_code(),
      GetHeaderString(head.headers, header_names::kAccessControlAllowOrigin),
      GetHeaderString(head.headers,
                      header_names::kAccessControlAllowCredentials),
      original_request.fetch_credentials_mode,
      tainted ? url::Origin() : *original_request.request_initiator);
  if (*detected_error_status)
    return nullptr;

  base::Optional<mojom::CORSError> error;
  error = CheckPreflight(head.headers->response_code());
  if (error) {
    *detected_error_status = CORSErrorStatus(*error);
    return nullptr;
  }

  if (original_request.is_external_request) {
    *detected_error_status = CheckExternalPreflight(GetHeaderString(
        head.headers, header_names::kAccessControlAllowExternal));
    if (*detected_error_status)
      return nullptr;
  }

  auto result = PreflightResult::Create(
      original_request.fetch_credentials_mode,
      GetHeaderString(head.headers, header_names::kAccessControlAllowMethods),
      GetHeaderString(head.headers, header_names::kAccessControlAllowHeaders),
      GetHeaderString(head.headers, header_names::kAccessControlMaxAge),
      &error);

  if (error)
    *detected_error_status = CORSErrorStatus(*error);
  return result;
}

base::Optional<CORSErrorStatus> CheckPreflightResult(
    PreflightResult* result,
    const ResourceRequest& original_request) {
  base::Optional<CORSErrorStatus> status =
      result->EnsureAllowedCrossOriginMethod(original_request.method);
  if (status)
    return status;

  return result->EnsureAllowedCrossOriginHeaders(
      original_request.headers, original_request.is_revalidating);
}

// TODO(toyoshim): Remove this class once the Network Service is enabled.
// This wrapper class is used to tell the actual request's |request_id| to call
// CreateLoaderAndStart() for the legacy implementation that requires the ID
// to be unique while SimpleURLLoader always set it to 0.
class WrappedLegacyURLLoaderFactory final : public mojom::URLLoaderFactory {
 public:
  static WrappedLegacyURLLoaderFactory* GetSharedInstance() {
    static base::NoDestructor<WrappedLegacyURLLoaderFactory> factory;
    return &*factory;
  }

  ~WrappedLegacyURLLoaderFactory() override = default;

  void SetFactoryAndRequestId(mojom::URLLoaderFactory* factory,
                              int32_t request_id) {
    factory_ = factory;
    request_id_ = request_id;
  }

  void CheckIdle() {
    DCHECK(!factory_);
    DCHECK_EQ(0, request_id_);
  }

  // mojom::URLLoaderFactory:
  void CreateLoaderAndStart(::network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            ::network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    factory_->CreateLoaderAndStart(std::move(loader), routing_id, request_id_,
                                   options, request, std::move(client),
                                   traffic_annotation);
    factory_ = nullptr;
    request_id_ = 0;
  }

  void Clone(mojom::URLLoaderFactoryRequest factory) override {
    // Should not be called because retry logic is disabled to use
    // SimpleURLLoader. Could not work correctly by design.
    NOTREACHED();
  }

 private:
  mojom::URLLoaderFactory* factory_ = nullptr;
  int32_t request_id_ = 0;
};

}  // namespace

class PreflightController::PreflightLoader final {
 public:
  PreflightLoader(PreflightController* controller,
                  CompletionCallback completion_callback,
                  const ResourceRequest& request,
                  bool tainted,
                  const net::NetworkTrafficAnnotationTag& annotation_tag,
                  base::OnceCallback<void()> preflight_finalizer)
      : controller_(controller),
        completion_callback_(std::move(completion_callback)),
        original_request_(request),
        tainted_(tainted),
        preflight_finalizer_(std::move(preflight_finalizer)) {
    loader_ = SimpleURLLoader::Create(CreatePreflightRequest(request, tainted),
                                      annotation_tag);
  }

  void Request(mojom::URLLoaderFactory* loader_factory, int32_t request_id) {
    DCHECK(loader_);

    loader_->SetOnRedirectCallback(base::BindRepeating(
        &PreflightLoader::HandleRedirect, base::Unretained(this)));
    loader_->SetOnResponseStartedCallback(base::BindRepeating(
        &PreflightLoader::HandleResponseHeader, base::Unretained(this)));

    // TODO(toyoshim): Stop using WrappedLegacyURLLoaderFactory once the Network
    // Service is enabled by default. This is a workaround to use an allowed
    // request_id in the legacy URLLoaderFactory.
    WrappedLegacyURLLoaderFactory::GetSharedInstance()->SetFactoryAndRequestId(
        loader_factory, request_id);
    loader_->DownloadToString(
        WrappedLegacyURLLoaderFactory::GetSharedInstance(),
        base::BindOnce(&PreflightLoader::HandleResponseBody,
                       base::Unretained(this)),
        0);
    WrappedLegacyURLLoaderFactory::GetSharedInstance()->CheckIdle();
  }

 private:
  void HandleRedirect(const net::RedirectInfo& redirect_info,
                      const network::ResourceResponseHead& response_head,
                      std::vector<std::string>* to_be_removed_headers) {
    // Preflight should not allow any redirect.
    FinalizeLoader();

    std::move(completion_callback_)
        .Run(net::ERR_FAILED,
             CORSErrorStatus(mojom::CORSError::kPreflightDisallowedRedirect),
             base::nullopt);

    RemoveFromController();
    // |this| is deleted here.
  }

  void HandleResponseHeader(const GURL& final_url,
                            const ResourceResponseHead& head) {
    FinalizeLoader();

    timing_info_.start_time = head.request_start;
    timing_info_.finish_time = base::TimeTicks::Now();
    timing_info_.alpn_negotiated_protocol = head.alpn_negotiated_protocol;
    timing_info_.connection_info = head.connection_info;
    head.headers->GetNormalizedHeader("Timing-Allow-Origin",
                                      &timing_info_.timing_allow_origin);
    timing_info_.transfer_size = head.encoded_data_length;

    base::Optional<CORSErrorStatus> detected_error_status;
    std::unique_ptr<PreflightResult> result = CreatePreflightResult(
        final_url, head, original_request_, tainted_, &detected_error_status);

    if (result) {
      // Preflight succeeded. Check |original_request_| with |result|.
      DCHECK(!detected_error_status);
      detected_error_status =
          CheckPreflightResult(result.get(), original_request_);
    }

    // TODO(toyoshim): Check the spec if we cache |result| regardless of
    // following checks.
    if (!detected_error_status) {
      controller_->AppendToCache(*original_request_.request_initiator,
                                 original_request_.url, std::move(result));
    }

    base::Optional<PreflightTimingInfo> timing_info;
    if (!detected_error_status)
      timing_info = std::move(timing_info_);
    std::move(completion_callback_)
        .Run(detected_error_status ? net::ERR_FAILED : net::OK,
             detected_error_status, std::move(timing_info));

    RemoveFromController();
    // |this| is deleted here.
  }

  void HandleResponseBody(std::unique_ptr<std::string> response_body) {
    // Reached only when the request fails without receiving headers, e.g.
    // unknown hosts, unreachable remote, reset by peer, and so on.
    // See https://crbug.com/826868 for related discussion.
    DCHECK(!response_body);
    const int error = loader_->NetError();
    DCHECK_NE(error, net::OK);
    FinalizeLoader();
    std::move(completion_callback_).Run(error, base::nullopt, base::nullopt);
    RemoveFromController();
    // |this| is deleted here.
  }

  void FinalizeLoader() {
    DCHECK(loader_);
    if (preflight_finalizer_)
      std::move(preflight_finalizer_).Run();
    loader_.reset();
  }

  // Removes |this| instance from |controller_|. Once the method returns, |this|
  // is already removed.
  void RemoveFromController() { controller_->RemoveLoader(this); }

  // PreflightController owns all PreflightLoader instances, and should outlive.
  PreflightController* const controller_;

  // Holds SimpleURLLoader instance for the CORS-preflight request.
  std::unique_ptr<SimpleURLLoader> loader_;

  PreflightTimingInfo timing_info_;

  // Holds caller's information.
  PreflightController::CompletionCallback completion_callback_;
  const ResourceRequest original_request_;

  const bool tainted_;

  // This is needed because we sometimes need to cancel the preflight loader
  // synchronously.
  // TODO(yhirano): Remove this when the network service is fully enabled.
  base::OnceCallback<void()> preflight_finalizer_;

  DISALLOW_COPY_AND_ASSIGN(PreflightLoader);
};

// static
std::unique_ptr<ResourceRequest>
PreflightController::CreatePreflightRequestForTesting(
    const ResourceRequest& request,
    bool tainted) {
  return CreatePreflightRequest(request, tainted);
}

// static
PreflightController* PreflightController::GetDefaultController() {
  static base::NoDestructor<PreflightController> controller;
  return &*controller;
}

PreflightController::PreflightController() = default;

PreflightController::~PreflightController() = default;

void PreflightController::PerformPreflightCheck(
    CompletionCallback callback,
    int32_t request_id,
    const ResourceRequest& request,
    bool tainted,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    mojom::URLLoaderFactory* loader_factory,
    base::OnceCallback<void()> preflight_finalizer) {
  DCHECK(request.request_initiator);

  if (!request.is_external_request &&
      cache_.CheckIfRequestCanSkipPreflight(
          request.request_initiator->Serialize(), request.url,
          request.fetch_credentials_mode, request.method, request.headers,
          request.is_revalidating)) {
    std::move(callback).Run(net::OK, base::nullopt, base::nullopt);
    return;
  }

  auto emplaced_pair = loaders_.emplace(std::make_unique<PreflightLoader>(
      this, std::move(callback), request, tainted, annotation_tag,
      std::move(preflight_finalizer)));
  (*emplaced_pair.first)->Request(loader_factory, request_id);
}

void PreflightController::RemoveLoader(PreflightLoader* loader) {
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);
}

void PreflightController::AppendToCache(
    const url::Origin& origin,
    const GURL& url,
    std::unique_ptr<PreflightResult> result) {
  cache_.AppendEntry(origin.Serialize(), url, std::move(result));
}

}  // namespace cors

}  // namespace network

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_factory.h"

#include "base/logging.h"
#include "net/base/load_flags.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/url_loader_factory.h"

namespace network {

namespace cors {

CORSURLLoaderFactory::CORSURLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    mojom::URLLoaderFactoryRequest request,
    const OriginAccessList* origin_access_list)
    : context_(context),
      disable_web_security_(params && params->disable_web_security),
      network_loader_factory_(std::make_unique<network::URLLoaderFactory>(
          context,
          std::move(params),
          std::move(resource_scheduler_client),
          this)),
      origin_access_list_(origin_access_list) {
  DCHECK(context_);
  DCHECK(origin_access_list_);
  bindings_.AddBinding(this, std::move(request));
  bindings_.set_connection_error_handler(base::BindRepeating(
      &CORSURLLoaderFactory::DeleteIfNeeded, base::Unretained(this)));
}

CORSURLLoaderFactory::CORSURLLoaderFactory(
    bool disable_web_security,
    std::unique_ptr<mojom::URLLoaderFactory> network_loader_factory,
    const base::RepeatingCallback<void(int)>& preflight_finalizer,
    const OriginAccessList* origin_access_list)
    : disable_web_security_(disable_web_security),
      network_loader_factory_(std::move(network_loader_factory)),
      preflight_finalizer_(preflight_finalizer),
      origin_access_list_(origin_access_list) {
  DCHECK(origin_access_list_);
}

CORSURLLoaderFactory::~CORSURLLoaderFactory() = default;

void CORSURLLoaderFactory::OnLoaderCreated(
    std::unique_ptr<mojom::URLLoader> loader) {
  loaders_.insert(std::move(loader));
}

void CORSURLLoaderFactory::DestroyURLLoader(mojom::URLLoader* loader) {
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);

  DeleteIfNeeded();
}

void CORSURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!IsSane(resource_request)) {
    client->OnComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  if (base::FeatureList::IsEnabled(features::kOutOfBlinkCORS) &&
      !disable_web_security_) {
    auto loader = std::make_unique<CORSURLLoader>(
        std::move(request), routing_id, request_id, options,
        base::BindOnce(&CORSURLLoaderFactory::DestroyURLLoader,
                       base::Unretained(this)),
        resource_request, std::move(client), traffic_annotation,
        network_loader_factory_.get(), preflight_finalizer_,
        origin_access_list_);
    auto* raw_loader = loader.get();
    OnLoaderCreated(std::move(loader));
    raw_loader->Start();
  } else {
    network_loader_factory_->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, resource_request,
        std::move(client), traffic_annotation);
  }
}

void CORSURLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  // The cloned factories stop working when this factory is destructed.
  bindings_.AddBinding(this, std::move(request));
}

void CORSURLLoaderFactory::ClearBindings() {
  bindings_.CloseAllBindings();
}

void CORSURLLoaderFactory::DeleteIfNeeded() {
  if (!context_)
    return;
  if (bindings_.empty() && loaders_.empty())
    context_->DestroyURLLoaderFactory(this);
}

bool CORSURLLoaderFactory::IsSane(const ResourceRequest& request) {
  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS cannot work.
  if (!request.request_initiator &&
      request.fetch_request_mode != mojom::FetchRequestMode::kNavigate &&
      request.fetch_request_mode != mojom::FetchRequestMode::kNoCORS) {
    LOG(WARNING) << "|fetch_request_mode| is " << request.fetch_request_mode
                 << ", but |request_initiator| is not set.";
    return false;
  }

  const auto load_flags_pattern = net::LOAD_DO_NOT_SAVE_COOKIES |
                                  net::LOAD_DO_NOT_SEND_COOKIES |
                                  net::LOAD_DO_NOT_SEND_AUTH_DATA;
  // The credentials mode and load_flags should match.
  if (request.fetch_credentials_mode == mojom::FetchCredentialsMode::kOmit &&
      (request.load_flags & load_flags_pattern) != load_flags_pattern) {
    LOG(WARNING) << "|fetch_credentials_mode| and |load_flags| contradict each "
                    "other.";
    return false;
  }

  // TODO(yhirano): If the request mode is "no-cors", the redirect mode should
  // be "follow".
  return true;
}

}  // namespace cors

}  // namespace network

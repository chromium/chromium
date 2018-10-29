// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_script_loader.h"

#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

SharedWorkerScriptLoader::SharedWorkerScriptLoader(
    int process_id,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host,
    base::WeakPtr<AppCacheHost> appcache_host,
    ResourceContext* resource_context,
    scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : process_id_(process_id),
      routing_id_(routing_id),
      request_id_(request_id),
      options_(options),
      resource_request_(resource_request),
      client_(std::move(client)),
      service_worker_provider_host_(service_worker_provider_host),
      resource_context_(resource_context),
      default_loader_factory_(std::move(default_loader_factory)),
      traffic_annotation_(traffic_annotation),
      url_loader_client_binding_(this),
      weak_factory_(this) {
  DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());

  if (service_worker_provider_host_) {
    std::unique_ptr<NavigationLoaderInterceptor> service_worker_interceptor =
        ServiceWorkerRequestHandler::InitializeForSharedWorker(
            resource_request_, service_worker_provider_host_);
    if (service_worker_interceptor)
      interceptors_.push_back(std::move(service_worker_interceptor));
  }

  if (appcache_host) {
    std::unique_ptr<NavigationLoaderInterceptor> appcache_interceptor =
        AppCacheRequestHandler::InitializeForMainResourceNetworkService(
            resource_request_, appcache_host, default_loader_factory_);
    if (appcache_interceptor)
      interceptors_.push_back(std::move(appcache_interceptor));
  }

  Start();
}

SharedWorkerScriptLoader::~SharedWorkerScriptLoader() = default;

base::WeakPtr<SharedWorkerScriptLoader> SharedWorkerScriptLoader::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SharedWorkerScriptLoader::Start() {
  if (interceptor_index_ < interceptors_.size()) {
    auto* interceptor = interceptors_[interceptor_index_++].get();
    interceptor->MaybeCreateLoader(
        resource_request_, resource_context_,
        base::BindOnce(&SharedWorkerScriptLoader::MaybeStartLoader,
                       weak_factory_.GetWeakPtr(), interceptor),
        base::BindOnce(&SharedWorkerScriptLoader::LoadFromNetwork,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  LoadFromNetwork(false);
}

void SharedWorkerScriptLoader::MaybeStartLoader(
    NavigationLoaderInterceptor* interceptor,
    SingleRequestURLLoaderFactory::RequestHandler single_request_handler) {
  DCHECK(interceptor);

  // Create SubresourceLoaderParams for intercepting subresource requests and
  // populating the "controller" field in ServiceWorkerContainer. This can be
  // null if the interceptor is not interested in this request.
  subresource_loader_params_ =
      interceptor->MaybeCreateSubresourceLoaderParams();

  if (single_request_handler) {
    // The interceptor elected to handle the request. Use it.
    network::mojom::URLLoaderClientPtr client;
    url_loader_client_binding_.Bind(mojo::MakeRequest(&client));
    url_loader_factory_ = base::MakeRefCounted<SingleRequestURLLoaderFactory>(
        std::move(single_request_handler));
    url_loader_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader_), routing_id_, request_id_, options_,
        resource_request_, std::move(client), traffic_annotation_);
    // We continue in URLLoaderClient calls.
    return;
  }

  // We shouldn't try the remaining interceptors if this interceptor provides
  // SubresourceLoaderParams. For details, see comments on
  // NavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams().
  if (subresource_loader_params_)
    interceptor_index_ = interceptors_.size();

  // Continue until all the interceptors are tried.
  Start();
}

void SharedWorkerScriptLoader::LoadFromNetwork(
    bool reset_subresource_loader_params) {
  default_loader_used_ = true;
  network::mojom::URLLoaderClientPtr client;
  if (url_loader_client_binding_)
    url_loader_client_binding_.Unbind();
  url_loader_client_binding_.Bind(mojo::MakeRequest(&client));
  url_loader_factory_ = default_loader_factory_;
  url_loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader_), routing_id_, request_id_, options_,
      resource_request_, std::move(client), traffic_annotation_);
  // We continue in URLLoaderClient calls.
}

// URLLoader -------------------------------------------------------------------
// When this class gets a FollowRedirect IPC from the renderer, it restarts with
// the new URL.

void SharedWorkerScriptLoader::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  DCHECK(!modified_request_headers.has_value()) << "Redirect with modified "
                                                   "headers was not supported "
                                                   "yet. crbug.com/845683";
  DCHECK(redirect_info_);

  // |should_clear_upload| is unused because there is no body anyway.
  DCHECK(!resource_request_.request_body);
  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      resource_request_.url, resource_request_.method, *redirect_info_,
      modified_request_headers, &resource_request_.headers,
      &should_clear_upload);

  resource_request_.url = redirect_info_->new_url;
  resource_request_.method = redirect_info_->new_method;
  resource_request_.site_for_cookies = redirect_info_->new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info_->new_referrer);
  resource_request_.referrer_policy = redirect_info_->new_referrer_policy;

  // Restart the request.
  interceptor_index_ = 0;
  url_loader_client_binding_.Unbind();
  redirect_info_.reset();

  // Cancel the request on ResourceDispatcherHost so that we can fall back
  // to network again.
  DCHECK(ResourceDispatcherHostImpl::Get());
  ResourceDispatcherHostImpl::Get()->CancelRequest(process_id_, request_id_);

  Start();
}

void SharedWorkerScriptLoader::ProceedWithResponse() {
  // Only for navigations.
  NOTREACHED();
}

// Below we make a small effort to support the other URLLoader functions by
// forwarding to the current |url_loader_| if any, but don't bother queuing
// state or propagating state to a new URLLoader upon redirect.
void SharedWorkerScriptLoader::SetPriority(net::RequestPriority priority,
                                           int32_t intra_priority_value) {
  if (url_loader_)
    url_loader_->SetPriority(priority, intra_priority_value);
}

void SharedWorkerScriptLoader::PauseReadingBodyFromNet() {
  if (url_loader_)
    url_loader_->PauseReadingBodyFromNet();
}

void SharedWorkerScriptLoader::ResumeReadingBodyFromNet() {
  if (url_loader_)
    url_loader_->ResumeReadingBodyFromNet();
}

// URLLoaderClient ----------------------------------------------------------
// This class forwards any client messages to the outer client in the renderer.
// Additionally, on redirects it saves the redirect info so if the renderer
// calls FollowRedirect(), it can do so.

void SharedWorkerScriptLoader::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  client_->OnReceiveResponse(response_head);
}

void SharedWorkerScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  if (--redirect_limit_ == 0) {
    client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
    return;
  }

  redirect_info_ = redirect_info;
  client_->OnReceiveRedirect(redirect_info, response_head);
}

void SharedWorkerScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  client_->OnUploadProgress(current_position, total_size,
                            std::move(ack_callback));
}

void SharedWorkerScriptLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  client_->OnReceiveCachedMetadata(data);
}

void SharedWorkerScriptLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void SharedWorkerScriptLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  client_->OnStartLoadingResponseBody(std::move(consumer));
}

void SharedWorkerScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code == net::OK)
    service_worker_provider_host_->CompleteSharedWorkerPreparation();
  client_->OnComplete(status);
}

bool SharedWorkerScriptLoader::MaybeCreateLoaderForResponse(
    const network::ResourceResponseHead& response,
    network::mojom::URLLoaderPtr* response_url_loader,
    network::mojom::URLLoaderClientRequest* response_client_request,
    ThrottlingURLLoader* url_loader) {
  // TODO(crbug/898755): This is odd that NavigationLoaderInterceptor::
  // MaybeCreateLoader() is called directly from SharedWorkerScriptLoader. But
  // NavigationLoaderInterceptor::MaybeCreateLoaderForResponse() is called from
  // SharedWorkerScriptFetcher::OnReceiveResponse(). This is due to the wired
  // design of SharedWorkerScriptLoader and SharedWorkerScriptFetcher and the
  // interceptors. The interceptors should be owned by
  // SharedWorkerScriptFetcher.
  DCHECK(default_loader_used_);
  for (auto& interceptor : interceptors_) {
    bool skip_other_interceptors = false;
    if (interceptor->MaybeCreateLoaderForResponse(
            resource_request_.url, response, response_url_loader,
            response_client_request, url_loader, &skip_other_interceptors)) {
      // Both ServiceWorkerRequestHandler and AppCacheRequestHandler don't set
      // skip_other_interceptors.
      DCHECK(!skip_other_interceptors);
      subresource_loader_params_ =
          interceptor->MaybeCreateSubresourceLoaderParams();
      return true;
    }
  }
  return false;
}

}  // namespace content

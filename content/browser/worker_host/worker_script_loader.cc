// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_loader.h"

#include "base/functional/bind.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"

namespace content {

WorkerScriptLoader::WorkerScriptLoader(
    int process_id,
    const DedicatedOrSharedWorkerToken& worker_token,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    base::WeakPtr<ServiceWorkerMainResourceHandle> service_worker_handle,
    const BrowserContextGetter& browser_context_getter,
    scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    ukm::SourceId ukm_source_id)
    : request_id_(request_id),
      options_(options),
      resource_request_(resource_request),
      client_(std::move(client)),
      service_worker_handle_(std::move(service_worker_handle)),
      browser_context_getter_(browser_context_getter),
      default_loader_factory_(std::move(default_loader_factory)),
      traffic_annotation_(traffic_annotation),
      ukm_source_id_(ukm_source_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_worker_handle_) {
    // The DedicatedWorkerHost or SharedWorkerHost is already destroyed.
    Abort();
    return;
  }
  auto service_worker_interceptor =
      ServiceWorkerMainResourceLoaderInterceptor::CreateForWorker(
          resource_request_, isolation_info, process_id, worker_token,
          service_worker_handle_);

  if (service_worker_interceptor)
    interceptors_.push_back(std::move(service_worker_interceptor));

  Start();
}

WorkerScriptLoader::~WorkerScriptLoader() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

base::WeakPtr<WorkerScriptLoader> WorkerScriptLoader::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_factory_.GetWeakPtr();
}

void WorkerScriptLoader::Abort() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
}

void WorkerScriptLoader::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!completed_);

  // The DedicatedWorkerHost or SharedWorkerHost is already destroyed.
  if (!service_worker_handle_) {
    Abort();
    return;
  }

  BrowserContext* browser_context = browser_context_getter_.Run();
  if (!browser_context) {
    Abort();
    return;
  }

  if (interceptor_index_ < interceptors_.size()) {
    auto* interceptor = interceptors_[interceptor_index_++].get();
    interceptor->MaybeCreateLoader(
        resource_request_, browser_context,
        base::BindOnce(&WorkerScriptLoader::MaybeStartLoader,
                       weak_factory_.GetWeakPtr(), interceptor),
        base::BindOnce(&WorkerScriptLoader::LoadFromNetwork,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  LoadFromNetwork(false);
}

void WorkerScriptLoader::MaybeStartLoader(
    NavigationLoaderInterceptor* interceptor,
    scoped_refptr<network::SharedURLLoaderFactory> single_request_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!completed_);
  DCHECK(interceptor);

  if (!service_worker_handle_) {
    // The DedicatedWorkerHost or SharedWorkerHost is already destroyed.
    Abort();
    return;
  }

  // Create SubresourceLoaderParams for intercepting subresource requests and
  // populating the "controller" field in ServiceWorkerContainer. This can be
  // null if the interceptor is not interested in this request.
  subresource_loader_params_ =
      interceptor->MaybeCreateSubresourceLoaderParams();

  if (single_request_factory) {
    // The interceptor elected to handle the request. Use it.
    url_loader_factory_ = std::move(single_request_factory);
    url_loader_.reset();
    url_loader_factory_->CreateLoaderAndStart(
        url_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
        resource_request_,
        url_loader_client_receiver_.BindNewPipeAndPassRemote(),
        traffic_annotation_);
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

void WorkerScriptLoader::LoadFromNetwork(bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!completed_);

  default_loader_used_ = true;
  url_loader_client_receiver_.reset();
  url_loader_factory_ = default_loader_factory_;
  url_loader_.reset();
  url_loader_factory_->CreateLoaderAndStart(
      url_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
      resource_request_, url_loader_client_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation_);
  // We continue in URLLoaderClient calls.
}

// URLLoader -------------------------------------------------------------------
// When this class gets a FollowRedirect IPC from the renderer, it restarts with
// the new URL.

void WorkerScriptLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!new_url.has_value()) << "Redirect with modified URL was not "
                                  "supported yet. crbug.com/845683";
  DCHECK(redirect_info_);

  // |should_clear_upload| is unused because there is no body anyway.
  DCHECK(!resource_request_.request_body);
  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      resource_request_.url, resource_request_.method, *redirect_info_,
      removed_headers, modified_headers, &resource_request_.headers,
      &should_clear_upload);
  resource_request_.cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);
  for (const std::string& name : removed_headers)
    resource_request_.cors_exempt_headers.RemoveHeader(name);

  resource_request_.url = redirect_info_->new_url;
  resource_request_.method = redirect_info_->new_method;
  resource_request_.site_for_cookies = redirect_info_->new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info_->new_referrer);
  resource_request_.referrer_policy = redirect_info_->new_referrer_policy;

  // Restart the request.
  interceptor_index_ = 0;
  url_loader_client_receiver_.reset();
  redirect_info_.reset();

  Start();
}

// Below we make a small effort to support the other URLLoader functions by
// forwarding to the current |url_loader_| if any, but don't bother queuing
// state or propagating state to a new URLLoader upon redirect.
void WorkerScriptLoader::SetPriority(net::RequestPriority priority,
                                     int32_t intra_priority_value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (url_loader_)
    url_loader_->SetPriority(priority, intra_priority_value);
}

void WorkerScriptLoader::PauseReadingBodyFromNet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (url_loader_)
    url_loader_->PauseReadingBodyFromNet();
}

void WorkerScriptLoader::ResumeReadingBodyFromNet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (url_loader_)
    url_loader_->ResumeReadingBodyFromNet();
}
// URLLoader end --------------------------------------------------------------

// URLLoaderClient ------------------------------------------------------------
// This class forwards any client messages to the outer client in the renderer.
// Additionally, on redirects it saves the redirect info so if the renderer
// calls FollowRedirect(), it can do so.

void WorkerScriptLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  client_->OnReceiveEarlyHints(std::move(early_hints));
}

void WorkerScriptLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  client_->OnReceiveResponse(std::move(response_head), std::move(body),
                             std::move(cached_metadata));
}

void WorkerScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (--redirect_limit_ == 0) {
    CommitCompleted(
        network::URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
    return;
  }

  redirect_info_ = redirect_info;
  client_->OnReceiveRedirect(redirect_info, std::move(response_head));
}

void WorkerScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  client_->OnUploadProgress(current_position, total_size,
                            std::move(ack_callback));
}

void WorkerScriptLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kWorkerScriptLoader);
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void WorkerScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CommitCompleted(status);
}

// URLLoaderClient end ---------------------------------------------------------

bool WorkerScriptLoader::MaybeCreateLoaderForResponse(
    network::mojom::URLResponseHeadPtr* response_head,
    mojo::ScopedDataPipeConsumerHandle* response_body,
    mojo::PendingRemote<network::mojom::URLLoader>* response_url_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>*
        response_client_receiver,
    blink::ThrottlingURLLoader* url_loader) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug/898755): This is odd that NavigationLoaderInterceptor::
  // MaybeCreateLoader() is called directly from WorkerScriptLoader. But
  // NavigationLoaderInterceptor::MaybeCreateLoaderForResponse() is called from
  // WorkerScriptFetcher::OnReceiveResponse(). This is due to the wired design
  // of WorkerScriptLoader and WorkerScriptFetcher and the interceptors. The
  // interceptors should be owned by WorkerScriptFetcher.
  DCHECK(default_loader_used_);
  for (auto& interceptor : interceptors_) {
    bool skip_other_interceptors = false;
    bool will_return_unsafe_redirect = false;
    if (interceptor->MaybeCreateLoaderForResponse(
            resource_request_, response_head, response_body,
            response_url_loader, response_client_receiver, url_loader,
            &skip_other_interceptors, &will_return_unsafe_redirect)) {
      // ServiceWorkerMainResourceLoaderInterceptor doesn't set
      // skip_other_interceptors or will_return_unsafe_redirect.
      DCHECK(!skip_other_interceptors);
      DCHECK(!will_return_unsafe_redirect);
      subresource_loader_params_ =
          interceptor->MaybeCreateSubresourceLoaderParams();
      return true;
    }
  }
  return false;
}

void WorkerScriptLoader::CommitCompleted(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!completed_);
  completed_ = true;

  if (status.error_code == net::OK && service_worker_handle_) {
    // TODO(https://crbug.com/999049): Pass the PolicyContainerPolicies. It can
    // be built from `WorkerScriptLoader::OnReceiveResponse` from the
    // `response_head->parsed_headers`.
    service_worker_handle_->OnBeginWorkerCommit(PolicyContainerPolicies(),
                                                ukm_source_id_);
  }

  client_->OnComplete(status);

  // We're done. Ensure we no longer send messages to our client, and no longer
  // talk to the loader we're a client of.
  client_.reset();
  url_loader_client_receiver_.reset();
  url_loader_.reset();
}

}  // namespace content

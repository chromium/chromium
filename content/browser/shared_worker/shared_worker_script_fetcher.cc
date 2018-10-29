// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_script_fetcher.h"

#include "base/feature_list.h"
#include "content/browser/shared_worker/shared_worker_script_loader.h"
#include "content/browser/shared_worker/shared_worker_script_loader_factory.h"
#include "content/common/throttling_url_loader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_loader_throttle.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"

namespace content {

namespace {

const net::NetworkTrafficAnnotationTag
    kSharedWorkerScriptLoadTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("shared_worker_script_load",
                                            R"(
      semantics {
        sender: "Shared Worker Script Load"
        description:
          "This request is issued by SharedWorker to fetch its main script."
        trigger:
          "Calling new SharedWorker()."
        data: "Anything the initiator wants to send."
        destination: OTHER
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "This request can be prevented by disabling JavaScript."
        chrome_policy {
          URLBlacklist {
            URLBlacklist: { entries: '*' }
          }
        }
        chrome_policy {
          URLWhitelist {
            URLWhitelist { }
          }
        }
      }
)");

}  // namespace

void SharedWorkerScriptFetcher::CreateAndStart(
    std::unique_ptr<SharedWorkerScriptLoaderFactory> script_loader_factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    std::unique_ptr<network::ResourceRequest> resource_request,
    CreateAndStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  // This fetcher will delete itself. See the class level comment.
  (new SharedWorkerScriptFetcher(std::move(script_loader_factory),
                                 std::move(resource_request),
                                 std::move(callback)))
      ->Start(std::move(throttles));
}

SharedWorkerScriptFetcher::SharedWorkerScriptFetcher(
    std::unique_ptr<SharedWorkerScriptLoaderFactory> script_loader_factory,
    std::unique_ptr<network::ResourceRequest> resource_request,
    CreateAndStartCallback callback)
    : script_loader_factory_(std::move(script_loader_factory)),
      resource_request_(std::move(resource_request)),
      callback_(std::move(callback)),
      response_url_loader_binding_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

SharedWorkerScriptFetcher::~SharedWorkerScriptFetcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void SharedWorkerScriptFetcher::Start(
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto shared_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          script_loader_factory_.get());

  // SharedWorker doesn't have a frame.
  int32_t routing_id = MSG_ROUTING_NONE;

  // NetworkService is not interested in the request ID.
  int request_id = -1;

  url_loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
      std::move(shared_url_loader_factory), std::move(throttles), routing_id,
      request_id, network::mojom::kURLLoadOptionNone, resource_request_.get(),
      this, kSharedWorkerScriptLoadTrafficAnnotation,
      base::ThreadTaskRunnerHandle::Get());
}

void SharedWorkerScriptFetcher::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::WeakPtr<SharedWorkerScriptLoader> script_loader =
      script_loader_factory_->GetScriptLoader();
  if (script_loader && script_loader->default_loader_used_) {
    // If the default network loader was used to handle the URL load request we
    // need to see if the request interceptors want to potentially create a new
    // loader for the response, e.g. AppCache's fallback.
    DCHECK(!response_url_loader_);
    network::mojom::URLLoaderClientRequest response_client_request;
    if (script_loader->MaybeCreateLoaderForResponse(head, &response_url_loader_,
                                                    &response_client_request,
                                                    url_loader_.get())) {
      DCHECK(response_url_loader_);
      response_url_loader_binding_.Bind(std::move(response_client_request));
      subresource_loader_params_ = script_loader->TakeSubresourceLoaderParams();
      url_loader_.reset();
      // OnReceiveResponse() will be called again.
      return;
    }
  }

  blink::mojom::SharedWorkerMainScriptLoadParamsPtr main_script_load_params =
      blink::mojom::SharedWorkerMainScriptLoadParams::New();

  // Fill in params for loading shared worker's main script and subresources.
  main_script_load_params->response_head = head;
  if (url_loader_) {
    // The main script was served by a request interceptor or the default
    // network loader.
    DCHECK(!response_url_loader_);
    main_script_load_params->url_loader_client_endpoints =
        url_loader_->Unbind();
    subresource_loader_params_ = script_loader->TakeSubresourceLoaderParams();
  } else {
    // The main script was served by the default network loader first, and then
    // a request interceptor created another loader |response_url_loader_| for
    // serving an alternative response.
    DCHECK(response_url_loader_);
    DCHECK(response_url_loader_binding_.is_bound());
    main_script_load_params->url_loader_client_endpoints =
        network::mojom::URLLoaderClientEndpoints::New(
            response_url_loader_.PassInterface(),
            response_url_loader_binding_.Unbind());
  }

  for (size_t i = 0; i < redirect_infos_.size(); ++i) {
    main_script_load_params->redirect_infos.emplace_back(redirect_infos_[i]);
    main_script_load_params->redirect_response_heads.emplace_back(
        redirect_response_heads_[i]);
  }

  std::move(callback_).Run(std::move(main_script_load_params),
                           std::move(subresource_loader_params_),
                           true /* success */);
  delete this;
}

void SharedWorkerScriptFetcher::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  redirect_infos_.push_back(redirect_info);
  redirect_response_heads_.push_back(head);
  url_loader_->FollowRedirect(base::nullopt);
}

void SharedWorkerScriptFetcher::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  NOTREACHED();
}

void SharedWorkerScriptFetcher::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  NOTREACHED();
}

void SharedWorkerScriptFetcher::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTREACHED();
}

void SharedWorkerScriptFetcher::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  // Not reached. At this point, the loader and client endpoints must have
  // been unbound and forwarded to the renderer.
  NOTREACHED();
}

void SharedWorkerScriptFetcher::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // We can reach here only when loading fails before receiving a response head.
  DCHECK_NE(net::OK, status.error_code);
  std::move(callback_).Run(nullptr /* main_script_load_params */,
                           base::nullopt /* subresource_loader_params */,
                           false /* success */);
  delete this;
}

}  // namespace content

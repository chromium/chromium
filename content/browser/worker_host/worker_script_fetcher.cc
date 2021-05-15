// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_fetcher.h"

#include "base/feature_list.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/browser/worker_host/worker_script_loader.h"
#include "content/browser/worker_host/worker_script_loader_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

namespace {

const net::NetworkTrafficAnnotationTag kWorkerScriptLoadTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("worker_script_load",
                                        R"(
      semantics {
        sender: "Web Worker Script Load"
        description:
          "This request is issued by Web Worker to fetch its main script."
        trigger:
          "Calling new Worker() or SharedWorker()."
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

void WorkerScriptFetcher::CreateAndStart(
    std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
    std::unique_ptr<network::ResourceRequest> resource_request,
    CreateAndStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This fetcher will delete itself. See the class level comment.
  (new WorkerScriptFetcher(std::move(script_loader_factory),
                           std::move(resource_request), std::move(callback)))
      ->Start(std::move(throttles));
}

WorkerScriptFetcher::WorkerScriptFetcher(
    std::unique_ptr<WorkerScriptLoaderFactory> script_loader_factory,
    std::unique_ptr<network::ResourceRequest> resource_request,
    CreateAndStartCallback callback)
    : script_loader_factory_(std::move(script_loader_factory)),
      request_id_(GlobalRequestID::MakeBrowserInitiated().request_id),
      resource_request_(std::move(resource_request)),
      callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WorkerScriptFetcher::~WorkerScriptFetcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void WorkerScriptFetcher::Start(
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto shared_url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          script_loader_factory_.get());

  url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      std::move(shared_url_loader_factory), std::move(throttles), request_id_,
      network::mojom::kURLLoadOptionNone, resource_request_.get(), this,
      kWorkerScriptLoadTrafficAnnotation, base::ThreadTaskRunnerHandle::Get());
}

void WorkerScriptFetcher::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void WorkerScriptFetcher::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  response_head_ = std::move(response_head);
}

void WorkerScriptFetcher::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::WeakPtr<WorkerScriptLoader> script_loader =
      script_loader_factory_->GetScriptLoader();
  if (script_loader && script_loader->default_loader_used_) {
    // If the default network loader was used to handle the URL load request we
    // need to see if the request interceptors want to potentially create a new
    // loader for the response, e.g. AppCache's fallback.
    DCHECK(!response_url_loader_);
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        response_client_receiver;
    if (script_loader->MaybeCreateLoaderForResponse(
            &response_head_, &response_body, &response_url_loader_,
            &response_client_receiver, url_loader_.get())) {
      DCHECK(response_url_loader_);
      response_url_loader_receiver_.Bind(std::move(response_client_receiver));
      subresource_loader_params_ = script_loader->TakeSubresourceLoaderParams();
      url_loader_.reset();
      // OnReceiveResponse() will be called again.
      return;
    }
  }

  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params =
      blink::mojom::WorkerMainScriptLoadParams::New();
  main_script_load_params->request_id = request_id_;
  main_script_load_params->response_head = std::move(response_head_);
  main_script_load_params->response_body = std::move(response_body);
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
    DCHECK(response_url_loader_receiver_.is_bound());
    main_script_load_params->url_loader_client_endpoints =
        network::mojom::URLLoaderClientEndpoints::New(
            std::move(response_url_loader_),
            response_url_loader_receiver_.Unbind());
  }

  main_script_load_params->redirect_infos = std::move(redirect_infos_);
  main_script_load_params->redirect_response_heads =
      std::move(redirect_response_heads_);

  std::move(callback_).Run(std::move(main_script_load_params),
                           std::move(subresource_loader_params_),
                           true /* success */);
  delete this;
}

void WorkerScriptFetcher::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  redirect_infos_.push_back(redirect_info);
  redirect_response_heads_.push_back(std::move(response_head));
  url_loader_->FollowRedirect({}, /* removed_headers */
                              {}, /* modified_headers */
                              {} /* modified_cors_exempt_headers */);
}

void WorkerScriptFetcher::OnUploadProgress(int64_t current_position,
                                           int64_t total_size,
                                           OnUploadProgressCallback callback) {
  NOTREACHED();
}

void WorkerScriptFetcher::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  NOTREACHED();
}

void WorkerScriptFetcher::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  NOTREACHED();
}

void WorkerScriptFetcher::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We can reach here only when loading fails before receiving a response_head.
  DCHECK_NE(net::OK, status.error_code);
  std::move(callback_).Run(nullptr /* main_script_load_params */,
                           absl::nullopt /* subresource_loader_params */,
                           false /* success */);
  delete this;
}

}  // namespace content

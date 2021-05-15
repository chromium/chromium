// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_subresource_url_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

namespace {

// URLLoader implementation that utilizes either a network loader
// or an appcache loader depending on where the resources should
// be loaded from. This class binds to the remote client in the
// renderer and internally creates one or the other kind of loader.
// The URLLoader and URLLoaderClient interfaces are proxied between
// the remote consumer and the chosen internal loader.
//
// This class owns and scopes the lifetime of the AppCacheRequestHandler
// for the duration of a subresource load.
class SubresourceLoader : public network::mojom::URLLoader,
                          public network::mojom::URLLoaderClient {
 public:
  SubresourceLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& annotation,
      base::WeakPtr<AppCacheHost> appcache_host,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory)
      : remote_receiver_(this, std::move(url_loader_receiver)),
        remote_client_(std::move(client)),
        request_(request),
        request_id_(request_id),
        options_(options),
        traffic_annotation_(annotation),
        network_loader_factory_(std::move(network_loader_factory)),
        host_(appcache_host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    remote_receiver_.set_disconnect_handler(base::BindOnce(
        &SubresourceLoader::OnMojoDisconnect, base::Unretained(this)));
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SubresourceLoader::Start, weak_factory_.GetWeakPtr()));
  }

 private:
  ~SubresourceLoader() override = default;

  void OnMojoDisconnect() { delete this; }

  void Start() {
    if (!host_) {
      remote_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }
    handler_ = host_->CreateRequestHandler(
        std::make_unique<AppCacheRequest>(request_), request_.destination,
        request_.should_reset_appcache, FrameTreeNode::kFrameTreeNodeInvalidId);
    if (!handler_) {
      CreateAndStartNetworkLoader();
      return;
    }
    handler_->MaybeCreateSubresourceLoader(
        request_, base::BindOnce(&SubresourceLoader::ContinueStart,
                                 weak_factory_.GetWeakPtr()));
  }

  void ContinueStart(SingleRequestURLLoaderFactory::RequestHandler handler) {
    if (handler)
      CreateAndStartAppCacheLoader(std::move(handler));
    else
      CreateAndStartNetworkLoader();
  }

  void CreateAndStartAppCacheLoader(
      SingleRequestURLLoaderFactory::RequestHandler handler) {
    DCHECK(!appcache_loader_) << "only expected to be called onced";
    DCHECK(handler);

    // Disconnect from the network loader first.
    local_client_receiver_.reset();
    network_loader_.reset();

    std::move(handler).Run(request_,
                           appcache_loader_.BindNewPipeAndPassReceiver(),
                           local_client_receiver_.BindNewPipeAndPassRemote());
  }

  void CreateAndStartNetworkLoader() {
    DCHECK(!appcache_loader_);
    network_loader_factory_->CreateLoaderAndStart(
        network_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
        request_, local_client_receiver_.BindNewPipeAndPassRemote(),
        traffic_annotation_);
    if (has_set_priority_)
      network_loader_->SetPriority(priority_, intra_priority_value_);
    if (has_paused_reading_)
      network_loader_->PauseReadingBodyFromNet();
  }

  // network::mojom::URLLoader implementation
  // Called by the remote client in the renderer.
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {
    DCHECK(modified_headers.IsEmpty() && modified_cors_exempt_headers.IsEmpty())
        << "Redirect with modified headers was not supported yet. "
           "crbug.com/845683";
    if (!handler_) {
      network_loader_->FollowRedirect(
          removed_headers, {} /* modified_headers */,
          {} /* modified_cors_exempt_headers */, absl::nullopt /* new_url */);
      return;
    }
    DCHECK(network_loader_);
    DCHECK(!appcache_loader_);
    handler_->MaybeFollowSubresourceRedirect(
        redirect_info_,
        base::BindOnce(&SubresourceLoader::ContinueFollowRedirect,
                       weak_factory_.GetWeakPtr()));
  }

  // network::mojom::URLLoader implementation
  void ContinueFollowRedirect(
      SingleRequestURLLoaderFactory::RequestHandler handler) {
    if (handler) {
      CreateAndStartAppCacheLoader(std::move(handler));
    } else {
      network_loader_->FollowRedirect(
          {} /* removed_headers */, {} /* modified_headers */,
          {} /* modified_cors_exempt_headers */, absl::nullopt /* new_url */);
    }
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    has_set_priority_ = true;
    priority_ = priority;
    intra_priority_value_ = intra_priority_value;
    if (network_loader_)
      network_loader_->SetPriority(priority, intra_priority_value);
  }

  void PauseReadingBodyFromNet() override {
    has_paused_reading_ = true;
    if (network_loader_)
      network_loader_->PauseReadingBodyFromNet();
  }

  void ResumeReadingBodyFromNet() override {
    has_paused_reading_ = false;
    if (network_loader_)
      network_loader_->ResumeReadingBodyFromNet();
  }

  // network::mojom::URLLoaderClient implementation
  // Called by either the appcache or network loader, whichever is in use.
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override {
    // Don't MaybeFallback for appcache produced responses.
    if (appcache_loader_ || !handler_) {
      remote_client_->OnReceiveResponse(std::move(response_head));
      return;
    }

    did_receive_network_response_ = true;
    auto response_head_clone = response_head.Clone();
    handler_->MaybeFallbackForSubresourceResponse(
        std::move(response_head),
        base::BindOnce(&SubresourceLoader::ContinueOnReceiveResponse,
                       weak_factory_.GetWeakPtr(),
                       std::move(response_head_clone)));
  }

  void ContinueOnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      SingleRequestURLLoaderFactory::RequestHandler handler) {
    if (handler) {
      CreateAndStartAppCacheLoader(std::move(handler));
    } else {
      remote_client_->OnReceiveResponse(std::move(response_head));
    }
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    DCHECK(network_loader_) << "appcache loader does not produce redirects";
    if (!redirect_limit_--) {
      OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
      return;
    }
    if (!handler_) {
      remote_client_->OnReceiveRedirect(redirect_info_,
                                        std::move(response_head));
      return;
    }
    redirect_info_ = redirect_info;
    handler_->MaybeFallbackForSubresourceRedirect(
        redirect_info,
        base::BindOnce(&SubresourceLoader::ContinueOnReceiveRedirect,
                       weak_factory_.GetWeakPtr(), std::move(response_head)));
  }

  void ContinueOnReceiveRedirect(
      network::mojom::URLResponseHeadPtr response_head,
      SingleRequestURLLoaderFactory::RequestHandler handler) {
    if (handler) {
      CreateAndStartAppCacheLoader(std::move(handler));
    } else {
      remote_client_->OnReceiveRedirect(redirect_info_,
                                        std::move(response_head));
    }
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    remote_client_->OnUploadProgress(current_position, total_size,
                                     std::move(ack_callback));
  }

  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override {
    remote_client_->OnReceiveCachedMetadata(std::move(data));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    remote_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    remote_client_->OnStartLoadingResponseBody(std::move(body));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (!network_loader_ || !handler_ || did_receive_network_response_ ||
        status.error_code == net::OK) {
      remote_client_->OnComplete(status);
      return;
    }
    handler_->MaybeFallbackForSubresourceResponse(
        network::mojom::URLResponseHead::New(),
        base::BindOnce(&SubresourceLoader::ContinueOnComplete,
                       weak_factory_.GetWeakPtr(), status));
  }

  void ContinueOnComplete(
      const network::URLLoaderCompletionStatus& status,
      SingleRequestURLLoaderFactory::RequestHandler handler) {
    if (handler)
      CreateAndStartAppCacheLoader(std::move(handler));
    else
      remote_client_->OnComplete(status);
  }

  // The receiver and remote client associated with the renderer.
  mojo::Receiver<network::mojom::URLLoader> remote_receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> remote_client_;

  network::ResourceRequest request_;
  int32_t request_id_;
  uint32_t options_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;
  net::RedirectInfo redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;
  bool did_receive_network_response_ = false;
  bool has_paused_reading_ = false;
  bool has_set_priority_ = false;
  net::RequestPriority priority_;
  int32_t intra_priority_value_;

  // Core appcache logic that decides how to handle a request.
  std::unique_ptr<AppCacheRequestHandler> handler_;

  // The local receiver to either our network or appcache loader,
  // we only use one of them at any given time.
  mojo::Receiver<network::mojom::URLLoaderClient> local_client_receiver_{this};
  mojo::Remote<network::mojom::URLLoader> network_loader_;
  mojo::Remote<network::mojom::URLLoader> appcache_loader_;

  base::WeakPtr<AppCacheHost> host_;

  base::WeakPtrFactory<SubresourceLoader> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SubresourceLoader);
};

}  // namespace

// Implements the URLLoaderFactory mojom for AppCache requests.
AppCacheSubresourceURLFactory::AppCacheSubresourceURLFactory(
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    base::WeakPtr<AppCacheHost> host)
    : network_loader_factory_(std::move(network_loader_factory)),
      appcache_host_(host) {
  receivers_.set_disconnect_handler(
      base::BindRepeating(&AppCacheSubresourceURLFactory::OnMojoDisconnect,
                          base::Unretained(this)));
}

AppCacheSubresourceURLFactory::~AppCacheSubresourceURLFactory() = default;

// static
bool AppCacheSubresourceURLFactory::CreateURLLoaderFactory(
    base::WeakPtr<AppCacheHost> host,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        loader_factory_receiver) {
  DCHECK(host.get());
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory;
  // The partition has shutdown, return without binding |loader_factory|.
  if (!host->service()->partition())
    return false;
  network_loader_factory =
      host->service()
          ->partition()
          ->GetURLLoaderFactoryForBrowserProcessWithCORBEnabled();
  // This instance is effectively reference counted by the number of pipes open
  // to it and will get deleted when all clients drop their connections.
  // Please see OnMojoDisconnect() for details.
  auto* impl = new AppCacheSubresourceURLFactory(
      std::move(network_loader_factory), host);
  impl->Clone(std::move(loader_factory_receiver));

  // Save the factory in the host to ensure that we don't create it again when
  // the cache is selected, etc.
  host->SetAppCacheSubresourceFactory(impl);
  return true;
}

void AppCacheSubresourceURLFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (request.request_initiator.has_value() && appcache_host_ &&
      !appcache_host_->security_policy_handle()->CanAccessDataForOrigin(
          request.request_initiator.value())) {
    static auto* initiator_origin_key = base::debug::AllocateCrashKeyString(
        "initiator_origin", base::debug::CrashKeySize::Size64);
    base::debug::SetCrashKeyString(
        initiator_origin_key, request.request_initiator.value().Serialize());

    mojo::ReportBadMessage(
        "APPCACHE_SUBRESOURCE_URL_FACTORY_INVALID_INITIATOR");
    return;
  }

  // Subresource requests from renderer processes should not be allowed to use
  // network::mojom::FetchRequestMode::kNavigate.
  if (request.mode == network::mojom::RequestMode::kNavigate) {
    mojo::ReportBadMessage("APPCACHE_SUBRESOURCE_URL_FACTORY_NAVIGATE");
    return;
  }

  new SubresourceLoader(std::move(url_loader_receiver), request_id, options,
                        request, std::move(client), traffic_annotation,
                        appcache_host_, network_loader_factory_);
}

void AppCacheSubresourceURLFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

base::WeakPtr<AppCacheSubresourceURLFactory>
AppCacheSubresourceURLFactory::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppCacheSubresourceURLFactory::OnMojoDisconnect() {
  if (receivers_.empty())
    delete this;
}

}  // namespace content

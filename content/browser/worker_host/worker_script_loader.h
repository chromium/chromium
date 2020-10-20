// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/public/browser/service_worker_client_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace blink {
class ThrottlingURLLoader;
}  // namespace blink

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class AppCacheHost;
class BrowserContext;
class NavigationLoaderInterceptor;
class ResourceContext;
class ServiceWorkerMainResourceHandle;

// The URLLoader for loading a shared worker script. Only used for the main
// script request.
//
// This acts much like NavigationURLLoaderImpl. It allows a
// NavigationLoaderInterceptor to intercept the request with its own loader, and
// goes to |default_loader_factory| otherwise. Once a loader is started, this
// class acts as the URLLoaderClient for it, forwarding messages to the outer
// client. On redirects, it starts over with the new request URL, possibly
// starting a new loader and becoming the client of that.
//
// Lives on the UI thread.
class WorkerScriptLoader : public network::mojom::URLLoader,
                           public network::mojom::URLLoaderClient {
 public:
  // Returns the resource context, or nullptr during shutdown. Must be called on
  // the IO thread.
  using ResourceContextGetter = base::RepeatingCallback<ResourceContext*(void)>;

  // Returns the browser context, or nullptr during shutdown. Must be called on
  // the UI thread.
  using BrowserContextGetter = base::RepeatingCallback<BrowserContext*(void)>;

  // |default_loader_factory| is used to load the script if the load is not
  // intercepted by a feature like service worker. Typically it will load the
  // script from the NetworkService. However, it may internally contain
  // non-NetworkService factories used for non-http(s) URLs, e.g., a
  // chrome-extension:// URL.
  WorkerScriptLoader(
      int process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      base::WeakPtr<ServiceWorkerMainResourceHandle> service_worker_handle,
      base::WeakPtr<AppCacheHost> appcache_host,
      const BrowserContextGetter& browser_context_getter,
      scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);
  ~WorkerScriptLoader() override;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient:
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // Returns a URLLoader client endpoint if an interceptor wants to handle the
  // response, i.e. return a different response. For e.g. AppCache may have
  // fallback content.
  bool MaybeCreateLoaderForResponse(
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* response_url_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>*
          response_client_receiver,
      blink::ThrottlingURLLoader* url_loader);

  base::Optional<SubresourceLoaderParams> TakeSubresourceLoaderParams() {
    return std::move(subresource_loader_params_);
  }

  base::WeakPtr<WorkerScriptLoader> GetWeakPtr();

  // Set to true if the default URLLoader (network service) was used for the
  // current request.
  bool default_loader_used_ = false;

 private:
  void Abort();
  void Start();
  void MaybeStartLoader(
      NavigationLoaderInterceptor* interceptor,
      scoped_refptr<network::SharedURLLoaderFactory> single_request_factory);
  void LoadFromNetwork(bool reset_subresource_loader_params);
  void CommitCompleted(const network::URLLoaderCompletionStatus& status);

  // The order of the interceptors is important. The former interceptor can
  // preferentially get a chance to intercept a network request.
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors_;
  size_t interceptor_index_ = 0;

  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  const int32_t routing_id_;
  const int32_t request_id_;
  const uint32_t options_;
  network::ResourceRequest resource_request_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  base::WeakPtr<ServiceWorkerMainResourceHandle> service_worker_handle_;
  BrowserContextGetter browser_context_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  base::Optional<net::RedirectInfo> redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;

  mojo::Remote<network::mojom::URLLoader> url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_client_receiver_{
      this};
  // The factory used to request the script. This is the same as
  // |default_loader_factory_| if a service worker or other interceptor didn't
  // elect to handle the request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  bool completed_ = false;

  base::WeakPtrFactory<WorkerScriptLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WorkerScriptLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_LOADER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_LOADER_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_LOADER_H_

#include "base/macros.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "content/common/single_request_url_loader_factory.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class AppCacheHost;
class ThrottlingURLLoader;
class NavigationLoaderInterceptor;
class ResourceContext;
class ServiceWorkerProviderHost;

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
// Lives on the IO thread.
class SharedWorkerScriptLoader : public network::mojom::URLLoader,
                                 public network::mojom::URLLoaderClient {
 public:
  // |default_loader_factory| is used to load the script if the load is not
  // intercepted by a feature like service worker. Typically it will load the
  // script from the NetworkService. However, it may internally contain
  // non-NetworkService factories used for non-http(s) URLs, e.g., a
  // chrome-extension:// URL.
  SharedWorkerScriptLoader(
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
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);
  ~SharedWorkerScriptLoader() override;

  // network::mojom::URLLoader:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient:
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // Returns a URLLoader client endpoint if an interceptor wants to handle the
  // response, i.e. return a different response. For e.g. AppCache may have
  // fallback content.
  bool MaybeCreateLoaderForResponse(
      const network::ResourceResponseHead& response,
      network::mojom::URLLoaderPtr* response_url_loader,
      network::mojom::URLLoaderClientRequest* response_client_request,
      ThrottlingURLLoader* url_loader);

  base::Optional<SubresourceLoaderParams> TakeSubresourceLoaderParams() {
    return std::move(subresource_loader_params_);
  }

  base::WeakPtr<SharedWorkerScriptLoader> GetWeakPtr();

  // Set to true if the default URLLoader (network service) was used for the
  // current request.
  bool default_loader_used_ = false;

 private:
  void Start();
  void MaybeStartLoader(
      NavigationLoaderInterceptor* interceptor,
      SingleRequestURLLoaderFactory::RequestHandler single_request_handler);
  void LoadFromNetwork(bool reset_subresource_loader_params);

  // The order of the interceptors is important. The former interceptor can
  // preferentially get a chance to intercept a network request.
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors_;
  size_t interceptor_index_ = 0;

  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  const int process_id_;
  const int32_t routing_id_;
  const int32_t request_id_;
  const uint32_t options_;
  network::ResourceRequest resource_request_;
  network::mojom::URLLoaderClientPtr client_;
  base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host_;
  ResourceContext* resource_context_;
  scoped_refptr<network::SharedURLLoaderFactory> default_loader_factory_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  base::Optional<net::RedirectInfo> redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;

  network::mojom::URLLoaderPtr url_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> url_loader_client_binding_;
  // The factory used to request the script. This is the same as
  // |default_loader_factory_| if a service worker or other interceptor didn't
  // elect to handle the request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<SharedWorkerScriptLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerScriptLoader);
};

}  // namespace content
#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_LOADER_H_

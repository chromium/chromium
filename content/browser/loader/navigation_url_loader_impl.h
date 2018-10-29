// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/ssl_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class NavigationData;
class NavigationLoaderInterceptor;
class ResourceContext;
class StoragePartition;
struct GlobalRequestID;

class CONTENT_EXPORT NavigationURLLoaderImpl : public NavigationURLLoader {
 public:
  // The caller is responsible for ensuring that |delegate| outlives the loader.
  // Note |initial_interceptors| is there for test purposes only.
  NavigationURLLoaderImpl(
      ResourceContext* resource_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerNavigationHandle* service_worker_handle,
      AppCacheNavigationHandle* appcache_handle,
      NavigationURLLoaderDelegate* delegate,
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
          initial_interceptors);
  ~NavigationURLLoaderImpl() override;

  // NavigationURLLoader implementation:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override;
  void ProceedWithResponse() override;

  void OnReceiveResponse(
      scoped_refptr<network::ResourceResponse> response,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<NavigationData> navigation_data,
      const GlobalRequestID& global_request_id,
      bool is_download,
      bool is_stream);
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         scoped_refptr<network::ResourceResponse> response);
  void OnComplete(const network::URLLoaderCompletionStatus& status);

  // Overrides loading of frame requests when the network service is disabled.
  // If the callback returns true, the frame request was intercepted. Otherwise
  // it should be loaded normally through ResourceDispatcherHost. Passing an
  // empty callback will restore the default behavior.
  // This method must be called either on the IO thread or before threads start.
  // This callback is run on the IO thread.
  using BeginNavigationInterceptor = base::RepeatingCallback<bool(
      network::mojom::URLLoaderRequest* request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      network::mojom::URLLoaderClientPtr* client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)>;
  static void SetBeginNavigationInterceptorForTesting(
      const BeginNavigationInterceptor& interceptor);

 private:
  class URLLoaderRequestController;
  void OnRequestStarted(base::TimeTicks timestamp);

  void BindNonNetworkURLLoaderFactoryRequest(
      int frame_tree_node_id,
      const GURL& url,
      network::mojom::URLLoaderFactoryRequest factory);

  NavigationURLLoaderDelegate* delegate_;

  // Lives on the IO thread.
  std::unique_ptr<URLLoaderRequestController> request_controller_;

  bool allow_download_;

  // Factories to handle navigation requests for non-network resources.
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap
      non_network_url_loader_factories_;

  base::WeakPtrFactory<NavigationURLLoaderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_

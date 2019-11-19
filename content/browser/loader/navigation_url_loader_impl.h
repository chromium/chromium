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
#include "content/common/navigation_params.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/previews_state.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class BrowserContext;
class NavigationLoaderInterceptor;
class PrefetchedSignedExchangeCache;
class StoragePartition;
class StoragePartitionImpl;
struct GlobalRequestID;

class CONTENT_EXPORT NavigationURLLoaderImpl : public NavigationURLLoader {
 public:
  // The caller is responsible for ensuring that |delegate| outlives the loader.
  // Note |initial_interceptors| is there for test purposes only.
  NavigationURLLoaderImpl(
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerNavigationHandle* service_worker_handle,
      AppCacheNavigationHandle* appcache_handle,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      NavigationURLLoaderDelegate* delegate,
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
          initial_interceptors);
  ~NavigationURLLoaderImpl() override;

  // NavigationURLLoader implementation:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      PreviewsState new_previews_state) override;

  void OnReceiveResponse(
      scoped_refptr<network::ResourceResponse> response_head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      mojo::ScopedDataPipeConsumerHandle response_body,
      const GlobalRequestID& global_request_id,
      bool is_download,
      base::TimeDelta total_ui_to_io_time,
      base::Time io_post_time);
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         scoped_refptr<network::ResourceResponse> response,
                         base::Time io_post_time);
  void OnComplete(const network::URLLoaderCompletionStatus& status);

  // Intercepts loading of frame requests when network service is enabled and
  // either a network::mojom::TrustedURLLoaderHeaderClient is being used or for
  // schemes not handled by network service (e.g. files). This must be called on
  // the UI thread or before threads start.
  using URLLoaderFactoryInterceptor = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* receiver)>;
  static void SetURLLoaderFactoryInterceptorForTesting(
      const URLLoaderFactoryInterceptor& interceptor);

  // Creates a URLLoaderFactory for a navigation. The factory uses
  // |header_client|. This should have the same settings as the factory from the
  // URLLoaderFactoryGetter. Called on the UI thread.
  static void CreateURLLoaderFactoryWithHeaderClient(
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          header_client,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      StoragePartitionImpl* partition);

  // Returns a Request ID for browser-initiated navigation requests. Called on
  // the IO thread.
  static GlobalRequestID MakeGlobalRequestID();

 private:
  class URLLoaderRequestController;
  void OnRequestStarted(base::TimeTicks timestamp);

  void BindNonNetworkURLLoaderFactoryReceiver(
      int frame_tree_node_id,
      const GURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  NavigationURLLoaderDelegate* delegate_;

  // Lives on the IO thread.
  std::unique_ptr<URLLoaderRequestController> request_controller_;

  NavigationDownloadPolicy download_policy_;

  // Factories to handle navigation requests for non-network resources.
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap
      non_network_url_loader_factories_;

  // Counts the time overhead of all the hops from the IO to the UI threads.
  base::TimeDelta io_to_ui_time_;

  base::WeakPtrFactory<NavigationURLLoaderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_H_

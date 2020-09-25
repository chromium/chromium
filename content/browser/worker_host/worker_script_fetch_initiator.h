// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCH_INITIATOR_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCH_INITIATOR_H_

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_client_info.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace blink {
class PendingURLLoaderFactoryBundle;
}  // namespace blink

namespace net {
class SiteForCookies;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class AppCacheHost;
class BrowserContext;
class RenderFrameHost;
class ServiceWorkerContextWrapper;
class ServiceWorkerMainResourceHandle;
class ServiceWorkerObjectHost;
class StoragePartitionImpl;
struct SubresourceLoaderParams;

// PlzWorker:
// WorkerScriptFetchInitiator is the entry point of browser-side script fetch
// for WorkerScriptFetcher.
// TODO(falken): These are all static functions, it should just be a namespace
// or merged elsewhere.
class CONTENT_EXPORT WorkerScriptFetchInitiator {
 public:
  using CompletionCallback = base::OnceCallback<void(
      bool success,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>,
      blink::mojom::WorkerMainScriptLoadParamsPtr,
      blink::mojom::ControllerServiceWorkerInfoPtr,
      base::WeakPtr<ServiceWorkerObjectHost>,
      const GURL& final_response_url)>;

  // Creates a worker script fetcher and starts it. Must be called on the UI
  // thread. |callback| will be called with the result on the UI thread.
  static void Start(
      int worker_process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      const GURL& initial_request_url,
      RenderFrameHost* creator_render_frame_host,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& request_initiator,
      const net::IsolationInfo& trusted_isolation_info,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::ResourceType resource_type,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      ServiceWorkerMainResourceHandle* service_worker_handle,
      base::WeakPtr<AppCacheHost> appcache_host,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_override,
      StoragePartitionImpl* storage_partition,
      const std::string& storage_domain,
      CompletionCallback callback);

  // Used for specifying how URLLoaderFactoryBundle is used.
  enum class LoaderType { kMainResource, kSubResource };

  // Creates a loader factory bundle. Must be called on the UI thread. For
  // nested workers, |creator_render_frame_host| can be null.
  static std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateFactoryBundle(LoaderType loader_type,
                      int worker_process_id,
                      StoragePartitionImpl* storage_partition,
                      const std::string& storage_domain,
                      bool file_support,
                      bool filesystem_url_support,
                      RenderFrameHost* creator_render_frame_host);

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkerScriptFetchInitiatorTest,
                           DetermineFinalResponseUrl);

  // Adds additional request headers to |resource_request|. Must be called on
  // the UI thread.
  static void AddAdditionalRequestHeaders(
      network::ResourceRequest* resource_request,
      BrowserContext* browser_context);

  static void CreateScriptLoader(
      int worker_process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      const GURL& initial_request_url,
      RenderFrameHost* creator_render_frame_host,
      const net::IsolationInfo& trusted_isolation_info,
      std::unique_ptr<network::ResourceRequest> resource_request,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          factory_bundle_for_browser_info,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      ServiceWorkerMainResourceHandle* service_worker_handle,
      base::WeakPtr<AppCacheHost> appcache_host,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_override,
      CompletionCallback callback);

  static void DidCreateScriptLoader(
      CompletionCallback callback,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      const GURL& initial_request_url,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      bool success);

  // Calculate the final response URL from the redirect chain, URLs fetched by
  // the service worker and the initial request URL. The logic is mostly based
  // on what blink::ResourceResponse::ResponseUrl() does.
  //
  // Exposed for testing.
  static GURL DetermineFinalResponseUrl(
      const GURL& initial_request_url,
      blink::mojom::WorkerMainScriptLoadParams* main_script_load_params);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCH_INITIATOR_H_

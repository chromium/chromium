// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_fetch_initiator.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/post_task.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/worker_script_fetcher.h"
#include "content/browser/worker_host/worker_script_loader.h"
#include "content/browser/worker_host/worker_script_loader_factory.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "url/origin.h"

namespace content {

// static
void WorkerScriptFetchInitiator::Start(
    int worker_process_id,
    const GURL& script_url,
    RenderFrameHost* creator_render_frame_host,
    const url::Origin& request_initiator,
    const net::NetworkIsolationKey& trusted_network_isolation_key,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    ResourceType resource_type,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    ServiceWorkerNavigationHandle* service_worker_handle,
    base::WeakPtr<AppCacheHost> appcache_host,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_override,
    StoragePartitionImpl* storage_partition,
    const std::string& storage_domain,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(storage_partition);
  DCHECK(resource_type == ResourceType::kWorker ||
         resource_type == ResourceType::kSharedWorker)
      << static_cast<int>(resource_type);

  BrowserContext* browser_context = storage_partition->browser_context();
  ResourceContext* resource_context =
      browser_context ? browser_context->GetResourceContext() : nullptr;
  if (!browser_context || !resource_context) {
    // The browser is shutting down. Just drop this request.
    return;
  }

  bool constructor_uses_file_url =
      request_initiator.scheme() == url::kFileScheme;

  // TODO(https://crbug.com/987517): Filesystem URL support on shared workers
  // are now broken.
  bool filesystem_url_support = resource_type == ResourceType::kWorker;

  // Set up the factory bundle for non-NetworkService URLs, e.g.,
  // chrome-extension:// URLs. One factory bundle is consumed by the browser
  // for WorkerScriptLoaderFactory, and one is sent to the renderer for
  // subresource loading.
  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      factory_bundle_for_browser = CreateFactoryBundle(
          LoaderType::kMainResource, worker_process_id, storage_partition,
          storage_domain, constructor_uses_file_url, filesystem_url_support);
  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      subresource_loader_factories = CreateFactoryBundle(
          LoaderType::kSubResource, worker_process_id, storage_partition,
          storage_domain, constructor_uses_file_url, filesystem_url_support);

  // Create a resource request for initiating worker script fetch from the
  // browser process.
  std::unique_ptr<network::ResourceRequest> resource_request;

  // Determine the referrer for the worker script request based on the spec.
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  Referrer sanitized_referrer = Referrer::SanitizeForRequest(
      script_url,
      Referrer(outside_fetch_client_settings_object->outgoing_referrer,
               outside_fetch_client_settings_object->referrer_policy));

  resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = script_url;
  resource_request->site_for_cookies = script_url;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->network_isolation_key =
      trusted_network_isolation_key;
  resource_request->request_initiator = request_initiator;
  resource_request->referrer = sanitized_referrer.url,
  resource_request->referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
      outside_fetch_client_settings_object->referrer_policy);
  resource_request->resource_type = static_cast<int>(resource_type);
  resource_request->credentials_mode = credentials_mode;
  if (creator_render_frame_host) {
    resource_request->render_frame_id =
        creator_render_frame_host->GetRoutingID();
  }

  // For a classic worker script request:
  // https://html.spec.whatwg.org/C/#fetch-a-classic-worker-script
  // Step 1: "Let request be a new request whose ..., mode is "same-origin",
  // ..."
  //
  // For a module worker script request:
  // https://html.spec.whatwg.org/C/#fetch-a-single-module-script
  // Step 6: "If destination is "worker" or "sharedworker" and the top-level
  // module fetch flag is set, then set request's mode to "same-origin"."
  resource_request->mode = network::mojom::RequestMode::kSameOrigin;

  switch (resource_type) {
    case ResourceType::kWorker:
      resource_request->fetch_request_context_type =
          static_cast<int>(blink::mojom::RequestContextType::WORKER);
      break;
    case ResourceType::kSharedWorker:
      resource_request->fetch_request_context_type =
          static_cast<int>(blink::mojom::RequestContextType::SHARED_WORKER);
      break;
    default:
      NOTREACHED() << static_cast<int>(resource_type);
      break;
  }

  // Upgrade the request to an a priori authenticated URL, if appropriate.
  // https://w3c.github.io/webappsec-upgrade-insecure-requests/#upgrade-request
  resource_request->upgrade_if_insecure =
      outside_fetch_client_settings_object->insecure_requests_policy ==
      blink::mojom::InsecureRequestsPolicy::kUpgrade;

  AddAdditionalRequestHeaders(resource_request.get(), browser_context);

  CreateScriptLoader(
      worker_process_id, creator_render_frame_host, std::move(resource_request),
      storage_partition, std::move(factory_bundle_for_browser),
      std::move(subresource_loader_factories),
      std::move(service_worker_context), service_worker_handle,
      std::move(appcache_host), std::move(blob_url_loader_factory),
      std::move(url_loader_factory_override), std::move(callback));
}

std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
WorkerScriptFetchInitiator::CreateFactoryBundle(
    LoaderType loader_type,
    int worker_process_id,
    StoragePartitionImpl* storage_partition,
    const std::string& storage_domain,
    bool file_support,
    bool filesystem_url_support) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
  non_network_factories[url::kDataScheme] =
      std::make_unique<DataURLLoaderFactory>();
  if (filesystem_url_support) {
    // TODO(https://crbug.com/986188): Pass ChildProcessHost::kInvalidUniqueID
    // instead of valid |worker_process_id| for |factory_bundle_for_browser|
    // once CanCommitURL-like check is implemented in PlzWorker.
    non_network_factories[url::kFileSystemScheme] =
        CreateFileSystemURLLoaderFactory(
            worker_process_id, RenderFrameHost::kNoFrameTreeNodeId,
            storage_partition->GetFileSystemContext(), storage_domain);
  }

  switch (loader_type) {
    case LoaderType::kMainResource:
      GetContentClient()
          ->browser()
          ->RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
              storage_partition->browser_context(), &non_network_factories);
      break;
    case LoaderType::kSubResource:
      GetContentClient()
          ->browser()
          ->RegisterNonNetworkSubresourceURLLoaderFactories(
              worker_process_id, MSG_ROUTING_NONE, &non_network_factories);
      break;
  }

  auto factory_bundle = std::make_unique<blink::URLLoaderFactoryBundleInfo>();
  for (auto& pair : non_network_factories) {
    const std::string& scheme = pair.first;
    std::unique_ptr<network::mojom::URLLoaderFactory> factory =
        std::move(pair.second);

    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(factory), factory_remote.InitWithNewPipeAndPassReceiver());
    factory_bundle->pending_scheme_specific_factories().emplace(
        scheme, std::move(factory_remote));
  }

  if (file_support) {
    auto file_factory = std::make_unique<FileURLLoaderFactory>(
        storage_partition->browser_context()->GetPath(),
        storage_partition->browser_context()->GetSharedCorsOriginAccessList(),
        // USER_VISIBLE because worker script fetch may affect the UI.
        base::TaskPriority::USER_VISIBLE);
    mojo::PendingRemote<network::mojom::URLLoaderFactory> file_factory_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(file_factory),
        file_factory_remote.InitWithNewPipeAndPassReceiver());
    factory_bundle->pending_scheme_specific_factories().emplace(
        url::kFileScheme, std::move(file_factory_remote));
  }

  return factory_bundle;
}

// TODO(nhiroki): Align this function with AddAdditionalRequestHeaders() in
// navigation_request.cc, FrameFetchContext, and WorkerFetchContext.
void WorkerScriptFetchInitiator::AddAdditionalRequestHeaders(
    network::ResourceRequest* resource_request,
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(nhiroki): Return early when the request is neither HTTP nor HTTPS
  // (i.e., Blob URL or Data URL). This should be checked by
  // SchemeIsHTTPOrHTTPS(), but currently cross-origin workers on extensions
  // are allowed and the check doesn't work well. See https://crbug.com/867302.

  // Set the "Accept" header.
  resource_request->headers.SetHeaderIfMissing(network::kAcceptHeader,
                                               network::kDefaultAcceptHeader);

  blink::mojom::RendererPreferences renderer_preferences;
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      browser_context, &renderer_preferences);
  UpdateAdditionalHeadersForBrowserInitiatedRequest(
      &resource_request->headers, browser_context,
      /*should_update_existing_headers=*/false, renderer_preferences);

  SetFetchMetadataHeadersForBrowserInitiatedRequest(resource_request);
}

void WorkerScriptFetchInitiator::CreateScriptLoader(
    int worker_process_id,
    RenderFrameHost* creator_render_frame_host,
    std::unique_ptr<network::ResourceRequest> resource_request,
    StoragePartitionImpl* storage_partition,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        factory_bundle_for_browser_info,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    ServiceWorkerNavigationHandle* service_worker_handle,
    base::WeakPtr<AppCacheHost> appcache_host,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_override,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Create the URL loader factory for WorkerScriptLoaderFactory to use to load
  // the main script.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (blob_url_loader_factory) {
    // If we have a blob_url_loader_factory just use that directly rather than
    // creating a new URLLoaderFactoryBundle.
    url_loader_factory = std::move(blob_url_loader_factory);
  } else if (url_loader_factory_override) {
    // For unit tests.
    url_loader_factory = std::move(url_loader_factory_override);
  } else {
    // Add the default factory to the bundle for browser.
    DCHECK(factory_bundle_for_browser_info);

    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver =
            factory_bundle_for_browser_info->pending_default_factory()
                .InitWithNewPipeAndPassReceiver();
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        default_header_client;
    bool bypass_redirect_checks = false;
    GetContentClient()->browser()->WillCreateURLLoaderFactory(
        storage_partition->browser_context(), creator_render_frame_host,
        worker_process_id,
        ContentBrowserClient::URLLoaderFactoryType::kWorkerMainResource,
        *resource_request->request_initiator, &default_factory_receiver,
        &default_header_client, &bypass_redirect_checks);
    factory_bundle_for_browser_info->set_bypass_redirect_checks(
        bypass_redirect_checks);

    // TODO(nhiroki): Call
    // devtools_instrumentation::WillCreateURLLoaderFactory() here.

    // Get the direct network factory. This doesn't support reconnection to the
    // network service after a crash, but it's OK since it's used only for a
    // single request to fetch the worker's main script during startup. If the
    // network service crashes, worker startup should simply fail.
    auto network_factory =
        storage_partition->GetURLLoaderFactoryForBrowserProcess();
    network_factory->Clone(std::move(default_factory_receiver));

    url_loader_factory = base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
        std::move(factory_bundle_for_browser_info));
  }

  // Start loading a web worker main script.
  // TODO(nhiroki): Figure out what we should do in |wc_getter| for loading web
  // worker's main script. Returning the WebContents of the closest ancestor's
  // frame is a possible option, but it doesn't work when a shared worker
  // creates a dedicated worker after the closest ancestor's frame is gone. The
  // frame tree node ID has the same issue.
  base::RepeatingCallback<WebContents*()> wc_getter =
      base::BindRepeating([]() -> WebContents* { return nullptr; });
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      GetContentClient()->browser()->CreateURLLoaderThrottles(
          *resource_request, storage_partition->browser_context(), wc_getter,
          nullptr /* navigation_ui_data */,
          RenderFrameHost::kNoFrameTreeNodeId);

  // Create a BrowserContext getter using |service_worker_context|.
  // This context is aware of shutdown and safely returns a nullptr
  // instead of a destroyed BrowserContext in that case.
  auto browser_context_getter =
      base::BindRepeating(&ServiceWorkerContextWrapper::browser_context,
                          std::move(service_worker_context));

  WorkerScriptFetcher::CreateAndStart(
      std::make_unique<WorkerScriptLoaderFactory>(
          worker_process_id, service_worker_handle, std::move(appcache_host),
          browser_context_getter, std::move(url_loader_factory)),
      std::move(throttles), std::move(resource_request),
      base::BindOnce(WorkerScriptFetchInitiator::DidCreateScriptLoader,
                     std::move(callback),
                     std::move(subresource_loader_factories)));
}

void WorkerScriptFetchInitiator::DidCreateScriptLoader(
    CompletionCallback callback,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If a URLLoaderFactory for AppCache is supplied, use that.
  if (subresource_loader_params &&
      subresource_loader_params->pending_appcache_loader_factory) {
    subresource_loader_factories->pending_appcache_factory() =
        std::move(subresource_loader_params->pending_appcache_loader_factory);
  }

  // Prepare the controller service worker info to pass to the renderer.
  blink::mojom::ControllerServiceWorkerInfoPtr controller;
  base::WeakPtr<ServiceWorkerObjectHost> controller_service_worker_object_host;
  if (subresource_loader_params &&
      subresource_loader_params->controller_service_worker_info) {
    controller =
        std::move(subresource_loader_params->controller_service_worker_info);
    controller_service_worker_object_host =
        subresource_loader_params->controller_service_worker_object_host;
  }

  std::move(callback).Run(
      std::move(subresource_loader_factories),
      std::move(main_script_load_params), std::move(controller),
      std::move(controller_service_worker_object_host), success);
}

}  // namespace content

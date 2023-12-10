// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_fetcher.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/worker_host/worker_script_loader.h"
#include "content/browser/worker_host/worker_script_loader_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/isolation_info.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

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
          URLBlocklist {
            URLBlocklist: { entries: '*' }
          }
        }
        chrome_policy {
          URLAllowlist {
            URLAllowlist { }
          }
        }
      }
)");

// TODO(nhiroki): Align this function with AddAdditionalRequestHeaders() in
// navigation_request.cc, FrameFetchContext, and WorkerFetchContext.
void AddAdditionalRequestHeaders(network::ResourceRequest* resource_request,
                                 BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(nhiroki): Return early when the request is neither HTTP nor HTTPS
  // (i.e., Blob URL or Data URL). This should be checked by
  // SchemeIsHTTPOrHTTPS(), but currently cross-origin workers on extensions
  // are allowed and the check doesn't work well. See https://crbug.com/867302.

  // Set the "Accept" header.
  resource_request->headers.SetHeaderIfMissing(
      net::HttpRequestHeaders::kAccept, network::kDefaultAcceptHeaderValue);

  blink::RendererPreferences renderer_preferences;
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      browser_context, &renderer_preferences);
  UpdateAdditionalHeadersForBrowserInitiatedRequest(
      &resource_request->headers, browser_context,
      /*should_update_existing_headers=*/false, renderer_preferences,
      /*is_for_worker_script=*/true);
}

void DidCreateScriptLoader(
    WorkerScriptFetcher::CompletionCallback callback,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    const network::mojom::ClientSecurityStatePtr& client_security_state,
    absl::optional<GlobalRenderFrameHostId> ancestor_render_frame_host_id,
    const GURL& initial_request_url,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    absl::optional<SubresourceLoaderParams> subresource_loader_params,
    const network::URLLoaderCompletionStatus* completion_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(main_script_load_params.is_null(), completion_status == nullptr);
  DCHECK(!(main_script_load_params.is_null() &&
           subresource_loader_params.has_value()));

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

  // Figure out the final response URL.
  GURL final_response_url;
  network::mojom::IPAddressSpace response_address_space =
      network::mojom::IPAddressSpace::kUnknown;

  // The load succeeded iff `main_script_load_params` is not nullptr.
  if (main_script_load_params) {
    final_response_url = WorkerScriptFetcher::DetermineFinalResponseUrl(
        initial_request_url, main_script_load_params.get());

    response_address_space = network::CalculateResourceAddressSpace(
        final_response_url,
        main_script_load_params->response_head->remote_endpoint);
  } else if (completion_status->cors_error_status) {
    response_address_space =
        completion_status->cors_error_status->resource_address_space;
    if (response_address_space == network::mojom::IPAddressSpace::kUnknown) {
      response_address_space =
          completion_status->cors_error_status->target_address_space;
    }
  }

  if (client_security_state && ancestor_render_frame_host_id) {
    // Attempt to log the private network access on the ancestor RFHI, if still
    // alive. There is no content/ API to log a `WebFeature` use for a worker.
    auto* ancestor_render_frame_host =
        RenderFrameHostImpl::FromID(*ancestor_render_frame_host_id);
    if (ancestor_render_frame_host &&
        network::IsLessPublicAddressSpace(
            response_address_space, client_security_state->ip_address_space)) {
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          ancestor_render_frame_host,
          blink::mojom::WebFeature::kPrivateNetworkAccessFetchedWorkerScript);
    }
  }

  std::move(callback).Run(
      std::move(subresource_loader_factories),
      std::move(main_script_load_params), std::move(controller),
      std::move(controller_service_worker_object_host), final_response_url);
}

bool ShouldCreateWebUILoader(RenderFrameHostImpl* creator_render_frame_host) {
  if (!creator_render_frame_host)
    return false;

  if (creator_render_frame_host->GetWebUI() == nullptr)
    return false;

  auto requesting_scheme =
      creator_render_frame_host->GetLastCommittedOrigin().scheme();
  if (requesting_scheme == kChromeUIScheme)
    return true;
  if (requesting_scheme == kChromeUIUntrustedScheme)
    return true;
  if (requesting_scheme == kChromeDevToolsScheme)
    return true;
  return false;
}

}  // namespace

void WorkerScriptFetcher::CreateAndStart(
    int worker_process_id,
    const DedicatedOrSharedWorkerToken& worker_token,
    const GURL& initial_request_url,
    RenderFrameHostImpl* ancestor_render_frame_host,
    RenderFrameHostImpl* creator_render_frame_host,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& request_initiator,
    const blink::StorageKey& request_initiator_storage_key,
    const net::IsolationInfo& trusted_isolation_info,
    network::mojom::ClientSecurityStatePtr client_security_state,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    network::mojom::RequestDestination request_destination,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    ServiceWorkerMainResourceHandle* service_worker_handle,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_override,
    StoragePartitionImpl* storage_partition,
    const std::string& storage_domain,
    ukm::SourceId worker_source_id,
    DevToolsAgentHostImpl* devtools_agent_host,
    const base::UnguessableToken& devtools_worker_token,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_security_state);
  DCHECK(storage_partition);
  DCHECK(devtools_agent_host);
  DCHECK(request_destination == network::mojom::RequestDestination::kWorker ||
         request_destination ==
             network::mojom::RequestDestination::kSharedWorker)
      << static_cast<int>(request_destination);

  BrowserContext* browser_context = storage_partition->browser_context();
  if (!browser_context || browser_context->ShutdownStarted()) {
    // The browser is shutting down. Just drop this request.
    return;
  }

  bool constructor_uses_file_url =
      request_initiator.scheme() == url::kFileScheme;

  // TODO(https://crbug.com/987517): Filesystem URL support on shared workers
  // are now broken.
  bool filesystem_url_support =
      request_destination == network::mojom::RequestDestination::kWorker;

  // Set up the factory bundle for non-NetworkService URLs, e.g.,
  // chrome-extension:// URLs. One factory bundle is consumed by the browser
  // for WorkerScriptLoaderFactory, and one is sent to the renderer for
  // subresource loading.
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      factory_bundle_for_browser = CreateFactoryBundle(
          LoaderType::kMainResource, worker_process_id, storage_partition,
          storage_domain, constructor_uses_file_url, filesystem_url_support,
          creator_render_frame_host, request_initiator_storage_key);
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories = CreateFactoryBundle(
          LoaderType::kSubResource, worker_process_id, storage_partition,
          storage_domain, constructor_uses_file_url, filesystem_url_support,
          creator_render_frame_host, request_initiator_storage_key);

  // Create a resource request for initiating worker script fetch from the
  // browser process.
  std::unique_ptr<network::ResourceRequest> resource_request;

  // Determine the referrer for the worker script request based on the spec.
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  Referrer sanitized_referrer = Referrer::SanitizeForRequest(
      initial_request_url,
      Referrer(outside_fetch_client_settings_object->outgoing_referrer,
               outside_fetch_client_settings_object->referrer_policy));

  resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = initial_request_url;
  resource_request->site_for_cookies = site_for_cookies;
  resource_request->request_initiator = request_initiator;
  resource_request->referrer = sanitized_referrer.url,
  resource_request->referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
      outside_fetch_client_settings_object->referrer_policy);
  resource_request->destination = request_destination;
  resource_request->credentials_mode = credentials_mode;
  // To be used for the first party context check.
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      ancestor_render_frame_host->GetStorageKey().ToPartialNetIsolationInfo();

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

  switch (request_destination) {
    case network::mojom::RequestDestination::kWorker:
      resource_request->resource_type =
          static_cast<int>(blink::mojom::ResourceType::kWorker);
      break;
    case network::mojom::RequestDestination::kSharedWorker:
      resource_request->resource_type =
          static_cast<int>(blink::mojom::ResourceType::kSharedWorker);
      break;
    default:
      NOTREACHED() << static_cast<int>(request_destination);
      break;
  }

  // Upgrade the request to an a priori authenticated URL, if appropriate.
  // https://w3c.github.io/webappsec-upgrade-insecure-requests/#upgrade-request
  resource_request->upgrade_if_insecure =
      outside_fetch_client_settings_object->insecure_requests_policy ==
      blink::mojom::InsecureRequestsPolicy::kUpgrade;

  AddAdditionalRequestHeaders(resource_request.get(), browser_context);

  // Notify that the request for fetching the main worker script is about to
  // start to DevTools. It fires `Network.onRequestWillBeSent` event. For
  // dedicated workers, `creator_render_frame_host` can be null when a worker
  // is nested. So reports to DevTools in the ancestor's frame instead. For
  // shared workers, `ancestor_render_frame_host` and
  // `creator_render_frame_host` are always same.
  devtools_instrumentation::OnWorkerMainScriptRequestWillBeSent(
      FrameTreeNode::From(ancestor_render_frame_host), devtools_worker_token,
      *resource_request);

  WorkerScriptFetcher::CreateScriptLoader(
      worker_process_id, worker_token, initial_request_url,
      ancestor_render_frame_host, creator_render_frame_host,
      trusted_isolation_info, std::move(client_security_state),
      std::move(resource_request), std::move(factory_bundle_for_browser),
      std::move(subresource_loader_factories),
      std::move(service_worker_context), service_worker_handle,
      std::move(blob_url_loader_factory),
      std::move(url_loader_factory_override), worker_source_id,
      devtools_agent_host, devtools_worker_token, std::move(callback));
}

void WorkerScriptFetcher::CreateScriptLoader(
    int worker_process_id,
    const DedicatedOrSharedWorkerToken& worker_token,
    const GURL& initial_request_url,
    RenderFrameHostImpl* ancestor_render_frame_host,
    RenderFrameHostImpl* creator_render_frame_host,
    const net::IsolationInfo& trusted_isolation_info,
    network::mojom::ClientSecurityStatePtr client_security_state,
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        factory_bundle_for_browser_info,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    ServiceWorkerMainResourceHandle* service_worker_handle,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_override,
    ukm::SourceId worker_source_id,
    DevToolsAgentHostImpl* devtools_agent_host,
    const base::UnguessableToken& devtools_worker_token,
    WorkerScriptFetcher::CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(devtools_agent_host);
  DCHECK(client_security_state);

  RenderProcessHost* factory_process =
      RenderProcessHost::FromID(worker_process_id);
  DCHECK(factory_process);  // Checked by callers of the Start method.

  BrowserContext* browser_context = factory_process->GetBrowserContext();
  DCHECK(browser_context);  // Checked in the Start method.

  // Do not enforce COEP on the main script fetch.
  client_security_state->cross_origin_embedder_policy =
      network::CrossOriginEmbedderPolicy();

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
    mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer;
    mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer;
    // If we have a |creator_render_frame_host| associate the load with that
    // RenderFrameHostImpl. Note that |factory_process| may be different than
    // the |creator_render_frame_host|'s RenderProcessHost.
    if (creator_render_frame_host) {
      url_loader_network_observer =
          factory_process->GetStoragePartition()
              ->CreateURLLoaderNetworkObserverForFrame(
                  creator_render_frame_host->GetProcess()->GetID(),
                  creator_render_frame_host->GetRoutingID());
      devtools_observer = NetworkServiceDevToolsObserver::MakeSelfOwned(
          creator_render_frame_host->GetDevToolsFrameToken().ToString());
    }

    const url::Origin& request_initiator = *resource_request->request_initiator;
    // TODO(https://crbug.com/1060837): Pass the Mojo remote which is connected
    // to the COEP reporter in DedicatedWorkerHost.
    network::mojom::URLLoaderFactoryParamsPtr factory_params =
        URLLoaderFactoryParamsHelper::CreateForWorker(
            factory_process, request_initiator, trusted_isolation_info,
            /*coep_reporter=*/mojo::NullRemote(),
            std::move(url_loader_network_observer),
            std::move(devtools_observer), client_security_state.Clone(),
            /*debug_tag=*/"CreateScriptLoader");
    // We are sure the URLLoaderFactory made with the param is only used within
    // `WorkerScriptFetcher` in the browser process. We can mark this trusted
    // safely.
    factory_params->is_trusted = true;

    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver =
            factory_bundle_for_browser_info->pending_default_factory()
                .InitWithNewPipeAndPassReceiver();
    bool bypass_redirect_checks = false;
    GetContentClient()->browser()->WillCreateURLLoaderFactory(
        browser_context, creator_render_frame_host, factory_process->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kWorkerMainResource,
        request_initiator,
        /*navigation_id=*/absl::nullopt,
        /* TODO(https://crbug.com/1103288): The UKM ID could be computed */
        ukm::kInvalidSourceIdObj, &default_factory_receiver,
        &factory_params->header_client, &bypass_redirect_checks,
        nullptr /* disable_secure_dns */, &factory_params->factory_override,
        /*navigation_response_task_runner=*/nullptr);
    factory_bundle_for_browser_info->set_bypass_redirect_checks(
        bypass_redirect_checks);

    devtools_instrumentation::WillCreateURLLoaderFactoryForWorkerMainScript(
        devtools_agent_host, devtools_worker_token,
        &factory_params->factory_override);
    factory_process->CreateURLLoaderFactory(std::move(default_factory_receiver),
                                            std::move(factory_params));

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
      CreateContentBrowserURLLoaderThrottles(
          *resource_request, browser_context, wc_getter,
          nullptr /* navigation_ui_data */,
          RenderFrameHost::kNoFrameTreeNodeId);

  // Create a BrowserContext getter using |service_worker_context|.
  // This context is aware of shutdown and safely returns a nullptr
  // instead of a destroyed BrowserContext in that case.
  auto browser_context_getter =
      base::BindRepeating(&ServiceWorkerContextWrapper::browser_context,
                          std::move(service_worker_context));

  absl::optional<GlobalRenderFrameHostId> ancestor_render_frame_host_id;
  if (ancestor_render_frame_host) {
    ancestor_render_frame_host_id = ancestor_render_frame_host->GetGlobalId();
  }

  // This fetcher will delete itself. See the class level comment.
  auto* script_fetcher = new WorkerScriptFetcher(
      std::make_unique<WorkerScriptLoaderFactory>(
          worker_process_id, worker_token, trusted_isolation_info,
          service_worker_handle, browser_context_getter,
          std::move(url_loader_factory), worker_source_id),
      std::move(resource_request),
      base::BindOnce(DidCreateScriptLoader, std::move(callback),
                     std::move(subresource_loader_factories),
                     std::move(client_security_state),
                     std::move(ancestor_render_frame_host_id),
                     initial_request_url));
  script_fetcher->Start(std::move(throttles));
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
WorkerScriptFetcher::CreateFactoryBundle(
    LoaderType loader_type,
    int worker_process_id,
    StoragePartitionImpl* storage_partition,
    const std::string& storage_domain,
    bool file_support,
    bool filesystem_url_support,
    RenderFrameHostImpl* creator_render_frame_host,
    const blink::StorageKey& request_initiator_storage_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
  non_network_factories.emplace(url::kDataScheme,
                                DataURLLoaderFactory::Create());
  if (filesystem_url_support) {
    // TODO(https://crbug.com/986188): Pass ChildProcessHost::kInvalidUniqueID
    // instead of valid `worker_process_id` for `factory_bundle_for_browser`
    // once CanCommitURL-like check is implemented in PlzWorker.
    non_network_factories.emplace(
        url::kFileSystemScheme,
        CreateFileSystemURLLoaderFactory(
            worker_process_id, RenderFrameHost::kNoFrameTreeNodeId,
            storage_partition->GetFileSystemContext(), storage_domain,
            request_initiator_storage_key));
  }
  if (file_support) {
    // USER_VISIBLE because worker script fetch may affect the UI.
    base::TaskPriority file_factory_priority = base::TaskPriority::USER_VISIBLE;
    non_network_factories.emplace(
        url::kFileScheme, FileURLLoaderFactory::Create(
                              storage_partition->browser_context()->GetPath(),
                              storage_partition->browser_context()
                                  ->GetSharedCorsOriginAccessList(),
                              file_factory_priority));
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
              worker_process_id, MSG_ROUTING_NONE,
              request_initiator_storage_key.origin(), &non_network_factories);
      break;
  }

  // Create WebUI loader for chrome://, chrome-untrusted://, or devtools://
  // workers from WebUI frames of the same scheme.
  if (ShouldCreateWebUILoader(creator_render_frame_host)) {
    auto requesting_scheme =
        creator_render_frame_host->GetLastCommittedOrigin().scheme();
    non_network_factories.emplace(
        requesting_scheme,
        CreateWebUIURLLoaderFactory(
            creator_render_frame_host, requesting_scheme,
            /*allowed_hosts=*/base::flat_set<std::string>()));
  }

  auto factory_bundle =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>();
  for (auto& pair : non_network_factories) {
    const std::string& scheme = pair.first;
    mojo::PendingRemote<network::mojom::URLLoaderFactory>& pending_remote =
        pair.second;
    factory_bundle->pending_scheme_specific_factories().emplace(
        scheme, std::move(pending_remote));
  }

  return factory_bundle;
}

GURL WorkerScriptFetcher::DetermineFinalResponseUrl(
    const GURL& initial_request_url,
    blink::mojom::WorkerMainScriptLoadParams* main_script_load_params) {
  DCHECK(main_script_load_params);

  network::mojom::URLResponseHead* url_response_head =
      main_script_load_params->response_head.get();

  // First check the URL list from the service worker.
  if (!url_response_head->url_list_via_service_worker.empty()) {
    DCHECK(url_response_head->was_fetched_via_service_worker);
    return url_response_head->url_list_via_service_worker.back();
  }

  // Then check the list of redirects.
  if (!main_script_load_params->redirect_infos.empty())
    return main_script_load_params->redirect_infos.back().new_url;

  // No redirection happened. The initial request URL was used for the response.
  return initial_request_url;
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
      kWorkerScriptLoadTrafficAnnotation,
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void WorkerScriptFetcher::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void WorkerScriptFetcher::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!cached_metadata);
  if (!body)
    return;

  CHECK(!main_script_load_params_);
  CHECK(url_loader_);
  main_script_load_params_ = blink::mojom::WorkerMainScriptLoadParams::New();
  main_script_load_params_->request_id = request_id_;
  main_script_load_params_->response_head = std::move(response_head);
  main_script_load_params_->response_body = std::move(body);
  // The main script was served by a request interceptor or the default
  // network loader.
  main_script_load_params_->url_loader_client_endpoints = url_loader_->Unbind();
  main_script_load_params_->redirect_infos = std::move(redirect_infos_);
  main_script_load_params_->redirect_response_heads =
      std::move(redirect_response_heads_);

  subresource_loader_params_ =
      script_loader_factory_->GetScriptLoader()->TakeSubresourceLoaderParams();

  // Currently `parsed_headers` is null when FileURLLoader is used.
  if (main_script_load_params_->response_head->parsed_headers) {
    std::move(callback_).Run(std::move(main_script_load_params_),
                             std::move(subresource_loader_params_),
                             nullptr /* completion_status */);
    delete this;
    return;
  }

  GetNetworkService()->ParseHeaders(
      resource_request_->url, main_script_load_params_->response_head->headers,
      base::BindOnce(&WorkerScriptFetcher::DidParseHeaders,
                     weak_factory_.GetWeakPtr()));
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

void WorkerScriptFetcher::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kWorkerScriptFetcher);
  NOTREACHED();
}

void WorkerScriptFetcher::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status.error_code == net::OK) {
    // It's possible to reach here when the `response_head_` doesn't have a
    // `parsed_headers` and ask NetworkService to parse headers in
    // OnReceiveResponse(). DidParseHeaders() will be called eventually
    // and `this` will be deleted in it.
    return;
  }

  std::move(callback_).Run(nullptr /* main_script_load_params */,
                           absl::nullopt /* subresource_loader_params */,
                           &status);
  delete this;
}

void WorkerScriptFetcher::DidParseHeaders(
    network::mojom::ParsedHeadersPtr parsed_headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(main_script_load_params_);

  main_script_load_params_->response_head->parsed_headers =
      std::move(parsed_headers);

  std::move(callback_).Run(std::move(main_script_load_params_),
                           std::move(subresource_loader_params_),
                           nullptr /* completion_status */);
  delete this;
}

}  // namespace content

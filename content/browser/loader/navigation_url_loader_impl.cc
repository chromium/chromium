// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/download/public/common/download_stats.h"
#include "content/browser/about_url_loader_factory.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/bundled_exchanges_utils.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_request_handler.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_url_loader_factory_internal.h"
#include "content/common/net/record_load_histograms.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/webplugininfo.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

#if defined(OS_ANDROID)
#include "content/browser/android/content_url_loader_factory.h"
#endif

namespace content {

namespace {

class NavigationLoaderInterceptorBrowserContainer
    : public NavigationLoaderInterceptor {
 public:
  explicit NavigationLoaderInterceptorBrowserContainer(
      std::unique_ptr<URLLoaderRequestInterceptor> browser_interceptor)
      : browser_interceptor_(std::move(browser_interceptor)) {}

  ~NavigationLoaderInterceptorBrowserContainer() override = default;

  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override {
    browser_interceptor_->MaybeCreateLoader(
        tentative_resource_request, browser_context,
        base::BindOnce(
            [](LoaderCallback callback,
               URLLoaderRequestInterceptor::RequestHandler handler) {
              if (handler) {
                std::move(callback).Run(
                    base::MakeRefCounted<SingleRequestURLLoaderFactory>(
                        std::move(handler)));
              } else {
                std::move(callback).Run({});
              }
            },
            std::move(callback)));
  }

 private:
  std::unique_ptr<URLLoaderRequestInterceptor> browser_interceptor_;
};

// Only used on the IO thread.
base::LazyInstance<NavigationURLLoaderImpl::URLLoaderFactoryInterceptor>::Leaky
    g_loader_factory_interceptor = LAZY_INSTANCE_INITIALIZER;

size_t GetCertificateChainsSizeInKB(const net::SSLInfo& ssl_info) {
  base::Pickle cert_pickle;
  ssl_info.cert->Persist(&cert_pickle);
  base::Pickle unverified_cert_pickle;
  ssl_info.unverified_cert->Persist(&unverified_cert_pickle);
  return (cert_pickle.size() + unverified_cert_pickle.size()) / 1000;
}

const net::NetworkTrafficAnnotationTag kNavigationUrlLoaderTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("navigation_url_loader", R"(
      semantics {
        sender: "Navigation URL Loader"
        description:
          "This request is issued by a main frame navigation to fetch the "
          "content of the page that is being navigated to."
        trigger:
          "Navigating Chrome (by clicking on a link, bookmark, history item, "
          "using session restore, etc)."
        data:
          "Arbitrary site-controlled data can be included in the URL, HTTP "
          "headers, and request body. Requests may include cookies and "
          "site-specific credentials."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "This feature cannot be disabled."
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
      comments:
        "Chrome would be unable to navigate to websites without this type of "
        "request. Using either URLBlacklist or URLWhitelist policies (or a "
        "combination of both) limits the scope of these requests."
      )");

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    NavigationRequestInfo* request_info,
    int frame_tree_node_id) {
  // TODO(scottmg): Port over stuff from RDHI::BeginNavigationRequest() here.
  auto new_request = std::make_unique<network::ResourceRequest>();

  new_request->method = request_info->common_params->method;
  new_request->url = request_info->common_params->url;
  new_request->site_for_cookies = request_info->site_for_cookies;
  new_request->trusted_params = network::ResourceRequest::TrustedParams();
  new_request->trusted_params->network_isolation_key =
      request_info->network_isolation_key;

  if (request_info->is_main_frame) {
    new_request->trusted_params->update_network_isolation_key_on_redirect =
        network::mojom::UpdateNetworkIsolationKeyOnRedirect::
            kUpdateTopFrameAndFrameOrigin;
  } else {
    new_request->trusted_params->update_network_isolation_key_on_redirect =
        network::mojom::UpdateNetworkIsolationKeyOnRedirect::kUpdateFrameOrigin;
  }

  net::RequestPriority net_priority = net::HIGHEST;
  if (!request_info->is_main_frame &&
      base::FeatureList::IsEnabled(features::kLowPriorityIframes)) {
    net_priority = net::LOWEST;
  }
  new_request->priority = net_priority;

  new_request->render_frame_id = frame_tree_node_id;

  // The code below to set fields like request_initiator, referrer, etc has
  // been copied from ResourceDispatcherHostImpl. We did not refactor the
  // common code into a function, because RDHI uses accessor functions on the
  // URLRequest class to set these fields. whereas we use ResourceRequest here.
  new_request->request_initiator =
      request_info->common_params->initiator_origin;
  new_request->referrer = request_info->common_params->referrer->url;
  new_request->referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
      request_info->common_params->referrer->policy);
  new_request->headers.AddHeadersFromString(
      request_info->begin_params->headers);

  new_request->resource_type =
      static_cast<int>(request_info->is_main_frame ? ResourceType::kMainFrame
                                                   : ResourceType::kSubFrame);
  if (request_info->is_main_frame)
    new_request->update_first_party_url_on_redirect = true;

  int load_flags = request_info->begin_params->load_flags;
  if (request_info->is_main_frame) {
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;
    load_flags |= net::LOAD_CAN_USE_RESTRICTED_PREFETCH;
  }

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  new_request->load_flags = load_flags;

  new_request->request_body = request_info->common_params->post_data.get();
  new_request->report_raw_headers = request_info->report_raw_headers;
  new_request->has_user_gesture = request_info->common_params->has_user_gesture;
  new_request->enable_load_timing = true;

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (request_info->is_main_frame) {
    // `<portal>` acts like a top-level navigation, but we want to represent it
    // as a nested navigation for the purposes of security checks like
    // `Sec-Fetch-Mode`.
    new_request->mode =
        frame_tree_node &&
                WebContentsImpl::FromFrameTreeNode(frame_tree_node)->IsPortal()
            ? network::mojom::RequestMode::kNavigateNestedFrame
            : network::mojom::RequestMode::kNavigate;
  } else {
    new_request->mode = network::mojom::RequestMode::kNavigateNestedFrame;
    if (frame_tree_node && (frame_tree_node->frame_owner_element_type() ==
                                blink::FrameOwnerElementType::kObject ||
                            frame_tree_node->frame_owner_element_type() ==
                                blink::FrameOwnerElementType::kEmbed)) {
      new_request->mode = network::mojom::RequestMode::kNavigateNestedObject;
    }
  }

  new_request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  new_request->redirect_mode = network::mojom::RedirectMode::kManual;
  new_request->fetch_request_context_type =
      static_cast<int>(request_info->begin_params->request_context_type);
  new_request->upgrade_if_insecure = request_info->upgrade_if_insecure;
  new_request->throttling_profile_id = request_info->devtools_frame_token;
  new_request->transition_type = request_info->common_params->transition;
  new_request->previews_state = request_info->common_params->previews_state;
  new_request->devtools_request_id =
      request_info->devtools_navigation_token.ToString();
  new_request->obey_origin_policy = request_info->obey_origin_policy;
  return new_request;
}

// Called for requests that we don't have a URLLoaderFactory for.
void UnknownSchemeCallback(
    bool handled_externally,
    const network::ResourceRequest& /* resource_request */,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
      ->OnComplete(network::URLLoaderCompletionStatus(
          handled_externally ? net::ERR_ABORTED : net::ERR_UNKNOWN_URL_SCHEME));
}

}  // namespace

// Kept around during the lifetime of the navigation request, and is
// responsible for dispatching a ResourceRequest to the appropriate
// URLLoader.  In order to get the right URLLoader it builds a vector
// of NavigationLoaderInterceptors and successively calls MaybeCreateLoader
// on each until the request is successfully handled. The same sequence
// may be performed multiple times when redirects happen.
// TODO(michaeln): Expose this class and add more unittests.
class NavigationURLLoaderImpl::URLLoaderRequestController
    : public network::mojom::URLLoaderClient {
 public:
  URLLoaderRequestController(
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
          initial_interceptors,
      std::unique_ptr<network::ResourceRequest> resource_request,
      BrowserContext* browser_context,
      const GURL& url,
      bool is_main_frame,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          proxied_factory_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          proxied_factory_remote,
      std::set<std::string> known_schemes,
      bool bypass_redirect_checks,
      const base::WeakPtr<NavigationURLLoaderImpl>& owner)
      : interceptors_(std::move(initial_interceptors)),
        resource_request_(std::move(resource_request)),
        url_(url),
        owner_(owner),
        proxied_factory_receiver_(std::move(proxied_factory_receiver)),
        proxied_factory_remote_(std::move(proxied_factory_remote)),
        known_schemes_(std::move(known_schemes)),
        bypass_redirect_checks_(bypass_redirect_checks),
        browser_context_(browser_context) {}

  ~URLLoaderRequestController() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // If neither OnCompleted nor OnReceivedResponse has been invoked, the
    // request was canceled before receiving a response, so log a cancellation.
    // Results after receiving a non-error response are logged in the renderer,
    // if the request is passed to one. If it's a download, or not passed to a
    // renderer for some other reason, results will not be logged for the
    // request. The net::OK check may not be necessary - the case where OK is
    // received without receiving any headers looks broken, anyways.
    if (!received_response_ && (!status_ || status_->error_code != net::OK)) {
      RecordLoadHistograms(
          url_, static_cast<ResourceType>(resource_request_->resource_type),
          status_ ? status_->error_code : net::ERR_ABORTED);
    }
  }

  static uint32_t GetURLLoaderOptions(bool is_main_frame) {
    uint32_t options = network::mojom::kURLLoadOptionNone;

    // Ensure that Mime sniffing works.
    options |= network::mojom::kURLLoadOptionSniffMimeType;

    if (is_main_frame) {
      // SSLInfo is not needed on subframe responses because users can inspect
      // only the certificate for the main frame when using the info bubble.
      options |= network::mojom::kURLLoadOptionSendSSLInfoWithResponse;
      options |= network::mojom::kURLLoadOptionSendSSLInfoForCertificateError;
    }

    return options;
  }

  void Start(
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          network_loader_factory_info,
      ServiceWorkerNavigationHandle*
          service_worker_navigation_handle /* for UI thread only */,
      ServiceWorkerNavigationHandleCore*
          service_worker_navigation_handle_core /* for IO thread only */,
      AppCacheNavigationHandle* appcache_handle,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      scoped_refptr<SignedExchangePrefetchMetricRecorder>
          signed_exchange_prefetch_metric_recorder,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui,
      bool needs_loader_factory_interceptor,
      base::Time ui_post_time,
      std::string accept_langs) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!started_);
    ui_to_io_time_ += (base::Time::Now() - ui_post_time);
    global_request_id_ = MakeGlobalRequestID();
    frame_tree_node_id_ = request_info->frame_tree_node_id;
    started_ = true;
    web_contents_getter_ = base::BindRepeating(
        &WebContents::FromFrameTreeNodeId, frame_tree_node_id_);
    navigation_ui_data_ = std::move(navigation_ui_data);
    service_worker_navigation_handle_ = service_worker_navigation_handle;

    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&NavigationURLLoaderImpl::OnRequestStarted,
                                  owner_, base::TimeTicks::Now()));

    DCHECK(network_loader_factory_info);
    network_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(network_loader_factory_info));
    if (needs_loader_factory_interceptor &&
        g_loader_factory_interceptor.Get()) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory;
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
          factory.InitWithNewPipeAndPassReceiver();
      g_loader_factory_interceptor.Get().Run(&receiver);
      network_loader_factory_->Clone(std::move(receiver));
      network_loader_factory_ =
          base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
              std::move(factory));
    }

    std::string accept_value = network::kFrameAcceptHeader;
    if (signed_exchange_utils::IsSignedExchangeHandlingEnabled(
            browser_context_)) {
      accept_value.append(kAcceptHeaderSignedExchangeSuffix);
    }
    resource_request_->headers.SetHeader(network::kAcceptHeader, accept_value);

    // NetworkService cases only.
    // Requests to WebUI scheme won't get redirected to/from other schemes
    // or be intercepted, so we just let it go here.
    if (factory_for_webui.is_valid()) {
      url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
          base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
              std::move(factory_for_webui)),
          CreateURLLoaderThrottles(), 0 /* routing_id */,
          global_request_id_.request_id, network::mojom::kURLLoadOptionNone,
          resource_request_.get(), this, kNavigationUrlLoaderTrafficAnnotation,
          base::ThreadTaskRunnerHandle::Get());
      return;
    }

    // Requests to Blob scheme won't get redirected to/from other schemes
    // or be intercepted, so we just let it go here.
    if (request_info->common_params->url.SchemeIsBlob() &&
        request_info->blob_url_loader_factory) {
      url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
          network::SharedURLLoaderFactory::Create(
              std::move(request_info->blob_url_loader_factory)),
          CreateURLLoaderThrottles(), 0 /* routing_id */,
          global_request_id_.request_id, network::mojom::kURLLoadOptionNone,
          resource_request_.get(), this, kNavigationUrlLoaderTrafficAnnotation,
          base::ThreadTaskRunnerHandle::Get());
      return;
    }

    CreateInterceptors(request_info.get(), appcache_handle,
                       prefetched_signed_exchange_cache,
                       signed_exchange_prefetch_metric_recorder, accept_langs);
    Restart();
  }

  void CreateInterceptors(NavigationRequestInfo* request_info,
                          AppCacheNavigationHandle* appcache_handle,
                          scoped_refptr<PrefetchedSignedExchangeCache>
                              prefetched_signed_exchange_cache,
                          scoped_refptr<SignedExchangePrefetchMetricRecorder>
                              signed_exchange_prefetch_metric_recorder,
                          const std::string& accept_langs) {
    if (prefetched_signed_exchange_cache) {
      std::unique_ptr<NavigationLoaderInterceptor>
          prefetched_signed_exchange_interceptor =
              prefetched_signed_exchange_cache->MaybeCreateInterceptor(
                  request_info->common_params->url);
      if (prefetched_signed_exchange_interceptor) {
        interceptors_.push_back(
            std::move(prefetched_signed_exchange_interceptor));
      }
    }

    // Set up an interceptor for service workers.
    if (service_worker_navigation_handle_) {
      std::unique_ptr<NavigationLoaderInterceptor> service_worker_interceptor =
          ServiceWorkerRequestHandler::CreateForNavigation(
              resource_request_->url,
              service_worker_navigation_handle_->AsWeakPtr(), *request_info);
      // The interceptor may not be created in certain cases (e.g., the origin
      // is not secure).
      if (service_worker_interceptor)
        interceptors_.push_back(std::move(service_worker_interceptor));
    }

    // Set-up an interceptor for AppCache if non-null |appcache_handle| is
    // given.
    if (appcache_handle) {
      CHECK(appcache_handle->host());
      std::unique_ptr<NavigationLoaderInterceptor> appcache_interceptor =
          AppCacheRequestHandler::InitializeForMainResourceNetworkService(
              *resource_request_, appcache_handle->host()->GetWeakPtr());
      if (appcache_interceptor)
        interceptors_.push_back(std::move(appcache_interceptor));
    }

    // Set-up an interceptor for SignedExchange handling if it is enabled.
    if (signed_exchange_utils::IsSignedExchangeHandlingEnabled(
            browser_context_)) {
      interceptors_.push_back(CreateSignedExchangeRequestHandler(
          *request_info, network_loader_factory_,
          std::move(signed_exchange_prefetch_metric_recorder),
          std::move(accept_langs)));
    }

    // See if embedders want to add interceptors.
    std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>
        browser_interceptors =
            GetContentClient()
                ->browser()
                ->WillCreateURLLoaderRequestInterceptors(
                    navigation_ui_data_.get(), request_info->frame_tree_node_id,
                    network_loader_factory_);
    if (!browser_interceptors.empty()) {
      for (auto& browser_interceptor : browser_interceptors) {
        interceptors_.push_back(
            std::make_unique<NavigationLoaderInterceptorBrowserContainer>(
                std::move(browser_interceptor)));
      }
    }
  }

  // This could be called multiple times to follow a chain of redirects.
  void Restart() {
    // Clear |url_loader_| if it's not the default one (network). This allows
    // the restarted request to use a new loader, instead of, e.g., reusing the
    // AppCache or service worker loader. For an optimization, we keep and reuse
    // the default url loader if the all |interceptors_| doesn't handle the
    // redirected request. If the network service is enabled, reset the loader
    // if the redirected URL's scheme and the previous URL scheme don't match in
    // their use or disuse of the network service loader.
    if (!default_loader_used_ ||
        (url_chain_.size() > 1 &&
         IsURLHandledByNetworkService(url_chain_[url_chain_.size() - 1]) !=
             IsURLHandledByNetworkService(url_chain_[url_chain_.size() - 2]))) {
      url_loader_.reset();
    }
    interceptor_index_ = 0;
    received_response_ = false;
    head_ = network::ResourceResponseHead();
    MaybeStartLoader(nullptr /* interceptor */,
                     {} /* single_request_factory */);
  }

  // |interceptor| is non-null if this is called by one of the interceptors
  // (via a LoaderCallback).
  // |single_request_handler| is the RequestHandler given by the |interceptor|,
  // non-null if the interceptor wants to handle the request.
  void MaybeStartLoader(
      NavigationLoaderInterceptor* interceptor,
      scoped_refptr<network::SharedURLLoaderFactory> single_request_factory) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(started_);

    if (single_request_factory) {
      // |interceptor| wants to handle the request with
      // |single_request_handler|.
      DCHECK(interceptor);

      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
          CreateURLLoaderThrottles();
      // Intercepted requests need MimeSniffingThrottle to do mime sniffing.
      // Non-intercepted requests usually go through the regular network
      // URLLoader, which does mime sniffing.
      throttles.push_back(std::make_unique<blink::MimeSniffingThrottle>(
          base::ThreadTaskRunnerHandle::Get()));

      default_loader_used_ = false;
      url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(single_request_factory), std::move(throttles),
          frame_tree_node_id_, global_request_id_.request_id,
          network::mojom::kURLLoadOptionNone, resource_request_.get(), this,
          kNavigationUrlLoaderTrafficAnnotation,
          base::ThreadTaskRunnerHandle::Get());

      subresource_loader_params_ =
          interceptor->MaybeCreateSubresourceLoaderParams();
      if (interceptor->ShouldBypassRedirectChecks())
        bypass_redirect_checks_ = true;
      return;
    }

    // Before falling back to the next interceptor, see if |interceptor| still
    // wants to give additional info to the frame for subresource loading. In
    // that case we will just fall back to the default loader (i.e. won't go on
    // to the next interceptors) but send the subresource_loader_params to the
    // child process. This is necessary for correctness in the cases where, e.g.
    // there's a controlling service worker that doesn't have a fetch event
    // handler so it doesn't intercept requests. In that case we still want to
    // skip AppCache.
    if (interceptor) {
      subresource_loader_params_ =
          interceptor->MaybeCreateSubresourceLoaderParams();

      // If non-null |subresource_loader_params_| is returned, make sure
      // we skip the next interceptors.
      if (subresource_loader_params_)
        interceptor_index_ = interceptors_.size();
    }

    // See if the next interceptor wants to handle the request.
    if (interceptor_index_ < interceptors_.size()) {
      auto* next_interceptor = interceptors_[interceptor_index_++].get();
      next_interceptor->MaybeCreateLoader(
          *resource_request_, browser_context_,
          base::BindOnce(&URLLoaderRequestController::MaybeStartLoader,
                         base::Unretained(this), next_interceptor),
          base::BindOnce(
              &URLLoaderRequestController::FallbackToNonInterceptedRequest,
              base::Unretained(this)));
      return;
    }

    // If we already have the default |url_loader_| we must come here after a
    // redirect. No interceptors wanted to intercept the redirected request, so
    // let the loader just follow the redirect.
    if (url_loader_) {
      DCHECK(!redirect_info_.new_url.is_empty());
      url_loader_->FollowRedirect(std::move(url_loader_removed_headers_),
                                  std::move(url_loader_modified_headers_));
      return;
    }

    // No interceptors wanted to handle this request.
    uint32_t options = network::mojom::kURLLoadOptionNone;
    scoped_refptr<network::SharedURLLoaderFactory> factory =
        PrepareForNonInterceptedRequest(&options);
    url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
        std::move(factory), CreateURLLoaderThrottles(), frame_tree_node_id_,
        global_request_id_.request_id, options, resource_request_.get(),
        this /* client */, kNavigationUrlLoaderTrafficAnnotation,
        base::ThreadTaskRunnerHandle::Get());
  }

  // This is the |fallback_callback| passed to
  // NavigationLoaderInterceptor::MaybeCreateLoader. It allows an interceptor
  // to initially elect to handle a request, and later decide to fallback to
  // the default behavior. This is needed for service worker network fallback
  // and signed exchange (SXG) fallback redirect.
  void FallbackToNonInterceptedRequest(bool reset_subresource_loader_params) {
    if (reset_subresource_loader_params)
      subresource_loader_params_.reset();

    uint32_t options = network::mojom::kURLLoadOptionNone;
    scoped_refptr<network::SharedURLLoaderFactory> factory =
        PrepareForNonInterceptedRequest(&options);
    if (url_loader_) {
      // |url_loader_| is using the factory for the interceptor that decided to
      // fallback, so restart it with the non-interceptor factory.
      url_loader_->RestartWithFactory(std::move(factory), options);
    } else {
      // In SXG cases we don't have |url_loader_| because it was reset when the
      // SXG interceptor intercepted the response in
      // MaybeCreateLoaderForResponse.
      DCHECK(response_loader_receiver_.is_bound());
      response_loader_receiver_.reset();
      url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(factory), CreateURLLoaderThrottles(), frame_tree_node_id_,
          global_request_id_.request_id, options, resource_request_.get(),
          this /* client */, kNavigationUrlLoaderTrafficAnnotation,
          base::ThreadTaskRunnerHandle::Get());
    }
  }

  scoped_refptr<network::SharedURLLoaderFactory>
  PrepareForNonInterceptedRequest(uint32_t* out_options) {
    // TODO(https://crbug.com/796425): We temporarily wrap raw
    // mojom::URLLoaderFactory pointers into SharedURLLoaderFactory. Need to
    // further refactor the factory getters to avoid this.
    scoped_refptr<network::SharedURLLoaderFactory> factory;

    if (!IsURLHandledByNetworkService(resource_request_->url)) {
      if (known_schemes_.find(resource_request_->url.scheme()) ==
          known_schemes_.end()) {
        mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory;
        bool handled = GetContentClient()->browser()->HandleExternalProtocol(
            resource_request_->url, web_contents_getter_,
            ChildProcessHost::kInvalidUniqueID, navigation_ui_data_.get(),
            resource_request_->resource_type ==
                static_cast<int>(ResourceType::kMainFrame),
            static_cast<ui::PageTransition>(resource_request_->transition_type),
            resource_request_->has_user_gesture,
            resource_request_->request_initiator, &loader_factory);

        if (loader_factory) {
          factory =
              base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
                  std::move(loader_factory));
        } else {
          factory = base::MakeRefCounted<SingleRequestURLLoaderFactory>(
              base::BindOnce(UnknownSchemeCallback, handled));
        }
      } else {
        mojo::Remote<network::mojom::URLLoaderFactory>& non_network_factory =
            non_network_url_loader_factories_[resource_request_->url.scheme()];
        if (!non_network_factory.is_bound()) {
          owner_->BindNonNetworkURLLoaderFactoryReceiver(
              frame_tree_node_id_, resource_request_->url,
              non_network_factory.BindNewPipeAndPassReceiver());
        }
        factory =
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                non_network_factory.get());
      }

      if (g_loader_factory_interceptor.Get()) {
        mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
            factory_remote.InitWithNewPipeAndPassReceiver();
        g_loader_factory_interceptor.Get().Run(&receiver);
        factory->Clone(std::move(receiver));
        factory = base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(factory_remote));
      }
    } else {
      default_loader_used_ = true;

      // NOTE: We only support embedders proxying network-service-bound requests
      // not handled by NavigationLoaderInterceptors above (e.g. Service Worker
      // or AppCache). Hence this code is only reachable when one of the above
      // interceptors isn't used and the URL is either a data URL or has a
      // scheme which is handled by the network service.
      if (proxied_factory_receiver_.is_valid()) {
        DCHECK(proxied_factory_remote_.is_valid());
        // We don't worry about reconnection since it's a single navigation.
        network_loader_factory_->Clone(std::move(proxied_factory_receiver_));
        factory = base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(proxied_factory_remote_));
      } else {
        factory = network_loader_factory_;
      }
    }
    url_chain_.push_back(resource_request_->url);
    *out_options =
        GetURLLoaderOptions(resource_request_->resource_type ==
                            static_cast<int>(ResourceType::kMainFrame));
    return factory;
  }

  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      PreviewsState new_previews_state,
                      base::Time ui_post_time) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!redirect_info_.new_url.is_empty());
    ui_to_io_time_ += (base::Time::Now() - ui_post_time);

    // Update |resource_request_| and call Restart to give our |interceptors_| a
    // chance at handling the new location. If no interceptor wants to take
    // over, we'll use the existing url_loader to follow the redirect, see
    // MaybeStartLoader.
    // TODO(michaeln): This is still WIP and is based on URLRequest::Redirect,
    // there likely remains more to be done.
    // a. For subframe navigations, the Origin header may need to be modified
    //    differently?

    bool should_clear_upload = false;
    net::RedirectUtil::UpdateHttpRequest(
        resource_request_->url, resource_request_->method, redirect_info_,
        removed_headers, modified_headers, &resource_request_->headers,
        &should_clear_upload);
    if (should_clear_upload) {
      // The request body is no longer applicable.
      resource_request_->request_body.reset();
    }

    resource_request_->url = redirect_info_.new_url;
    resource_request_->method = redirect_info_.new_method;
    resource_request_->site_for_cookies = redirect_info_.new_site_for_cookies;

    // See if navigation network isolation key needs to be updated.
    if (resource_request_->resource_type ==
        static_cast<int>(ResourceType::kMainFrame)) {
      url::Origin origin = url::Origin::Create(resource_request_->url);
      resource_request_->trusted_params->network_isolation_key =
          net::NetworkIsolationKey(origin, origin);
    } else {
      DCHECK_EQ(static_cast<int>(ResourceType::kSubFrame),
                resource_request_->resource_type);
      url::Origin subframe_origin = url::Origin::Create(resource_request_->url);
      base::Optional<url::Origin> top_frame_origin =
          resource_request_->trusted_params->network_isolation_key
              .GetTopFrameOrigin();
      DCHECK(top_frame_origin);
      resource_request_->trusted_params->network_isolation_key =
          net::NetworkIsolationKey(top_frame_origin.value(), subframe_origin);
    }

    resource_request_->referrer = GURL(redirect_info_.new_referrer);
    resource_request_->referrer_policy = redirect_info_.new_referrer_policy;
    resource_request_->previews_state = new_previews_state;
    url_chain_.push_back(redirect_info_.new_url);

    // Need to cache modified headers for |url_loader_| since it doesn't use
    // |resource_request_| during redirect.
    url_loader_removed_headers_ = removed_headers;
    url_loader_modified_headers_ = modified_headers;

    // Don't send Accept: application/signed-exchange for fallback redirects.
    if (redirect_info_.is_signed_exchange_fallback_redirect) {
      url_loader_modified_headers_.SetHeader(network::kAcceptHeader,
                                             network::kFrameAcceptHeader);
      resource_request_->headers.SetHeader(network::kAcceptHeader,
                                           network::kFrameAcceptHeader);
    }

    Restart();
  }

  base::Optional<SubresourceLoaderParams> TakeSubresourceLoaderParams() {
    return std::move(subresource_loader_params_);
  }

 private:
  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override {
    // Wait for OnStartLoadingResponseBody() before sending anything to the
    // renderer process.
    if (!response_body_.is_valid()) {
      head_ = head;
      return;
    }
    received_response_ = true;

    // If the default loader (network) was used to handle the URL load request
    // we need to see if the interceptors want to potentially create a new
    // loader for the response. e.g. AppCache.
    if (MaybeCreateLoaderForResponse(head))
      return;

    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints;

    if (url_loader_) {
      url_loader_client_endpoints = url_loader_->Unbind();
    } else {
      url_loader_client_endpoints =
          network::mojom::URLLoaderClientEndpoints::New(
              response_url_loader_.PassInterface(),
              response_loader_receiver_.Unbind());
    }

    // 304 responses should abort the navigation, rather than display the page.
    // This needs to be after the URLLoader has been moved to
    // |url_loader_client_endpoints| in order to abort the request, to avoid
    // receiving unexpected call.
    if (head->headers &&
        head->headers->response_code() == net::HTTP_NOT_MODIFIED) {
      // Call CancelWithError instead of OnComplete so that if there is an
      // intercepting URLLoaderFactory it gets notified.
      url_loader_->CancelWithError(
          net::ERR_ABORTED,
          base::StringPiece(base::NumberToString(net::ERR_ABORTED)));
      return;
    }

    bool is_download;

    bool must_download = download_utils::MustDownload(url_, head->headers.get(),
                                                      head->mime_type);
    bool known_mime_type = blink::IsSupportedMimeType(head->mime_type);

#if BUILDFLAG(ENABLE_PLUGINS)
    if (!head->intercepted_by_plugin && !must_download && !known_mime_type) {
      // No plugin throttles intercepted the response. Ask if the plugin
      // registered to PluginService wants to handle the request.
      CheckPluginAndContinueOnReceiveResponse(
          head, std::move(url_loader_client_endpoints),
          true /* is_download_if_not_handled_by_plugin */,
          std::vector<WebPluginInfo>());
      return;
    }
#endif

    // When a plugin intercepted the response, we don't want to download it.
    is_download =
        !head->intercepted_by_plugin && (must_download || !known_mime_type);

    CallOnReceivedResponse(head, std::move(url_loader_client_endpoints),
                           is_download);
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  void CheckPluginAndContinueOnReceiveResponse(
      const network::ResourceResponseHead& head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_download_if_not_handled_by_plugin,
      const std::vector<WebPluginInfo>& plugins) {
    bool stale;
    WebPluginInfo plugin;
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
    int render_process_id =
        frame_tree_node->current_frame_host()->GetProcess()->GetID();
    int routing_id = frame_tree_node->current_frame_host()->GetRoutingID();
    bool has_plugin = PluginService::GetInstance()->GetPluginInfo(
        render_process_id, routing_id, resource_request_->url, url::Origin(),
        head.mime_type, false /* allow_wildcard */, &stale, &plugin, nullptr);

    if (stale) {
      // Refresh the plugins asynchronously.
      PluginService::GetInstance()->GetPlugins(base::BindOnce(
          &URLLoaderRequestController::CheckPluginAndContinueOnReceiveResponse,
          weak_factory_.GetWeakPtr(), head,
          std::move(url_loader_client_endpoints),
          is_download_if_not_handled_by_plugin));
      return;
    }

    bool is_download = !has_plugin && is_download_if_not_handled_by_plugin;

    CallOnReceivedResponse(head, std::move(url_loader_client_endpoints),
                           is_download);
  }
#endif

  void CallOnReceivedResponse(
      const network::ResourceResponseHead& head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_download) {
    scoped_refptr<network::ResourceResponse> response(
        new network::ResourceResponse());
    response->head = head;

    owner_->OnReceiveResponse(response, std::move(url_loader_client_endpoints),
                              std::move(response_body_), global_request_id_,
                              is_download, ui_to_io_time_, base::Time::Now());
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    if (!bypass_redirect_checks_ &&
        !IsSafeRedirectTarget(url_, redirect_info.new_url)) {
      // Call CancelWithError instead of OnComplete so that if there is an
      // intercepting URLLoaderFactory (created through the embedder's
      // ContentBrowserClient::WillCreateURLLoaderFactory) it gets notified.
      url_loader_->CancelWithError(
          net::ERR_UNSAFE_REDIRECT,
          base::StringPiece(base::NumberToString(net::ERR_UNSAFE_REDIRECT)));
      return;
    }

    if (--redirect_limit_ == 0) {
      // Call CancelWithError instead of OnComplete so that if there is an
      // intercepting URLLoaderFactory it gets notified.
      url_loader_->CancelWithError(
          net::ERR_TOO_MANY_REDIRECTS,
          base::StringPiece(base::NumberToString(net::ERR_TOO_MANY_REDIRECTS)));
      return;
    }

    // Store the redirect_info for later use in FollowRedirect where we give
    // our interceptors_ a chance to intercept the request for the new location.
    redirect_info_ = redirect_info;

    scoped_refptr<network::ResourceResponse> response(
        new network::ResourceResponse());
    response->head = head;
    url_ = redirect_info.new_url;

    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NavigationURLLoaderImpl::OnReceiveRedirect, owner_,
                       redirect_info, response, base::Time::Now()));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    NOTREACHED();
  }

  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override {
    NOTREACHED();
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle response_body) override {
    response_body_ = std::move(response_body);
    OnReceiveResponse(head_);
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    UMA_HISTOGRAM_BOOLEAN(
        "Navigation.URLLoaderNetworkService.OnCompleteHasSSLInfo",
        status.ssl_info.has_value());

    // Successful load must have used OnResponseStarted first. In this case, the
    // URLLoaderClient has already been transferred to the renderer process and
    // OnComplete is not expected to be called here.
    if (status.error_code == net::OK) {
      base::debug::DumpWithoutCrashing();
      return;
    }

    if (status.ssl_info.has_value()) {
      UMA_HISTOGRAM_MEMORY_KB(
          "Navigation.URLLoaderNetworkService.OnCompleteCertificateChainsSize",
          GetCertificateChainsSizeInKB(status.ssl_info.value()));
    }

    // If the default loader (network) was used to handle the URL load request
    // we need to see if the interceptors want to potentially create a new
    // loader for the response. e.g. AppCache.
    //
    // Note: Despite having received a response, the HTTP_NOT_MODIFIED(304) ones
    //       are ignored using OnComplete(net::ERR_ABORTED). No interceptor must
    //       be used in this case.
    if (!received_response_ &&
        MaybeCreateLoaderForResponse(network::ResourceResponseHead())) {
      return;
    }

    status_ = status;
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NavigationURLLoaderImpl::OnComplete, owner_, status));
  }

  // Returns true if an interceptor wants to handle the response, i.e. return a
  // different response. For e.g. AppCache may have fallback content.
  bool MaybeCreateLoaderForResponse(
      const network::ResourceResponseHead& response) {
    if (!default_loader_used_ &&
        !bundled_exchanges_utils::CanLoadAsBundledExchanges(
            url_, response.mime_type)) {
      return false;
    }
    for (size_t i = 0u; i < interceptors_.size(); ++i) {
      NavigationLoaderInterceptor* interceptor = interceptors_[i].get();
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          response_client_receiver;
      bool skip_other_interceptors = false;
      bool will_return_unsafe_redirect = false;
      if (interceptor->MaybeCreateLoaderForResponse(
              *resource_request_, response, &response_body_,
              &response_url_loader_, &response_client_receiver,
              url_loader_.get(), &skip_other_interceptors,
              &will_return_unsafe_redirect)) {
        if (will_return_unsafe_redirect)
          bypass_redirect_checks_ = true;
        response_loader_receiver_.reset();
        response_loader_receiver_.Bind(std::move(response_client_receiver));
        default_loader_used_ = false;
        url_loader_.reset();     // Consumed above.
        response_body_.reset();  // Consumed above.
        if (skip_other_interceptors) {
          std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
              new_interceptors;
          new_interceptors.push_back(std::move(interceptors_[i]));
          new_interceptors.swap(interceptors_);
          // Reset the state of ServiceWorkerProviderHost.
          // Currently we don't support Service Worker in Signed Exchange
          // pages. The page will not be controlled by service workers. And
          // Service Worker related APIs will fail with NoDocumentURL error.
          // TODO(crbug/898733): Support SignedExchange loading and Service
          // Worker integration.
          if (service_worker_navigation_handle_) {
            RunOrPostTaskOnThread(
                FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                base::BindOnce(
                    [](ServiceWorkerNavigationHandleCore* core) {
                      base::WeakPtr<ServiceWorkerProviderHost> host =
                          core->provider_host();
                      if (host) {
                        host->SetControllerRegistration(
                            nullptr, false /* notify_controllerchange */);
                        host->UpdateUrls(GURL(), GURL(), base::nullopt);
                      }
                    },
                    // Unretained() is safe because the handle owns the core,
                    // and core gets deleted on the core thread in a task that
                    // must occur after this task.
                    base::Unretained(
                        service_worker_navigation_handle_->core())));
          }
        }
        return true;
      }
    }
    return false;
  }

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles() {
    return GetContentClient()->browser()->CreateURLLoaderThrottles(
        *resource_request_, browser_context_, web_contents_getter_,
        navigation_ui_data_.get(), frame_tree_node_id_);
  }

  std::unique_ptr<SignedExchangeRequestHandler>
  CreateSignedExchangeRequestHandler(
      const NavigationRequestInfo& request_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<SignedExchangePrefetchMetricRecorder>
          signed_exchange_prefetch_metric_recorder,
      std::string accept_langs) {
    // It is safe to pass the callback of CreateURLLoaderThrottles with the
    // unretained |this|, because the passed callback will be used by a
    // SignedExchangeHandler which is indirectly owned by |this| until its
    // header is verified and parsed, that's where the getter is used.
    return std::make_unique<SignedExchangeRequestHandler>(
        GetURLLoaderOptions(request_info.is_main_frame),
        request_info.frame_tree_node_id, request_info.devtools_navigation_token,
        std::move(url_loader_factory),
        base::BindRepeating(
            &URLLoaderRequestController::CreateURLLoaderThrottles,
            base::Unretained(this)),
        std::move(signed_exchange_prefetch_metric_recorder),
        std::move(accept_langs));
  }

  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptors_;
  size_t interceptor_index_ = 0;

  std::unique_ptr<network::ResourceRequest> resource_request_;
  // Non-NetworkService: |request_info_| is updated along with
  // |resource_request_| on redirects.
  std::unique_ptr<NavigationRequestInfo> request_info_;
  int frame_tree_node_id_ = 0;
  GlobalRequestID global_request_id_;
  net::RedirectInfo redirect_info_;
  int redirect_limit_ = net::URLRequest::kMaxRedirects;
  base::Callback<WebContents*()> web_contents_getter_;
  std::unique_ptr<NavigationUIData> navigation_ui_data_;
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  std::unique_ptr<blink::ThrottlingURLLoader> url_loader_;

  // Caches the modified request headers provided by clients during redirect,
  // will be consumed by next |url_loader_->FollowRedirect()|.
  std::vector<std::string> url_loader_removed_headers_;
  net::HttpRequestHeaders url_loader_modified_headers_;

  std::vector<GURL> url_chain_;

  // Current URL that is being navigated, updated after redirection.
  GURL url_;

  // Currently used by the AppCache loader to pass its factory to the
  // renderer which enables it to handle subresources.
  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  // This is referenced only on the UI thread.
  base::WeakPtr<NavigationURLLoaderImpl> owner_;

  // Set to true if the default URLLoader (network service) was used for the
  // current navigation.
  bool default_loader_used_ = false;

  // URLLoaderClient receiver for loaders created for responses received from
  // the network loader.
  mojo::Receiver<network::mojom::URLLoaderClient> response_loader_receiver_{
      this};

  // URLLoader instance for response loaders, i.e loaders created for handing
  // responses received from the network URLLoader.
  network::mojom::URLLoaderPtr response_url_loader_;

  // Set to true if we receive a valid response from a URLLoader, i.e.
  // URLLoaderClient::OnReceivedResponse() is called.
  bool received_response_ = false;

  bool started_ = false;

  // Lazily initialized and used in the case of non-network resource
  // navigations. Keyed by URL scheme.
  std::map<std::string, mojo::Remote<network::mojom::URLLoaderFactory>>
      non_network_url_loader_factories_;

  // Non-NetworkService:
  // Generator of a request handler for sending request to the network. This
  // captures all of parameters to create a
  // SingleRequestURLLoaderFactory::RequestHandler. Used only when
  // NetworkService is disabled.
  // Set |was_request_intercepted| to true if the request was intercepted by an
  // interceptor and the request is falling back to the network. In that case,
  // any interceptors won't intercept the request.
  base::RepeatingCallback<SingleRequestURLLoaderFactory::RequestHandler(
      bool /* was_request_intercepted */)>
      default_request_handler_factory_;

  // The completion status if it has been received. This is needed to handle
  // the case that the response is intercepted by download, and OnComplete() is
  // already called while we are transferring the |url_loader_| and response
  // body to download code.
  base::Optional<network::URLLoaderCompletionStatus> status_;

  // Before creating this URLLoaderRequestController on UI thread, the embedder
  // may have elected to proxy the URLLoaderFactory receiver, in which case
  // these fields will contain input (remote) and output (receiver) endpoints
  // for the proxy. If this controller is handling a receiver for which proxying
  // is supported, receivers will be plumbed through these endpoints.
  //
  // Note that these are only used for receivers that go to the Network Service.
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      proxied_factory_receiver_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxied_factory_remote_;

  // The schemes that this loader can use. For anything else we'll try external
  // protocol handlers.
  std::set<std::string> known_schemes_;

  // True when a proxy will handle the redirect checks, or when an interceptor
  // intentionally returned unsafe redirect response
  // (eg: NavigationLoaderInterceptor for loading local bundled exchanges file).
  bool bypass_redirect_checks_;

  // Used to reset the state of ServiceWorkerProviderHost when
  // SignedExchangeRequestHandler will handle the response.
  base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host_;
  ServiceWorkerNavigationHandle* service_worker_navigation_handle_ = nullptr;

  // Counts the time overhead of all the hops from the UI to the IO threads.
  base::TimeDelta ui_to_io_time_;

  // Only used when NavigationLoaderOnUI is enabled:
  BrowserContext* browser_context_;

  network::ResourceResponseHead head_;
  mojo::ScopedDataPipeConsumerHandle response_body_;

  mutable base::WeakPtrFactory<URLLoaderRequestController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(URLLoaderRequestController);
};

// TODO(https://crbug.com/790734): pass |navigation_ui_data| along with the
// request so that it could be modified.
NavigationURLLoaderImpl::NavigationURLLoaderImpl(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_navigation_handle,
    AppCacheNavigationHandle* appcache_handle,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    NavigationURLLoaderDelegate* delegate,
    std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
        initial_interceptors)
    : delegate_(delegate),
      download_policy_(request_info->common_params->download_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int frame_tree_node_id = request_info->frame_tree_node_id;

  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", this,
      request_info->common_params->navigation_start, "FrameTreeNode id",
      frame_tree_node_id);

  ServiceWorkerNavigationHandleCore* service_worker_navigation_handle_core =
      service_worker_navigation_handle
          ? service_worker_navigation_handle->core()
          : nullptr;

  std::unique_ptr<network::ResourceRequest> new_request =
      CreateResourceRequest(request_info.get(), frame_tree_node_id);

  auto* partition = static_cast<StoragePartitionImpl*>(storage_partition);
  scoped_refptr<SignedExchangePrefetchMetricRecorder>
      signed_exchange_prefetch_metric_recorder =
          partition->GetPrefetchURLLoaderService()
              ->signed_exchange_prefetch_metric_recorder();

  std::string accept_langs = GetContentClient()->browser()->GetAcceptLangs(
      partition->browser_context());

  // Check if a web UI scheme wants to handle this request.
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  const auto& schemes = URLDataManagerBackend::GetWebUISchemes();
  std::string scheme = new_request->url.scheme();
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui;
  if (base::Contains(schemes, scheme)) {
    auto factory_receiver = factory_for_webui.InitWithNewPipeAndPassReceiver();
    GetContentClient()->browser()->WillCreateURLLoaderFactory(
        partition->browser_context(), frame_tree_node->current_frame_host(),
        frame_tree_node->current_frame_host()->GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
        &factory_receiver, nullptr /* header_client */,
        nullptr /* bypass_redirect_checks */);
    CreateWebUIURLLoaderBinding(frame_tree_node->current_frame_host(), scheme,
                                std::move(factory_receiver));
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> proxied_factory_remote;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      proxied_factory_receiver;
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  bool bypass_redirect_checks = false;
  if (frame_tree_node) {
    // |frame_tree_node| may be null in some unit test environments.
    GetContentClient()
        ->browser()
        ->RegisterNonNetworkNavigationURLLoaderFactories(
            frame_tree_node_id, &non_network_url_loader_factories_);

    // The embedder may want to proxy all network-bound URLLoaderFactory
    // receivers that it can. If it elects to do so, we'll pass its proxy
    // endpoints off to the URLLoaderRequestController where wthey will be
    // connected if the request type supports proxying.
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory;
    auto factory_receiver = pending_factory.InitWithNewPipeAndPassReceiver();
    bool use_proxy = GetContentClient()->browser()->WillCreateURLLoaderFactory(
        partition->browser_context(), frame_tree_node->current_frame_host(),
        frame_tree_node->current_frame_host()->GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
        &factory_receiver, &header_client, &bypass_redirect_checks);
    if (devtools_instrumentation::WillCreateURLLoaderFactory(
            frame_tree_node->current_frame_host(), true /* is_navigation */,
            false /* is_download */, &factory_receiver)) {
      use_proxy = true;
    }
    if (use_proxy) {
      proxied_factory_receiver = std::move(factory_receiver);
      proxied_factory_remote = std::move(pending_factory);
    }

    const std::string storage_domain;
    non_network_url_loader_factories_[url::kFileSystemScheme] =
        CreateFileSystemURLLoaderFactory(ChildProcessHost::kInvalidUniqueID,
                                         frame_tree_node->frame_tree_node_id(),
                                         partition->GetFileSystemContext(),
                                         storage_domain);
  }

  non_network_url_loader_factories_[url::kAboutScheme] =
      std::make_unique<AboutURLLoaderFactory>();

  non_network_url_loader_factories_[url::kDataScheme] =
      std::make_unique<DataURLLoaderFactory>();

  std::unique_ptr<network::mojom::URLLoaderFactory> file_url_loader_factory =
      std::make_unique<FileURLLoaderFactory>(
          partition->browser_context()->GetPath(),
          partition->browser_context()->GetSharedCorsOriginAccessList(),
          // USER_VISIBLE because loaded file resources may affect the UI.
          base::TaskPriority::USER_VISIBLE);

  if (frame_tree_node) {  // May be nullptr in some unit tests.
    devtools_instrumentation::WillCreateURLLoaderFactory(
        frame_tree_node->current_frame_host(), true /* is_navigation */,
        false /* is_download */, &file_url_loader_factory);
  }

  non_network_url_loader_factories_[url::kFileScheme] =
      std::move(file_url_loader_factory);

#if defined(OS_ANDROID)
  non_network_url_loader_factories_[url::kContentScheme] =
      std::make_unique<ContentURLLoaderFactory>(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
#endif

  std::set<std::string> known_schemes;
  for (auto& iter : non_network_url_loader_factories_)
    known_schemes.insert(iter.first);

  bool needs_loader_factory_interceptor = false;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> network_factory_info =
      partition->GetURLLoaderFactoryForBrowserProcess()->Clone();
  if (header_client) {
    needs_loader_factory_interceptor = true;
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    CreateURLLoaderFactoryWithHeaderClient(
        std::move(header_client),
        factory_remote.InitWithNewPipeAndPassReceiver(), partition);
    network_factory_info =
        std::make_unique<network::WrapperSharedURLLoaderFactoryInfo>(
            std::move(factory_remote));
  }

  DCHECK(!request_controller_);
  request_controller_ = std::make_unique<URLLoaderRequestController>(
      std::move(initial_interceptors), std::move(new_request), browser_context,
      request_info->common_params->url, request_info->is_main_frame,
      std::move(proxied_factory_receiver), std::move(proxied_factory_remote),
      std::move(known_schemes), bypass_redirect_checks,
      weak_factory_.GetWeakPtr());
  request_controller_->Start(
      std::move(network_factory_info), service_worker_navigation_handle,
      service_worker_navigation_handle_core, appcache_handle,
      std::move(prefetched_signed_exchange_cache),
      std::move(signed_exchange_prefetch_metric_recorder),
      std::move(request_info), std::move(navigation_ui_data),
      std::move(factory_for_webui), needs_loader_factory_interceptor,
      base::Time::Now(), std::move(accept_langs));
}

NavigationURLLoaderImpl::~NavigationURLLoaderImpl() {
}

void NavigationURLLoaderImpl::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    PreviewsState new_previews_state) {
  request_controller_->FollowRedirect(removed_headers, modified_headers,
                                      new_previews_state, base::Time::Now());
}

void NavigationURLLoaderImpl::OnReceiveResponse(
    scoped_refptr<network::ResourceResponse> response_head,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    mojo::ScopedDataPipeConsumerHandle response_body,
    const GlobalRequestID& global_request_id,
    bool is_download,
    base::TimeDelta total_ui_to_io_time,
    base::Time io_post_time) {
  const base::TimeDelta kMinTime = base::TimeDelta::FromMicroseconds(1);
  const base::TimeDelta kMaxTime = base::TimeDelta::FromMilliseconds(100);
  const int kBuckets = 50;
  io_to_ui_time_ += (base::Time::Now() - io_post_time);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Navigation.NavigationURLLoaderImplIOPostTime",
      io_to_ui_time_ + total_ui_to_io_time, kMinTime, kMaxTime, kBuckets);

  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderImpl", this, "success", true);

  if (is_download)
    download_policy_.RecordHistogram();

  // TODO(scottmg): This needs to do more of what
  // NavigationResourceHandler::OnResponseStarted() does.
  delegate_->OnResponseStarted(
      std::move(url_loader_client_endpoints), std::move(response_head),
      std::move(response_body),
      GlobalRequestID(global_request_id.child_id, global_request_id.request_id),
      is_download, download_policy_,
      request_controller_->TakeSubresourceLoaderParams());
}

void NavigationURLLoaderImpl::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    scoped_refptr<network::ResourceResponse> response_head,
    base::Time io_post_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  io_to_ui_time_ += (base::Time::Now() - io_post_time);
  delegate_->OnRequestRedirected(redirect_info, std::move(response_head));
}

void NavigationURLLoaderImpl::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderImpl", this, "success", false);
  delegate_->OnRequestFailed(status);
}

// static
void NavigationURLLoaderImpl::SetURLLoaderFactoryInterceptorForTesting(
    const URLLoaderFactoryInterceptor& interceptor) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_loader_factory_interceptor.Get() = interceptor;
}

// static
void NavigationURLLoaderImpl::CreateURLLoaderFactoryWithHeaderClient(
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        header_client,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
    StoragePartitionImpl* partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->header_client = std::move(header_client);
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_trusted = true;
  params->is_corb_enabled = false;
  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);

  partition->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(factory_receiver), std::move(params));
}

// static
GlobalRequestID NavigationURLLoaderImpl::MakeGlobalRequestID() {
  // Start at -2 on the same lines as ResourceDispatcherHostImpl.
  static std::atomic_int s_next_request_id{-2};
  return GlobalRequestID(-1, s_next_request_id--);
}

void NavigationURLLoaderImpl::OnRequestStarted(base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnRequestStarted(timestamp);
}

void NavigationURLLoaderImpl::BindNonNetworkURLLoaderFactoryReceiver(
    int frame_tree_node_id,
    const GURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver) {
  auto it = non_network_url_loader_factories_.find(url.scheme());
  if (it == non_network_url_loader_factories_.end()) {
    DVLOG(1) << "Ignoring request with unknown scheme: " << url.spec();
    return;
  }

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  auto* frame = frame_tree_node->current_frame_host();
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      frame->GetSiteInstance()->GetBrowserContext(), frame,
      frame->GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
      &factory_receiver, nullptr /* header_client */,
      nullptr /* bypass_redirect_checks */);
  it->second->Clone(std::move(factory_receiver));
}

}  // namespace content

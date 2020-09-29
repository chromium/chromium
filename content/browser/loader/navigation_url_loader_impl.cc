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
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
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
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_main_resource_handle_core.h"
#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_request_handler.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/webplugininfo.h"
#include "media/media_buildflags.h"
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
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/record_load_histograms.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

#if defined(OS_ANDROID)
#include "content/browser/android/content_url_loader_factory.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
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

// TODO(kinuko): |request_info| can likely be given as a const ref.
std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    NavigationRequestInfo* request_info,
    int frame_tree_node_id,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer) {
  auto new_request = std::make_unique<network::ResourceRequest>();

  new_request->method = request_info->common_params->method;
  new_request->url = request_info->common_params->url;
  new_request->site_for_cookies =
      request_info->isolation_info.site_for_cookies();
  new_request->force_ignore_site_for_cookies =
      request_info->begin_params->force_ignore_site_for_cookies;
  new_request->trusted_params = network::ResourceRequest::TrustedParams();
  new_request->trusted_params->isolation_info = request_info->isolation_info;
  new_request->trusted_params->cookie_observer = std::move(cookie_observer);
  new_request->trusted_params->client_security_state =
      request_info->client_security_state.Clone();
  new_request->is_main_frame = request_info->is_main_frame;

  net::RequestPriority net_priority = net::HIGHEST;
  if (!request_info->is_main_frame &&
      base::FeatureList::IsEnabled(features::kLowPriorityIframes)) {
    net_priority = net::LOWEST;
  }
  new_request->priority = net_priority;

  new_request->render_frame_id = frame_tree_node_id;

  new_request->request_initiator =
      request_info->common_params->initiator_origin;
  new_request->referrer = request_info->common_params->referrer->url;
  new_request->referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
      request_info->common_params->referrer->policy);
  new_request->headers.AddHeadersFromString(
      request_info->begin_params->headers);
  new_request->cors_exempt_headers = request_info->cors_exempt_headers;

  new_request->resource_type = static_cast<int>(
      request_info->is_main_frame ? blink::mojom::ResourceType::kMainFrame
                                  : blink::mojom::ResourceType::kSubFrame);
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
  new_request->mode = network::mojom::RequestMode::kNavigate;
  new_request->destination = request_info->begin_params->request_destination;

  if (ui::PageTransitionIsWebTriggerable(
          request_info->common_params->transition)) {
    new_request->trusted_params->has_user_activation =
        request_info->common_params->has_user_gesture;
  } else {
    new_request->trusted_params->has_user_activation = true;
  }

  new_request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  new_request->redirect_mode = network::mojom::RedirectMode::kManual;
  new_request->upgrade_if_insecure = request_info->upgrade_if_insecure;
  new_request->throttling_profile_id = request_info->devtools_frame_token;
  new_request->transition_type = request_info->common_params->transition;
  new_request->previews_state = request_info->common_params->previews_state;
  new_request->devtools_request_id =
      request_info->devtools_navigation_token.ToString();
  new_request->obey_origin_policy = request_info->obey_origin_policy;
  if (request_info->begin_params->trust_token_params) {
    new_request->trust_token_params =
        *request_info->begin_params->trust_token_params;
  }
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

const char* FrameAcceptHeaderValue() {
#if BUILDFLAG(ENABLE_AV1_DECODER)
  static const char kFrameAcceptHeaderValueWithAvif[] =
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,"
      "image/webp,image/apng,*/*;q=0.8";
  static const char* accept_value =
      base::FeatureList::IsEnabled(blink::features::kAVIF)
          ? kFrameAcceptHeaderValueWithAvif
          : network::kFrameAcceptHeaderValue;
  return accept_value;
#else
  return network::kFrameAcceptHeaderValue;
#endif
}

}  // namespace

// TODO(kinuko): Fix the method ordering and move these methods after the ctor.
NavigationURLLoaderImpl::~NavigationURLLoaderImpl() {
  // If neither OnCompleted nor OnReceivedResponse has been invoked, the
  // request was canceled before receiving a response, so log a cancellation.
  // Results after receiving a non-error response are logged in the renderer,
  // if the request is passed to one. If it's a download, or not passed to a
  // renderer for some other reason, results will not be logged for the
  // request. The net::OK check may not be necessary - the case where OK is
  // received without receiving any headers looks broken, anyways.
  if (!received_response_ && (!status_ || status_->error_code != net::OK)) {
    blink::RecordLoadHistograms(
        url::Origin::Create(url_), resource_request_->destination,
        status_ ? status_->error_code : net::ERR_ABORTED);
  }
}

uint32_t NavigationURLLoaderImpl::GetURLLoaderOptions(bool is_main_frame) {
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

void NavigationURLLoaderImpl::Start(
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    AppCacheNavigationHandle* appcache_handle,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    scoped_refptr<SignedExchangePrefetchMetricRecorder>
        signed_exchange_prefetch_metric_recorder,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui,
    std::string accept_langs,
    bool needs_loader_factory_interceptor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!started_);
  DCHECK(!head_);
  head_ = network::mojom::URLResponseHead::New();
  started_ = true;

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NavigationURLLoaderImpl::NotifyRequestStarted,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));

  // TODO(kinuko): This can likely be initialized in the ctor.
  network_loader_factory_ = network_loader_factory;
  if (needs_loader_factory_interceptor && g_loader_factory_interceptor.Get()) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory;
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
        factory.InitWithNewPipeAndPassReceiver();
    g_loader_factory_interceptor.Get().Run(&receiver);
    network_loader_factory_->Clone(std::move(receiver));
    network_loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(factory));
  }

  std::string accept_header_value = FrameAcceptHeaderValue();
  if (signed_exchange_utils::IsSignedExchangeHandlingEnabled(
          browser_context_)) {
    accept_header_value.append(kAcceptHeaderSignedExchangeSuffix);
  }
  resource_request_->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                       accept_header_value);

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
  if (request_info_->common_params->url.SchemeIsBlob() &&
      request_info_->blob_url_loader_factory) {
    url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
        network::SharedURLLoaderFactory::Create(
            std::move(request_info_->blob_url_loader_factory)),
        CreateURLLoaderThrottles(), 0 /* routing_id */,
        global_request_id_.request_id, network::mojom::kURLLoadOptionNone,
        resource_request_.get(), this, kNavigationUrlLoaderTrafficAnnotation,
        base::ThreadTaskRunnerHandle::Get());
    return;
  }

  CreateInterceptors(appcache_handle, prefetched_signed_exchange_cache,
                     signed_exchange_prefetch_metric_recorder, accept_langs);
  Restart();
}

void NavigationURLLoaderImpl::CreateInterceptors(
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
                url_, frame_tree_node_id_);
    if (prefetched_signed_exchange_interceptor) {
      interceptors_.push_back(
          std::move(prefetched_signed_exchange_interceptor));
    }
  }

  // Set up an interceptor for service workers.
  if (service_worker_handle_) {
    auto service_worker_interceptor =
        ServiceWorkerMainResourceLoaderInterceptor::CreateForNavigation(
            resource_request_->url, service_worker_handle_->AsWeakPtr(),
            *request_info_);
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
        *request_info_, network_loader_factory_,
        std::move(signed_exchange_prefetch_metric_recorder),
        std::move(accept_langs)));
  }

  // See if embedders want to add interceptors.
  std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>
      browser_interceptors =
          GetContentClient()->browser()->WillCreateURLLoaderRequestInterceptors(
              navigation_ui_data_.get(), frame_tree_node_id_,
              network_loader_factory_);
  if (!browser_interceptors.empty()) {
    for (auto& browser_interceptor : browser_interceptors) {
      interceptors_.push_back(
          std::make_unique<NavigationLoaderInterceptorBrowserContainer>(
              std::move(browser_interceptor)));
    }
  }
}

void NavigationURLLoaderImpl::Restart() {
  // Clear |url_loader_| if it's not the default one (network). This allows
  // the restarted request to use a new loader, instead of, e.g., reusing the
  // AppCache or service worker loader. For an optimization, we keep and reuse
  // the default url loader if the all |interceptors_| doesn't handle the
  // redirected request. If the network service is enabled, reset the loader
  // if the redirected URL's scheme and the previous URL scheme don't match in
  // their use or disuse of the network service loader.
  if (!default_loader_used_ ||
      (url_chain_.size() > 1 &&
       blink::network_utils::IsURLHandledByNetworkService(
           url_chain_[url_chain_.size() - 1]) !=
           blink::network_utils::IsURLHandledByNetworkService(
               url_chain_[url_chain_.size() - 2]))) {
    if (url_loader_)
      url_loader_->ResetForFollowRedirect();
    url_loader_.reset();
  }
  interceptor_index_ = 0;
  received_response_ = false;
  head_ = network::mojom::URLResponseHead::New();
  MaybeStartLoader(nullptr /* interceptor */, {} /* single_request_factory */);
}

void NavigationURLLoaderImpl::MaybeStartLoader(
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
    // If |url_loader_| already exists, this means we are following a redirect
    // using an interceptor. In this case we should make sure to reset the
    // loader, similar to what is done in Restart().
    if (url_loader_)
      url_loader_->ResetForFollowRedirect();
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
        base::BindOnce(&NavigationURLLoaderImpl::MaybeStartLoader,
                       base::Unretained(this), next_interceptor),
        base::BindOnce(
            &NavigationURLLoaderImpl::FallbackToNonInterceptedRequest,
            base::Unretained(this)));
    return;
  }

  // If we already have the default |url_loader_| we must come here after a
  // redirect. No interceptors wanted to intercept the redirected request, so
  // let the loader just follow the redirect.
  if (url_loader_) {
    DCHECK(!redirect_info_.new_url.is_empty());
    url_loader_->FollowRedirect(
        std::move(url_loader_removed_headers_),
        std::move(url_loader_modified_headers_),
        std::move(url_loader_modified_cors_exempt_headers_));
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

void NavigationURLLoaderImpl::FallbackToNonInterceptedRequest(
    bool reset_subresource_loader_params) {
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
NavigationURLLoaderImpl::PrepareForNonInterceptedRequest(
    uint32_t* out_options) {
  // TODO(https://crbug.com/796425): We temporarily wrap raw
  // mojom::URLLoaderFactory pointers into SharedURLLoaderFactory. Need to
  // further refactor the factory getters to avoid this.
  scoped_refptr<network::SharedURLLoaderFactory> factory;

  if (!blink::network_utils::IsURLHandledByNetworkService(
          resource_request_->url)) {
    if (known_schemes_.find(resource_request_->url.scheme()) ==
        known_schemes_.end()) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory;
      bool handled = GetContentClient()->browser()->HandleExternalProtocol(
          resource_request_->url, web_contents_getter_,
          ChildProcessHost::kInvalidUniqueID, navigation_ui_data_.get(),
          resource_request_->resource_type ==
              static_cast<int>(blink::mojom::ResourceType::kMainFrame),
          static_cast<ui::PageTransition>(resource_request_->transition_type),
          resource_request_->has_user_gesture,
          resource_request_->request_initiator, &loader_factory);

      if (loader_factory) {
        factory = base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(loader_factory));
      } else {
        factory = base::MakeRefCounted<SingleRequestURLLoaderFactory>(
            base::BindOnce(UnknownSchemeCallback, handled));
      }
    } else {
      mojo::Remote<network::mojom::URLLoaderFactory>& non_network_factory =
          non_network_url_loader_factory_remotes_[resource_request_->url
                                                      .scheme()];
      if (!non_network_factory.is_bound()) {
        BindAndInterceptNonNetworkURLLoaderFactoryReceiver(
            resource_request_->url,
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
      // Replace the network factory with the proxied version since this may
      // need to be used in redirects, and we've already consumed
      // |proxied_factory_receiver_|.
      network_loader_factory_ =
          base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
              std::move(proxied_factory_remote_));
    }
    factory = network_loader_factory_;
  }
  url_chain_.push_back(resource_request_->url);
  *out_options = GetURLLoaderOptions(
      resource_request_->resource_type ==
      static_cast<int>(blink::mojom::ResourceType::kMainFrame));
  return factory;
}

void NavigationURLLoaderImpl::FollowRedirectInternal(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    blink::PreviewsState new_previews_state,
    base::Time ui_post_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!redirect_info_.new_url.is_empty());

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
  resource_request_->trusted_params->isolation_info =
      resource_request_->trusted_params->isolation_info.CreateForRedirect(
          url::Origin::Create(resource_request_->url));

  resource_request_->referrer = GURL(redirect_info_.new_referrer);
  resource_request_->referrer_policy = redirect_info_.new_referrer_policy;
  resource_request_->previews_state = new_previews_state;
  url_chain_.push_back(redirect_info_.new_url);

  // Need to cache modified headers for |url_loader_| since it doesn't use
  // |resource_request_| during redirect.
  url_loader_removed_headers_ = removed_headers;
  url_loader_modified_headers_ = modified_headers;
  url_loader_modified_cors_exempt_headers_ = modified_cors_exempt_headers;

  // Don't send Accept: application/signed-exchange for fallback redirects.
  if (redirect_info_.is_signed_exchange_fallback_redirect) {
    url_loader_modified_headers_.SetHeader(net::HttpRequestHeaders::kAccept,
                                           FrameAcceptHeaderValue());
    resource_request_->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                         FrameAcceptHeaderValue());
  }

  Restart();
}

void NavigationURLLoaderImpl::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  head_ = std::move(head);
  on_receive_response_time_ = base::TimeTicks::Now();
}

void NavigationURLLoaderImpl::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle response_body) {
  if (!on_receive_response_time_.is_null()) {
    UMA_HISTOGRAM_TIMES(
        "Navigation.OnReceiveResponseToOnStartLoadingResponseBody",
        base::TimeTicks::Now() - on_receive_response_time_);
  }

  response_body_ = std::move(response_body);
  received_response_ = true;

  // If the default loader (network) was used to handle the URL load request
  // we need to see if the interceptors want to potentially create a new
  // loader for the response. e.g. AppCache.
  if (MaybeCreateLoaderForResponse(&head_))
    return;

  network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints;

  if (url_loader_) {
    url_loader_client_endpoints = url_loader_->Unbind();
  } else {
    url_loader_client_endpoints = network::mojom::URLLoaderClientEndpoints::New(
        std::move(response_url_loader_), response_loader_receiver_.Unbind());
  }

  // 304 responses should abort the navigation, rather than display the page.
  // This needs to be after the URLLoader has been moved to
  // |url_loader_client_endpoints| in order to abort the request, to avoid
  // receiving unexpected call.
  if (head_->headers &&
      head_->headers->response_code() == net::HTTP_NOT_MODIFIED) {
    // Call CancelWithError instead of OnComplete so that if there is an
    // intercepting URLLoaderFactory it gets notified.
    url_loader_->CancelWithError(
        net::ERR_ABORTED,
        base::StringPiece(base::NumberToString(net::ERR_ABORTED)));
    return;
  }

  bool must_download = download_utils::MustDownload(url_, head_->headers.get(),
                                                    head_->mime_type);
  bool known_mime_type = blink::IsSupportedMimeType(head_->mime_type);

#if BUILDFLAG(ENABLE_PLUGINS)
    if (!head_->intercepted_by_plugin && !must_download && !known_mime_type) {
      // No plugin throttles intercepted the response. Ask if the plugin
      // registered to PluginService wants to handle the request.
      CheckPluginAndContinueOnReceiveResponse(
          std::move(head_), std::move(url_loader_client_endpoints),
          true /* is_download_if_not_handled_by_plugin */,
          std::vector<WebPluginInfo>());
      return;
    }
#endif

    // When a plugin intercepted the response, we don't want to download it.
    bool is_download =
        !head_->intercepted_by_plugin && (must_download || !known_mime_type);

    CallOnReceivedResponse(std::move(head_),
                           std::move(url_loader_client_endpoints), is_download);
}

#if BUILDFLAG(ENABLE_PLUGINS)
void NavigationURLLoaderImpl::CheckPluginAndContinueOnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
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
      head->mime_type, false /* allow_wildcard */, &stale, &plugin, nullptr);

  if (stale) {
    // Refresh the plugins asynchronously.
    PluginService::GetInstance()->GetPlugins(base::BindOnce(
        &NavigationURLLoaderImpl::CheckPluginAndContinueOnReceiveResponse,
        weak_factory_.GetWeakPtr(), std::move(head),
        std::move(url_loader_client_endpoints),
        is_download_if_not_handled_by_plugin));
    return;
  }

  bool is_download = !has_plugin && is_download_if_not_handled_by_plugin;
  CallOnReceivedResponse(std::move(head),
                         std::move(url_loader_client_endpoints), is_download);
}
#endif

void NavigationURLLoaderImpl::CallOnReceivedResponse(
    network::mojom::URLResponseHeadPtr head,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    bool is_download) {
  network::mojom::URLResponseHead* head_ptr = head.get();
  auto on_receive_response = base::BindOnce(
      &NavigationURLLoaderImpl::NotifyResponseStarted,
      weak_factory_.GetWeakPtr(), std::move(head),
      std::move(url_loader_client_endpoints), std::move(response_body_),
      global_request_id_, is_download);

  ParseHeaders(url_, head_ptr, std::move(on_receive_response));
}

void NavigationURLLoaderImpl::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  net::Error error = net::OK;
  if (!bypass_redirect_checks_ &&
      !IsSafeRedirectTarget(url_, redirect_info.new_url)) {
    error = net::ERR_UNSAFE_REDIRECT;
  } else if (--redirect_limit_ == 0) {
    error = net::ERR_TOO_MANY_REDIRECTS;
    if (redirect_info.is_signed_exchange_fallback_redirect)
      UMA_HISTOGRAM_BOOLEAN("SignedExchange.FallbackRedirectLoop", true);
  }
  if (error != net::OK) {
    if (url_loader_) {
      // Call CancelWithError instead of OnComplete so that if there is an
      // intercepting URLLoaderFactory (created through the embedder's
      // ContentBrowserClient::WillCreateURLLoaderFactory) it gets notified.
      url_loader_->CancelWithError(
          error, base::StringPiece(base::NumberToString(error)));
    } else {
      // TODO(crbug.com/1052242): Make sure ResetWithReason() is called on the
      // original url_loader_.
      OnComplete(network::URLLoaderCompletionStatus(error));
    }
    return;
  }

  // Store the redirect_info for later use in FollowRedirect where we give
  // our interceptors_ a chance to intercept the request for the new location.
  redirect_info_ = redirect_info;

  url_ = redirect_info.new_url;

  network::mojom::URLResponseHead* head_ptr = head.get();
  auto on_receive_redirect = base::BindOnce(
      &NavigationURLLoaderImpl::NotifyRequestRedirected,
      weak_factory_.GetWeakPtr(), redirect_info, std::move(head));
  ParseHeaders(url_, head_ptr, std::move(on_receive_redirect));
}

void NavigationURLLoaderImpl::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  NOTREACHED();
}

void NavigationURLLoaderImpl::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  NOTREACHED();
}

void NavigationURLLoaderImpl::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
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
  if (!received_response_) {
    auto response = network::mojom::URLResponseHead::New();
    if (MaybeCreateLoaderForResponse(&response))
      return;
  }

  status_ = status;
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NavigationURLLoaderImpl::NotifyRequestFailed,
                                weak_factory_.GetWeakPtr(), status));
}

  // Returns true if an interceptor wants to handle the response, i.e. return a
  // different response. For e.g. AppCache may have fallback content.
bool NavigationURLLoaderImpl::MaybeCreateLoaderForResponse(
    network::mojom::URLResponseHeadPtr* response) {
  if (!default_loader_used_ &&
      !web_bundle_utils::CanLoadAsWebBundle(url_, (*response)->mime_type)) {
    return false;
  }
  for (auto& interceptor : interceptors_) {
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        response_client_receiver;
    bool skip_other_interceptors = false;
    bool will_return_unsafe_redirect = false;
    if (interceptor->MaybeCreateLoaderForResponse(
            *resource_request_, response, &response_body_,
            &response_url_loader_, &response_client_receiver, url_loader_.get(),
            &skip_other_interceptors, &will_return_unsafe_redirect)) {
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
        new_interceptors.push_back(std::move(interceptor));
        new_interceptors.swap(interceptors_);
        // Reset the state of ServiceWorkerContainerHost.
        // Currently we don't support Service Worker in Signed Exchange
        // pages. The page will not be controlled by service workers. And
        // Service Worker related APIs will fail with NoDocumentURL error.
        // TODO(crbug/898733): Support SignedExchange loading and Service
        // Worker integration.
        if (service_worker_handle_) {
          RunOrPostTaskOnThread(
              FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
              base::BindOnce(
                  [](ServiceWorkerMainResourceHandleCore* core) {
                    base::WeakPtr<ServiceWorkerContainerHost> container_host =
                        core->container_host();
                    if (container_host) {
                      container_host->SetControllerRegistration(
                          nullptr, false /* notify_controllerchange */);
                      container_host->UpdateUrls(GURL(), net::SiteForCookies(),
                                                 base::nullopt);
                    }
                  },
                  // Unretained() is safe because the handle owns the core,
                  // and core gets deleted on the core thread in a task that
                  // must occur after this task.
                  base::Unretained(service_worker_handle_->core())));
        }
      }
      return true;
    }
  }
  return false;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
NavigationURLLoaderImpl::CreateURLLoaderThrottles() {
  return CreateContentBrowserURLLoaderThrottles(
      *resource_request_, browser_context_, web_contents_getter_,
      navigation_ui_data_.get(), frame_tree_node_id_);
}

std::unique_ptr<SignedExchangeRequestHandler>
NavigationURLLoaderImpl::CreateSignedExchangeRequestHandler(
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
      base::BindRepeating(&NavigationURLLoaderImpl::CreateURLLoaderThrottles,
                          base::Unretained(this)),
      std::move(signed_exchange_prefetch_metric_recorder),
      std::move(accept_langs));
}

void NavigationURLLoaderImpl::ParseHeaders(
    const GURL& url,
    network::mojom::URLResponseHead* head,
    base::OnceClosure continuation) {
  // The main path:
  // --------------
  // The ParsedHeaders are already provided. No more work needed.
  //
  // Currently used when the response is coming from:
  // - Network
  // - ServiceWorker
  // - WebUI
  if (head->parsed_headers) {
    std::move(continuation).Run();
    return;
  }

  // As an optimization, when we know the parsed headers will be empty, we can
  // skip the network process roundtrip.
  // TODO(arthursonzogni): If there are any performance issues, consider
  // checking the |head->headers| contains at least one header to be parsed.
  if (!head->headers) {
    head->parsed_headers = network::mojom::ParsedHeaders::New();
    std::move(continuation).Run();
    return;
  }

  auto assign = [](base::OnceClosure continuation,
                   network::mojom::URLResponseHead* head,
                   network::mojom::ParsedHeadersPtr parsed_headers) {
    head->parsed_headers = std::move(parsed_headers);
    std::move(continuation).Run();
  };

  storage_partition_->GetNetworkContext()->ParseHeaders(
      url, head->headers,
      base::BindOnce(assign, std::move(continuation), head));
}

// TODO(https://crbug.com/790734): pass |navigation_ui_data| along with the
// request so that it could be modified.
NavigationURLLoaderImpl::NavigationURLLoaderImpl(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerMainResourceHandle* service_worker_handle,
    AppCacheNavigationHandle* appcache_handle,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    NavigationURLLoaderDelegate* delegate,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
    std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
        initial_interceptors)
    : delegate_(delegate),
      browser_context_(browser_context),
      storage_partition_(static_cast<StoragePartitionImpl*>(storage_partition)),
      service_worker_handle_(service_worker_handle),
      request_info_(std::move(request_info)),
      url_(request_info_->common_params->url),
      frame_tree_node_id_(request_info_->frame_tree_node_id),
      global_request_id_(GlobalRequestID::MakeBrowserInitiated()),
      web_contents_getter_(
          base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                              frame_tree_node_id_)),
      navigation_ui_data_(std::move(navigation_ui_data)),
      interceptors_(std::move(initial_interceptors)),
      download_policy_(request_info_->common_params->download_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", this,
      request_info_->common_params->navigation_start, "FrameTreeNode id",
      frame_tree_node_id_);

  scoped_refptr<SignedExchangePrefetchMetricRecorder>
      signed_exchange_prefetch_metric_recorder =
          storage_partition_->GetPrefetchURLLoaderService()
              ->signed_exchange_prefetch_metric_recorder();

  resource_request_ = CreateResourceRequest(
      request_info_.get(), frame_tree_node_id_, std::move(cookie_observer));

  std::string accept_langs =
      GetContentClient()->browser()->GetAcceptLangs(browser_context_);

  // Check if a web UI scheme wants to handle this request.
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  const auto& schemes = URLDataManagerBackend::GetWebUISchemes();
  std::string scheme = resource_request_->url.scheme();
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui;
  if (base::Contains(schemes, scheme)) {
    DCHECK(frame_tree_node);
    DCHECK(frame_tree_node->navigation_request());
    auto factory_receiver = factory_for_webui.InitWithNewPipeAndPassReceiver();
    GetContentClient()->browser()->WillCreateURLLoaderFactory(
        browser_context_, frame_tree_node->current_frame_host(),
        frame_tree_node->current_frame_host()->GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
        frame_tree_node->navigation_request()->GetNavigationId(),
        base::UkmSourceId::FromInt64(
            frame_tree_node->navigation_request()->GetNextPageUkmSourceId()),
        &factory_receiver, nullptr /* header_client */,
        nullptr /* bypass_redirect_checks */, nullptr /* disable_secure_dns */,
        nullptr /* factory_override */);

    mojo::Remote<network::mojom::URLLoaderFactory> direct_factory_for_webui(
        CreateWebUIURLLoaderFactory(frame_tree_node->current_frame_host(),
                                    scheme, {}));
    direct_factory_for_webui->Clone(std::move(factory_receiver));
  }

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  // |frame_tree_node| may be null in some unit test environments.
  if (frame_tree_node) {
    // Initialize proxied factory remote/receiver if necessary.
    // This also populates |bypass_redirect_checks_|.
    DCHECK(frame_tree_node->navigation_request());

    GetContentClient()
        ->browser()
        ->RegisterNonNetworkNavigationURLLoaderFactories(
            frame_tree_node_id_,
            base::UkmSourceId::FromInt64(frame_tree_node->navigation_request()
                                             ->GetNextPageUkmSourceId()),
            &non_network_uniquely_owned_factories_,
            &non_network_url_loader_factories_);

    // The embedder may want to proxy all network-bound URLLoaderFactory
    // receivers that it can. If it elects to do so, those proxies will be
    // connected when loader is created if the request type supports proxying.
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory;
    auto factory_receiver = pending_factory.InitWithNewPipeAndPassReceiver();
    // Here we give nullptr for |factory_override|, because CORS is no-op for
    // navigations.
    bool use_proxy = GetContentClient()->browser()->WillCreateURLLoaderFactory(
        browser_context_, frame_tree_node->current_frame_host(),
        frame_tree_node->current_frame_host()->GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
        frame_tree_node->navigation_request()->GetNavigationId(),
        base::UkmSourceId::FromInt64(
            frame_tree_node->navigation_request()->GetNextPageUkmSourceId()),
        &factory_receiver, &header_client, &bypass_redirect_checks_,
        nullptr /* disable_secure_dns */, nullptr /* factory_override */);
    if (devtools_instrumentation::WillCreateURLLoaderFactory(
            frame_tree_node->current_frame_host(), true /* is_navigation */,
            false /* is_download */, &factory_receiver,
            nullptr /* factory_override */)) {
      use_proxy = true;
    }
    if (use_proxy) {
      proxied_factory_receiver_ = std::move(factory_receiver);
      proxied_factory_remote_ = std::move(pending_factory);
    }

    const std::string storage_domain;
    non_network_url_loader_factories_.emplace(
        url::kFileSystemScheme,
        CreateFileSystemURLLoaderFactory(
            ChildProcessHost::kInvalidUniqueID,
            frame_tree_node->frame_tree_node_id(),
            storage_partition_->GetFileSystemContext(), storage_domain));
  }

  non_network_url_loader_factories_.emplace(url::kAboutScheme,
                                            AboutURLLoaderFactory::Create());

  non_network_url_loader_factories_.emplace(url::kDataScheme,
                                            DataURLLoaderFactory::Create());

  // USER_BLOCKING because this scenario is exactly one of the examples
  // given by the doc comment for USER_BLOCKING:
  // Loading and rendering a web page after the user clicks a link.
  base::TaskPriority file_factory_priority = base::TaskPriority::USER_BLOCKING;
  non_network_url_loader_factories_.emplace(
      url::kFileScheme, FileURLLoaderFactory::Create(
                            browser_context_->GetPath(),
                            browser_context_->GetSharedCorsOriginAccessList(),
                            file_factory_priority));

#if defined(OS_ANDROID)
  non_network_url_loader_factories_.emplace(url::kContentScheme,
                                            ContentURLLoaderFactory::Create());
#endif

  for (auto& iter : non_network_uniquely_owned_factories_)
    known_schemes_.insert(iter.first);
  for (auto& iter : non_network_url_loader_factories_)
    known_schemes_.insert(iter.first);

  bool needs_loader_factory_interceptor = false;
  scoped_refptr<network::SharedURLLoaderFactory> network_factory =
      storage_partition_->GetURLLoaderFactoryForBrowserProcess();
  if (header_client) {
    needs_loader_factory_interceptor = true;
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    CreateURLLoaderFactoryWithHeaderClient(
        std::move(header_client),
        factory_remote.InitWithNewPipeAndPassReceiver(), storage_partition_);
    network_factory =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(factory_remote));
  }

  Start(network_factory, appcache_handle,
        std::move(prefetched_signed_exchange_cache),
        std::move(signed_exchange_prefetch_metric_recorder),
        std::move(factory_for_webui), std::move(accept_langs),
        needs_loader_factory_interceptor);
}

void NavigationURLLoaderImpl::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    blink::PreviewsState new_previews_state) {
  FollowRedirectInternal(removed_headers, modified_headers,
                         modified_cors_exempt_headers, new_previews_state,
                         base::Time::Now());
}

void NavigationURLLoaderImpl::NotifyRequestStarted(base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnRequestStarted(timestamp);
}

void NavigationURLLoaderImpl::NotifyResponseStarted(
    network::mojom::URLResponseHeadPtr response_head,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    mojo::ScopedDataPipeConsumerHandle response_body,
    const GlobalRequestID& global_request_id,
    bool is_download) {
  // TODO(https://crbug.com/1068896): Remove
  // "Navigation.NavigationURLLoaderImplIOPostTime" histogram as well.

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
      is_download, download_policy_, std::move(subresource_loader_params_));
}

void NavigationURLLoaderImpl::NotifyRequestRedirected(
    net::RedirectInfo redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnRequestRedirected(redirect_info, std::move(response_head));
}

void NavigationURLLoaderImpl::NotifyRequestFailed(
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

void NavigationURLLoaderImpl::BindNonNetworkURLLoaderFactoryReceiver(
    const GURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver) {
  auto it = non_network_uniquely_owned_factories_.find(url.scheme());
  if (it != non_network_uniquely_owned_factories_.end()) {
    it->second->Clone(std::move(factory_receiver));
    return;
  }

  auto it2 = non_network_url_loader_factories_.find(url.scheme());
  if (it2 != non_network_url_loader_factories_.end()) {
    mojo::Remote<network::mojom::URLLoaderFactory> remote(
        std::move(it2->second));
    remote->Clone(std::move(factory_receiver));
    non_network_url_loader_factories_.erase(it2);
    return;
  }

  DVLOG(1) << "Ignoring request with unknown scheme: " << url.spec();
}

void NavigationURLLoaderImpl::
    BindAndInterceptNonNetworkURLLoaderFactoryReceiver(
        const GURL& url,
        mojo::PendingReceiver<network::mojom::URLLoaderFactory>
            factory_receiver) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  DCHECK(frame_tree_node);
  DCHECK(frame_tree_node->navigation_request());

  auto* frame = frame_tree_node->current_frame_host();
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      frame->GetSiteInstance()->GetBrowserContext(), frame,
      frame->GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
      frame_tree_node->navigation_request()->GetNavigationId(),
      base::UkmSourceId::FromInt64(
          frame_tree_node->navigation_request()->GetNextPageUkmSourceId()),
      &factory_receiver, nullptr /* header_client */,
      nullptr /* bypass_redirect_checks */, nullptr /* disable_secure_dns */,
      nullptr /* factory_override */);

  // TODO(lukasza, jam): It is unclear why FileURLLoaderFactory is the only
  // non-http factory that allows DevTools intereception.  For comparison all
  // non-WebUI, non-AppCache cases in RFHI::CommitNavigation allow DevTools
  // interception.  Let's try to be more consistent / less ad-hoc.
  if (url.SchemeIs(url::kFileScheme)) {
    if (frame_tree_node) {  // May be nullptr in some unit tests.
      devtools_instrumentation::WillCreateURLLoaderFactory(
          frame, true /* is_navigation */, false /* is_download */,
          &factory_receiver, nullptr /* factory_override */);
    }
  }

  BindNonNetworkURLLoaderFactoryReceiver(url, std::move(factory_receiver));
}

}  // namespace content

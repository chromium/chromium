// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/download/public/common/download_stats.h"
#include "content/browser/about_url_loader_factory.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/loader/navigation_early_hints_manager.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_interceptor.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_request_handler.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/webplugininfo.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/cpp/url_util.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"
#include "third_party/blink/public/common/loader/record_load_histograms.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_router_rule.mojom.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/content_url_loader_factory.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace content {

namespace {

BASE_FEATURE(kSkipUnnecessaryThreadHopsForParseHeaders,
             "SkipUnnecessaryThreadHopsForParseHeaders",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
                std::move(callback).Run(NavigationLoaderInterceptor::Result(
                    base::MakeRefCounted<
                        network::SingleRequestURLLoaderFactory>(
                        std::move(handler)),
                    /*subresource_loader_params=*/{}));
              } else {
                std::move(callback).Run(std::nullopt);
              }
            },
            std::move(callback)));
  }

  bool MaybeCreateLoaderForResponse(
      const network::URLLoaderCompletionStatus& status,
      const network::ResourceRequest& request,
      network::mojom::URLResponseHeadPtr* response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      mojo::PendingRemote<network::mojom::URLLoader>* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors) override {
    return browser_interceptor_->MaybeCreateLoaderForResponse(
        status, request, response_head, response_body, loader, client_receiver,
        url_loader);
  }

 private:
  std::unique_ptr<URLLoaderRequestInterceptor> browser_interceptor_;
};

class NavigationTimingThrottle : public blink::URLLoaderThrottle {
 public:
  NavigationTimingThrottle(bool is_outermost_main_frame, base::TimeTicks start)
      : is_outermost_main_frame_(is_outermost_main_frame), start_(start) {}

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    base::UmaHistogramTimes(
        base::StrCat({"Navigation.LoaderCreateToRequestStart.",
                      is_outermost_main_frame_ ? "MainFrame" : "Subframe"}),
        base::TimeTicks::Now() - start_);
  }

 private:
  bool is_outermost_main_frame_;
  base::TimeTicks start_;
};

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
      comments:
        "Chrome would be unable to navigate to websites without this type of "
        "request. Using either URLBlocklist or URLAllowlist policies (or a "
        "combination of both) limits the scope of these requests."
      )");

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    const NavigationRequestInfo& request_info,
    FrameTreeNode* frame_tree_node,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
    mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
        trust_token_observer,
    mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
        shared_dictionary_observer,
    mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
    mojo::PendingRemote<network::mojom::AcceptCHFrameObserver>
        accept_ch_frame_observer) {
  auto new_request = std::make_unique<network::ResourceRequest>();

  new_request->method = request_info.common_params->method;
  new_request->url = request_info.common_params->url;
  new_request->navigation_redirect_chain.push_back(new_request->url);
  new_request->site_for_cookies =
      request_info.isolation_info.site_for_cookies();
  new_request->trusted_params = network::ResourceRequest::TrustedParams();
  new_request->trusted_params->isolation_info = request_info.isolation_info;
  new_request->trusted_params->cookie_observer = std::move(cookie_observer);
  new_request->trusted_params->trust_token_observer =
      std::move(trust_token_observer);
  new_request->trusted_params->shared_dictionary_observer =
      std::move(shared_dictionary_observer);
  new_request->trusted_params->url_loader_network_observer =
      std::move(url_loader_network_observer);
  new_request->trusted_params->devtools_observer = std::move(devtools_observer);
  new_request->trusted_params->client_security_state =
      request_info.client_security_state.Clone();
  new_request->trusted_params->accept_ch_frame_observer =
      std::move(accept_ch_frame_observer);
  new_request->trusted_params->allow_cookies_from_browser =
      request_info.allow_cookies_from_browser;
  new_request->is_outermost_main_frame = request_info.is_outermost_main_frame;
  new_request->priority = net::HIGHEST;
  new_request->request_initiator = request_info.common_params->initiator_origin;
  new_request->referrer = request_info.common_params->referrer->url;
  new_request->referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
      request_info.common_params->referrer->policy);
  new_request->headers.AddHeadersFromString(request_info.begin_params->headers);
  new_request->cors_exempt_headers = request_info.cors_exempt_headers;
  new_request->devtools_accepted_stream_types =
      request_info.devtools_accepted_stream_types;
  // For ResourceType purposes, fenced frames are considered a kSubFrame.
  new_request->resource_type =
      static_cast<int>(request_info.is_outermost_main_frame
                           ? blink::mojom::ResourceType::kMainFrame
                           : blink::mojom::ResourceType::kSubFrame);
  // When set, `update_first_party_url_on_redirect` will cause a
  // server-redirect to update the URL used to determine if cookies are
  // first-party. Since fenced frames are main frames in terms of cookie
  // partitioning, this needs to be `is_main_frame` rather than
  // `is_outermost_main_frame`.
  if (request_info.is_main_frame) {
    new_request->update_first_party_url_on_redirect = true;
  }

  int load_flags = request_info.begin_params->load_flags;
  if (request_info.is_outermost_main_frame) {
    load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;
    load_flags |= net::LOAD_CAN_USE_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  }

  // Sync loads should have maximum priority and should be the only
  // requests that have the ignore limits flag set.
  DCHECK(!(load_flags & net::LOAD_IGNORE_LIMITS));

  new_request->load_flags = load_flags;

  new_request->request_body = request_info.common_params->post_data.get();
  new_request->has_user_gesture = request_info.common_params->has_user_gesture;
  new_request->enable_load_timing = true;
  new_request->mode = network::mojom::RequestMode::kNavigate;
  new_request->destination = request_info.common_params->request_destination;

  if (ui::PageTransitionIsWebTriggerable(
          ui::PageTransitionFromInt(request_info.common_params->transition))) {
    new_request->trusted_params->has_user_activation =
        request_info.common_params->has_user_gesture;
  } else {
    new_request->trusted_params->has_user_activation = true;
  }

  new_request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  new_request->redirect_mode = network::mojom::RedirectMode::kManual;
  new_request->upgrade_if_insecure = request_info.upgrade_if_insecure;
  new_request->throttling_profile_id = request_info.devtools_frame_token;
  new_request->transition_type = request_info.common_params->transition;
  devtools_instrumentation::MaybeAssignResourceRequestId(
      frame_tree_node, request_info.devtools_navigation_token.ToString(),
      *new_request);
  if (request_info.begin_params->trust_token_params) {
    new_request->trust_token_params =
        *request_info.begin_params->trust_token_params;
  }

  new_request->storage_access_api_status =
      request_info.begin_params->storage_access_api_status;

  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromFrameTreeNodeId(frame_tree_node->frame_tree_node_id()));
  new_request->attribution_reporting_support =
      web_contents ? web_contents->GetAttributionSupport()
                   : AttributionManager::GetAttributionSupport(
                         /*client_os_disabled=*/false);

  new_request->attribution_reporting_eligibility =
      request_info.begin_params->impression.has_value()
          ? network::mojom::AttributionReportingEligibility::kNavigationSource
          : network::mojom::AttributionReportingEligibility::kUnset;

  new_request->shared_storage_writable_eligible =
      request_info.shared_storage_writable_eligible;
  new_request->is_ad_tagged = request_info.is_ad_tagged;

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

void LogQueueTimeHistogram(std::string_view name,
                           bool is_outermost_main_frame) {
  auto* task = base::TaskAnnotator::CurrentTaskForThread();
  // Only log for non-delayed tasks with a valid queue_time.
  if (!task || task->queue_time.is_null() || !task->delayed_run_time.is_null())
    return;

  base::UmaHistogramTimes(
      base::StrCat(
          {name, is_outermost_main_frame ? ".MainFrame" : ".Subframe"}),
      base::TimeTicks::Now() - task->queue_time);
}

void LogAcceptCHFrameStatus(AcceptCHFrameRestart status) {
  base::UmaHistogramEnumeration("ClientHints.AcceptCHFrame", status);
}

bool IsSameOriginRedirect(const std::vector<GURL>& url_chain) {
  if (url_chain.size() < 2) {
    return false;
  }

  auto previous_origin = url::Origin::Create(url_chain[url_chain.size() - 2]);
  return previous_origin.IsSameOriginWith(url_chain[url_chain.size() - 1]);
}

// TODO(https://crbug.com/346000235) there is a known failure with extensions
// See test: ExtensionWebRequestApiTestWithContextType.HSTSUpgradeAfterRedirect
// We still want to check this in debug mode to avoid further regressions, but
// because it was affecting to many developers with DCHECK_IS_ON() this was
// downgraded as a DEBUG only check.
// After getting the approval to rewrite the CSP parser in Rust, we should be
// able to remove "pre-parsing" of CSP in the network process and directly
// parse them later in the browser process. This will make this check to become
// unnecessary.
#ifndef NDEBUG
void CheckParsedHeadersEquals(const network::mojom::ParsedHeadersPtr& lhs,
                              const network::mojom::ParsedHeadersPtr& rhs,
                              const GURL& url) {
  // If we're running this function it means we're re-parsing the headers from
  // cache and checking if they equal the prior parsing results. As the
  // Clear-Site-Data header isn't cached we want to be sure not to fail just
  // because the two parsing results mismatch in this expected way.
  network::mojom::ParsedHeadersPtr adjusted_lhs = lhs->Clone();
  if (rhs->client_hints_ignored_due_to_clear_site_data_header) {
    CHECK(!rhs->accept_ch);
    CHECK(!rhs->critical_ch);
    adjusted_lhs->accept_ch = std::nullopt;
    adjusted_lhs->critical_ch = std::nullopt;
    adjusted_lhs->client_hints_ignored_due_to_clear_site_data_header = true;
  }
  if (mojo::Equals(adjusted_lhs, rhs)) {
    return;
  }

  // TODO(crbug.com/40864513) Remove this instrumentation once fixed.
  auto to_string = [](const auto& policies) {
    std::string out;
    for (const auto& csp : policies) {
      out += csp->header->header_value + " | ";
    }
    return out;
  };
  SCOPED_CRASH_KEY_STRING256("bug1362779", "csp_adjusted_lhs",
                             to_string(adjusted_lhs->content_security_policy));
  SCOPED_CRASH_KEY_STRING256("bug1362779", "csp_rhs",
                             to_string(rhs->content_security_policy));
  SCOPED_CRASH_KEY_STRING32("bug1362779", "url", url.possibly_invalid_spec());

  CHECK(mojo::Equals(adjusted_lhs->content_security_policy,
                     rhs->content_security_policy));
  CHECK(mojo::Equals(adjusted_lhs->allow_csp_from, rhs->allow_csp_from));
  CHECK(mojo::Equals(adjusted_lhs->cross_origin_embedder_policy,
                     rhs->cross_origin_embedder_policy));
  CHECK(mojo::Equals(adjusted_lhs->cross_origin_opener_policy,
                     rhs->cross_origin_opener_policy));
  CHECK(mojo::Equals(adjusted_lhs->document_isolation_policy,
                     rhs->document_isolation_policy));
  CHECK(mojo::Equals(adjusted_lhs->origin_agent_cluster,
                     rhs->origin_agent_cluster));
  CHECK(mojo::Equals(adjusted_lhs->accept_ch, rhs->accept_ch));
  CHECK(mojo::Equals(adjusted_lhs->critical_ch, rhs->critical_ch));
  CHECK_EQ(adjusted_lhs->xfo, rhs->xfo);
  CHECK(mojo::Equals(adjusted_lhs->link_headers, rhs->link_headers));
  CHECK(mojo::Equals(adjusted_lhs->timing_allow_origin,
                     rhs->timing_allow_origin));
  CHECK(mojo::Equals(adjusted_lhs->supports_loading_mode,
                     rhs->supports_loading_mode));
  CHECK(mojo::Equals(adjusted_lhs->reporting_endpoints,
                     rhs->reporting_endpoints));
  CHECK(mojo::Equals(adjusted_lhs->cookie_indices, rhs->cookie_indices));
  CHECK(mojo::Equals(adjusted_lhs->avail_language, rhs->avail_language));
  CHECK(mojo::Equals(adjusted_lhs->content_language, rhs->content_language));
  CHECK(mojo::Equals(adjusted_lhs->no_vary_search_with_parse_error,
                     rhs->no_vary_search_with_parse_error));
  CHECK(mojo::Equals(adjusted_lhs->observe_browsing_topics,
                     rhs->observe_browsing_topics));
  CHECK(mojo::Equals(adjusted_lhs->allow_cross_origin_event_reporting,
                     rhs->allow_cross_origin_event_reporting));
  NOTREACHED_IN_MIGRATION()
      << "The parsed headers don't match, but we don't know which "
         "field does not match. Please add a DCHECK before this one "
         "checking for the missing field.";
}
#endif  // NDEBUG

}  // namespace

// TODO(kinuko): Fix the method ordering and move these methods after the ctor.
NavigationURLLoaderImpl::~NavigationURLLoaderImpl() {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationURLLoaderImpl::~NavigationURLLoaderImpl",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
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

void NavigationURLLoaderImpl::Start() {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationURLLoaderImpl::Start",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!started_);
  started_ = true;

  resource_request_->headers.SetHeader(
      net::HttpRequestHeaders::kAccept,
      FrameAcceptHeaderValue(/*allow_sxg_responses=*/true, browser_context_));

  // If not performing a PDF navigation, allow certain schemes to create loaders
  // directly, bypassing interceptors. (In the case of PDF navigation,
  // interception is required, but these loaders are not; see crbug.com/1253314
  // and crbug.com/1253984.)
  //
  // TODO(crbug.com/40794764): Consider getting rid of these exceptions.
  if (!request_info_->is_pdf) {
    // Requests to WebUI scheme won't get redirected to/from other schemes
    // or be intercepted, so we just let it go here.
    std::string scheme = request_info_->common_params->url.scheme();
    if (base::Contains(URLDataManagerBackend::GetWebUISchemes(), scheme)) {
      FrameTreeNode* frame_tree_node =
          FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
      CHECK(frame_tree_node);
      CHECK(frame_tree_node->navigation_request());

      CreateThrottlingLoaderAndStart(
          url_loader_factory::Create(
              ContentBrowserClient::URLLoaderFactoryType::kNavigation,
              url_loader_factory::TerminalParams::ForNonNetwork(
                  CreateWebUIURLLoaderFactory(
                      frame_tree_node->current_frame_host(), scheme, {}),
                  network::mojom::kBrowserProcessId),
              url_loader_factory::ContentClientParams(
                  browser_context_, frame_tree_node->current_frame_host(),
                  frame_tree_node->current_frame_host()->GetProcess()->GetID(),
                  resource_request_->request_initiator.value_or(url::Origin()),
                  net::IsolationInfo(),
                  ukm::SourceIdObj::FromInt64(ukm_source_id_),
                  /*bypass_redirect_checks=*/nullptr,
                  frame_tree_node->navigation_request()->GetNavigationId(),
                  GetUIThreadTaskRunner(
                      {BrowserTaskType::kNavigationNetworkResponse}))),
          /*additional_throttles=*/{});
      return;
    }

    // Requests to Blob scheme won't get redirected to/from other schemes or be
    // intercepted, so we just let it go here.
    if (request_info_->common_params->url.SchemeIsBlob() &&
        request_info_->blob_url_loader_factory) {
      CreateThrottlingLoaderAndStart(
          network::SharedURLLoaderFactory::Create(
              std::move(request_info_->blob_url_loader_factory)),
          /*additional_throttles=*/{});
      return;
    }
  }

  CreateInterceptors();
  Restart();
}

void NavigationURLLoaderImpl::CreateInterceptors() {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationURLLoaderImpl::CreateInterceptors",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (prefetched_signed_exchange_cache_) {
    std::unique_ptr<NavigationLoaderInterceptor>
        prefetched_signed_exchange_interceptor =
            prefetched_signed_exchange_cache_->MaybeCreateInterceptor(
                url_, frame_tree_node_id_,
                resource_request_->trusted_params->isolation_info);
    prefetched_signed_exchange_cache_.reset();
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
    if (service_worker_interceptor) {
      interceptors_.push_back(std::move(service_worker_interceptor));
    }
  }

  // Set-up an interceptor for SignedExchange handling if it is enabled.
  if (signed_exchange_utils::IsSignedExchangeHandlingEnabled(
          browser_context_)) {
    interceptors_.push_back(CreateSignedExchangeRequestHandler(
        *request_info_, network_loader_factory_));
  }

  // Set up an interceptor for prefetch.
  interceptors_.push_back(
      std::make_unique<PrefetchURLLoaderInterceptor>(
          frame_tree_node_id_, request_info_->initiator_document_token,
          request_info_->prefetch_serving_page_metrics_container));

  // See if embedders want to add interceptors.
  std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>
      browser_interceptors =
          GetContentClient()->browser()->WillCreateURLLoaderRequestInterceptors(
              navigation_ui_data_.get(), frame_tree_node_id_,
              request_info_->navigation_id,
              request_info_->force_no_https_upgrade,
              GetUIThreadTaskRunner(
                  {BrowserTaskType::kNavigationNetworkResponse}));
  if (!browser_interceptors.empty()) {
    for (auto& browser_interceptor : browser_interceptors) {
      interceptors_.push_back(
          std::make_unique<NavigationLoaderInterceptorBrowserContainer>(
              std::move(browser_interceptor)));
    }
  }
}

void NavigationURLLoaderImpl::Restart() {
  // Cancel all inflight early hints preloads except for same origin redirects.
  if (!IsSameOriginRedirect(resource_request_->navigation_redirect_chain)) {
    early_hints_manager_.reset();
  }

  // Clear `url_loader_` if it's not the default one (network). This allows
  // the restarted request to use a new loader, instead of, e.g., reusing the
  // service worker loader. For an optimization, we keep and reuse
  // the default url loader if the all `interceptors_` doesn't handle the
  // redirected request. If the network service is enabled, reset the loader
  // if the redirected URL's scheme and the previous URL scheme don't match in
  // their use or disuse of the network service loader.
  if (!default_loader_used_ ||
      (resource_request_->navigation_redirect_chain.size() > 1 &&
       network::IsURLHandledByNetworkService(
           resource_request_->navigation_redirect_chain
               [resource_request_->navigation_redirect_chain.size() - 1]) !=
           network::IsURLHandledByNetworkService(
               resource_request_->navigation_redirect_chain
                   [resource_request_->navigation_redirect_chain.size() -
                    2]))) {
    if (url_loader_) {
      url_loader_->ResetForFollowRedirect(
          *resource_request_.get(), url_loader_removed_headers_,
          url_loader_modified_headers_,
          url_loader_modified_cors_exempt_headers_);
      url_loader_removed_headers_.clear();
      url_loader_modified_headers_.Clear();
      url_loader_modified_cors_exempt_headers_.Clear();
    }
    url_loader_.reset();
  }
  received_response_ = false;
  head_update_params_ = ResponseHeadUpdateParams();
  MaybeStartLoader(/*next_interceptor_index=*/0,
                   /*interceptor_result=*/std::nullopt);
}

void NavigationURLLoaderImpl::MaybeStartLoader(
    size_t next_interceptor_index,
    std::optional<NavigationLoaderInterceptor::Result> interceptor_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(started_);

  if (interceptor_result) {
    subresource_loader_params_ =
        std::move(interceptor_result->subresource_loader_params);
    if (!interceptor_result->single_request_factory) {
      // Skip the subsequent interceptors and start with the default behavior.
      //
      // Here `subresource_loader_params_` can still have non-default values
      // e.g. when there's a controlling service worker that doesn't have a
      // fetch event handler so it doesn't intercept requests.
      StartNonInterceptedRequest(
          std::move(interceptor_result->response_head_update_params));
      return;
    }

    // Intercept the request with `interceptor_result->single_request_factory`.
    StartInterceptedRequest(
        std::move(interceptor_result->single_request_factory));
    return;
  }

  subresource_loader_params_ = {};

  if (next_interceptor_index >= interceptors_.size()) {
    // All interceptors have been checked and none has elected to handle the
    // request. Start with the default behavior.
    StartNonInterceptedRequest(ResponseHeadUpdateParams());
    return;
  }

  // Fallback to the next interceptor.
  auto* next_interceptor = interceptors_[next_interceptor_index].get();
  next_interceptor->MaybeCreateLoader(
      *resource_request_, browser_context_,
      base::BindOnce(&NavigationURLLoaderImpl::MaybeStartLoader,
                     weak_factory_.GetWeakPtr(), next_interceptor_index + 1),
      base::BindOnce(&NavigationURLLoaderImpl::FallbackToNonInterceptedRequest,
                     weak_factory_.GetWeakPtr()));
}

void NavigationURLLoaderImpl::StartInterceptedRequest(
    scoped_refptr<network::SharedURLLoaderFactory> single_request_factory) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> additional_throttles;
  // Intercepted requests need MimeSniffingThrottle to do mime sniffing.
  // Non-intercepted requests usually go through the regular network
  // URLLoader, which does mime sniffing.
  additional_throttles.push_back(std::make_unique<blink::MimeSniffingThrottle>(
      GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse})));

  default_loader_used_ = false;
  // If `url_loader_` already exists, this means we are following a redirect
  // using an interceptor. In this case we should make sure to reset the
  // loader, similar to what is done in Restart().
  if (url_loader_) {
    url_loader_->ResetForFollowRedirect(
        *resource_request_.get(), url_loader_removed_headers_,
        url_loader_modified_headers_, url_loader_modified_cors_exempt_headers_);
    url_loader_removed_headers_.clear();
    url_loader_modified_headers_.Clear();
    url_loader_modified_cors_exempt_headers_.Clear();
    url_loader_.reset();
  }

  CreateThrottlingLoaderAndStart(std::move(single_request_factory),
                                 std::move(additional_throttles));
}

void NavigationURLLoaderImpl::StartNonInterceptedRequest(
    ResponseHeadUpdateParams head_update_params) {
  // If we already have the default `url_loader_` we must come here after a
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

  head_update_params_ = std::move(head_update_params);
  scoped_refptr<network::SharedURLLoaderFactory> factory;
  if (network::IsURLHandledByNetworkService(resource_request_->url)) {
    factory = network_loader_factory_;
    default_loader_used_ = true;
  } else {
    factory = GetOrCreateNonNetworkLoaderFactory();
  }

  response_loader_receiver_.reset();
  CreateThrottlingLoaderAndStart(std::move(factory),
                                 /*additional_throttles=*/{});
}
void NavigationURLLoaderImpl::FallbackToNonInterceptedRequest(
    ResponseHeadUpdateParams head_update_params) {
  head_update_params_ = std::move(head_update_params);
  scoped_refptr<network::SharedURLLoaderFactory> factory;
  if (network::IsURLHandledByNetworkService(resource_request_->url)) {
    factory = network_loader_factory_;
    default_loader_used_ = true;
  } else {
    factory = GetOrCreateNonNetworkLoaderFactory();
  }
  uint32_t options =
      GetURLLoaderOptions(resource_request_->is_outermost_main_frame);

  // As `FallbackToNonInterceptedRequest()` is called only from ServiceWorker
  // after initially setting `interceptor_result->single_request_factory`,
  // `url_loader_` should be non-null and pointing to the
  // service-worker-intercepting loader. Restart it with the non-interceptor
  // factory.
  CHECK(url_loader_);
  url_loader_->RestartWithFactory(std::move(factory), options);
}

scoped_refptr<network::SharedURLLoaderFactory>
NavigationURLLoaderImpl::GetOrCreateNonNetworkLoaderFactory() {
  scoped_refptr<network::SharedURLLoaderFactory>& cached_factory =
      non_network_url_loader_factories_[resource_request_->url.scheme()];

  if (cached_factory) {
    return cached_factory;
  }

  auto [is_cacheable, factory] = CreateNonNetworkLoaderFactory(
      browser_context_, storage_partition_,
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_),
      ukm::SourceIdObj::FromInt64(ukm_source_id_), navigation_ui_data_.get(),
      *request_info_, web_contents_getter_, *resource_request_);

  if (is_cacheable) {
    cached_factory = factory;
  }

  return factory;
}

// static
std::pair</*is_cacheable=*/bool, scoped_refptr<network::SharedURLLoaderFactory>>
NavigationURLLoaderImpl::CreateNonNetworkLoaderFactory(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition,
    FrameTreeNode* frame_tree_node,
    const ukm::SourceIdObj& ukm_id,
    NavigationUIData* navigation_ui_data,
    const NavigationRequestInfo& request_info,
    base::RepeatingCallback<WebContents*()> web_contents_getter,
    const network::ResourceRequest& resource_request) {
  CHECK(frame_tree_node);
  CHECK(frame_tree_node->navigation_request());

  // First, check known schemes.
  if (mojo::PendingRemote<network::mojom::URLLoaderFactory> terminal =
          CreateTerminalNonNetworkLoaderFactory(
              browser_context, storage_partition, frame_tree_node,
              resource_request.url)) {
    auto* frame = frame_tree_node->current_frame_host();

    // TODO(lukasza, jam): It is unclear why FileURLLoaderFactory is the only
    // non-http factory that allows DevTools interception. For comparison all
    // non-WebUI cases in RFHI::CommitNavigation allow DevTools interception.
    // Let's try to be more consistent / less ad-hoc.
    std::optional<devtools_instrumentation::WillCreateURLLoaderFactoryParams>
        devtools_params =
            resource_request.url.SchemeIs(url::kFileScheme)
                ? std::make_optional(
                      devtools_instrumentation::
                          WillCreateURLLoaderFactoryParams::ForFrame(frame))
                : std::nullopt;
    return std::make_pair(
        /*is_cacheable=*/true,
        url_loader_factory::Create(
            ContentBrowserClient::URLLoaderFactoryType::kNavigation,
            url_loader_factory::TerminalParams::ForNonNetwork(
                std::move(terminal), network::mojom::kBrowserProcessId),
            url_loader_factory::ContentClientParams(
                frame->GetSiteInstance()->GetBrowserContext(), frame,
                frame->GetProcess()->GetID(), url::Origin(),
                net::IsolationInfo(), ukm_id,
                /*bypass_redirect_checks=*/nullptr,
                frame_tree_node->navigation_request()->GetNavigationId(),
                GetUIThreadTaskRunner(
                    {BrowserTaskType::kNavigationNetworkResponse})),
            devtools_params));
  }

  // Second, check external protocols.
  std::optional<url::Origin> initiating_origin;
  if (resource_request.navigation_redirect_chain.size() > 1) {
    // The last URL in `navigation_redirect_chain` is an external-protocol URL
    // (if handled by `HandleExternalProtocol`), and the second-to-last URL is
    // the URL that initiated the redirect to the external-protocol URL.
    initiating_origin = url::Origin::Create(
        resource_request.navigation_redirect_chain
            [resource_request.navigation_redirect_chain.size() - 2]);
  } else {
    initiating_origin = resource_request.request_initiator;
  }
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      terminal_external_protocol;
  bool handled = GetContentClient()->browser()->HandleExternalProtocol(
      resource_request.url, std::move(web_contents_getter),
      frame_tree_node->frame_tree_node_id(), navigation_ui_data,
      request_info.is_primary_main_frame,
      frame_tree_node->IsInFencedFrameTree(), request_info.sandbox_flags,
      static_cast<ui::PageTransition>(resource_request.transition_type),
      resource_request.has_user_gesture, initiating_origin,
      request_info.initiator_document_token
          ? RenderFrameHostImpl::FromDocumentToken(
                request_info.initiator_process_id,
                *request_info.initiator_document_token)
          : nullptr,
      request_info.isolation_info, &terminal_external_protocol);
  if (terminal_external_protocol) {
    return std::make_pair(
        /*is_cacheable=*/false,
        url_loader_factory::Create(
            ContentBrowserClient::URLLoaderFactoryType::kNavigation,
            url_loader_factory::TerminalParams::ForNonNetwork(
                std::move(terminal_external_protocol),
                network::mojom::kBrowserProcessId)));
  }

  // Finally handle as an unknown scheme.
  return std::make_pair(
      /*is_cacheable=*/false,
      url_loader_factory::Create(
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          url_loader_factory::TerminalParams::ForNonNetwork(
              base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
                  base::BindOnce(UnknownSchemeCallback, handled)),
              network::mojom::kBrowserProcessId)));
}

void NavigationURLLoaderImpl::CreateThrottlingLoaderAndStart(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
        additional_throttles) {
  TRACE_EVENT_WITH_FLOW0(
      "navigation", "NavigationURLLoaderImpl::CreateThrottlingLoaderAndStart",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK(!url_loader_);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      CreateURLLoaderThrottles();
  for (auto&& throttle : additional_throttles) {
    throttles.push_back(std::move(throttle));
  }

  uint32_t options =
      GetURLLoaderOptions(resource_request_->is_outermost_main_frame);

  url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      std::move(factory), std::move(throttles), global_request_id_.request_id,
      options, resource_request_.get(), /*client=*/this,
      kNavigationUrlLoaderTrafficAnnotation,
      GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse}));
}

void NavigationURLLoaderImpl::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // Early Hints should not come after actual response.
  DCHECK(!received_response_);
  DCHECK_NE(early_hints->ip_address_space,
            network::mojom::IPAddressSpace::kUnknown);

  // Ignore Early Hints for embed and object destination.
  if (request_info_->common_params->request_destination ==
          network::mojom::RequestDestination::kEmbed ||
      request_info_->common_params->request_destination ==
          network::mojom::RequestDestination::kObject) {
    return;
  }

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);

  // Allow Early Hints preload only for outermost main frames. Calculating
  // appropriate parameters to create URLLoaderFactory for subframes and fenced
  // frames are complicated and not supported yet.
  if (frame_tree_node->GetParentOrOuterDocument()) {
    return;
  }

  if (!early_hints_manager_) {
    std::optional<NavigationEarlyHintsManagerParams> params =
        delegate_->CreateNavigationEarlyHintsManagerParams(*early_hints);
    if (!params) {
      return;
    }
    early_hints_manager_ = std::make_unique<NavigationEarlyHintsManager>(
        *browser_context_, *storage_partition_, frame_tree_node_id_,
        std::move(*params));
  }

  early_hints_manager_->HandleEarlyHints(std::move(early_hints),
                                         *resource_request_.get());
}

void NavigationURLLoaderImpl::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK(!cached_metadata);
  LogQueueTimeHistogram("Navigation.QueueTime.OnReceiveResponse",
                        resource_request_->is_outermost_main_frame);

  // Early Hints preloads should not be committed for PDF.
  // See https://github.com/whatwg/html/issues/7823
  if (head->mime_type == "application/pdf" || head->mime_type == "text/pdf") {
    early_hints_manager_.reset();
  }

  //  TODO(crbug.com/40496584):Resolved an issue where creating RPHI would cause
  //  a crash when the browser context was shut down. We are actively exploring
  //  the appropriate long-term solution. Please remove this condition once the
  //  final fix is implemented.
  if (browser_context_->ShutdownStarted()) {
    return;
  }

  if (!response_body) {
    return;
  }

  response_body_ = std::move(response_body);
  received_response_ = true;

  if (!head_update_params_.load_timing_info.service_worker_start_time
           .is_null()) {
    head->load_timing.service_worker_start_time =
        head_update_params_.load_timing_info.service_worker_start_time;
    head->load_timing.service_worker_ready_time =
        head_update_params_.load_timing_info.service_worker_ready_time;
  }
  if (!head_update_params_.router_info.is_null()) {
    head->service_worker_router_info =
        std::move(head_update_params_.router_info);
  }
  if (!head_update_params_.load_timing_info
           .service_worker_router_evaluation_start.is_null()) {
    head->load_timing.service_worker_router_evaluation_start =
        head_update_params_.load_timing_info
            .service_worker_router_evaluation_start;
  }

  // If the default loader (network) was used to handle the URL load request
  // we need to see if the interceptors want to potentially create a new
  // loader for the response. e.g. service workers.
  //
  // As the navigation request has received a response, the URLLoader has
  // completed without any network errors. Some interceptors may still wish
  // to handle the response.
  auto status = network::URLLoaderCompletionStatus(net::OK);
  if (MaybeCreateLoaderForResponse(status, &head)) {
    return;
  }

  network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints;

  if (url_loader_) {
    url_loader_client_endpoints = url_loader_->Unbind();
  } else {
    url_loader_client_endpoints = network::mojom::URLLoaderClientEndpoints::New(
        std::move(response_url_loader_), response_loader_receiver_.Unbind());
  }

  // 304 responses should abort the navigation, rather than display the page.
  // This needs to be after the URLLoader has been moved to
  // `url_loader_client_endpoints` in order to abort the request, to avoid
  // receiving unexpected call.
  if (head->headers &&
      head->headers->response_code() == net::HTTP_NOT_MODIFIED) {
    // Call CancelWithError instead of OnComplete so that if there is an
    // intercepting URLLoaderFactory it gets notified.
    url_loader_->CancelWithError(
        net::ERR_ABORTED,
        std::string_view(base::NumberToString(net::ERR_ABORTED)));
    return;
  }

  bool must_download = download_utils::MustDownload(
      browser_context_, url_, head->headers.get(), head->mime_type);
  bool known_mime_type = blink::IsSupportedMimeType(head->mime_type);

#if BUILDFLAG(ENABLE_PLUGINS)
  if (!head->intercepted_by_plugin && !must_download && !known_mime_type) {
    // No plugin throttles intercepted the response. Ask if the plugin
    // registered to PluginService wants to handle the request.
    CheckPluginAndContinueOnReceiveResponse(
        std::move(head), std::move(url_loader_client_endpoints),
        /*is_download_if_not_handled_by_plugin=*/true,
        std::vector<WebPluginInfo>());
    return;
  }
#endif

  // When a plugin intercepted the response, we don't want to download it.
  bool is_download =
      !head->intercepted_by_plugin && (must_download || !known_mime_type);

  CallOnReceivedResponse(std::move(head),
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
  bool has_plugin = PluginService::GetInstance()->GetPluginInfo(
      browser_context_, resource_request_->url, head->mime_type,
      /*allow_wildcard=*/false, &stale, &plugin, nullptr);

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
  // Record navigation loader response metrics.  We don't want to record the
  // metrics for requests that had redirects to avoid adding noise to the
  // latency measurements.
  if (resource_request_->is_outermost_main_frame &&
      resource_request_->navigation_redirect_chain.size() == 1) {
    RecordReceivedResponseUkmForOutermostMainFrame();
  }

  network::mojom::URLResponseHead* head_ptr = head.get();
  // Record ServiceWorker Static Routing API metrics. This is only recorded
  // when the API is used.
  if (head_ptr->service_worker_router_info) {
    RecordServiceWorkerRouterEvaluationResults(
        head_ptr->service_worker_router_info.get());
  }
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
  LogQueueTimeHistogram("Navigation.QueueTime.OnReceiveRedirect",
                        resource_request_->is_outermost_main_frame);
  net::Error error = net::OK;
  if (!bypass_redirect_checks_ &&
      !IsSafeRedirectTarget(url_, redirect_info.new_url)) {
    error = net::ERR_UNSAFE_REDIRECT;
  } else if (--redirect_limit_ == 0) {
    error = net::ERR_TOO_MANY_REDIRECTS;
    if (redirect_info.is_signed_exchange_fallback_redirect) {
      UMA_HISTOGRAM_BOOLEAN("SignedExchange.FallbackRedirectLoop", true);
    }
  }
  if (error != net::OK) {
    if (url_loader_) {
      // Call CancelWithError instead of OnComplete so that if there is an
      // intercepting URLLoaderFactory (created through the embedder's
      // ContentBrowserClient::WillCreateURLLoaderFactory) it gets notified.
      url_loader_->CancelWithError(
          error, std::string_view(base::NumberToString(error)));
    } else {
      // TODO(crbug.com/40118809): Make sure ResetWithReason() is called
      // on the original `url_loader_`.
      OnComplete(network::URLLoaderCompletionStatus(error));
    }
    return;
  }

  // Store the redirect_info for later use in FollowRedirect where we give
  // our interceptors_ a chance to intercept the request for the new location.
  redirect_info_ = redirect_info;

  GURL previous_url = url_;
  url_ = redirect_info.new_url;

  network::mojom::URLResponseHead* head_ptr = head.get();
  auto on_receive_redirect = base::BindOnce(
      &NavigationURLLoaderImpl::NotifyRequestRedirected,
      weak_factory_.GetWeakPtr(), redirect_info, std::move(head));
  ParseHeaders(previous_url, head_ptr, std::move(on_receive_redirect));
}

void NavigationURLLoaderImpl::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void NavigationURLLoaderImpl::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kNavigationURLLoaderImpl);
}

void NavigationURLLoaderImpl::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // Successful load must have used OnResponseStarted first. In this case, the
  // URLLoaderClient has already been transferred to the renderer process and
  // OnComplete is not expected to be called here.
  if (status.error_code == net::OK) {
    SCOPED_CRASH_KEY_STRING256("NavigationURLLoader_Complete", "url",
                               url_.spec());
    base::debug::DumpWithoutCrashing();
    return;
  }

  // If the default loader (network) was used to handle the URL load request
  // we need to see if the interceptors want to potentially create a new
  // loader for the response. e.g. service worker.
  //
  // Note: Despite having received a response, the HTTP_NOT_MODIFIED(304) ones
  //       are ignored using OnComplete(net::ERR_ABORTED). No interceptor must
  //       be used in this case.
  if (!received_response_) {
    auto response = network::mojom::URLResponseHead::New();
    if (MaybeCreateLoaderForResponse(status, &response)) {
      return;
    }
  }

  status_ = status;
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NavigationURLLoaderImpl::NotifyRequestFailed,
                                weak_factory_.GetWeakPtr(), status));
}

void NavigationURLLoaderImpl::OnAcceptCHFrameReceived(
    const url::Origin& origin,
    const std::vector<network::mojom::WebClientHintsType>& accept_ch_frame,
    OnAcceptCHFrameReceivedCallback callback) {
  received_accept_ch_frame_ = true;
  if (!base::FeatureList::IsEnabled(network::features::kAcceptCHFrame)) {
    std::move(callback).Run(net::OK);
    return;
  }

  LogAcceptCHFrameStatus(AcceptCHFrameRestart::kFramePresent);

  // Given that this is happening in the middle of navigation, there should
  // always be an owning frame tree node
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  DCHECK(frame_tree_node);
  // Log each hint requested via an ACCEPT_CH Frame whether or not this caused
  // the connection to be restarted.
  auto* ukm_recorder = ukm::UkmRecorder::Get();
  for (const auto& hint : accept_ch_frame) {
    ukm::builders::ClientHints_AcceptCHFrameUsage(ukm_source_id_)
        .SetType(static_cast<int64_t>(hint))
        .Record(ukm_recorder->Get());
  }

  ClientHintsControllerDelegate* client_hint_delegate =
      browser_context_->GetClientHintsControllerDelegate();

  if (!client_hint_delegate) {
    std::move(callback).Run(net::OK);
    return;
  }

  // Filter out hints that are disabled by features and the like.
  blink::EnabledClientHints filtered_enabled_hints;
  for (const auto& hint : accept_ch_frame)
    filtered_enabled_hints.SetIsEnabled(hint, true);
  const std::vector<network::mojom::WebClientHintsType>& filtered_hints =
      filtered_enabled_hints.GetEnabledHints();

  if (!AreCriticalHintsMissing(origin, frame_tree_node, client_hint_delegate,
                               filtered_hints)) {
    std::move(callback).Run(net::OK);
    return;
  }

  net::HttpRequestHeaders modified_headers;
  client_hint_delegate->SetAdditionalClientHints(filtered_hints);
  AddNavigationRequestClientHintsHeaders(
      origin, &modified_headers, browser_context_, client_hint_delegate,
      frame_tree_node->navigation_request()->is_overriding_user_agent(),
      frame_tree_node,
      frame_tree_node->navigation_request()
          ->commit_params()
          .frame_policy.container_policy);
  client_hint_delegate->ClearAdditionalClientHints();

  LogAcceptCHFrameStatus(AcceptCHFrameRestart::kNavigationRestarted);

  // Only restart if new headers are actually added. Given that header values
  // can be changed via the navigation interceptors or previous restarts, the
  // header values are ignored and only the presence of header names are
  // checked.
  bool restart = false;
  net::HttpRequestHeaders::Iterator header_iter(modified_headers);
  while (header_iter.GetNext()) {
    if (!resource_request_->headers.HasHeader(header_iter.name())) {
      restart = true;
      break;
    }
  }

  if (!restart) {
    std::move(callback).Run(net::OK);
    return;
  }

  // While not a true redirect, a redirect loop can be simulated by repeatedly
  // closing the socket and presenting a different ALPS setting with each new
  // handshake.
  if (--accept_ch_restart_limit_ == 0) {
    LogAcceptCHFrameStatus(AcceptCHFrameRestart::kRedirectOverflow);
    OnComplete(network::URLLoaderCompletionStatus(
        net::ERR_TOO_MANY_ACCEPT_CH_RESTARTS));
    std::move(callback).Run(net::ERR_TOO_MANY_ACCEPT_CH_RESTARTS);
    return;
  }

  std::move(callback).Run(net::ERR_ABORTED);

  // If the request is restarted, all of the client hints should be replaced
  // the "original"/non-edited values.
  resource_request_->headers.MergeFrom(modified_headers);
  url_loader_.reset();
  Restart();
}

void NavigationURLLoaderImpl::Clone(
    mojo::PendingReceiver<network::mojom::AcceptCHFrameObserver> listener) {
  // Use |kNavigationNetworkResponse| thread runner. Messages received related
  // to AcceptCHFrame are not order dependent and can restart the navigation,
  // blocking navigation when they do.
  accept_ch_frame_observers_.Add(
      this, std::move(listener),
      GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse}));
}

// Returns true if an interceptor wants to handle the response, i.e. return a
// different response, e.g. service workers.
bool NavigationURLLoaderImpl::MaybeCreateLoaderForResponse(
    const network::URLLoaderCompletionStatus& status,
    network::mojom::URLResponseHeadPtr* response) {
  if (!default_loader_used_) {
    return false;
  }
  for (auto& interceptor : interceptors_) {
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        response_client_receiver;
    bool skip_other_interceptors = false;
    if (interceptor->MaybeCreateLoaderForResponse(
            status, *resource_request_, response, &response_body_,
            &response_url_loader_, &response_client_receiver, url_loader_.get(),
            &skip_other_interceptors)) {
      response_loader_receiver_.reset();
      response_loader_receiver_.Bind(
          std::move(response_client_receiver),
          GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse}));
      default_loader_used_ = false;
      url_loader_.reset();     // Consumed above.
      response_body_.reset();  // Consumed above.
      if (skip_other_interceptors) {
        std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
            new_interceptors;
        new_interceptors.push_back(std::move(interceptor));
        new_interceptors.swap(interceptors_);
        // Reset the state of ServiceWorkerClient.
        // Currently we don't support Service Worker in Signed Exchange
        // pages. The page will not be controlled by service workers. And
        // Service Worker related APIs will fail with NoDocumentURL error.
        // TODO(https://crbug/898733): Support SignedExchange loading and
        // Service Worker integration. Properly populate all params below, and
        // storage key in particular, when we want to support it.
        if (service_worker_handle_) {
          base::WeakPtr<ServiceWorkerClient> service_worker_client =
              service_worker_handle_->service_worker_client();
          if (service_worker_client) {
            service_worker_client->SetControllerRegistration(
                nullptr, /*notify_controllerchange=*/false);
            service_worker_client->UpdateUrls(GURL(), std::nullopt,
                                              blink::StorageKey());
          }
        }
      }
      return true;
    }
  }
  return false;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
NavigationURLLoaderImpl::CreateURLLoaderThrottles() {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationURLLoaderImpl::CreateURLLoaderThrottles",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  auto throttles = CreateContentBrowserURLLoaderThrottles(
      *resource_request_, browser_context_, web_contents_getter_,
      navigation_ui_data_.get(), frame_tree_node_id_,
      request_info_->navigation_id);
  throttles.push_back(std::make_unique<NavigationTimingThrottle>(
      resource_request_->is_outermost_main_frame, loader_creation_time_));
  return throttles;
}

std::unique_ptr<SignedExchangeRequestHandler>
NavigationURLLoaderImpl::CreateSignedExchangeRequestHandler(
    const NavigationRequestInfo& request_info,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // It is safe to pass the callback of CreateURLLoaderThrottles with the
  // unretained `this`, because the passed callback will be used by a
  // SignedExchangeHandler which is indirectly owned by `this` until its
  // header is verified and parsed, that's where the getter is used.
  return std::make_unique<SignedExchangeRequestHandler>(
      GetURLLoaderOptions(request_info.is_outermost_main_frame),
      request_info.frame_tree_node_id, request_info.devtools_navigation_token,
      std::move(url_loader_factory),
      base::BindRepeating(&NavigationURLLoaderImpl::CreateURLLoaderThrottles,
                          base::Unretained(this)),
      GetContentClient()->browser()->GetAcceptLangs(browser_context_));
}

void NavigationURLLoaderImpl::ParseHeaders(
    const GURL& url,
    network::mojom::URLResponseHead* head,
    base::OnceClosure continuation) {
  // As an optimization, when we know the parsed headers will be empty, we can
  // skip the network process roundtrip.
  // TODO(arthursonzogni): If there are any performance issues, consider
  // checking the `head->headers` contains at least one header to be parsed.
  if (!head->headers) {
    head->parsed_headers = network::mojom::ParsedHeaders::New();
  }

  // If the network service is running in process, skip unnecessary thread hops.
  if (base::FeatureList::IsEnabled(kSkipUnnecessaryThreadHopsForParseHeaders) &&
      IsInProcessNetworkService() && !head->parsed_headers) {
    head->parsed_headers =
        network::PopulateParsedHeaders(head->headers.get(), url);
  }

  // The main path:
  // --------------
  // The ParsedHeaders are already provided. No more work needed.
  //
  // Currently used when the response is coming from:
  // - Network
  // - ServiceWorker
  // - WebUI
  if (head->parsed_headers) {
#ifndef NDEBUG
    // In debug mode, force reparsing the headers and check that they match.
    auto check = [](base::OnceClosure continuation,
                    network::mojom::URLResponseHead* head, GURL url,
                    network::mojom::ParsedHeadersPtr parsed_headers) {
      CheckParsedHeadersEquals(parsed_headers, head->parsed_headers, url);
      std::move(continuation).Run();
    };
    GetNetworkService()->ParseHeaders(
        url, head->headers,
        base::BindOnce(check, std::move(continuation), head, url));
#else   // NDEBUG
    std::move(continuation).Run();
#endif  // NDEBUG
    return;
  }

  auto assign = [](base::OnceClosure continuation,
                   network::mojom::URLResponseHead* head,
                   network::mojom::ParsedHeadersPtr parsed_headers) {
    head->parsed_headers = std::move(parsed_headers);
    std::move(continuation).Run();
  };

  GetNetworkService()->ParseHeaders(
      url, head->headers,
      base::BindOnce(assign, std::move(continuation), head));
}

// TODO(crbug.com/40552600): pass `navigation_ui_data` along with the
// request so that it could be modified.
NavigationURLLoaderImpl::NavigationURLLoaderImpl(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerMainResourceHandle* service_worker_handle,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    NavigationURLLoaderDelegate* delegate,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
    mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
        trust_token_observer,
    mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
        shared_dictionary_observer,
    mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
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
      prefetched_signed_exchange_cache_(
          std::move(prefetched_signed_exchange_cache)),
      loader_creation_time_(base::TimeTicks::Now()),
      ukm_source_id_(FrameTreeNode::GloballyFindByID(frame_tree_node_id_)
                         ->navigation_request()
                         ->GetNextPageUkmSourceId()) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationURLLoaderImpl::NavigationURLLoaderImpl",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", TRACE_ID_LOCAL(this),
      request_info_->common_params->navigation_start, "FrameTreeNode id",
      frame_tree_node_id_);

  mojo::PendingRemote<network::mojom::AcceptCHFrameObserver>
      accept_ch_frame_observer;
  // Use |kNavigationNetworkResponse| thread runner. Messages received related
  // to AcceptCHFrame are not order dependent and can restart the navigation,
  // blocking navigation when they do.
  accept_ch_frame_observers_.Add(
      this, accept_ch_frame_observer.InitWithNewPipeAndPassReceiver(),
      GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse}));

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  DCHECK(frame_tree_node);
  DCHECK(frame_tree_node->navigation_request());

  resource_request_ = CreateResourceRequest(
      *request_info_, frame_tree_node, std::move(cookie_observer),
      std::move(trust_token_observer), std::move(shared_dictionary_observer),
      std::move(url_loader_network_observer), std::move(devtools_observer),
      std::move(accept_ch_frame_observer));

  network_loader_factory_ = CreateNetworkLoaderFactory(
      browser_context_, storage_partition_, frame_tree_node,
      ukm::SourceIdObj::FromInt64(ukm_source_id_), &bypass_redirect_checks_);
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
NavigationURLLoaderImpl::CreateTerminalNonNetworkLoaderFactory(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition,
    FrameTreeNode* frame_tree_node,
    const GURL& url) {
  // Use `ContentBrowserClient`-supplied factory if non-null, to allow the
  // embedder to override the default factories below.
  if (auto factory_from_client =
          GetContentClient()
              ->browser()
              ->CreateNonNetworkNavigationURLLoaderFactory(
                  url.scheme(), frame_tree_node->frame_tree_node_id())) {
    return factory_from_client;
  }

  if (url.scheme() == url::kFileSystemScheme) {
    bool is_nav_allowed =
        base::FeatureList::IsEnabled(
            blink::features::kFileSystemUrlNavigationForChromeAppsOnly) &&
        GetContentClient()->browser()->IsFileSystemURLNavigationAllowed(
            storage_partition->browser_context(), url);
    if (is_nav_allowed ||
        base::FeatureList::IsEnabled(
            blink::features::kFileSystemUrlNavigation) ||
        !frame_tree_node->navigation_request()->IsRendererInitiated()) {
      // TODO(crbug.com/40323778): Once DevTools has support for
      // sandboxed file system inspection there isn't much reason anymore to
      // support browser initiated filesystem: navigations, so remove this
      // entirely at that point.

      // Navigations in to filesystem: URLs are deprecated entirely for
      // renderer-initiated navigations except for those explicitly allowed by
      // the embedder. The logic below is appropriate for browser-initiated
      // navigations, but it is incorrect to always use first-party
      // StorageKeys for renderer-initiated navigations when third party
      // storage partitioning is enabled.
      const std::string storage_domain;
      return CreateFileSystemURLLoaderFactory(
          ChildProcessHost::kInvalidUniqueID,
          frame_tree_node->frame_tree_node_id(),
          storage_partition->GetFileSystemContext(), storage_domain,
          blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
    }

    return {};
  }

  if (url.scheme() == url::kAboutScheme) {
    return AboutURLLoaderFactory::Create();
  }
  if (url.scheme() == url::kDataScheme) {
    return DataURLLoaderFactory::Create();
  }

  if (url.scheme() == url::kFileScheme) {
    // USER_BLOCKING because this scenario is exactly one of the examples
    // given by the doc comment for USER_BLOCKING:
    // Loading and rendering a web page after the user clicks a link.
    base::TaskPriority file_factory_priority =
        base::TaskPriority::USER_BLOCKING;
    return FileURLLoaderFactory::Create(
        browser_context->GetPath(),
        browser_context->GetSharedCorsOriginAccessList(),
        file_factory_priority);
  }

#if BUILDFLAG(IS_ANDROID)
  if (url.scheme() == url::kContentScheme) {
    return ContentURLLoaderFactory::Create();
  }
#endif

  return {};
}

scoped_refptr<network::SharedURLLoaderFactory>
NavigationURLLoaderImpl::CreateNetworkLoaderFactory(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition,
    FrameTreeNode* frame_tree_node,
    const ukm::SourceIdObj& ukm_id,
    bool* bypass_redirect_checks) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;

  // The embedder may want to proxy all network-bound URLLoaderFactory
  // receivers that it can. If it elects to do so, those proxies will be
  // connected when loader is created if the request type supports proxying.
  network::URLLoaderFactoryBuilder factory_builder;
  // Here we give nullptr for `factory_override`, because CORS is no-op for
  // navigations.
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      browser_context, frame_tree_node->current_frame_host(),
      frame_tree_node->current_frame_host()->GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kNavigation, url::Origin(),
      net::IsolationInfo(),
      frame_tree_node->navigation_request()->GetNavigationId(), ukm_id,
      factory_builder, &header_client, bypass_redirect_checks,
      /*disable_secure_dns=*/nullptr, /*factory_override=*/nullptr,
      GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse}));
  devtools_instrumentation::WillCreateURLLoaderFactoryParams::ForFrame(
      frame_tree_node->current_frame_host())
      .Run(/*is_navigation=*/true,
           /*is_download=*/false, factory_builder,
           /*factory_override=*/nullptr);

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory;
  if (header_client) {
    return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
        CreateURLLoaderFactoryWithHeaderClient(std::move(header_client),
                                               std::move(factory_builder),
                                               storage_partition));
  } else {
    return std::move(factory_builder)
        .Finish(storage_partition->GetURLLoaderFactoryForBrowserProcess());
  }
}

void NavigationURLLoaderImpl::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!redirect_info_.new_url.is_empty());

  // Update `resource_request_` and call Restart to give our `interceptors_` a
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
  resource_request_->navigation_redirect_chain.push_back(
      redirect_info_.new_url);

  // Need to cache modified headers for `url_loader_` since it doesn't use
  // `resource_request_` during redirect.
  url_loader_removed_headers_ = removed_headers;
  url_loader_modified_headers_ = modified_headers;
  url_loader_modified_cors_exempt_headers_ = modified_cors_exempt_headers;

  // Don't send Accept: application/signed-exchange for fallback redirects.
  if (redirect_info_.is_signed_exchange_fallback_redirect) {
    std::string header_value =
        FrameAcceptHeaderValue(/*allow_sxg_responses=*/false, browser_context_);
    url_loader_modified_headers_.SetHeader(net::HttpRequestHeaders::kAccept,
                                           header_value);
    resource_request_->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                         header_value);
  }

  Restart();
}

bool NavigationURLLoaderImpl::SetNavigationTimeout(base::TimeDelta timeout) {
  // If the timer has already been started, don't change it.
  if (timeout_timer_.IsRunning()) {
    return false;
  }

  // Fail the navigation with error code ERR_TIMED_OUT if the timer triggers
  // before the navigation commits. (This triggers OnComplete() rather than
  // NotifyRequestFailed() to make sure that any NavigationLoaderInterceptors
  // can handle the result if needed.)
  timeout_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&NavigationURLLoaderImpl::OnComplete,
                     base::Unretained(this),
                     network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT)));
  return true;
}

void NavigationURLLoaderImpl::CancelNavigationTimeout() {
  timeout_timer_.Stop();
}

void NavigationURLLoaderImpl::NotifyResponseStarted(
    network::mojom::URLResponseHeadPtr response_head,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    mojo::ScopedDataPipeConsumerHandle response_body,
    const GlobalRequestID& global_request_id,
    bool is_download) {
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "navigation", "Navigation timeToResponseStarted", TRACE_ID_LOCAL(this),
      "&NavigationURLLoaderImpl", static_cast<void*>(this), "success", true);

  NavigationURLLoaderDelegate::EarlyHints early_hints;
  if (early_hints_manager_) {
    early_hints.was_resource_hints_received =
        early_hints_manager_->WasResourceHintsReceived();

    // Make Early Hints manager outlive this loader only when the response
    // headers are available. Dropping the manager cancels inflight preloads.
    if (response_head && response_head->headers) {
      early_hints.manager = std::move(early_hints_manager_);
    }
  }

  // TODO(scottmg): This needs to do more of what
  // NavigationResourceHandler::OnResponseStarted() does.
  delegate_->OnResponseStarted(
      std::move(url_loader_client_endpoints), std::move(response_head),
      std::move(response_body), global_request_id, is_download,
      resource_request_->trusted_params->isolation_info
          .network_anonymization_key(),
      std::move(subresource_loader_params_), std::move(early_hints));
}

void NavigationURLLoaderImpl::NotifyRequestRedirected(
    net::RedirectInfo redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnRequestRedirected(
      redirect_info,
      resource_request_->trusted_params->isolation_info
          .network_anonymization_key(),
      std::move(response_head));
}

void NavigationURLLoaderImpl::NotifyRequestFailed(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "navigation", "Navigation timeToResponseStarted", TRACE_ID_LOCAL(this),
      "&NavigationURLLoaderImpl", static_cast<void*>(this), "success", false);
  delegate_->OnRequestFailed(status);
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
NavigationURLLoaderImpl::CreateURLLoaderFactoryWithHeaderClient(
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        header_client,
    network::URLLoaderFactoryBuilder factory_builder,
    StoragePartitionImpl* partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (url_loader_factory::GetTestingInterceptor()) {
    url_loader_factory::GetTestingInterceptor().Run(
        network::mojom::kBrowserProcessId, factory_builder);
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->header_client = std::move(header_client);
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_trusted = true;
  params->is_orb_enabled = false;
  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);

  return std::move(factory_builder)
      .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
          partition->GetNetworkContext(), std::move(params));
}

void NavigationURLLoaderImpl::RecordReceivedResponseUkmForOutermostMainFrame() {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  DCHECK(frame_tree_node);

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::Navigation_ReceivedResponse builder(ukm_source_id_);
  base::TimeDelta latency = base::TimeTicks::Now() - loader_creation_time_;
  builder.SetHasAcceptCHFrame(received_accept_ch_frame_)
      .SetNavigationFirstResponseLatency(latency.InMilliseconds());
  builder.Record(ukm_recorder->Get());

  // Reset whether the ACCEPT_CH frame was received for the navigation.
  received_accept_ch_frame_ = false;
}

void NavigationURLLoaderImpl::RecordServiceWorkerRouterEvaluationResults(
    network::mojom::ServiceWorkerRouterInfo* router_info) {
  // Check if `matched_source_type` and `actual_source_type` exists. If
  // `matched_source_type` exists, `actual_source_type` should also exist.
  // Likewise, if `matched_source_type` does not exist, `actual_source_type`
  // should also not exist.
  CHECK_EQ(router_info->matched_source_type.has_value(),
           router_info->actual_source_type.has_value());
  ukm::builders::ServiceWorker_MainResourceLoadCompleted builder(
      ukm_source_id_);

  if (router_info->evaluation_worker_status) {
    builder.SetWorkerStatusOnEvaluation(
        static_cast<int64_t>(*router_info->evaluation_worker_status));
  }

  if (router_info->matched_source_type) {
    builder.SetMatchedFirstRouterSourceType(
        static_cast<int64_t>(*router_info->matched_source_type));
  }

  if (router_info->actual_source_type) {
    builder.SetActualRouterSourceType(
        static_cast<int64_t>(*router_info->actual_source_type));
  }

  builder
      .SetRouterRuleCount(ukm::GetExponentialBucketMinForCounts1000(
          router_info->route_rule_num))
      .SetRouterEvaluationTime(
          router_info->router_evaluation_time.InMicroseconds())
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace content

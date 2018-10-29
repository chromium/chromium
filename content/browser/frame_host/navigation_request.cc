// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/origin_policy_throttle.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/appcache_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/web_preferences.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Returns the net load flags to use based on the navigation type.
// TODO(clamy): Remove the blink code that sets the caching flags when
// PlzNavigate launches.
void UpdateLoadFlagsWithCacheFlags(
    int* load_flags,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool is_post) {
  switch (navigation_type) {
    case FrameMsg_Navigate_Type::RELOAD:
    case FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL:
      *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE:
      *load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case FrameMsg_Navigate_Type::RESTORE:
      *load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FrameMsg_Navigate_Type::RESTORE_WITH_POST:
      *load_flags |=
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FrameMsg_Navigate_Type::SAME_DOCUMENT:
    case FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT:
      if (is_post)
        *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT:
    case FrameMsg_Navigate_Type::HISTORY_DIFFERENT_DOCUMENT:
      if (is_post) {
        *load_flags |=
            net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      } else {
        *load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      }
      break;
  }
}

// TODO(clamy): This should be function in FrameTreeNode.
bool IsSecureFrame(FrameTreeNode* frame) {
  while (frame) {
    if (!IsPotentiallyTrustworthyOrigin(frame->current_origin()))
      return false;
    frame = frame->parent();
  }
  return true;
}

bool IsSecMetadataEnabled() {
  return base::FeatureList::IsEnabled(features::kSecMetadata) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

// This should match blink::ResourceRequest::needsHTTPOrigin.
bool NeedsHTTPOrigin(net::HttpRequestHeaders* headers,
                     const std::string& method) {
  // Don't add an Origin header if it is already present.
  if (headers->HasHeader(net::HttpRequestHeaders::kOrigin))
    return false;

  // Don't send an Origin header for GET or HEAD to avoid privacy issues.
  // For example, if an intranet page has a hyperlink to an external web
  // site, we don't want to include the Origin of the request because it
  // will leak the internal host name. Similar privacy concerns have lead
  // to the widespread suppression of the Referer header at the network
  // layer.
  if (method == "GET" || method == "HEAD")
    return false;

  // For non-GET and non-HEAD methods, always send an Origin header so the
  // server knows we support this feature.
  return true;
}

// TODO(clamy): This should match what's happening in
// blink::FrameFetchContext::addAdditionalRequestHeaders.
void AddAdditionalRequestHeaders(
    net::HttpRequestHeaders* headers,
    std::unique_ptr<net::HttpRequestHeaders> embedder_additional_headers,
    const GURL& url,
    FrameMsg_Navigate_Type::Value navigation_type,
    BrowserContext* browser_context,
    const std::string& method,
    const std::string user_agent_override,
    bool has_user_gesture,
    base::Optional<url::Origin> initiator_origin,
    FrameTreeNode* frame_tree_node) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  if (!base::GetFieldTrialParamByFeatureAsBool(features::kDataSaverHoldback,
                                               "holdback_web", false)) {
    bool is_reload =
        navigation_type == FrameMsg_Navigate_Type::RELOAD ||
        navigation_type == FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE ||
        navigation_type == FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
    if (is_reload)
      headers->RemoveHeader("Save-Data");

    if (GetContentClient()->browser()->IsDataSaverEnabled(browser_context))
      headers->SetHeaderIfMissing("Save-Data", "on");
  }

  // Attach additional request headers specified by embedder.
  if (embedder_additional_headers)
    headers->MergeFrom(*(embedder_additional_headers.get()));

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
  headers->SetHeaderIfMissing("Upgrade-Insecure-Requests", "1");

  headers->SetHeaderIfMissing(net::HttpRequestHeaders::kUserAgent,
                              user_agent_override.empty()
                                  ? GetContentClient()->GetUserAgent()
                                  : user_agent_override);

  // TODO(mkwst): Extract this logic out somewhere that can be shared between
  // Blink and //content.
  if (IsSecMetadataEnabled()) {
    std::string site_value = "cross-site";
    if (initiator_origin) {
      url::Origin target_origin = url::Origin::Create(url);
      if (initiator_origin->IsSameOriginWith(target_origin)) {
        site_value = "same-origin";
      } else if (net::registry_controlled_domains::SameDomainOrHost(
                     *initiator_origin, target_origin,
                     net::registry_controlled_domains::
                         INCLUDE_PRIVATE_REGISTRIES)) {
        site_value = "same-site";
      }
    }
    std::string value = base::StringPrintf(
        "cause=%s, destination=%s, site=%s",
        has_user_gesture ? "user-activated" : "forced",
        frame_tree_node->IsMainFrame() ? "document" : "nested-document",
        site_value.c_str());
    headers->SetHeaderIfMissing("Sec-Metadata", value);
  }

  // Ask whether we should request a policy.
  std::string origin_policy_request;
  if (OriginPolicyThrottle::ShouldRequestOriginPolicy(url,
                                                      &origin_policy_request)) {
    headers->SetHeader(net::HttpRequestHeaders::kSecOriginPolicy,
                       origin_policy_request);
  }

  // Next, set the HTTP Origin if needed.
  if (!NeedsHTTPOrigin(headers, method))
    return;

  // Create a unique origin.
  url::Origin origin;
  if (frame_tree_node->IsMainFrame()) {
    // For main frame, the origin is the url currently loading.
    origin = url::Origin::Create(url);
  } else if ((frame_tree_node->active_sandbox_flags() &
              blink::WebSandboxFlags::kOrigin) ==
             blink::WebSandboxFlags::kNone) {
    // The origin should be the origin of the root, except for sandboxed
    // frames which have a unique origin.
    origin = frame_tree_node->frame_tree()->root()->current_origin();
  }

  headers->SetHeader(net::HttpRequestHeaders::kOrigin, origin.Serialize());
}

// Should match the definition of
// blink::SchemeRegistry::ShouldTreatURLSchemeAsLegacy.
bool ShouldTreatURLSchemeAsLegacy(const GURL& url) {
  return url.SchemeIs(url::kFtpScheme) || url.SchemeIs(url::kGopherScheme);
}

bool ShouldPropagateUserActivation(const url::Origin& previous_origin,
                                   const url::Origin& new_origin) {
  if ((previous_origin.scheme() != "http" &&
       previous_origin.scheme() != "https") ||
      (new_origin.scheme() != "http" && new_origin.scheme() != "https")) {
    return false;
  }

  if (previous_origin.host() == new_origin.host())
    return true;

  std::string previous_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          previous_origin.host(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string new_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          new_origin.host(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return !previous_domain.empty() && previous_domain == new_domain;
}

}  // namespace

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateBrowserInitiated(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    bool browser_initiated,
    const std::string& extra_headers,
    const FrameNavigationEntry& frame_entry,
    const NavigationEntryImpl& entry,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    std::unique_ptr<NavigationUIData> navigation_ui_data) {
  // TODO(arthursonzogni): Form submission with the "GET" method is possible.
  // This is not currently handled here.
  bool is_form_submission = !!post_body;

  base::Optional<url::Origin> initiator =
      frame_tree_node->IsMainFrame()
          ? base::Optional<url::Origin>()
          : base::Optional<url::Origin>(
                frame_tree_node->frame_tree()->root()->current_origin());

  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, common_params,
      mojom::BeginNavigationParams::New(
          extra_headers, net::LOAD_NORMAL, false /* skip_service_worker */,
          blink::mojom::RequestContextType::LOCATION,
          blink::WebMixedContentContextType::kBlockable, is_form_submission,
          GURL() /* searchable_form_url */,
          std::string() /* searchable_form_encoding */, initiator,
          GURL() /* client_side_redirect_url */,
          base::nullopt /* devtools_initiator_info */),
      request_params, browser_initiated, false /* from_begin_navigation */,
      &frame_entry, &entry, std::move(navigation_ui_data), nullptr, nullptr));
  navigation_request->blob_url_loader_factory_ =
      frame_entry.blob_url_loader_factory();
  return navigation_request;
}

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateRendererInitiated(
    FrameTreeNode* frame_tree_node,
    NavigationEntryImpl* entry,
    const CommonNavigationParams& common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    int current_history_list_offset,
    int current_history_list_length,
    bool override_user_agent,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    blink::mojom::NavigationInitiatorPtr navigation_initiator) {
  // Only normal navigations to a different document or reloads are expected.
  // - Renderer-initiated fragment-navigations never take place in the browser,
  //   even with PlzNavigate.
  // - Restore-navigations are always browser-initiated.
  // - History-navigations use the browser-initiated path, event the ones that
  //   are initiated by a javascript script, please see the IPC message
  //   ViewHostMsg_GoToEntryAtOffset.
  DCHECK(FrameMsg_Navigate_Type::IsReload(common_params.navigation_type) ||
         common_params.navigation_type ==
             FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT);

  // TODO(clamy): See if the navigation start time should be measured in the
  // renderer and sent to the browser instead of being measured here.
  RequestNavigationParams request_params(
      override_user_agent,
      std::vector<GURL>(),  // redirects
      common_params.url, common_params.method,
      false,                          // can_load_local_resources
      PageState(),                    // page_state
      0,                              // nav_entry_id
      false,                          // is_history_navigation_in_new_child
      std::map<std::string, bool>(),  // subframe_unique_names
      false,  // intended_as_new_entry
      -1,     // |pending_history_list_offset| is set to -1 because
              // history-navigations do not use this path. See comments above.
      current_history_list_offset, current_history_list_length,
      false,  // is_view_source
      false /*should_clear_history_list*/);
  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, common_params, std::move(begin_params), request_params,
      false,  // browser_initiated
      true,   // from_begin_navigation
      nullptr, entry,
      nullptr,  // navigation_ui_data
      std::move(navigation_client), std::move(navigation_initiator)));
  navigation_request->blob_url_loader_factory_ =
      std::move(blob_url_loader_factory);
  return navigation_request;
}

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    const RequestNavigationParams& request_params,
    bool browser_initiated,
    bool from_begin_navigation,
    const FrameNavigationEntry* frame_entry,
    const NavigationEntryImpl* entry,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    blink::mojom::NavigationInitiatorPtr navigation_initiator)
    : frame_tree_node_(frame_tree_node),
      common_params_(common_params),
      begin_params_(std::move(begin_params)),
      request_params_(request_params),
      browser_initiated_(browser_initiated),
      navigation_ui_data_(std::move(navigation_ui_data)),
      state_(NOT_STARTED),
      restore_type_(RestoreType::NONE),
      is_view_source_(false),
      bindings_(NavigationEntryImpl::kInvalidBindings),
      response_should_be_rendered_(true),
      associated_site_instance_type_(AssociatedSiteInstanceType::NONE),
      from_begin_navigation_(from_begin_navigation),
      has_stale_copy_in_cache_(false),
      net_error_(net::OK),
      devtools_navigation_token_(base::UnguessableToken::Create()),
      request_navigation_client_(nullptr),
      commit_navigation_client_(nullptr),
      weak_factory_(this) {
  DCHECK(!browser_initiated || (entry != nullptr && frame_entry != nullptr));
  DCHECK(!IsRendererDebugURL(common_params_.url));
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "NavigationRequest", this,
                           "frame_tree_node",
                           frame_tree_node_->frame_tree_node_id(), "url",
                           common_params_.url.possibly_invalid_spec());

  // Sanitize the referrer.
  common_params_.referrer =
      Referrer::SanitizeForRequest(common_params_.url, common_params_.referrer);

  if (from_begin_navigation_) {
    // This is needed to have data URLs commit in the same SiteInstance as the
    // initiating renderer.
    source_site_instance_ =
        frame_tree_node->current_frame_host()->GetSiteInstance();

    if (IsPerNavigationMojoInterfaceEnabled()) {
      DCHECK(navigation_client.is_valid());
      request_navigation_client_ = mojom::NavigationClientAssociatedPtr();
      request_navigation_client_.Bind(std::move(navigation_client));
      // Binds the OnAbort callback
      request_navigation_client_.set_connection_error_handler(
          base::BindOnce(&NavigationRequest::OnRendererAbortedNavigation,
                         base::Unretained(this)));
      associated_site_instance_id_ = source_site_instance_->GetId();
    }
  } else {
    DCHECK(!navigation_client.is_valid());
    FrameNavigationEntry* frame_navigation_entry =
        entry->GetFrameEntry(frame_tree_node);
    if (frame_navigation_entry) {
      source_site_instance_ = frame_navigation_entry->source_site_instance();
      dest_site_instance_ = frame_navigation_entry->site_instance();
    }
    restore_type_ = entry->restore_type();
    is_view_source_ = entry->IsViewSourceMode();
    bindings_ = entry->bindings();
  }

  // Update the load flags with cache information.
  UpdateLoadFlagsWithCacheFlags(&begin_params_->load_flags,
                                common_params_.navigation_type,
                                common_params_.method == "POST");

  // Add necessary headers that may not be present in the
  // mojom::BeginNavigationParams.
  if (entry)
    nav_entry_id_ = entry->GetUniqueID();

  std::string user_agent_override;
  if (request_params.is_overriding_user_agent ||
      (entry && entry->GetIsOverridingUserAgent())) {
    user_agent_override =
        frame_tree_node_->navigator()->GetDelegate()->GetUserAgentOverride();
  }

  std::unique_ptr<net::HttpRequestHeaders> embedder_additional_headers;
  int additional_load_flags = 0;
  GetContentClient()->browser()->NavigationRequestStarted(
      frame_tree_node->frame_tree_node_id(), common_params_.url,
      &embedder_additional_headers, &additional_load_flags);
  begin_params_->load_flags |= additional_load_flags;

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_->headers);
  AddAdditionalRequestHeaders(
      &headers, std::move(embedder_additional_headers), common_params_.url,
      common_params_.navigation_type,
      frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
      common_params.method, user_agent_override,
      common_params_.has_user_gesture, begin_params_->initiator_origin,
      frame_tree_node);

  if (begin_params_->is_form_submission) {
    if (browser_initiated && !request_params.post_content_type.empty()) {
      // This is a form resubmit, so make sure to set the Content-Type header.
      headers.SetHeaderIfMissing(net::HttpRequestHeaders::kContentType,
                                 request_params.post_content_type);
    } else if (!browser_initiated) {
      // Save the Content-Type in case the form is resubmitted. This will get
      // sent back to the renderer in the CommitNavigation IPC. The renderer
      // will then send it back with the post body so that we can access it
      // along with the body in FrameNavigationEntry::page_state_.
      headers.GetHeader(net::HttpRequestHeaders::kContentType,
                        &request_params_.post_content_type);
    }
  }

  BrowserContext* browser_context =
      frame_tree_node_->navigator()->GetController()->GetBrowserContext();
  RendererPreferences render_prefs = frame_tree_node_->render_manager()
                                         ->current_host()
                                         ->GetDelegate()
                                         ->GetRendererPrefs(browser_context);
  if (render_prefs.enable_do_not_track)
    headers.SetHeader(kDoNotTrackHeader, "1");

  begin_params_->headers = headers.ToString();

  initiator_csp_context_.reset(new InitiatorCSPContext(
      common_params_.initiator_csp_info.initiator_csp,
      common_params_.initiator_csp_info.initiator_self_source,
      std::move(navigation_initiator)));
}

NavigationRequest::~NavigationRequest() {
  TRACE_EVENT_ASYNC_END0("navigation", "NavigationRequest", this);
  if (state_ == STARTED) {
    devtools_instrumentation::OnNavigationRequestFailed(
        *this, network::URLLoaderCompletionStatus(net::ERR_ABORTED));
  }
}

void NavigationRequest::BeginNavigation() {
  DCHECK(!loader_);
  DCHECK(state_ == NOT_STARTED || state_ == WAITING_FOR_RENDERER_RESPONSE);
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "BeginNavigation");

  state_ = STARTED;

#if defined(OS_ANDROID)
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());
  bool should_override_url_loading = false;

  if (!GetContentClient()->browser()->ShouldOverrideUrlLoading(
          frame_tree_node_->frame_tree_node_id(), browser_initiated_,
          request_params_.original_url, request_params_.original_method,
          common_params_.has_user_gesture, false,
          frame_tree_node_->IsMainFrame(), common_params_.transition,
          &should_override_url_loading)) {
    // A Java exception was thrown by the embedding application; we
    // need to return from this task. Specifically, it's not safe from
    // this point on to make any JNI calls.
    return;
  }

  // The content/ embedder might cause |this| to be deleted while
  // |ShouldOverrideUrlLoading| is called.
  // See https://crbug.com/770157.
  if (!this_ptr)
    return;

  if (should_override_url_loading) {
    // Don't create a NavigationHandle here to simulate what happened with the
    // old navigation code path (i.e. doesn't fire onPageFinished notification
    // for aborted loads).
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /*skip_throttles*/, base::nullopt /*error_page_content*/,
        false /*collapse_frame*/);
    return;
  }
#endif

  // Check Content Security Policy before the NavigationThrottles run. This
  // gives CSP a chance to modify requests that NavigationThrottles would
  // otherwise block. Similarly, the NavigationHandle is created afterwards, so
  // that it gets the request URL after potentially being modified by CSP.
  net::Error net_error = CheckContentSecurityPolicy(
      false /* has_followed redirect */,
      false /* url_upgraded_after_redirect */, false /* is_response_check */);
  if (net_error != net::OK) {
    // Create a navigation handle so that the correct error code can be set on
    // it by OnRequestFailedInternal().
    CreateNavigationHandle();
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(net_error),
                            false /* skip_throttles */,
                            base::nullopt /* error_page_content */,
                            false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckCredentialedSubresource() ==
          CredentialedSubresourceCheckResult::BLOCK_REQUEST ||
      CheckLegacyProtocolInSubresource() ==
          LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST) {
    // Create a navigation handle so that the correct error code can be set on
    // it by OnRequestFailedInternal().
    CreateNavigationHandle();
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /* skip_throttles  */, base::nullopt /* error_page_content */,
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  CreateNavigationHandle();

  if (IsURLHandledByNetworkStack(common_params_.url) &&
      !navigation_handle_->IsSameDocument()) {
    // Update PreviewsState if we are going to use the NetworkStack.
    common_params_.previews_state =
        GetContentClient()->browser()->DetermineAllowedPreviews(
            common_params_.previews_state, navigation_handle_.get());

    // It's safe to use base::Unretained because this NavigationRequest owns
    // the NavigationHandle where the callback will be stored.
    // TODO(clamy): pass the method to the NavigationHandle instead of a
    // boolean.
    navigation_handle_->WillStartRequest(
        base::Bind(&NavigationRequest::OnStartChecksComplete,
                   base::Unretained(this)));
    return;
  }

  // There is no need to make a network request for this navigation, so commit
  // it immediately.
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "ResponseStarted");
  state_ = RESPONSE_STARTED;

  // Select an appropriate RenderFrameHost.
  RenderFrameHostImpl* render_frame_host =
      frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
  NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(render_frame_host,
                                                           common_params_.url);

  // Inform the NavigationHandle that the navigation will commit.
  navigation_handle_->ReadyToCommitNavigation(render_frame_host, false);

  CommitNavigation();
}

void NavigationRequest::SetWaitingForRendererResponse() {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "WaitingForRendererResponse");
  DCHECK(state_ == NOT_STARTED);
  state_ = WAITING_FOR_RENDERER_RESPONSE;
}

void NavigationRequest::CreateNavigationHandle() {
  DCHECK_EQ(frame_tree_node_->navigation_request(), this);
  FrameTreeNode* frame_tree_node = frame_tree_node_;

  std::vector<GURL> redirect_chain;
  if (!begin_params_->client_side_redirect_url.is_empty())
    redirect_chain.push_back(begin_params_->client_side_redirect_url);
  redirect_chain.push_back(common_params_.url);

  bool is_external_protocol =
      !GetContentClient()->browser()->IsHandledURL(common_params_.url);

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_->headers);

  std::unique_ptr<NavigationHandleImpl> navigation_handle =
      NavigationHandleImpl::Create(
          common_params_.url, redirect_chain, frame_tree_node_,
          !browser_initiated_,
          FrameMsg_Navigate_Type::IsSameDocument(
              common_params_.navigation_type),
          common_params_.navigation_start, nav_entry_id_,
          common_params_.started_from_context_menu,
          common_params_.initiator_csp_info.should_check_main_world_csp,
          begin_params_->is_form_submission, std::move(navigation_ui_data_),
          common_params_.method, std::move(headers), common_params_.post_data,
          Referrer::SanitizeForRequest(common_params_.url,
                                       common_params_.referrer),
          common_params_.has_user_gesture, common_params_.transition,
          is_external_protocol, begin_params_->request_context_type,
          begin_params_->mixed_content_context_type,
          common_params_.input_start);

  if (!frame_tree_node->navigation_request()) {
    // A callback could have cancelled this request synchronously in which case
    // |this| is deleted.
    return;
  }

  navigation_handle_ = std::move(navigation_handle);

  if (!begin_params_->searchable_form_url.is_empty()) {
    navigation_handle_->set_searchable_form_url(
        begin_params_->searchable_form_url);
    navigation_handle_->set_searchable_form_encoding(
        begin_params_->searchable_form_encoding);
  }

  if (common_params_.source_location) {
    navigation_handle_->set_source_location(
        common_params_.source_location.value());
  }
}

std::unique_ptr<NavigationHandleImpl>
NavigationRequest::TakeNavigationHandle() {
  return std::move(navigation_handle_);
}

void NavigationRequest::ResetForCrossDocumentRestart() {
  DCHECK(
      FrameMsg_Navigate_Type::IsSameDocument(common_params_.navigation_type));

  // Reset the NavigationHandle, which is now incorrectly marked as
  // same-document. Ensure |loader_| does not exist as it can hold raw pointers
  // to objects owned by the handle (see the comment in the header).
  DCHECK(!loader_);
  navigation_handle_.reset();

  // Convert the navigation type to the appropriate cross-document one.
  if (common_params_.navigation_type ==
      FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT) {
    common_params_.navigation_type =
        FrameMsg_Navigate_Type::HISTORY_DIFFERENT_DOCUMENT;
  } else {
    DCHECK(common_params_.navigation_type ==
           FrameMsg_Navigate_Type::SAME_DOCUMENT);
    common_params_.navigation_type = FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
  }

  // Reset the state of the NavigationRequest.
  state_ = NOT_STARTED;
}

void NavigationRequest::RegisterSubresourceOverride(
    mojom::TransferrableURLLoaderPtr transferrable_loader) {
  if (!subresource_overrides_)
    subresource_overrides_.emplace();

  subresource_overrides_->push_back(std::move(transferrable_loader));
}

mojom::NavigationClient* NavigationRequest::GetCommitNavigationClient() {
  if (commit_navigation_client_ && commit_navigation_client_.is_bound())
    return commit_navigation_client_.get();
  return nullptr;
}

void NavigationRequest::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<network::ResourceResponse>& response) {
#if defined(OS_ANDROID)
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());

  bool should_override_url_loading = false;
  if (!GetContentClient()->browser()->ShouldOverrideUrlLoading(
          frame_tree_node_->frame_tree_node_id(), browser_initiated_,
          redirect_info.new_url, redirect_info.new_method,
          // Redirects are always not counted as from user gesture.
          false, true, frame_tree_node_->IsMainFrame(),
          common_params_.transition, &should_override_url_loading)) {
    // A Java exception was thrown by the embedding application; we
    // need to return from this task. Specifically, it's not safe from
    // this point on to make any JNI calls.
    return;
  }

  // The content/ embedder might cause |this| to be deleted while
  // |ShouldOverrideUrlLoading| is called.
  // See https://crbug.com/770157.
  if (!this_ptr)
    return;

  if (should_override_url_loading) {
    bool is_external_protocol =
        !GetContentClient()->browser()->IsHandledURL(common_params_.url);
    navigation_handle_->set_net_error_code(net::ERR_ABORTED);
    // Update the navigation handle to point to the new url to ensure
    // AwWebContents sees the new URL and thus passes that URL to onPageFinished
    // (rather than passing the old URL).
    navigation_handle_->UpdateStateFollowingRedirect(
        redirect_info.new_url, redirect_info.new_method,
        GURL(redirect_info.new_referrer), is_external_protocol,
        response->head.headers, response->head.connection_info,
        base::Bind(&NavigationRequest::OnRedirectChecksComplete,
                   base::Unretained(this)));
    frame_tree_node_->ResetNavigationRequest(false, true);
    return;
  }
#endif
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRedirectToURL(
          redirect_info.new_url)) {
    DVLOG(1) << "Denied redirect for "
             << redirect_info.new_url.possibly_invalid_spec();
    navigation_handle_->set_net_error_code(net::ERR_UNSAFE_REDIRECT);
    frame_tree_node_->ResetNavigationRequest(false, true);
    return;
  }

  // For renderer-initiated navigations we need to check if the source has
  // access to the URL. Browser-initiated navigations only rely on the
  // |CanRedirectToURL| test above.
  if (!browser_initiated_ && source_site_instance() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          source_site_instance()->GetProcess()->GetID(),
          redirect_info.new_url)) {
    DVLOG(1) << "Denied unauthorized redirect for "
             << redirect_info.new_url.possibly_invalid_spec();
    navigation_handle_->set_net_error_code(net::ERR_UNSAFE_REDIRECT);
    frame_tree_node_->ResetNavigationRequest(false, true);
    return;
  }

  // For now, DevTools needs the POST data sent to the renderer process even if
  // it is no longer a POST after the redirect.
  if (redirect_info.new_method != "POST")
    common_params_.post_data = nullptr;

  // Mark time for the Navigation Timing API.
  if (request_params_.navigation_timing.redirect_start.is_null()) {
    request_params_.navigation_timing.redirect_start =
        request_params_.navigation_timing.fetch_start;
  }
  request_params_.navigation_timing.redirect_end = base::TimeTicks::Now();
  request_params_.navigation_timing.fetch_start = base::TimeTicks::Now();

  request_params_.redirect_response.push_back(response->head);
  request_params_.redirect_infos.push_back(redirect_info);

  request_params_.redirects.push_back(common_params_.url);
  common_params_.url = redirect_info.new_url;
  common_params_.method = redirect_info.new_method;
  common_params_.referrer.url = GURL(redirect_info.new_referrer);
  common_params_.referrer =
      Referrer::SanitizeForRequest(common_params_.url, common_params_.referrer);

  // Check Content Security Policy before the NavigationThrottles run. This
  // gives CSP a chance to modify requests that NavigationThrottles would
  // otherwise block.
  net::Error net_error =
      CheckContentSecurityPolicy(true /* has_followed_redirect */,
                                 redirect_info.insecure_scheme_was_upgraded,
                                 false /* is_response_check */);
  if (net_error != net::OK) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net_error), false /*skip_throttles*/,
        base::nullopt /*error_page_content*/, false /*collapse_frame*/);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckCredentialedSubresource() ==
          CredentialedSubresourceCheckResult::BLOCK_REQUEST ||
      CheckLegacyProtocolInSubresource() ==
          LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /*skip_throttles*/, base::nullopt /*error_page_content*/,
        false /*collapse_frame*/);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Compute the SiteInstance to use for the redirect and pass its
  // RenderProcessHost if it has a process. Keep a reference if it has a
  // process, so that the SiteInstance and its associated process aren't deleted
  // before the navigation is ready to commit.
  scoped_refptr<SiteInstance> site_instance =
      frame_tree_node_->render_manager()->GetSiteInstanceForNavigationRequest(
          *this);
  speculative_site_instance_ =
      site_instance->HasProcess() ? site_instance : nullptr;

  // If the new site instance doesn't yet have a process, then tell the
  // SpareRenderProcessHostManager so it can decide whether to start warming up
  // the spare at this time (note that the actual behavior depends on
  // RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes).
  if (!site_instance->HasProcess()) {
    RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
        site_instance->GetBrowserContext());
  }

  // Check what the process of the SiteInstance is. It will be passed to the
  // NavigationHandle, and informed to expect a navigation to the redirected
  // URL.
  // Note: calling GetProcess on the SiteInstance can lead to the creation of a
  // new process if it doesn't have one. In this case, it should only be called
  // on a SiteInstance that already has a process.
  RenderProcessHost* expected_process =
      site_instance->HasProcess() ? site_instance->GetProcess() : nullptr;

  // It's safe to use base::Unretained because this NavigationRequest owns the
  // NavigationHandle where the callback will be stored.
  bool is_external_protocol =
      !GetContentClient()->browser()->IsHandledURL(common_params_.url);
  navigation_handle_->WillRedirectRequest(
      common_params_.url, common_params_.method, common_params_.referrer.url,
      is_external_protocol, response->head.headers,
      response->head.connection_info, expected_process,
      base::Bind(&NavigationRequest::OnRedirectChecksComplete,
                 base::Unretained(this)));
}

void NavigationRequest::OnResponseStarted(
    const scoped_refptr<network::ResourceResponse>& response,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<NavigationData> navigation_data,
    const GlobalRequestID& request_id,
    bool is_download,
    bool is_stream,
    base::Optional<SubresourceLoaderParams> subresource_loader_params) {
  DCHECK_EQ(state_, STARTED);
  DCHECK(response);
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "OnResponseStarted");
  state_ = RESPONSE_STARTED;

  // Check if the response should be sent to a renderer.
  response_should_be_rendered_ =
      !is_download && (!response->head.headers.get() ||
                       (response->head.headers->response_code() != 204 &&
                        response->head.headers->response_code() != 205));

  // Response that will not commit should be marked as aborted in the
  // NavigationHandle.
  if (!response_should_be_rendered_) {
    navigation_handle_->set_net_error_code(net::ERR_ABORTED);
    net_error_ = net::ERR_ABORTED;
  }

  // Update the service worker and AppCache params of the request params.
  request_params_.service_worker_provider_id =
      navigation_handle_->service_worker_handle()
          ? navigation_handle_->service_worker_handle()
                ->service_worker_provider_host_id()
          : kInvalidServiceWorkerProviderId;
  request_params_.appcache_host_id =
      navigation_handle_->appcache_handle()
          ? navigation_handle_->appcache_handle()->appcache_host_id()
          : kAppCacheNoHostId;

  // Update fetch start timing. While NavigationRequest updates fetch start
  // timing for redirects, it's not aware of service worker interception so
  // fetch start timing could happen earlier than worker start timing. Use
  // worker ready time if it is greater than the current value to make sure
  // fetch start timing always comes after worker start timing (if a service
  // worker intercepted the navigation).
  request_params_.navigation_timing.fetch_start =
      std::max(request_params_.navigation_timing.fetch_start,
               response->head.service_worker_ready_time);

  // A navigation is user activated if it contains a user gesture or the frame
  // received a gesture and the navigation is renderer initiated. If the
  // navigation is browser initiated, it has to come from the context menu.
  // In all cases, the previous and new URLs have to match the
  // `ShouldPropagateUserActivation` requirements (same eTLD+1).
  // There are two different checks:
  // 1. if the `frame_tree_node_` has an origin and is following the rules above
  //    with the target URL, it is used and the bit is set if the navigation is
  //    renderer initiated and the `frame_tree_node_` had a gesture. This should
  //    apply to same page navigations and is preferred over using the referrer
  //    as it can be changed.
  // 2. if referrer and the target url are following the rules above, two
  //    conditions will set the bit: navigation comes from a gesture and is
  //    renderer initiated (middle click/ctrl+click) or it is coming from a
  //    context menu. This should apply to pages that open in a new tab and we
  //    have to follow the referrer. It means that the activation might not be
  //    transmitted if it should have.
  if (request_params_.was_activated == WasActivatedOption::kUnknown) {
    request_params_.was_activated = WasActivatedOption::kNo;

    if (navigation_handle_->IsRendererInitiated() &&
        (frame_tree_node_->has_received_user_gesture() ||
         frame_tree_node_->has_received_user_gesture_before_nav()) &&
        ShouldPropagateUserActivation(
            frame_tree_node_->current_origin(),
            url::Origin::Create(navigation_handle_->GetURL()))) {
      request_params_.was_activated = WasActivatedOption::kYes;
      // TODO(805871): the next check is relying on
      // navigation_handle_->GetReferrer() but should ideally use a more
      // reliable source for the originating URL when the navigation is renderer
      // initiated.
    } else if (((navigation_handle_->HasUserGesture() &&
                 navigation_handle_->IsRendererInitiated()) ||
                navigation_handle_->WasStartedFromContextMenu()) &&
               ShouldPropagateUserActivation(
                   url::Origin::Create(navigation_handle_->GetReferrer().url),
                   url::Origin::Create(navigation_handle_->GetURL()))) {
      request_params_.was_activated = WasActivatedOption::kYes;
    }
  }

  // Select an appropriate renderer to commit the navigation.
  RenderFrameHostImpl* render_frame_host = nullptr;
  if (response_should_be_rendered_) {
    render_frame_host =
        frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
    NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(
        render_frame_host, common_params_.url);
  }
  DCHECK(render_frame_host || !response_should_be_rendered_);

  if (!browser_initiated_ && render_frame_host &&
      render_frame_host != frame_tree_node_->current_frame_host()) {
    // Reset the source location information if the navigation will not commit
    // in the current renderer process. This information originated in another
    // process (the current one), it should not be transferred to the new one.
    common_params_.source_location.reset();

    // Allow the embedder to cancel the cross-process commit if needed.
    // TODO(clamy): Rename ShouldTransferNavigation once PlzNavigate ships.
    if (!frame_tree_node_->navigator()->GetDelegate()->ShouldTransferNavigation(
            frame_tree_node_->IsMainFrame())) {
      navigation_handle_->set_net_error_code(net::ERR_ABORTED);
      frame_tree_node_->ResetNavigationRequest(false, true);
      return;
    }
  }

  if (navigation_data)
    navigation_handle_->set_navigation_data(std::move(navigation_data));

  // This must be set before DetermineCommittedPreviews is called.
  navigation_handle_->set_proxy_server(response->head.proxy_server);

  // Update the previews state of the request.
  common_params_.previews_state =
      GetContentClient()->browser()->DetermineCommittedPreviews(
          common_params_.previews_state, navigation_handle_.get(),
          response->head.headers.get());

  // Store the response and the URLLoaderClient endpoints until checks have been
  // processed.
  response_ = response;
  url_loader_client_endpoints_ = std::move(url_loader_client_endpoints);
  ssl_info_ = response->head.ssl_info.has_value() ? *response->head.ssl_info
                                                  : net::SSLInfo();
  is_download_ = is_download;

  subresource_loader_params_ = std::move(subresource_loader_params);

  // Since we've made the final pick for the RenderFrameHost above, the picked
  // RenderFrameHost's process should be considered "tainted" for future
  // process reuse decisions. That is, a site requiring a dedicated process
  // should not reuse this process, unless it's same-site with the URL we're
  // committing.  An exception is for URLs that do not "use up" the
  // SiteInstance, such as about:blank or chrome-native://.
  //
  // Note that although NavigationThrottles could still cancel the navigation
  // as part of WillProcessResponse below, we must update the process here,
  // since otherwise there could be a race if a NavigationThrottle defers the
  // navigation, and in the meantime another navigation reads the incorrect
  // IsUnused() value from the same process when making a process reuse
  // decision.
  if (render_frame_host &&
      SiteInstanceImpl::ShouldAssignSiteForURL(common_params_.url)) {
    render_frame_host->GetProcess()->SetIsUsed();

    // For sites that require a dedicated process, set the site URL now if it
    // hasn't been set already. This will lock the process to that site, which
    // will prevent other sites from incorrectly reusing this process. See
    // https://crbug.com/738634.
    SiteInstanceImpl* instance = render_frame_host->GetSiteInstance();
    if (!instance->HasSite() &&
        SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
            instance->GetBrowserContext(), common_params_.url)) {
      instance->SetSite(common_params_.url);
    }
  }

  devtools_instrumentation::OnNavigationResponseReceived(*this, *response);

  // The response code indicates that this is an error page, but we don't
  // know how to display the content.  We follow Firefox here and show our
  // own error page instead of intercepting the request as a stream or a
  // download.
  if (is_download && (response->head.headers.get() &&
                      (response->head.headers->response_code() / 100 != 2))) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_RESPONSE),
        false /* skip_throttles */, base::nullopt /* error_page_content */,
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // The CSP 'navigate-to' directive needs to know whether the response is a
  // redirect or not in order to perform its checks. This is the reason
  // why we need to check the CSP both on request and response.
  net::Error net_error = CheckContentSecurityPolicy(
      navigation_handle_->WasServerRedirect() /* has_followed_redirect */,
      false /* url_upgraded_after_redirect */, true /* is_response_check */);
  if (net_error != net::OK) {
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(net_error),
                            false /* skip_throttles */,
                            base::nullopt /* error_page_content */,
                            false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Check if the navigation should be allowed to proceed.
  navigation_handle_->WillProcessResponse(
      render_frame_host, response->head.headers.get(),
      response->head.connection_info, response->head.socket_address, ssl_info_,
      request_id, common_params_.should_replace_current_entry, is_download,
      is_stream, response->head.is_signed_exchange_inner_response,
      response->head.was_fetched_via_cache,
      base::Bind(&NavigationRequest::OnWillProcessResponseChecksComplete,
                 base::Unretained(this)));
}

void NavigationRequest::OnRequestFailed(
    const network::URLLoaderCompletionStatus& status) {
  bool collapse_frame =
      status.extended_error_code ==
      static_cast<int>(blink::ResourceRequestBlockedReason::kCollapsedByClient);
  OnRequestFailedInternal(status, false /* skip_throttles */,
                          base::nullopt /* error_page_content */,
                          collapse_frame);
}

void NavigationRequest::OnRequestFailedInternal(
    const network::URLLoaderCompletionStatus& status,
    bool skip_throttles,
    const base::Optional<std::string>& error_page_content,
    bool collapse_frame) {
  DCHECK(state_ == STARTED || state_ == RESPONSE_STARTED);
  DCHECK(!(status.error_code == net::ERR_ABORTED &&
           error_page_content.has_value()));
  common_params_.previews_state = content::PREVIEWS_OFF;

  devtools_instrumentation::OnNavigationRequestFailed(*this, status);

  // TODO(https://crbug.com/757633): Check that ssl_info.has_value() if
  // net_error is a certificate error.
  TRACE_EVENT_ASYNC_STEP_INTO1("navigation", "NavigationRequest", this,
                               "OnRequestFailed", "error", status.error_code);
  state_ = FAILED;
  if (navigation_handle_.get()) {
    navigation_handle_->set_net_error_code(
        static_cast<net::Error>(status.error_code));
  }

  int expected_pending_entry_id =
      navigation_handle_.get() ? navigation_handle_->pending_nav_entry_id()
                               : nav_entry_id_;
  frame_tree_node_->navigator()->DiscardPendingEntryIfNeeded(
      expected_pending_entry_id);

  // If the request was canceled by the user do not show an error page.
  if (status.error_code == net::ERR_ABORTED) {
    frame_tree_node_->ResetNavigationRequest(false, true);
    return;
  }

  if (collapse_frame) {
    DCHECK(!frame_tree_node_->IsMainFrame());
    DCHECK_EQ(net::ERR_BLOCKED_BY_CLIENT, status.error_code);
    frame_tree_node_->SetCollapsed(true);
  }

  RenderFrameHostImpl* render_frame_host = nullptr;
  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    // Main frame error pages must be isolated from the source or destination
    // process.
    //
    // Note: Since this navigation resulted in an error, clear the expected
    // process for the original navigation since for main frames the error page
    // will go into a new process.
    // TODO(nasko): Investigate whether GetFrameHostForNavigation can properly
    // account for clearing the expected process if it clears the speculative
    // RenderFrameHost. See https://crbug.com/793127.
    navigation_handle_->SetExpectedProcess(nullptr);
    render_frame_host =
        frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
  } else {
    if (ShouldKeepErrorPageInCurrentProcess(status.error_code)) {
      render_frame_host = frame_tree_node_->current_frame_host();
    } else {
      render_frame_host =
          frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
    }
  }

  DCHECK(render_frame_host);

  NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(render_frame_host,
                                                           common_params_.url);

  has_stale_copy_in_cache_ = status.exists_in_cache;
  net_error_ = status.error_code;

  if (skip_throttles) {
    // The NavigationHandle shouldn't be notified about renderer-debug URLs.
    // They will be handled by the renderer process.
    CommitErrorPage(render_frame_host, error_page_content);
  } else {
    // Check if the navigation should be allowed to proceed.
    navigation_handle_->WillFailRequest(
        render_frame_host, status.ssl_info,
        base::Bind(&NavigationRequest::OnFailureChecksComplete,
                   base::Unretained(this), render_frame_host));
  }
}

bool NavigationRequest::ShouldKeepErrorPageInCurrentProcess(int net_error) {
  // Decide whether to leave the error page in the original process.
  // * If this was a renderer-initiated navigation, and the request is blocked
  //   because the initiating document wasn't allowed to make the request,
  //   commit the error in the existing process. This is a strategy to to
  //   avoid creating a process for the destination, which may belong to an
  //   origin with a higher privilege level.
  // * Error pages resulting from errors like network outage, no network, or
  //   DNS error can reasonably expect that a reload at a later point in time
  //   would work. These should be allowed to transfer away from the current
  //   process: they do belong to whichever process that will host the
  //   destination URL, as a reload will end up committing in that process
  //   anyway.
  // * Error pages that arise during browser-initiated navigations to blocked
  //   URLs should be allowed to transfer away from the current process, which
  //   didn't request the navigation and may have a higher privilege level
  //   than the blocked destination.
  return net_error == net::ERR_BLOCKED_BY_CLIENT && !browser_initiated();
}

void NavigationRequest::OnRequestStarted(base::TimeTicks timestamp) {
  frame_tree_node_->navigator()->LogResourceRequestTime(timestamp,
                                                        common_params_.url);
}

void NavigationRequest::OnStartChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);
  DCHECK(result.action() != NavigationThrottle::BLOCK_RESPONSE);

  if (on_start_checks_complete_closure_)
    on_start_checks_complete_closure_.Run();
  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL ||
      result.action() == NavigationThrottle::BLOCK_REQUEST ||
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
#if DCHECK_IS_ON()
    if (result.action() == NavigationThrottle::BLOCK_REQUEST) {
      DCHECK(result.net_error_code() == net::ERR_BLOCKED_BY_CLIENT ||
             result.net_error_code() == net::ERR_BLOCKED_BY_ADMINISTRATOR);
    }
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    else if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE) {
      DCHECK_EQ(result.net_error_code(), net::ERR_ABORTED);
    }
#endif

    bool collapse_frame =
        result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE;

    // If the start checks completed synchronously, which could happen if there
    // is no onbeforeunload handler or if a NavigationThrottle cancelled it,
    // then this could cause reentrancy into NavigationController. So use a
    // PostTask to avoid that.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &NavigationRequest::OnRequestFailedInternal,
            weak_factory_.GetWeakPtr(),
            network::URLLoaderCompletionStatus(result.net_error_code()),
            true /* skip_throttles */, result.error_page_content(),
            collapse_frame));

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Use the SiteInstance of the navigating RenderFrameHost to get access to
  // the StoragePartition. Using the url of the navigation will result in a
  // wrong StoragePartition being picked when a WebView is navigating.
  DCHECK_NE(AssociatedSiteInstanceType::NONE, associated_site_instance_type_);
  RenderFrameHostImpl* navigating_frame_host =
      associated_site_instance_type_ == AssociatedSiteInstanceType::SPECULATIVE
          ? frame_tree_node_->render_manager()->speculative_frame_host()
          : frame_tree_node_->current_frame_host();
  DCHECK(navigating_frame_host);

  navigation_handle_->SetExpectedProcess(navigating_frame_host->GetProcess());

  BrowserContext* browser_context =
      frame_tree_node_->navigator()->GetController()->GetBrowserContext();
  StoragePartition* partition = BrowserContext::GetStoragePartition(
      browser_context, navigating_frame_host->GetSiteInstance());
  DCHECK(partition);

  // |loader_| should not exist if the service worker handle and app cache
  // handles will be destroyed, since it holds raw pointers to them. See the
  // comment in the header for |loader_|.
  DCHECK(!loader_);

  // Only initialize the ServiceWorkerNavigationHandle if it can be created for
  // this frame.
  bool can_create_service_worker =
      (frame_tree_node_->pending_frame_policy().sandbox_flags &
       blink::WebSandboxFlags::kOrigin) != blink::WebSandboxFlags::kOrigin;
  request_params_.should_create_service_worker = can_create_service_worker;
  if (can_create_service_worker) {
    ServiceWorkerContextWrapper* service_worker_context =
        static_cast<ServiceWorkerContextWrapper*>(
            partition->GetServiceWorkerContext());
    navigation_handle_->InitServiceWorkerHandle(service_worker_context);
  }

  if (IsSchemeSupportedForAppCache(common_params_.url)) {
    if (navigating_frame_host->GetRenderViewHost()
            ->GetWebkitPreferences()
            .application_cache_enabled) {
      navigation_handle_->InitAppCacheHandle(
          static_cast<ChromeAppCacheService*>(partition->GetAppCacheService()));
    }
  }

  // Mark the fetch_start (Navigation Timing API).
  request_params_.navigation_timing.fetch_start = base::TimeTicks::Now();

  GURL base_url;
#if defined(OS_ANDROID)
  // On Android, a base URL can be set for the frame. If this the case, it is
  // the URL to use for cookies.
  NavigationEntry* last_committed_entry =
      frame_tree_node_->navigator()->GetController()->GetLastCommittedEntry();
  if (last_committed_entry)
    base_url = last_committed_entry->GetBaseURLForDataURL();
#endif
  const GURL& top_document_url =
      !base_url.is_empty()
          ? base_url
          : frame_tree_node_->frame_tree()->root()->current_url();

  // Walk the ancestor chain to determine whether all frames are same-site. If
  // not, the |site_for_cookies| is set to an empty URL.
  //
  // TODO(mkwst): This is incorrect. It ought to use the definition from
  // 'Document::SiteForCookies()' in Blink, which special-cases extension
  // URLs and a few other sharp edges.
  const FrameTreeNode* current = frame_tree_node_->parent();
  bool ancestors_are_same_site = true;
  while (current && ancestors_are_same_site) {
    if (!net::registry_controlled_domains::SameDomainOrHost(
            top_document_url, current->current_url(),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      ancestors_are_same_site = false;
    }
    current = current->parent();
  }

  const GURL& site_for_cookies =
      (ancestors_are_same_site || !base_url.is_empty())
          ? (frame_tree_node_->IsMainFrame() ? common_params_.url
                                             : top_document_url)
          : GURL::EmptyGURL();
  bool parent_is_main_frame = !frame_tree_node_->parent()
                                  ? false
                                  : frame_tree_node_->parent()->IsMainFrame();

  std::unique_ptr<NavigationUIData> navigation_ui_data;
  if (navigation_handle_->GetNavigationUIData())
    navigation_ui_data = navigation_handle_->GetNavigationUIData()->Clone();

  bool is_for_guests_only =
      navigation_handle_->GetStartingSiteInstance()->GetSiteURL().
          SchemeIs(kGuestScheme);

  // Give DevTools a chance to override begin params (headers, skip SW)
  // before actually loading resource.
  bool report_raw_headers = false;
  devtools_instrumentation::ApplyNetworkRequestOverrides(
      frame_tree_node_, begin_params_.get(), &report_raw_headers);
  devtools_instrumentation::OnNavigationRequestWillBeSent(*this);

  loader_ = NavigationURLLoader::Create(
      browser_context->GetResourceContext(), partition,
      std::make_unique<NavigationRequestInfo>(
          common_params_, begin_params_.Clone(), site_for_cookies,
          frame_tree_node_->IsMainFrame(), parent_is_main_frame,
          IsSecureFrame(frame_tree_node_->parent()),
          frame_tree_node_->frame_tree_node_id(), is_for_guests_only,
          report_raw_headers,
          navigating_frame_host->GetVisibilityState() ==
              blink::mojom::PageVisibilityState::kPrerender,
          upgrade_if_insecure_,
          blob_url_loader_factory_ ? blob_url_loader_factory_->Clone()
                                   : nullptr,
          devtools_navigation_token(),
          frame_tree_node_->devtools_frame_token()),
      std::move(navigation_ui_data),
      navigation_handle_->service_worker_handle(),
      navigation_handle_->appcache_handle(), this);
}

void NavigationRequest::OnRedirectChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);
  DCHECK(result.action() != NavigationThrottle::BLOCK_RESPONSE);

  bool collapse_frame =
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE;

  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE if needed.
    DCHECK(result.action() == NavigationThrottle::CANCEL ||
           result.net_error_code() == net::ERR_ABORTED);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(), collapse_frame);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (result.action() == NavigationThrottle::BLOCK_REQUEST ||
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
    DCHECK(result.net_error_code() == net::ERR_BLOCKED_BY_CLIENT ||
           result.net_error_code() == net::ERR_BLOCKED_BY_ADMINISTRATOR);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(), collapse_frame);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  devtools_instrumentation::OnNavigationRequestWillBeSent(*this);

  base::Optional<net::HttpRequestHeaders> embedder_additional_headers;
  GetContentClient()->browser()->NavigationRequestRedirected(
      frame_tree_node_->frame_tree_node_id(), common_params_.url,
      &embedder_additional_headers);

  loader_->FollowRedirect(base::nullopt,
                          std::move(embedder_additional_headers));
}

void NavigationRequest::OnFailureChecksComplete(
    RenderFrameHostImpl* render_frame_host,
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);

  int old_net_error = net_error_;
  net_error_ = result.net_error_code();
  navigation_handle_->set_net_error_code(static_cast<net::Error>(net_error_));

  // TODO(crbug.com/774663): We may want to take result.action() into account.
  if (net::ERR_ABORTED == result.net_error_code()) {
    frame_tree_node_->ResetNavigationRequest(false, true);
    return;
  }

  // Ensure that WillFailRequest() isn't changing the error code in a way that
  // switches the destination process for the error page - see
  // https://crbug.com/817881.  This is not a concern with error page
  // isolation, where all errors will go into one process.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    CHECK_EQ(ShouldKeepErrorPageInCurrentProcess(old_net_error),
             ShouldKeepErrorPageInCurrentProcess(net_error_))
        << " Unsupported error code change in WillFailRequest(): from "
        << net_error_ << " to " << result.net_error_code();
  }

  // Sanity check that we haven't changed the RenderFrameHost picked for the
  // error page in OnRequestFailedInternal when running the WillFailRequest
  // checks.
  CHECK_EQ(navigation_handle_->GetRenderFrameHost(), render_frame_host);

  CommitErrorPage(render_frame_host, result.error_page_content());
  // DO NOT ADD CODE after this. The previous call to CommitErrorPage caused
  // the destruction of the NavigationRequest.
}

void NavigationRequest::OnWillProcessResponseChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);

  // If the NavigationThrottles allowed the navigation to continue, have the
  // processing of the response resume in the network stack.
  if (result.action() == NavigationThrottle::PROCEED) {
    // NetworkService doesn't use ResourceDispatcherHost.
    bool served_via_resource_dispatcher_host =
        !base::FeatureList::IsEnabled(network::features::kNetworkService);
    // When S13nServiceWorker is on, it doesn't use ResourceDispatcherHost when
    // a service worker serves the response.
    served_via_resource_dispatcher_host =
        served_via_resource_dispatcher_host &&
        !(blink::ServiceWorkerUtils::IsServicificationEnabled() &&
          response_->head.was_fetched_via_service_worker);

    // NetworkService or S13nServiceWorker: If this is a download, intercept the
    // navigation response and pass it to DownloadManager, and cancel the
    // navigation.
    if (is_download_ && !served_via_resource_dispatcher_host) {
      // TODO(arthursonzogni): Pass the real ResourceRequest. For the moment
      // only these 4 parameters will be used, but it may evolve quickly.
      auto resource_request = std::make_unique<network::ResourceRequest>();
      resource_request->url = common_params_.url;
      resource_request->method = common_params_.method;
      resource_request->request_initiator = begin_params_->initiator_origin;
      resource_request->referrer = common_params_.referrer.url;
      resource_request->has_user_gesture = common_params_.has_user_gesture;

      BrowserContext* browser_context =
          frame_tree_node_->navigator()->GetController()->GetBrowserContext();
      DownloadManagerImpl* download_manager = static_cast<DownloadManagerImpl*>(
          BrowserContext::GetDownloadManager(browser_context));
      download_manager->InterceptNavigation(
          std::move(resource_request), navigation_handle_->GetRedirectChain(),
          response_, std::move(url_loader_client_endpoints_),
          ssl_info_.cert_status, frame_tree_node_->frame_tree_node_id());

      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED),
          false /*skip_throttles*/, base::nullopt /*error_page_content*/,
          false /*collapse_frame*/);
      // DO NOT ADD CODE after this. The previous call to OnRequestFailed has
      // destroyed the NavigationRequest.
      return;
    }

    // Call ProceedWithResponse()
    // Note: There is no need to call ProceedWithResponse() when the Network
    // Service is enabled. See https://crbug.com/791049.
    if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      // |url_loader_client_endpoints_| is always valid, except in some tests
      // where the TestNavigationLoader is used.
      if (url_loader_client_endpoints_) {
        network::mojom::URLLoaderPtr url_loader(
            std::move(url_loader_client_endpoints_->url_loader));
        url_loader->ProceedWithResponse();
        url_loader_client_endpoints_->url_loader = url_loader.PassInterface();
      } else {
        loader_->ProceedWithResponse();
      }
    }
  }

  // Abort the request if needed. This includes requests that were blocked by
  // NavigationThrottles and requests that should not commit (e.g. downloads,
  // 204/205s). This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL ||
      !response_should_be_rendered_) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    if (!response_should_be_rendered_) {
      // DO NOT ADD CODE after this. The previous call to OnRequestFailed has
      // destroyed the NavigationRequest.
      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED),
          true /* skip_throttles */, base::nullopt /* error_page_content */,
          false /* collapse_frame */);
      return;
    }

    DCHECK(result.action() == NavigationThrottle::CANCEL ||
           result.net_error_code() == net::ERR_ABORTED);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(),
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (result.action() == NavigationThrottle::BLOCK_RESPONSE) {
    DCHECK_EQ(net::ERR_BLOCKED_BY_RESPONSE, result.net_error_code());
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(),
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  CommitNavigation();

  // DO NOT ADD CODE after this. The previous call to CommitNavigation caused
  // the destruction of the NavigationRequest.
}

void NavigationRequest::CommitErrorPage(
    RenderFrameHostImpl* render_frame_host,
    const base::Optional<std::string>& error_page_content) {
  UpdateRequestNavigationParamsHistory();
  frame_tree_node_->TransferNavigationRequestOwnership(render_frame_host);
  if (IsPerNavigationMojoInterfaceEnabled() && request_navigation_client_ &&
      request_navigation_client_.is_bound()) {
    // Two cases are possible here:
    // Either we have a same-site navigation in which case the navigation
    // request needs to be canceled in the RFH, so we need to rebind the handler
    // to a post-ReadyToCommit handler.
    // TODO(ahemery): Implement this second abort handler.
    // Or this navigation is cross-site: the original document should no longer
    // be able to cancel it.
    IgnorePipeDisconnection();
    if (associated_site_instance_id_ ==
        render_frame_host->GetSiteInstance()->GetId()) {
      // Reuse the request NavigationClient for commit.
      commit_navigation_client_ = std::move(request_navigation_client_);
    }
    associated_site_instance_id_.reset();
  }

  navigation_handle_->ReadyToCommitNavigation(render_frame_host, true);
  render_frame_host->FailedNavigation(
      navigation_handle_->GetNavigationId(), common_params_, request_params_,
      has_stale_copy_in_cache_, net_error_, error_page_content);
}

void NavigationRequest::CommitNavigation() {
  UpdateRequestNavigationParamsHistory();
  DCHECK(response_ || !IsURLHandledByNetworkStack(common_params_.url) ||
         navigation_handle_->IsSameDocument());
  DCHECK(!common_params_.url.SchemeIs(url::kJavaScriptScheme));

  // Send the applicable origin policy (if any) along with the request.
  // (The policy is fetched by a throttle and is thus available only now.)
  DCHECK(common_params_.origin_policy.empty());
  common_params_.origin_policy = navigation_handle_->origin_policy();

  // Retrieve the RenderFrameHost that needs to commit the navigation.
  RenderFrameHostImpl* render_frame_host =
      navigation_handle_->GetRenderFrameHost();
  DCHECK(render_frame_host ==
             frame_tree_node_->render_manager()->current_frame_host() ||
         render_frame_host ==
             frame_tree_node_->render_manager()->speculative_frame_host());

  frame_tree_node_->TransferNavigationRequestOwnership(render_frame_host);
  if (IsPerNavigationMojoInterfaceEnabled() && request_navigation_client_ &&
      request_navigation_client_.is_bound()) {
    // Two cases are possible here:
    // Either we have a same-site navigation in which case the navigation
    // request needs to be canceled in the RFH, so we need to rebind the handler
    // to a post-ReadyToCommit handler.
    // TODO(ahemery): Implement this second abort handler.
    // Or this navigation is cross-site: the original document should no longer
    // be able to cancel it.
    IgnorePipeDisconnection();
    if (associated_site_instance_id_ ==
        render_frame_host->GetSiteInstance()->GetId()) {
      // Reuse the request NavigationClient for commit.
      commit_navigation_client_ = std::move(request_navigation_client_);
    }
    associated_site_instance_id_.reset();
  }
  render_frame_host->CommitNavigation(
      navigation_handle_->GetNavigationId(), response_.get(),
      std::move(url_loader_client_endpoints_), common_params_, request_params_,
      is_view_source_, std::move(subresource_loader_params_),
      std::move(subresource_overrides_), devtools_navigation_token_);

  // Give SpareRenderProcessHostManager a heads-up about the most recently used
  // BrowserContext.  This is mostly needed to make sure the spare is warmed-up
  // if it wasn't done in RenderProcessHostImpl::GetProcessHostForSiteInstance.
  RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
      render_frame_host->GetSiteInstance()->GetBrowserContext());
}

bool NavigationRequest::IsAllowedByCSPDirective(
    CSPContext* context,
    CSPDirective::Name directive,
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check,
    CSPContext::CheckCSPDisposition disposition) {
  GURL url;
  // If this request was upgraded in the net stack, downgrade the URL back to
  // HTTP before checking report only policies.
  if (url_upgraded_after_redirect &&
      disposition == CSPContext::CheckCSPDisposition::CHECK_REPORT_ONLY_CSP &&
      common_params_.url.SchemeIs(url::kHttpsScheme)) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpScheme);
    url = common_params_.url.ReplaceComponents(replacements);
  } else {
    url = common_params_.url;
  }
  return context->IsAllowedByCsp(
      directive, url, has_followed_redirect, is_response_check,
      common_params_.source_location.value_or(SourceLocation()), disposition,
      begin_params_->is_form_submission);
}

net::Error NavigationRequest::CheckCSPDirectives(
    RenderFrameHostImpl* parent,
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check,
    CSPContext::CheckCSPDisposition disposition) {
  bool navigate_to_allowed = IsAllowedByCSPDirective(
      initiator_csp_context_.get(), CSPDirective::NavigateTo,
      has_followed_redirect, url_upgraded_after_redirect, is_response_check,
      disposition);

  bool frame_src_allowed = true;
  if (parent) {
    frame_src_allowed = IsAllowedByCSPDirective(
        parent, CSPDirective::FrameSrc, has_followed_redirect,
        url_upgraded_after_redirect, is_response_check, disposition);
  }

  if (navigate_to_allowed && frame_src_allowed)
    return net::OK;

  // If 'frame-src' fails, ERR_BLOCKED_BY_CLIENT is used instead.
  // If both checks fail, ERR_BLOCKED_BY_CLIENT is used to keep the existing
  // behaviour before 'navigate-to' was introduced.
  if (!frame_src_allowed)
    return net::ERR_BLOCKED_BY_CLIENT;

  // net::ERR_ABORTED is used to ensure that the navigation is cancelled
  // when the 'navigate-to' directive check is failed. This is a better user
  // experience as the user is not presented with an error page.
  return net::ERR_ABORTED;
}

net::Error NavigationRequest::CheckContentSecurityPolicy(
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check) {
  if (common_params_.url.SchemeIs(url::kAboutScheme))
    return net::OK;

  if (common_params_.initiator_csp_info.should_check_main_world_csp ==
      CSPDisposition::DO_NOT_CHECK) {
    return net::OK;
  }

  FrameTreeNode* parent_ftn = frame_tree_node()->parent();
  RenderFrameHostImpl* parent =
      parent_ftn ? parent_ftn->current_frame_host() : nullptr;

  // TODO(andypaicu,https://crbug.com/837627): the current_frame_host is the
  // wrong RenderFrameHost. We should be using the navigation initiator
  // RenderFrameHost.
  initiator_csp_context_->SetReportingRenderFrameHost(
      frame_tree_node()->current_frame_host());

  // CSP checking happens in three phases, per steps 3-5 of
  // https://fetch.spec.whatwg.org/#main-fetch:
  //
  // (1) Check report-only policies and trigger reports for any violations.
  // (2) Upgrade the request to HTTPS if necessary.
  // (3) Check enforced policies (triggering reports for any violations of those
  //     policies) and block the request if necessary.
  //
  // This sequence of events allows site owners to learn about (via step 1) any
  // requests that are upgraded in step 2.

  net::Error report_only_csp_status = CheckCSPDirectives(
      parent, has_followed_redirect, url_upgraded_after_redirect,
      is_response_check, CSPContext::CHECK_REPORT_ONLY_CSP);

  // upgrade-insecure-requests is handled in the network code for redirects,
  // only do the upgrade here if this is not a redirect.
  if (!has_followed_redirect && !frame_tree_node()->IsMainFrame()) {
    if (parent &&
        parent->ShouldModifyRequestUrlForCsp(true /* is subresource */)) {
      upgrade_if_insecure_ = true;
      parent->ModifyRequestUrlForCsp(&common_params_.url);
      request_params_.original_url = common_params_.url;
    }
  }

  net::Error enforced_csp_status = CheckCSPDirectives(
      parent, has_followed_redirect, url_upgraded_after_redirect,
      is_response_check, CSPContext::CHECK_ENFORCED_CSP);
  if (enforced_csp_status != net::OK)
    return enforced_csp_status;
  return report_only_csp_status;
}

NavigationRequest::CredentialedSubresourceCheckResult
NavigationRequest::CheckCredentialedSubresource() const {
  // It only applies to subframes.
  if (frame_tree_node_->IsMainFrame())
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  // URLs with no embedded credentials should load correctly.
  if (!common_params_.url.has_username() && !common_params_.url.has_password())
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  // Relative URLs on top-level pages that were loaded with embedded credentials
  // should load correctly.
  FrameTreeNode* parent_ftn = frame_tree_node_->parent();
  DCHECK(parent_ftn);
  const GURL& parent_url = parent_ftn->current_url();
  if (url::Origin::Create(parent_url)
          .IsSameOriginWith(url::Origin::Create(common_params_.url)) &&
      parent_url.username() == common_params_.url.username() &&
      parent_url.password() == common_params_.url.password()) {
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;
  }

  // Warn the user about the request being blocked.
  RenderFrameHostImpl* parent = parent_ftn->current_frame_host();
  DCHECK(parent);
  const char* console_message =
      "Subresource requests whose URLs contain embedded credentials (e.g. "
      "`https://user:pass@host/`) are blocked. See "
      "https://www.chromestatus.com/feature/5669008342777856 for more "
      "details.";
  parent->AddMessageToConsole(CONSOLE_MESSAGE_LEVEL_WARNING, console_message);

  if (!base::FeatureList::IsEnabled(features::kBlockCredentialedSubresources))
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  return CredentialedSubresourceCheckResult::BLOCK_REQUEST;
}

NavigationRequest::LegacyProtocolInSubresourceCheckResult
NavigationRequest::CheckLegacyProtocolInSubresource() const {
  // It only applies to subframes.
  if (frame_tree_node_->IsMainFrame())
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  if (!ShouldTreatURLSchemeAsLegacy(common_params_.url))
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  FrameTreeNode* parent_ftn = frame_tree_node_->parent();
  DCHECK(parent_ftn);
  const GURL& parent_url = parent_ftn->current_url();
  if (ShouldTreatURLSchemeAsLegacy(parent_url))
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  // Warn the user about the request being blocked.
  RenderFrameHostImpl* parent = parent_ftn->current_frame_host();
  DCHECK(parent);
  const char* console_message =
      "Subresource requests using legacy protocols (like `ftp:`) are blocked. "
      "Please deliver web-accessible resources over modern protocols like "
      "HTTPS. See https://www.chromestatus.com/feature/5709390967472128 for "
      "details.";
  parent->AddMessageToConsole(CONSOLE_MESSAGE_LEVEL_WARNING, console_message);

  return LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST;
}

void NavigationRequest::UpdateRequestNavigationParamsHistory() {
  NavigationController* navigation_controller =
      frame_tree_node_->navigator()->GetController();
  request_params_.current_history_list_offset =
      navigation_controller->GetCurrentEntryIndex();
  request_params_.current_history_list_length =
      navigation_controller->GetEntryCount();
}

void NavigationRequest::OnRendererAbortedNavigation() {
  frame_tree_node_->navigator()->CancelNavigation(frame_tree_node_, false);

  // Do not add code after this, NavigationRequest has been destroyed.
}

void NavigationRequest::IgnorePipeDisconnection() {
  return request_navigation_client_.set_connection_error_handler(
      base::DoNothing());
}

}  // namespace content

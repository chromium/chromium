// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_resource_request_utils.h"

#include "components/variations/net/variations_http_headers.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/preload_pipeline_info_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"
#include "url/origin.h"

namespace content {

namespace {

bool IsFirstPartyContext(const network::ResourceRequest& resource_request) {
  // TODO(crbug.com/40135370): Consider passing the Owner if we can get it.
  // However, we really only care about having the owner for requests initiated
  // on the renderer side.
  return variations::IsFirstPartyContext(variations::Owner::kUnknown,
                                         resource_request);
}

}  // namespace

// TODO(crbug.com/452392023): Currently this is for speculation rules
// prefetch only, but it should be extended to other prefetch embedder
// triggers.
// TODO(crbug.com/452392023): Add missing fields and update
// `tools/traffic_annotation/safe_list.txt`.
constexpr net::NetworkTrafficAnnotationTag
    kNavigationalPrefetchTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("speculation_rules_prefetch",
                                            R"(
          semantics {
            sender: "Speculation Rules Prefetch Loader"
            description:
              "Prefetches the mainframe HTML of a page specified via "
              "speculation rules. This is done out-of-band of normal "
              "prefetches to allow total isolation of this request from the "
              "rest of browser traffic and user state like cookies and cache."
            trigger:
              "Used only when this feature and speculation rules feature are "
              "enabled."
            data: "None."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control this via a setting specific to each content "
              "embedder."
            policy_exception_justification: "Not implemented."
        })");

// ------------------------------------------------------------------------
// [1] Additional headers:
void AddAdditionalHeaders(net::HttpRequestHeaders& request_headers,
                          const PrefetchRequest& prefetch_request) {
  const auto& additional_headers = prefetch_request.additional_headers();
  // Ignore "User-Agent" override by `additional_headers` if UA override fix are
  // enabled.
  // TODO(crbug.com/383779480): Add tests.
  if (base::FeatureList::IsEnabled(
          features::kPreloadingRespectUserAgentOverride)) {
    net::HttpRequestHeaders additional_headers_without_ua = additional_headers;
    additional_headers_without_ua.RemoveHeader(
        net::HttpRequestHeaders::kUserAgent);
    request_headers.MergeFrom(additional_headers_without_ua);
  } else {
    request_headers.MergeFrom(additional_headers);
  }
}

// ------------------------------------------------------------------------
// [2] `Sec-Purpose`:
// Returns "Sec-Purpose" header value for a prefetch request to
// `request_url_origin`.
void AddSecPurposeHeader(net::HttpRequestHeaders& request_headers,
                         const url::Origin& request_url_origin,
                         const PrefetchRequest& prefetch_request) {
  const char* header_value = [&]() {
    switch (
        prefetch_request.preload_pipeline_info_planned_max_preloading_type()) {
      case PreloadingType::kPrefetch:
        if (prefetch_request.IsProxyRequiredForURL(request_url_origin)) {
          return blink::kSecPurposePrefetchAnonymousClientIpHeaderValue;
        } else {
          return blink::kSecPurposePrefetchHeaderValue;
        }
      case PreloadingType::kPrerenderUntilScript:
      case PreloadingType::kPrerender:
        if (prefetch_request.IsProxyRequiredForURL(request_url_origin)) {
          // Note that this path would be reachable if a prefetch ahead of
          // prerender were triggered with a speculation candidate with
          // `requires_anonymous_client_ip_when_cross_origin`. But such
          // Speculation Rules are discarded in blink.
          //
          // See
          // https://github.com/WICG/nav-speculation/blob/main/triggers.md#requirements
          NOTREACHED();
        } else {
          return blink::kSecPurposePrefetchPrerenderHeaderValue;
        }
      case PreloadingType::kUnspecified:
      case PreloadingType::kPreconnect:
      case PreloadingType::kNoStatePrefetch:
        NOTREACHED();
    }
  }();

  request_headers.SetHeader(blink::kSecPurposeHeaderName, header_value);
}

// ------------------------------------------------------------------------
// [2] `Sec-Speculation-Tags`:
// Adds Speculation Rules Tags headers for a prefetch request to
// `request_url_origin` to `request_headers`.
void AddSpeculationTagsHeader(net::HttpRequestHeaders& request_headers,
                              const url::Origin& request_url_origin,
                              const PrefetchRequest& prefetch_request) {
  // Sec-Speculation-Tags is set only when the prefetch is triggered
  // by speculation rules and it is not cross-site prefetch.
  // To see more details:
  // https://github.com/WICG/nav-speculation/blob/main/speculation-rules-tags.md#the-cross-site-case
  if (prefetch_request.speculation_rules_tags().has_value() &&
      !prefetch_request.IsCrossSiteRequest(request_url_origin)) {
    std::optional<std::string> serialized_list =
        prefetch_request.speculation_rules_tags()
            ->ConvertStringToHeaderString();
    CHECK(serialized_list.has_value());
    request_headers.SetHeader(blink::kSecSpeculationTagsHeaderName,
                              serialized_list.value());
  }
}

// ------------------------------------------------------------------------
// [2] `X-Client-Data`:
// Adds "X-Client-Data" header for a prefetch request to `request_url`.
// `cors_exempt_headers` corresponds to `ResourceRequest::cors_exempt_headers`.
// Actually only the origin of `request_url` is used for decision.
void AddVariationsHeaderForPrefetch(
    net::HttpRequestHeaders& cors_exempt_headers,
    const GURL& request_url,
    const PrefetchRequest& prefetch_request,
    bool is_first_party_context_for_variations) {
  CHECK(prefetch_request.browser_context());

  // Add X-Client-Data header with experiment IDs from field trials.
  if (std::optional<std::string> value =
          variations::GetVariationsHeaderValueToAppend(
              request_url,
              prefetch_request.browser_context()->IsOffTheRecord()
                  ? variations::InIncognito::kYes
                  : variations::InIncognito::kNo,
              variations::SignedIn::kNo,
              is_first_party_context_for_variations)) {
    cors_exempt_headers.SetHeaderIfMissing(variations::kClientDataHeader,
                                           *value);
  }
}

void UpdateVariationsHeaderForPrefetch(
    network::ResourceRequest& resource_request,
    const PrefetchRequest& prefetch_request) {
  // Remove `variations::kClientDataHeader` from `resource_request_->headers`,
  // to keep the existing behavior. While `AddVariationsHeaderForPrefetch()`
  // adds `variations::kClientDataHeader` to
  // `resource_request->cors_exempt_headers`, it's also possible that
  // `variations::kClientDataHeader` is added to `resource_request_->headers`
  // via `request().additional_headers()`.
  //
  // TODO(crbug.com/467177773): The processing of
  // `variations::kClientDataHeader` is separated from other headers, to keep
  // the behavior of `variations::kClientDataHeader` during the main fixes for
  // crbug.com/467177773. The behavior of `variations::kClientDataHeader` should
  // be fixed together with other related bugs, by e.g. restructuring
  // `variations::AppendVariationsHeader()` and plumbing the
  // `variations::kClientDataHeader` removal and modification to
  // `FollowRedirect()`.
  // TODO(crbug.com/454082776): Remove `variations::kClientDataHeader` from
  // `resource_request->cors_exempt_headers`.
  resource_request.headers.RemoveHeader(variations::kClientDataHeader);
  AddVariationsHeaderForPrefetch(resource_request.cors_exempt_headers,
                                 resource_request.url, prefetch_request,
                                 IsFirstPartyContext(resource_request));
}

// ------------------------------------------------------------------------
// [3] WebContents `User-Agent` override:
// Returns if WebContents-level UA overrides should be applied for a prefetch
// request for `request_url`. Note that not only the User-Agent header but
// also Client-Hints headers are affected by the UA overrides.
// The returned value is for an initial guess and shouldn't be used without a
// plan for the header validation (crbug.com/444065296).
bool ShouldApplyUserAgentOverride(const GURL& request_url,
                                  const PrefetchRequest& prefetch_request) {
  if (!base::FeatureList::IsEnabled(
          features::kPreloadingRespectUserAgentOverride)) {
    return false;
  }

  WebContents* referring_web_contents =
      prefetch_request.referring_web_contents().get();
  if (!referring_web_contents) {
    return false;
  }
  // The empty `ua_string_override` means no registered UA overrides.
  if (const blink::UserAgentOverride& ua_override =
          referring_web_contents->GetUserAgentOverride();
      ua_override.ua_string_override.empty()) {
    return false;
  }
  raw_ptr<WebContentsDelegate> delegate = referring_web_contents->GetDelegate();
  NavigationController::UserAgentOverrideOption option =
      delegate ? delegate->ShouldOverrideUserAgentForPreloading(request_url)
               : NavigationController::UA_OVERRIDE_INHERIT;
  // Use the primary main frame of initiator's WebContents to guess if we should
  // apply UA overrides in this prefetch request. Note that this decision is
  // independent with that of policy checking on ClientHints headers. This is an
  // estimation, i.e., can lead to wrong choices in some cases (e.g., where the
  // prefetched result is used in prerender for another WebContents).
  // TODO(crbug.com/444065296): Update this comment after the header comparison
  // between prefetch and prerender is implemented.
  auto* render_frame_host = referring_web_contents->GetPrimaryMainFrame();
  CHECK(render_frame_host);
  auto& nav_controller = static_cast<NavigationControllerImpl&>(
      render_frame_host->GetController());
  return nav_controller.ShouldOverrideUserAgentInNextNavigation(option);
}

// Adds the `User-Agent` header by UA override from WebContents if applicable.
void MaybeApplyOverrideForWebContentsUserAgentHeader(
    net::HttpRequestHeaders& request_headers,
    const GURL& request_url,
    const PrefetchRequest& prefetch_request) {
  if (!ShouldApplyUserAgentOverride(request_url, prefetch_request)) {
    return;
  }
  WebContents* referring_web_contents =
      prefetch_request.referring_web_contents().get();
  if (!referring_web_contents) {
    return;
  }
  // TODO(crbug.com/444065296): This is an initial guess, because e.g.
  // `referring_web_contents` might be different from the navigation target's
  // WebContents. Validate this against the actual navigation's header.
  const blink::UserAgentOverride& ua_override =
      referring_web_contents->GetUserAgentOverride();
  CHECK(!ua_override.ua_string_override.empty());
  request_headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                            ua_override.ua_string_override);
}

// ------------------------------------------------------------------------
// [2] Client Hints:
// [4] DevTools overrides (Client Hints):
// Adds client hints headers to a request bound for `origin`.
// TODO(crbug.com/422193319): Reconsider the appropriate place to set DevTools
// override of non-UA Client Hints.
void AddClientHintsHeaders(net::HttpRequestHeaders& request_headers,
                           const url::Origin& origin,
                           const PrefetchRequest& prefetch_request) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchClientHints)) {
    return;
  }
  if (!prefetch_request.browser_context()) {
    return;
  }
  ClientHintsControllerDelegate* client_hints_delegate =
      prefetch_request.browser_context()->GetClientHintsControllerDelegate();
  if (!client_hints_delegate) {
    return;
  }

  auto* referring_ftn = base::FeatureList::IsEnabled(
                            features::kPrefetchDevtoolsUserAgentOverride) &&
                                prefetch_request.referring_web_contents()
                            ? FrameTreeNode::From(RenderFrameHostImpl::FromID(
                                  prefetch_request.referring_web_contents()
                                      ->GetPrimaryMainFrame()
                                      ->GetGlobalId()))
                            : nullptr;

  // TODO(crbug.com/41497015): Consider supporting UA override mode here.
  const bool is_ua_override_on = false;
  net::HttpRequestHeaders client_hints_headers;
  if (prefetch_request.is_javascript_enabled()) {
    // Add Client Hints headers
    //
    // Historically, `AddClientHintsHeadersToPrefetchNavigation` added
    // Client Hints headers iff `prefetch_request.is_javascript_enabled()`, so
    // the `if` block here is to persist the behavior.
    // TODO(crbug.com/394716357): Revisit if we really want to allow prefetch
    // for non-Javascript enabled profile/origins.
    //
    // The request headers added by `referring_ftn` is the initial guess for the
    // request headers that will be used in the navigations served by this
    // prefetch, and can be different from the navigation target's
    // `FrameTreeNode` (crbug.com/444065296).
    // TODO(crbug.com/444065296): Validate the Client Hint headers added here
    // using `referring_ftn` against the navigation request's headers.
    AddClientHintsHeadersToPrefetchNavigation(
        origin, &client_hints_headers, prefetch_request.browser_context(),
        client_hints_delegate, is_ua_override_on, referring_ftn);
  }

  // Merge in the client hints which are suitable to include given this is a
  // prefetch, and potentially a cross-site only. (This logic might need to be
  // revisited if we ever supported prefetching in another site's partition,
  // such as in a subframe.)
  const bool is_cross_site = prefetch_request.IsCrossSiteRequest(origin);
  const auto cross_site_behavior =
      features::kPrefetchClientHintsCrossSiteBehavior.Get();
  if (!is_cross_site ||
      cross_site_behavior ==
          features::PrefetchClientHintsCrossSiteBehavior::kAll) {
    request_headers.MergeFrom(client_hints_headers);
  } else if (cross_site_behavior ==
             features::PrefetchClientHintsCrossSiteBehavior::kLowEntropy) {
    for (const auto& [ch, header] : network::GetClientHintToNameMap()) {
      if (blink::IsClientHintSentByDefault(ch)) {
        std::optional<std::string> header_value =
            client_hints_headers.GetHeader(header);
        if (header_value) {
          request_headers.SetHeader(header, std::move(header_value).value());
        }
      }
    }
  }
}

// ------------------------------------------------------------------------
// [4] DevTools overrides (`User-Agent`, `Accept-Language`, non-UA Client
// Hints):
void MaybeApplyOverrideForDevtoolsUserAgentHeader(
    net::HttpRequestHeaders& request_headers,
    const PrefetchRequest& prefetch_request) {
  if (!base::FeatureList::IsEnabled(
          features::kPrefetchDevtoolsUserAgentOverride) ||
      !prefetch_request.referring_web_contents()) {
    return;
  }

  auto* referring_ftn = FrameTreeNode::From(
      RenderFrameHostImpl::FromID(prefetch_request.referring_web_contents()
                                      ->GetPrimaryMainFrame()
                                      ->GetGlobalId()));
  // This is an initial guess (crbug.com/444065296), e.g. ideally, the
  // DevTools UA overrides of the navigation target FrameTreeNode should be
  // used, but this is not available at the time of prefetch, so we use the
  // prefetch initiator's FrameTreeNode instead as an initial guess.
  // TODO(crbug.com/444065296): Validate the header against the actual
  // navigation's request header.
  //
  // For now, we only apply a part of
  // `devtools_instrumentation::ApplyNetworkRequestOverrides()` which is
  // applied to navigational request in
  // `NavigationRequest::OnStartChecksComplete()`.
  if (referring_ftn && RenderFrameDevToolsAgentHost::GetFor(referring_ftn)) {
    // Add/override `User-Agent` headers for DevTools emulation mode  by
    // `referring_ftn`'s devtools emulation mode.
    // TODO(crbug.com/422193319): This part only addresses devtools emulation
    // mode UA override. There are other types of UA overrides, which are at
    // WebContents level.
    devtools_instrumentation::ApplyEmulationOverrides(
        RenderFrameDevToolsAgentHost::GetFor(referring_ftn), &request_headers);
  }
}

PrefetchUpdateHeadersParams PrepareInitialHeadersForPrefetchPhase1(
    const GURL& request_url,
    const PrefetchRequest& prefetch_request) {
  PrefetchUpdateHeadersParams params;

  url::Origin request_url_origin = url::Origin::Create(request_url);

  // ------------------------------------------------------------------------
  // [1] Additional headers:
  AddAdditionalHeaders(params.modified_headers, prefetch_request);

  // ------------------------------------------------------------------------
  // [2] `Upgrade-Insecure-Requests`:
  params.modified_headers.SetHeader("Upgrade-Insecure-Requests", "1");

  // ------------------------------------------------------------------------
  // [2] `Purpose`:
  if (!base::FeatureList::IsEnabled(
          blink::features::kRemovePurposeHeaderForPrefetch)) {
    params.modified_headers.SetHeader(blink::kPurposeHeaderName,
                                      blink::kSecPurposePrefetchHeaderValue);
  }

  // ------------------------------------------------------------------------
  // [2] `Sec-Purpose`:
  AddSecPurposeHeader(params.modified_headers, request_url_origin,
                      prefetch_request);

  // ------------------------------------------------------------------------
  // [2] `Sec-Speculation-Tags`:
  AddSpeculationTagsHeader(params.modified_headers, request_url_origin,
                           prefetch_request);

  return params;
}

PrefetchUpdateHeadersParams PrepareInitialHeadersForPrefetchPhase2(
    const GURL& request_url,
    const PrefetchRequest& prefetch_request,
    bool is_first_party_context_for_variations_header) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PrefetchUpdateHeadersParams params;

  url::Origin request_url_origin = url::Origin::Create(request_url);

  // [2] `Accept`:
  CHECK(prefetch_request.browser_context());
  params.modified_headers.SetHeader(
      net::HttpRequestHeaders::kAccept,
      FrameAcceptHeaderValue(/*allow_sxg_responses=*/true,
                             prefetch_request.browser_context()));

  // ------------------------------------------------------------------------
  // [2] `X-Client-Data`:
  if (prefetch_request.should_append_variations_header()) {
    AddVariationsHeaderForPrefetch(
        params.modified_cors_exempt_headers, request_url, prefetch_request,
        is_first_party_context_for_variations_header);
  }

  // ------------------------------------------------------------------------
  // [2] Embedder headers:
  {
    std::vector<std::string> removed_headers;
    net::HttpRequestHeaders modified_headers;
    net::HttpRequestHeaders modified_cors_exempt_headers;
    // The current callee only uses the origin of `request_url`.
    GetContentClient()->browser()->ModifyRequestHeadersForPrefetch(
        request_url, removed_headers, modified_headers,
        modified_cors_exempt_headers);
    params.modified_headers.MergeFrom(modified_headers);
    params.modified_cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);
  }

  // TODO(crbug.com/444065296): The following headers are an initial guess.
  // Validate them against the actual navigation's header.

  // ------------------------------------------------------------------------
  // [3] `User-Agent` override:
  MaybeApplyOverrideForWebContentsUserAgentHeader(
      params.modified_headers, request_url, prefetch_request);

  // ------------------------------------------------------------------------
  // [2] Client Hints:
  // [4] DevTools overrides (Client Hints):
  // TODO(crbug.com/422193319): Reconsider the appropriate place to set DevTools
  // override of non-UA Client Hints.
  AddClientHintsHeaders(params.modified_headers, request_url_origin,
                        prefetch_request);

  // ------------------------------------------------------------------------
  // [4] DevTools overrides (`User-Agent`, `Accept-Language`, non-UA Client
  // Hints): The DevTools override is executed AFTER the WebContents override
  // above because the DevTools override has higher priority than the
  // WebContents override. See also the comment in
  // `PrefetchContainer::MakeResourceRequest()` for the overriding order.
  MaybeApplyOverrideForDevtoolsUserAgentHeader(params.modified_headers,
                                               prefetch_request);

  return params;
}

std::tuple<PrefetchUpdateHeadersParams, PrefetchUpdateHeadersParams>
PrepareRedirectHeadersForPrefetch(const GURL& request_url,
                                  const PrefetchRequest& prefetch_request) {
  // There are sometimes other headers that are modified during navigation
  // redirects; see `NavigationRequest::OnRedirectChecksComplete` (including
  // some which are added by throttles). These aren't yet supported for
  // prefetch, including browsing topics.

  PrefetchUpdateHeadersParams updates_for_resource_request;
  PrefetchUpdateHeadersParams updates_for_follow_redirect;

  url::Origin request_url_origin = url::Origin::Create(request_url);

  // ------------------------------------------------------------------------
  // [2] `Sec-Purpose`:
  AddSecPurposeHeader(updates_for_resource_request.modified_headers,
                      request_url_origin, prefetch_request);
  if (base::FeatureList::IsEnabled(
          features::kPrefetchFixHeaderUpdatesOnRedirect)) {
    AddSecPurposeHeader(updates_for_follow_redirect.modified_headers,
                        request_url_origin, prefetch_request);
  }

  // ------------------------------------------------------------------------
  // [2] `Sec-Speculation-Tags`:
  updates_for_resource_request.removed_headers.push_back(
      blink::kSecSpeculationTagsHeaderName);
  AddSpeculationTagsHeader(updates_for_resource_request.modified_headers,
                           request_url_origin, prefetch_request);
  if (base::FeatureList::IsEnabled(
          features::kPrefetchFixHeaderUpdatesOnRedirect)) {
    updates_for_follow_redirect.removed_headers.push_back(
        blink::kSecSpeculationTagsHeaderName);
    AddSpeculationTagsHeader(updates_for_follow_redirect.modified_headers,
                             request_url_origin, prefetch_request);
  }

  // ------------------------------------------------------------------------
  // [2] Embedder headers:
  {
    std::vector<std::string> removed_headers;
    net::HttpRequestHeaders modified_headers;
    net::HttpRequestHeaders modified_cors_exempt_headers;
    GetContentClient()->browser()->ModifyRequestHeadersForPrefetch(
        request_url, removed_headers, modified_headers,
        modified_cors_exempt_headers);
    auto add_embedder_headers = [&](PrefetchUpdateHeadersParams& params) {
      params.removed_headers.reserve(params.removed_headers.size() +
                                     removed_headers.size());
      params.removed_headers.insert(params.removed_headers.end(),
                                    removed_headers.begin(),
                                    removed_headers.end());
      params.modified_headers.MergeFrom(modified_headers);
      params.modified_cors_exempt_headers.MergeFrom(
          modified_cors_exempt_headers);
    };
    add_embedder_headers(updates_for_resource_request);
    add_embedder_headers(updates_for_follow_redirect);
  }

  // ------------------------------------------------------------------------
  // [3] WebContents override (`User-Agent`):
  // TODO(crbug.com/441612842): Support User-Agent overrides, which is applied
  // for the initial request by
  // `MaybeApplyOverrideForWebContentsUserAgentHeader()`.

  // ------------------------------------------------------------------------
  // [2] Client Hints:
  // [4] DevTools overrides (User-Agent Client Hints):
  // Remove any existing client hints headers, then (re-)add the new client
  // hints that are appropriate for the redirect.
  if (base::FeatureList::IsEnabled(features::kPrefetchClientHints)) {
    const auto& client_hints = network::GetClientHintToNameMap();
    updates_for_resource_request.removed_headers.reserve(
        updates_for_resource_request.removed_headers.size() +
        client_hints.size());
    for (const auto& [_, header] : client_hints) {
      updates_for_resource_request.removed_headers.push_back(header);
    }
    AddClientHintsHeaders(updates_for_resource_request.modified_headers,
                          request_url_origin, prefetch_request);

    if (base::FeatureList::IsEnabled(
            features::kPrefetchFixHeaderUpdatesOnRedirect)) {
      updates_for_follow_redirect.removed_headers.reserve(
          updates_for_follow_redirect.removed_headers.size() +
          client_hints.size());
      for (const auto& [_, header] : client_hints) {
        updates_for_follow_redirect.removed_headers.push_back(header);
      }
      AddClientHintsHeaders(updates_for_follow_redirect.modified_headers,
                            request_url_origin, prefetch_request);
    }
  }

  // ------------------------------------------------------------------------
  // [4] DevTools overrides (`User-Agent`, `Accept-Language`, non-UA Client
  // Hints):
  // TODO(crbug.com/422193319): Reconsider the appropriate place to set DevTools
  // override of non-UA Client Hints.
  {
    MaybeApplyOverrideForDevtoolsUserAgentHeader(
        updates_for_resource_request.modified_headers, prefetch_request);

    if (base::FeatureList::IsEnabled(
            features::kPrefetchFixHeaderUpdatesOnRedirect)) {
      MaybeApplyOverrideForDevtoolsUserAgentHeader(
          updates_for_follow_redirect.modified_headers, prefetch_request);
    }
  }

  return std::make_tuple(std::move(updates_for_resource_request),
                         std::move(updates_for_follow_redirect));
}

mojo::PendingRemote<network::mojom::DevToolsObserver>
MaybeMakeSelfOwnedNetworkServiceDevToolsObserverForPrefetch(
    const PrefetchRequest& prefetch_request) {
  auto* renderer_initiator_info = prefetch_request.GetRendererInitiatorInfo();
  if (!renderer_initiator_info) {
    // Don't emit CDP events if the trigger is not speculation rules.
    return mojo::NullRemote();
  }

  auto* ftn =
      FrameTreeNode::From(renderer_initiator_info->GetRenderFrameHost());
  if (!ftn) {
    // Don't emit CDP events if the initiator document isn't alive.
    return mojo::NullRemote();
  }

  return NetworkServiceDevToolsObserver::MakeSelfOwned(ftn);
}

// ------------------------------------------------------------------------
// Constructs a `ResourceRequest` without headers.
// Headers should be added using `PrepareInitialHeadersForPrefetch*()`, in
// `MakeInitialResourceRequestForPrefetch()` or separately for OMT prefetch.
std::unique_ptr<network::ResourceRequest>
MakeInitialResourceRequestWithoutHeadersForPrefetch(
    const PrefetchRequest& prefetch_request,
    bool is_decoy) {
  const GURL& url = prefetch_request.key().url();
  url::Origin origin = url::Origin::Create(url);
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));

  auto priority = [&] {
    if (prefetch_request.priority().has_value()) {
      switch (prefetch_request.priority().value()) {
        case PrefetchPriority::kLow:
          return net::RequestPriority::IDLE;
        case PrefetchPriority::kMedium:
          return net::RequestPriority::LOW;
        case PrefetchPriority::kHigh:
          return net::RequestPriority::MEDIUM;
        case PrefetchPriority::kHighest:
          return net::RequestPriority::HIGHEST;
      }
    }

    // TODO(crbug.com/426404355): Migrate to use `PrefetchPriority`.
    if (IsSpeculationRuleType(
            prefetch_request.prefetch_type().trigger_type())) {
      // This may seem inverted (surely immediate prefetches would be higher
      // priority), but the fact that we're doing this at all for more
      // conservative candidates suggests a strong engagement signal.
      //
      // TODO(crbug.com/40276985): Ideally, we would actually use a combination
      // of the actual engagement seen (rather than the minimum required to
      // trigger the candidate) and the declared eagerness, and update them as
      // the prefetch becomes increasingly likely.
      blink::mojom::SpeculationEagerness eagerness =
          prefetch_request.prefetch_type().GetEagerness();
      switch (eagerness) {
        case blink::mojom::SpeculationEagerness::kConservative:
          return net::RequestPriority::MEDIUM;
        case blink::mojom::SpeculationEagerness::kModerate:
          return net::RequestPriority::LOW;
        // TODO(crbug.com/40287486, crbug.com/406927300): Set appropriate value
        // after changing the behavior for `kEager`
        case blink::mojom::SpeculationEagerness::kEager:
        case blink::mojom::SpeculationEagerness::kImmediate:
          return net::RequestPriority::IDLE;
      }
    } else {
      if (base::FeatureList::IsEnabled(
              features::kPrefetchNetworkPriorityForEmbedders)) {
        return net::RequestPriority::MEDIUM;
      } else {
        return net::RequestPriority::IDLE;
      }
    }
  }();

  mojo::PendingRemote<network::mojom::DevToolsObserver>
      devtools_observer_remote;
  if (!is_decoy) {
    devtools_observer_remote =
        MaybeMakeSelfOwnedNetworkServiceDevToolsObserverForPrefetch(
            prefetch_request);
  }

  // If we ever implement prefetching for subframes, this value should be
  // reconsidered, as this causes us to reset the site for cookies on cross-site
  // redirect.
  const bool is_main_frame = true;

  auto resource_request = CreateResourceRequestForNavigation(
      net::HttpRequestHeaders::kGetMethod, url,
      network::mojom::RequestDestination::kDocument,
      prefetch_request.initial_referrer(), isolation_info,
      std::move(devtools_observer_remote), priority, is_main_frame);

  // Note: Even without LOAD_DISABLE_CACHE, a cross-site prefetch uses a
  // separate network context, which means responses cached before the prefetch
  // are not visible to the prefetch, and anything cached by this request will
  // not be visible outside of the network context.
  resource_request->load_flags = net::LOAD_PREFETCH;

  // TODO(crbug.com/455296998): Remove this code for M145.
  if (prefetch_request.should_bypass_http_cache()) {
    resource_request->load_flags |= net::LOAD_DISABLE_CACHE;
  }

  // ------------------------------------------------------------------------
  // There are sometimes other headers that are set during navigation.  These
  // aren't yet supported for prefetch, including browsing topics.

  resource_request->devtools_request_id =
      base::UnguessableToken::Create().ToString();

  // `URLLoaderNetworkServiceObserver`
  // (`resource_request->trusted_params->url_loader_network_observer`) is NOT
  // set here, because for prefetching request we don't want to ask users e.g.
  // for authentication/cert errors, and instead make the prefetch fail. Because
  // of this, `ServiceWorkerClient::GetOngoingNavigationRequestBeforeCommit()`
  // is never called. `NavPrefetchBrowserTest` has the corresponding test
  // coverage.

  // Prefetches with `skip_service_worker` == `true` shouldn't serve navigation
  // with `skip_service_worker` == `false`, but right now we don't support such
  // prefetches.
  // TODO(https://crbug.com/438478667): Revisit this.
  CHECK(!resource_request->skip_service_worker);

  return resource_request;
}

std::unique_ptr<network::ResourceRequest> MakeInitialResourceRequestForPrefetch(
    const PrefetchRequest& prefetch_request,
    bool is_decoy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto resource_request = MakeInitialResourceRequestWithoutHeadersForPrefetch(
      prefetch_request, is_decoy);

  PrefetchUpdateHeadersParams headers_params1 =
      PrepareInitialHeadersForPrefetchPhase1(resource_request->url,
                                             prefetch_request);

  PrefetchUpdateHeadersParams headers_params2 =
      PrepareInitialHeadersForPrefetchPhase2(
          resource_request->url, prefetch_request,
          IsFirstPartyContext(*resource_request));

  CHECK(headers_params1.removed_headers.empty());
  resource_request->headers.MergeFrom(headers_params1.modified_headers);
  resource_request->cors_exempt_headers.MergeFrom(
      headers_params1.modified_cors_exempt_headers);

  CHECK(headers_params2.removed_headers.empty());
  resource_request->headers.MergeFrom(headers_params2.modified_headers);
  resource_request->cors_exempt_headers.MergeFrom(
      headers_params2.modified_cors_exempt_headers);

  return resource_request;
}

std::unique_ptr<network::ResourceRequest>
MakeInitialResourceRequestForPrePrefetch(
    const PrefetchRequest& prefetch_request,
    const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers) {
  auto resource_request = MakeInitialResourceRequestWithoutHeadersForPrefetch(
      prefetch_request, /*is_decoy=*/false);

  PrefetchUpdateHeadersParams omt_calculated_headers =
      PrepareInitialHeadersForPrefetchPhase1(prefetch_request.key().url(),
                                             prefetch_request);

  CHECK(omt_calculated_headers.removed_headers.empty());
  resource_request->headers.MergeFrom(omt_calculated_headers.modified_headers);
  resource_request->cors_exempt_headers.MergeFrom(
      omt_calculated_headers.modified_cors_exempt_headers);

  CHECK(ui_thread_pre_calculated_headers.removed_headers.empty());
  resource_request->headers.MergeFrom(
      ui_thread_pre_calculated_headers.modified_headers);
  resource_request->cors_exempt_headers.MergeFrom(
      ui_thread_pre_calculated_headers.modified_cors_exempt_headers);

  return resource_request;
}

}  // namespace content

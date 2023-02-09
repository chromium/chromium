/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-blink.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/back_forward_cache_loader_helper_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/loader_factory_for_frame.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/resource_load_observer_for_frame.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

mojom::FetchCacheMode DetermineFrameCacheMode(Frame* frame) {
  if (!frame)
    return mojom::FetchCacheMode::kDefault;
  auto* local_frame = DynamicTo<LocalFrame>(frame);
  if (!local_frame)
    return DetermineFrameCacheMode(frame->Tree().Parent());

  // Does not propagate cache policy for subresources after the load event.
  // TODO(toyoshim): We should be able to remove following parents' policy check
  // if each frame has a relevant WebFrameLoadType for reload and history
  // navigations.
  if (local_frame->GetDocument()->LoadEventFinished())
    return mojom::FetchCacheMode::kDefault;

  // Respects BypassingCache rather than parent's policy.
  WebFrameLoadType load_type =
      local_frame->Loader().GetDocumentLoader()->LoadType();
  if (load_type == WebFrameLoadType::kReloadBypassingCache)
    return mojom::FetchCacheMode::kBypassCache;

  // Respects parent's policy if it has a special one.
  mojom::FetchCacheMode parent_cache_mode =
      DetermineFrameCacheMode(frame->Tree().Parent());
  if (parent_cache_mode != mojom::FetchCacheMode::kDefault)
    return parent_cache_mode;

  // Otherwise, follows WebFrameLoadType.
  switch (load_type) {
    case WebFrameLoadType::kStandard:
    case WebFrameLoadType::kReplaceCurrentItem:
      return mojom::FetchCacheMode::kDefault;
    case WebFrameLoadType::kBackForward:
      // Mutates the policy for POST requests to avoid form resubmission.
      return mojom::FetchCacheMode::kForceCache;
    case WebFrameLoadType::kReload:
      return mojom::FetchCacheMode::kDefault;
    case WebFrameLoadType::kReloadBypassingCache:
      return mojom::FetchCacheMode::kBypassCache;
  }
  NOTREACHED();
  return mojom::FetchCacheMode::kDefault;
}

}  // namespace

struct FrameFetchContext::FrozenState final : GarbageCollected<FrozenState> {
  FrozenState(const KURL& url,
              ContentSecurityPolicy* content_security_policy,
              net::SiteForCookies site_for_cookies,
              scoped_refptr<const SecurityOrigin> top_frame_origin,
              const ClientHintsPreferences& client_hints_preferences,
              float device_pixel_ratio,
              const String& user_agent,
              const absl::optional<UserAgentMetadata>& user_agent_metadata,
              bool is_svg_image_chrome_client,
              bool is_prerendering,
              const String& reduced_accept_language)
      : url(url),
        content_security_policy(content_security_policy),
        site_for_cookies(std::move(site_for_cookies)),
        top_frame_origin(std::move(top_frame_origin)),
        client_hints_preferences(client_hints_preferences),
        device_pixel_ratio(device_pixel_ratio),
        user_agent(user_agent),
        user_agent_metadata(user_agent_metadata),
        is_svg_image_chrome_client(is_svg_image_chrome_client),
        is_prerendering(is_prerendering),
        reduced_accept_language(reduced_accept_language) {}

  const KURL url;
  const scoped_refptr<const SecurityOrigin> parent_security_origin;
  const Member<ContentSecurityPolicy> content_security_policy;
  const net::SiteForCookies site_for_cookies;
  const scoped_refptr<const SecurityOrigin> top_frame_origin;
  const ClientHintsPreferences client_hints_preferences;
  const float device_pixel_ratio;
  const String user_agent;
  const absl::optional<UserAgentMetadata> user_agent_metadata;
  const bool is_svg_image_chrome_client;
  const bool is_prerendering;
  const String reduced_accept_language;

  void Trace(Visitor* visitor) const {
    visitor->Trace(content_security_policy);
  }
};

ResourceFetcher* FrameFetchContext::CreateFetcherForCommittedDocument(
    DocumentLoader& loader,
    Document& document) {
  auto& properties = *MakeGarbageCollected<DetachableResourceFetcherProperties>(
      *MakeGarbageCollected<FrameResourceFetcherProperties>(loader, document));
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);
  auto* frame_fetch_context =
      MakeGarbageCollected<FrameFetchContext>(loader, document, properties);
  ResourceFetcherInit init(
      properties, frame_fetch_context,
      frame->GetTaskRunner(TaskType::kNetworking),
      frame->GetTaskRunner(TaskType::kNetworkingUnfreezable),
      MakeGarbageCollected<LoaderFactoryForFrame>(loader, *frame->DomWindow()),
      frame->DomWindow(),
      MakeGarbageCollected<BackForwardCacheLoaderHelperImpl>(*frame));
  init.use_counter =
      MakeGarbageCollected<DetachableUseCounter>(frame->DomWindow());
  init.console_logger = MakeGarbageCollected<DetachableConsoleLogger>(
      document.GetExecutionContext());
  // Frame loading should normally start with |kTight| throttling, as the
  // frame will be in layout-blocking state until the <body> tag is inserted
  init.initial_throttling_policy =
      ResourceLoadScheduler::ThrottlingPolicy::kTight;
  init.frame_or_worker_scheduler = frame->GetFrameScheduler();
  init.archive = loader.Archive();
  init.loading_behavior_observer = frame_fetch_context;
  ResourceFetcher* fetcher = MakeGarbageCollected<ResourceFetcher>(init);
  fetcher->SetResourceLoadObserver(
      MakeGarbageCollected<ResourceLoadObserverForFrame>(
          loader, document, fetcher->GetProperties()));
  fetcher->SetImagesEnabled(frame->GetSettings()->GetImagesEnabled());
  fetcher->SetAutoLoadImages(
      frame->GetSettings()->GetLoadsImagesAutomatically());
  fetcher->SetEarlyHintsPreloadedResources(
      loader.GetEarlyHintsPreloadedResources());
  return fetcher;
}

FrameFetchContext::FrameFetchContext(
    DocumentLoader& document_loader,
    Document& document,
    const DetachableResourceFetcherProperties& properties)
    : BaseFetchContext(properties,
                       MakeGarbageCollected<DetachableConsoleLogger>(
                           document.GetExecutionContext())),
      document_loader_(document_loader),
      document_(document) {}

net::SiteForCookies FrameFetchContext::GetSiteForCookies() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->site_for_cookies;
  return document_->SiteForCookies();
}

scoped_refptr<const SecurityOrigin> FrameFetchContext::GetTopFrameOrigin()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->top_frame_origin;
  return document_->TopFrameOrigin();
}

SubresourceFilter* FrameFetchContext::GetSubresourceFilter() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  return document_loader_->GetSubresourceFilter();
}

LocalFrame* FrameFetchContext::GetFrame() const {
  return document_->GetFrame();
}

LocalFrameClient* FrameFetchContext::GetLocalFrameClient() const {
  return GetFrame()->Client();
}

void FrameFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request) {
  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.Url().IsEmpty() && !request.Url().ProtocolIsInHTTPFamily())
    return;

  if (GetResourceFetcherProperties().IsDetached())
    return;

  // TODO(crbug.com/1375791): Consider overriding Attribution-Reporting-Support
  // header.
  if (base::FeatureList::IsEnabled(
          blink::features::kAttributionReportingCrossAppWeb) &&
      !request.HttpHeaderField(http_names::kAttributionReportingEligible)
           .IsNull() &&
      request.HttpHeaderField(http_names::kAttributionReportingSupport)
          .IsNull()) {
    if (AttributionSrcLoader* attribution_src_loader =
            GetFrame()->GetAttributionSrcLoader()) {
      request.AddHttpHeaderField(http_names::kAttributionReportingSupport,
                                 attribution_src_loader->GetSupportHeader());
    }
  }
}

// TODO(toyoshim, arthursonzogni): PlzNavigate doesn't use this function to set
// the ResourceRequest's cache policy. The cache policy determination needs to
// be factored out from FrameFetchContext and moved to the FrameLoader for
// instance.
mojom::FetchCacheMode FrameFetchContext::ResourceRequestCachePolicy(
    const ResourceRequest& request,
    ResourceType type,
    FetchParameters::DeferOption defer) const {
  if (GetResourceFetcherProperties().IsDetached())
    return mojom::FetchCacheMode::kDefault;

  DCHECK(GetFrame());
  const auto cache_mode = DetermineFrameCacheMode(GetFrame());

  // TODO(toyoshim): Revisit to consider if this clause can be merged to
  // determineWebCachePolicy or determineFrameCacheMode.
  if (cache_mode == mojom::FetchCacheMode::kDefault &&
      request.IsConditional()) {
    return mojom::FetchCacheMode::kValidateCache;
  }
  return cache_mode;
}

void FrameFetchContext::PrepareRequest(
    ResourceRequest& request,
    ResourceLoaderOptions& options,
    WebScopedVirtualTimePauser& virtual_time_pauser,
    ResourceType resource_type) {
  // TODO(yhirano): Clarify which statements are actually needed when
  // this is called during redirect.
  const bool for_redirect = request.GetRedirectInfo().has_value();

  SetFirstPartyCookie(request);
  if (request.GetRequestContext() ==
      mojom::blink::RequestContextType::SERVICE_WORKER) {
    // The top frame origin is defined to be null for service worker main
    // resource requests.
    DCHECK(!request.TopFrameOrigin());
  } else {
    request.SetTopFrameOrigin(GetTopFrameOrigin());
  }

  const bool ua_reduced =
      request.HttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAReduced)
              .c_str()) == "?1";
  const bool ua_full =
      request.HttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kFullUserAgent)
              .c_str()) == "?1";

  String user_agent =
      ua_full ? GetFullUserAgent()
              : (ua_reduced ? GetReducedUserAgent() : GetUserAgent());
  base::UmaHistogramBoolean("Blink.Fetch.ReducedUserAgent", ua_reduced);
  request.SetHTTPUserAgent(AtomicString(user_agent));

  if (GetResourceFetcherProperties().IsDetached())
    return;

  request.SetUkmSourceId(document_->UkmSourceID());
  request.SetHasStorageAccess(document_->HasStorageAccess());

  if (document_loader_->ForceFetchCacheMode())
    request.SetCacheMode(*document_loader_->ForceFetchCacheMode());

  GetLocalFrameClient()->DispatchWillSendRequest(request);
  FrameScheduler* frame_scheduler = GetFrame()->GetFrameScheduler();
  if (!for_redirect && frame_scheduler) {
    virtual_time_pauser = frame_scheduler->CreateWebScopedVirtualTimePauser(
        request.Url().GetString(),
        WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
  }

  probe::PrepareRequest(Probe(), document_loader_, request, options,
                        resource_type);

  // ServiceWorker hook ups.
  if (document_loader_->GetServiceWorkerNetworkProvider()) {
    WrappedResourceRequest webreq(request);
    document_loader_->GetServiceWorkerNetworkProvider()->WillSendRequest(
        webreq);
  }
}

void FrameFetchContext::AddResourceTiming(
    mojom::blink::ResourceTimingInfoPtr info,
    const AtomicString& initiator_type) {
  // Normally, |document_| is cleared on Document shutdown. In that case,
  // early return, as there is nothing to report the resource timing to.
  if (GetResourceFetcherProperties().IsDetached())
    return;

  // Timing for main resource is handled in DocumentLoader.
  // All other resources are reported to the corresponding Document.
  DOMWindowPerformance::performance(*document_->domWindow())
      ->AddResourceTiming(std::move(info), initiator_type);
}

bool FrameFetchContext::AllowImage(bool images_enabled, const KURL& url) const {
  if (GetResourceFetcherProperties().IsDetached())
    return true;
  if (auto* settings_client = GetContentSettingsClient())
    images_enabled = settings_client->AllowImage(images_enabled, url);
  return images_enabled;
}

void FrameFetchContext::ModifyRequestForCSP(ResourceRequest& resource_request) {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  GetFrame()->Loader().ModifyRequestForCSP(
      resource_request,
      &GetResourceFetcherProperties().GetFetchClientSettingsObject(),
      document_->domWindow(), mojom::blink::RequestContextFrameType::kNone);
}

void FrameFetchContext::AddClientHintsIfNecessary(
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  // If the feature is enabled, then client hints are allowed only on secure
  // URLs.
  if (!ClientHintsPreferences::IsClientHintsAllowed(request.Url()))
    return;

  // Check if |url| is allowed to run JavaScript. If not, client hints are not
  // attached to the requests that initiate on the render side.
  if (!AllowScriptFromSourceWithoutNotifying(
          request.Url(), GetContentSettingsClient(), GetSettings())) {
    return;
  }

  // The Permissions policy is used to enable hints for all subresources, based
  // on the policy of the requesting document, and the origin of the resource.
  const PermissionsPolicy* policy =
      document_
          ? document_->domWindow()->GetSecurityContext().GetPermissionsPolicy()
          : nullptr;

  url::Origin resource_origin =
      SecurityOrigin::Create(request.Url())->ToUrlOrigin();
  bool is_1p_origin = IsFirstPartyOrigin(request.Url());

  absl::optional<UserAgentMetadata> ua = GetUserAgentMetadata();

  absl::optional<ClientHintImageInfo> image_info;
  absl::optional<WTF::AtomicString> prefers_color_scheme;
  absl::optional<WTF::AtomicString> prefers_reduced_motion;

  if (document_) {  // Only get frame info if the frame is not detached
    image_info = ClientHintImageInfo();
    image_info->dpr = GetDevicePixelRatio();
    image_info->resource_width = resource_width;
    if (!GetResourceFetcherProperties().IsDetached() && GetFrame()->View()) {
      image_info->viewport_width = GetFrame()->View()->ViewportWidth();
      image_info->viewport_height = GetFrame()->View()->ViewportHeight();
    }

    prefers_color_scheme = document_->InDarkMode()
                               ? network::kPrefersColorSchemeDark
                               : network::kPrefersColorSchemeLight;
    prefers_reduced_motion = GetSettings()->GetPrefersReducedMotion()
                                 ? network::kPrefersReducedMotionReduce
                                 : network::kPrefersReducedMotionNoPreference;
  }

  // GetClientHintsPreferences() has things parsed for this document
  // by browser (from accept-ch header on this response or previously persisted)
  // with renderer-parsed http-equiv merged in.
  BaseFetchContext::AddClientHintsIfNecessary(
      GetClientHintsPreferences(), resource_origin, is_1p_origin, ua, policy,
      image_info, prefers_color_scheme, prefers_reduced_motion, request);
}

void FrameFetchContext::AddReducedAcceptLanguageIfNecessary(
    ResourceRequest& request) {
  // If the feature is enabled, then reduce accept language are allowed only on
  // http and https.

  // For detached frame, we check whether the feature flag turns on because it
  // will crash when detach frame calls GetExecutionContext().
  if (GetResourceFetcherProperties().IsDetached() &&
      !base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage)) {
    return;
  }

  if (!GetResourceFetcherProperties().IsDetached() &&
      !RuntimeEnabledFeatures::ReduceAcceptLanguageEnabled(
          GetExecutionContext())) {
    return;
  }

  if (!request.Url().ProtocolIsInHTTPFamily())
    return;

  const String& reduced_accept_language = GetReducedAcceptLanguage();
  if (!reduced_accept_language.empty() &&
      request.HttpHeaderField(http_names::kAcceptLanguage).empty()) {
    request.SetHttpHeaderField(http_names::kAcceptLanguage,
                               reduced_accept_language.Ascii().c_str());
  }
}

void FrameFetchContext::PopulateResourceRequest(
    ResourceType type,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request,
    const ResourceLoaderOptions& options) {
  if (!GetResourceFetcherProperties().IsDetached())
    probe::SetDevToolsIds(Probe(), request, options.initiator_info);

  ModifyRequestForCSP(request);
  AddClientHintsIfNecessary(resource_width, request);
  AddReducedAcceptLanguageIfNecessary(request);
}

bool FrameFetchContext::IsPrerendering() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->is_prerendering;
  return document_->IsPrerendering();
}

void FrameFetchContext::SetFirstPartyCookie(ResourceRequest& request) {
  // Set the first party for cookies url if it has not been set yet (new
  // requests). This value will be updated during redirects, consistent with
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00#section-2.1.1?
  if (!request.SiteForCookiesSet())
    request.SetSiteForCookies(GetSiteForCookies());
}

bool FrameFetchContext::AllowScriptFromSource(const KURL& url) const {
  if (AllowScriptFromSourceWithoutNotifying(url, GetContentSettingsClient(),
                                            GetSettings())) {
    return true;
  }
  WebContentSettingsClient* settings_client = GetContentSettingsClient();
  if (settings_client)
    settings_client->DidNotAllowScript();
  return false;
}

// static
bool FrameFetchContext::AllowScriptFromSourceWithoutNotifying(
    const KURL& url,
    WebContentSettingsClient* settings_client,
    Settings* settings) {
  bool allow_script = !settings || settings->GetScriptEnabled();
  if (settings_client)
    allow_script = settings_client->AllowScriptFromSource(allow_script, url);
  return allow_script;
}

bool FrameFetchContext::IsFirstPartyOrigin(const KURL& url) const {
  if (GetResourceFetcherProperties().IsDetached())
    return false;

  return GetFrame()
      ->Tree()
      .Top()
      .GetSecurityContext()
      ->GetSecurityOrigin()
      ->IsSameOriginWith(SecurityOrigin::Create(url).get());
}

bool FrameFetchContext::ShouldBlockRequestByInspector(const KURL& url) const {
  if (GetResourceFetcherProperties().IsDetached())
    return false;
  bool should_block_request = false;
  probe::ShouldBlockRequest(Probe(), url, &should_block_request);
  return should_block_request;
}

void FrameFetchContext::DispatchDidBlockRequest(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    ResourceRequestBlockedReason blocked_reason,
    ResourceType resource_type) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  probe::DidBlockRequest(Probe(), resource_request, document_loader_, Url(),
                         options, blocked_reason, resource_type);
}

ContentSecurityPolicy* FrameFetchContext::GetContentSecurityPolicyForWorld(
    const DOMWrapperWorld* world) const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->content_security_policy;

  return document_->GetExecutionContext()->GetContentSecurityPolicyForWorld(
      world);
}

bool FrameFetchContext::IsSVGImageChromeClient() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->is_svg_image_chrome_client;

  return GetFrame()->GetChromeClient().IsSVGImageChromeClient();
}

void FrameFetchContext::CountUsage(WebFeature feature) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  document_loader_->GetUseCounter().Count(feature, GetFrame());
}

void FrameFetchContext::CountDeprecation(WebFeature feature) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  Deprecation::CountDeprecation(document_->domWindow(), feature);
}

bool FrameFetchContext::ShouldBlockWebSocketByMixedContentCheck(
    const KURL& url) const {
  if (GetResourceFetcherProperties().IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return false;
  }
  return !MixedContentChecker::IsWebSocketAllowed(*this, GetFrame(), url);
}

std::unique_ptr<WebSocketHandshakeThrottle>
FrameFetchContext::CreateWebSocketHandshakeThrottle() {
  if (GetResourceFetcherProperties().IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return nullptr;
  }
  if (!GetFrame())
    return nullptr;
  return WebFrame::FromCoreFrame(GetFrame())
      ->ToWebLocalFrame()
      ->Client()
      ->CreateWebSocketHandshakeThrottle();
}

bool FrameFetchContext::ShouldBlockFetchByMixedContentCheck(
    mojom::blink::RequestContextType request_context,
    network::mojom::blink::IPAddressSpace target_address_space,
    const absl::optional<ResourceRequest::RedirectInfo>& redirect_info,
    const KURL& url,
    ReportingDisposition reporting_disposition,
    const absl::optional<String>& devtools_id) const {
  if (GetResourceFetcherProperties().IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return false;
  }
  const KURL& url_before_redirects =
      redirect_info ? redirect_info->original_url : url;
  ResourceRequest::RedirectStatus redirect_status =
      redirect_info ? RedirectStatus::kFollowedRedirect
                    : RedirectStatus::kNoRedirect;
  return MixedContentChecker::ShouldBlockFetch(
      GetFrame(), request_context, target_address_space, url_before_redirects,
      redirect_status, url, devtools_id, reporting_disposition,
      document_loader_->GetContentSecurityNotifier());
}

bool FrameFetchContext::ShouldBlockFetchAsCredentialedSubresource(
    const ResourceRequest& resource_request,
    const KURL& url) const {
  // URLs with no embedded credentials should load correctly.
  if (url.User().empty() && url.Pass().empty())
    return false;

  if (resource_request.GetRequestContext() ==
      mojom::blink::RequestContextType::XML_HTTP_REQUEST) {
    return false;
  }

  // Relative URLs on top-level pages that were loaded with embedded credentials
  // should load correctly.
  // TODO(mkwst): This doesn't work when the subresource is an iframe.
  // See https://crbug.com/756846.
  if (Url().User() == url.User() && Url().Pass() == url.Pass() &&
      SecurityOrigin::Create(url)->IsSameOriginWith(
          GetResourceFetcherProperties()
              .GetFetchClientSettingsObject()
              .GetSecurityOrigin())) {
    return false;
  }

  CountDeprecation(WebFeature::kRequestedSubresourceWithEmbeddedCredentials);

  return true;
}

const KURL& FrameFetchContext::Url() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->url;
  return document_->Url();
}

ContentSecurityPolicy* FrameFetchContext::GetContentSecurityPolicy() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->content_security_policy;
  return document_->domWindow()->GetContentSecurityPolicy();
}

WebContentSettingsClient* FrameFetchContext::GetContentSettingsClient() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  return GetFrame()->GetContentSettingsClient();
}

Settings* FrameFetchContext::GetSettings() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  DCHECK(GetFrame());
  return GetFrame()->GetSettings();
}

String FrameFetchContext::GetUserAgent() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->user_agent;
  return GetFrame()->Loader().UserAgent();
}

String FrameFetchContext::GetFullUserAgent() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->user_agent;
  return GetFrame()->Loader().FullUserAgent();
}

String FrameFetchContext::GetReducedUserAgent() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->user_agent;
  return GetFrame()->Loader().ReducedUserAgent();
}

absl::optional<UserAgentMetadata> FrameFetchContext::GetUserAgentMetadata()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->user_agent_metadata;
  return GetLocalFrameClient()->UserAgentMetadata();
}

const PermissionsPolicy* FrameFetchContext::GetPermissionsPolicy() const {
  return document_ ? document_->domWindow()
                         ->GetSecurityContext()
                         .GetPermissionsPolicy()
                   : nullptr;
}

const ClientHintsPreferences FrameFetchContext::GetClientHintsPreferences()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->client_hints_preferences;
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  return frame->GetClientHintsPreferences();
}

String FrameFetchContext::GetReducedAcceptLanguage() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->reduced_accept_language;
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  // If accept language override from inspector emulation, set Accept-Language
  // header as the overridden value.
  String override_accept_language;
  probe::ApplyAcceptLanguageOverride(Probe(), &override_accept_language);
  return override_accept_language.empty()
             ? frame->GetReducedAcceptLanguage().GetString()
             : network_utils::GenerateAcceptLanguageHeader(
                   override_accept_language);
}

float FrameFetchContext::GetDevicePixelRatio() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->device_pixel_ratio;
  return document_->DevicePixelRatio();
}

FetchContext* FrameFetchContext::Detach() {
  if (GetResourceFetcherProperties().IsDetached())
    return this;

  // If the Sec-CH-UA-Full client hint header is set on the request, then the
  // full User-Agent string should be set on the User-Agent request header.
  // If the Sec-CH-UA-Reduced client hint header is set on the request, then the
  // reduced User-Agent string should also be set on the User-Agent request
  // header.
  const ClientHintsPreferences& client_hints_prefs =
      GetClientHintsPreferences();
  String user_agent = client_hints_prefs.ShouldSend(
                          network::mojom::WebClientHintsType::kFullUserAgent)
                          ? GetFullUserAgent()
                          : client_hints_prefs.ShouldSend(
                                network::mojom::WebClientHintsType::kUAReduced)
                                ? GetReducedUserAgent()
                                : GetUserAgent();

  frozen_state_ = MakeGarbageCollected<FrozenState>(
      Url(), GetContentSecurityPolicy(), GetSiteForCookies(),
      GetTopFrameOrigin(), client_hints_prefs, GetDevicePixelRatio(),
      user_agent, GetUserAgentMetadata(), IsSVGImageChromeClient(),
      IsPrerendering(), GetReducedAcceptLanguage());
  document_loader_ = nullptr;
  document_ = nullptr;
  return this;
}

void FrameFetchContext::Trace(Visitor* visitor) const {
  visitor->Trace(document_loader_);
  visitor->Trace(document_);
  visitor->Trace(frozen_state_);
  BaseFetchContext::Trace(visitor);
}

bool FrameFetchContext::CalculateIfAdSubresource(
    const ResourceRequestHead& resource_request,
    const absl::optional<KURL>& alias_url,
    ResourceType type,
    const FetchInitiatorInfo& initiator_info) {
  // Mark the resource as an Ad if the BaseFetchContext thinks it's an ad.
  bool known_ad = BaseFetchContext::CalculateIfAdSubresource(
      resource_request, alias_url, type, initiator_info);
  if (GetResourceFetcherProperties().IsDetached() ||
      !GetFrame()->GetAdTracker()) {
    return known_ad;
  }

  // The AdTracker needs to know about the request as well, and may also mark it
  // as an ad.
  const KURL& url = alias_url ? alias_url.value() : resource_request.Url();
  return GetFrame()->GetAdTracker()->CalculateIfAdSubresource(
      document_->domWindow(), url, type, initiator_info, known_ad);
}

void FrameFetchContext::DidObserveLoadingBehavior(
    LoadingBehaviorFlag behavior) {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  GetFrame()->Loader().GetDocumentLoader()->DidObserveLoadingBehavior(behavior);
}

std::unique_ptr<ResourceLoadInfoNotifierWrapper>
FrameFetchContext::CreateResourceLoadInfoNotifierWrapper() {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  return GetLocalFrameClient()->CreateResourceLoadInfoNotifierWrapper();
}

mojom::blink::ContentSecurityNotifier&
FrameFetchContext::GetContentSecurityNotifier() const {
  DCHECK(!GetResourceFetcherProperties().IsDetached());
  return document_loader_->GetContentSecurityNotifier();
}

ExecutionContext* FrameFetchContext::GetExecutionContext() const {
  return document_->GetExecutionContext();
}

absl::optional<ResourceRequestBlockedReason> FrameFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    const absl::optional<ResourceRequest::RedirectInfo>& redirect_info) const {
  if (!GetResourceFetcherProperties().IsDetached() &&
      document_->IsFreezingInProgress() && !resource_request.GetKeepalive()) {
    GetDetachableConsoleLogger().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kError,
            "Only fetch keepalive is allowed during onfreeze: " +
                url.GetString()));
    return ResourceRequestBlockedReason::kOther;
  }
  return BaseFetchContext::CanRequest(type, resource_request, url, options,
                                      reporting_disposition, redirect_info);
}

CoreProbeSink* FrameFetchContext::Probe() const {
  return probe::ToCoreProbeSink(GetFrame()->GetDocument());
}

void FrameFetchContext::UpdateSubresourceLoadMetrics(
    uint32_t number_of_subresources_loaded,
    uint32_t number_of_subresource_loads_handled_by_service_worker,
    bool pervasive_payload_requested,
    int64_t pervasive_bytes_fetched,
    int64_t total_bytes_fetched) {
  document_loader_->UpdateSubresourceLoadMetrics(
      number_of_subresources_loaded,
      number_of_subresource_loads_handled_by_service_worker,
      pervasive_payload_requested, pervasive_bytes_fetched,
      total_bytes_fetched);
}

}  // namespace blink

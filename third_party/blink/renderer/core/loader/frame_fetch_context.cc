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
#include "base/optional.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-blink.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
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
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"
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
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_chrome_client.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Client hints sent to third parties are controlled through two mechanisms,
// based on the state of the experimental flag "FeaturePolicyForClientHints".
//
// If that flag is disabled (the default), then all hints are always sent for
// first-party subresources, and the kAllowClientHintsToThirdParty feature
// controls whether some specific hints are sent to third parties. (Only
// device-memory, resource-width, viewport-width and DPR are sent under this
// model). This feature is enabled by default on Android, and disabled by
// default on all other platforms.
//
// When the runtime flag is enabled, all client hints except UA are controlled
// entirely by feature policy on all platforms. In that case, hints will
// generally be sent for first-party resources, and not for third-party
// resources, unless specifically enabled by policy.

// Determines FetchCacheMode for |frame|. This FetchCacheMode should be a base
// policy to consider one of each resource belonging to the frame, and should
// not count resource specific conditions in.
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

// Simple function to add quotes to make headers strings.
const AtomicString SerializeHeaderString(std::string str) {
  std::string output;
  if (!str.empty()) {
    output = net::structured_headers::SerializeItem(
                 net::structured_headers::Item(str))
                 .value_or(std::string());
  }

  return AtomicString(output.c_str());
}

}  // namespace

struct FrameFetchContext::FrozenState final : GarbageCollected<FrozenState> {
  FrozenState(const KURL& url,
              scoped_refptr<const SecurityOrigin> parent_security_origin,
              const ContentSecurityPolicy* content_security_policy,
              net::SiteForCookies site_for_cookies,
              scoped_refptr<const SecurityOrigin> top_frame_origin,
              const ClientHintsPreferences& client_hints_preferences,
              float device_pixel_ratio,
              const String& user_agent,
              const base::Optional<UserAgentMetadata>& user_agent_metadata,
              bool is_svg_image_chrome_client)
      : url(url),
        parent_security_origin(std::move(parent_security_origin)),
        content_security_policy(content_security_policy),
        site_for_cookies(std::move(site_for_cookies)),
        top_frame_origin(std::move(top_frame_origin)),
        client_hints_preferences(client_hints_preferences),
        device_pixel_ratio(device_pixel_ratio),
        user_agent(user_agent),
        user_agent_metadata(user_agent_metadata),
        is_svg_image_chrome_client(is_svg_image_chrome_client) {}

  const KURL url;
  const scoped_refptr<const SecurityOrigin> parent_security_origin;
  const Member<const ContentSecurityPolicy> content_security_policy;
  const net::SiteForCookies site_for_cookies;
  const scoped_refptr<const SecurityOrigin> top_frame_origin;
  const ClientHintsPreferences client_hints_preferences;
  const float device_pixel_ratio;
  const String user_agent;
  const base::Optional<UserAgentMetadata> user_agent_metadata;
  const bool is_svg_image_chrome_client;

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
      frame->DomWindow());
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
  return fetcher;
}

FrameFetchContext::FrameFetchContext(
    DocumentLoader& document_loader,
    Document& document,
    const DetachableResourceFetcherProperties& properties)
    : BaseFetchContext(properties),
      document_loader_(document_loader),
      document_(document),
      save_data_enabled_(
          GetNetworkStateNotifier().SaveDataEnabled() &&
          !GetFrame()->GetSettings()->GetDataSaverHoldbackWebApi()) {}

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

PreviewsResourceLoadingHints*
FrameFetchContext::GetPreviewsResourceLoadingHints() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  return document_loader_->GetPreviewsResourceLoadingHints();
}

PreviewsState FrameFetchContext::previews_state() const {
  if (GetResourceFetcherProperties().IsDetached())
    return PreviewsTypes::kPreviewsUnspecified;
  return document_loader_->GetPreviewsState();
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

  // Reload should reflect the current data saver setting.
  if (IsReloadLoadType(document_loader_->LoadType()))
    request.ClearHttpHeaderField(http_names::kSaveData);

  if (save_data_enabled_)
    request.SetHttpHeaderField(http_names::kSaveData, "on");

  AddBackForwardCacheExperimentHTTPHeaderIfNeeded(
      document_->GetExecutionContext(), request);
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

  String user_agent = GetUserAgent();
  request.SetHTTPUserAgent(AtomicString(user_agent));

  if (GetResourceFetcherProperties().IsDetached())
    return;

  if (document_loader_->ForceFetchCacheMode())
    request.SetCacheMode(*document_loader_->ForceFetchCacheMode());

  if (request.GetPreviewsState() == PreviewsTypes::kPreviewsUnspecified) {
    PreviewsState request_previews_state = document_loader_->GetPreviewsState();
    if (request_previews_state == PreviewsTypes::kPreviewsUnspecified)
      request_previews_state = PreviewsTypes::kPreviewsOff;
    request.SetPreviewsState(request_previews_state);
  }

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

void FrameFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  // Normally, |document_| is cleared on Document shutdown. However, Documents
  // for HTML imports will also not have a LocalFrame set: in that case, also
  // early return, as there is nothing to report the resource timing to.
  if (GetResourceFetcherProperties().IsDetached())
    return;

  // Timing for main resource is handled in DocumentLoader.
  // All other resources are reported to the corresponding Document.
  DOMWindowPerformance::performance(*document_->domWindow())
      ->GenerateAndAddResourceTiming(info);
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
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  // If the feature is enabled, then client hints are allowed only on secure
  // URLs.
  if (!ClientHintsPreferences::IsClientHintsAllowed(request.Url()))
    return;

  // Check if |url| is allowed to run JavaScript. If not, client hints are not
  // attached to the requests that initiate on the render side.
  if (!AllowScriptFromSourceWithoutNotifying(request.Url()))
    return;

  // When the runtime flag "FeaturePolicyForClientHints" is enabled, feature
  // policy is used to enable hints for all subresources, based on the policy of
  // the requesting document, and the origin of the resource.
  const FeaturePolicy* policy =
      document_
          ? document_->domWindow()->GetSecurityContext().GetFeaturePolicy()
          : nullptr;

  url::Origin resource_origin =
      SecurityOrigin::Create(request.Url())->ToUrlOrigin();
  bool is_1p_origin = IsFirstPartyOrigin(request.Url());

  // Sec-CH-UA is special: we always send the header to all origins that are
  // eligible for client hints (e.g. secure transport, JavaScript enabled).
  //
  // https://github.com/WICG/ua-client-hints
  //
  // One exception, however, is that a custom UA is sometimes set without
  // specifying accomponying client hints, in which case we disable sending
  // them.
  base::Optional<UserAgentMetadata> ua = GetUserAgentMetadata();
  if (ClientHintsPreferences::UserAgentClientHintEnabled() && ua.has_value()) {
    // ShouldSendClientHint is called to make sure UA is controlled by
    // FeaturePolicy.
    if (ShouldSendClientHint(ClientHintsMode::kStandard, policy,
                             resource_origin, is_1p_origin,
                             network::mojom::blink::WebClientHintsType::kUA,
                             hints_preferences)) {
      request.SetHttpHeaderField(
          blink::kClientHintsHeaderMapping[static_cast<size_t>(
              network::mojom::blink::WebClientHintsType::kUA)],
          ua->SerializeBrandVersionList().c_str());
    }

    // We also send Sec-CH-UA-Mobile to all hints. It is a one-bit header
    // identifying if the browser has opted for a "mobile" experience
    // Formatted using the "sh-boolean" format from:
    // https://httpwg.org/http-extensions/draft-ietf-httpbis-header-structure.html#boolean
    // ShouldSendClientHint is called to make sure it's controlled by
    // FeaturePolicy.
    if (ShouldSendClientHint(
            ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAMobile,
            hints_preferences)) {
      request.SetHttpHeaderField(
          blink::kClientHintsHeaderMapping[static_cast<size_t>(
              network::mojom::blink::WebClientHintsType::kUAMobile)],
          ua->mobile ? "?1" : "?0");
    }
  }

  // If the frame is detached, then don't send any hints other than UA.
  if (!policy)
    return;


  if (!RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
      !base::FeatureList::IsEnabled(features::kAllowClientHintsToThirdParty) &&
      !is_1p_origin) {
    // No client hints for 3p origins.
    return;
  }

  // The next 4 hints should be enabled if we're allowing legacy hints to third
  // parties, or if FeaturePolicy delegation says they are allowed.
  if (ShouldSendClientHint(
          ClientHintsMode::kLegacy, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kDeviceMemory,
          hints_preferences)) {
    request.SetHttpHeaderField(
        "Device-Memory",
        AtomicString(String::Number(
            ApproximatedDeviceMemory::GetApproximatedDeviceMemory())));
  }

  float dpr = GetDevicePixelRatio();
  if (ShouldSendClientHint(
          ClientHintsMode::kLegacy, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kDpr, hints_preferences)) {
    request.SetHttpHeaderField("DPR", AtomicString(String::Number(dpr)));
  }

  if (ShouldSendClientHint(
          ClientHintsMode::kLegacy, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kViewportWidth,
          hints_preferences) &&
      !GetResourceFetcherProperties().IsDetached() && GetFrame()->View()) {
    request.SetHttpHeaderField(
        "Viewport-Width",
        AtomicString(String::Number(GetFrame()->View()->ViewportWidth())));
  }

  if (ShouldSendClientHint(
          ClientHintsMode::kLegacy, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kResourceWidth,
          hints_preferences)) {
    if (resource_width.is_set) {
      float physical_width = resource_width.width * dpr;
      request.SetHttpHeaderField(
          "Width", AtomicString(String::Number(ceil(physical_width))));
    }
  }

  if (ShouldSendClientHint(
          ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kRtt, hints_preferences)) {
    base::Optional<base::TimeDelta> http_rtt =
        GetNetworkStateNotifier().GetWebHoldbackHttpRtt();
    if (!http_rtt) {
      http_rtt = GetNetworkStateNotifier().HttpRtt();
    }

    uint32_t rtt =
        GetNetworkStateNotifier().RoundRtt(request.Url().Host(), http_rtt);
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kRtt)],
        AtomicString(String::Number(rtt)));
  }

  if (ShouldSendClientHint(ClientHintsMode::kStandard, policy, resource_origin,
                           is_1p_origin,
                           network::mojom::blink::WebClientHintsType::kDownlink,
                           hints_preferences)) {
    base::Optional<double> throughput_mbps =
        GetNetworkStateNotifier().GetWebHoldbackDownlinkThroughputMbps();
    if (!throughput_mbps) {
      throughput_mbps = GetNetworkStateNotifier().DownlinkThroughputMbps();
    }

    double mbps = GetNetworkStateNotifier().RoundMbps(request.Url().Host(),
                                                      throughput_mbps);
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kDownlink)],
        AtomicString(String::Number(mbps)));
  }

  if (ShouldSendClientHint(
          ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kEct, hints_preferences)) {
    base::Optional<WebEffectiveConnectionType> holdback_ect =
        GetNetworkStateNotifier().GetWebHoldbackEffectiveType();
    if (!holdback_ect)
      holdback_ect = GetNetworkStateNotifier().EffectiveType();

    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kEct)],
        AtomicString(NetworkStateNotifier::EffectiveConnectionTypeToString(
            holdback_ect.value())));
  }

  if (ShouldSendClientHint(ClientHintsMode::kStandard, policy, resource_origin,
                           is_1p_origin,
                           network::mojom::blink::WebClientHintsType::kLang,
                           hints_preferences)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kLang)],
        GetFrame()
            ->DomWindow()
            ->navigator()
            ->SerializeLanguagesForClientHintHeader());
  }

  if (ua.has_value() &&
      ShouldSendClientHint(ClientHintsMode::kStandard, policy, resource_origin,
                           is_1p_origin,
                           network::mojom::blink::WebClientHintsType::kUAArch,
                           hints_preferences)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kUAArch)],
        SerializeHeaderString(ua->architecture));
  }

  if (ua.has_value() &&
      ShouldSendClientHint(
          ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kUAPlatform,
          hints_preferences)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kUAPlatform)],
        SerializeHeaderString(ua->platform));
  }

  if (ua.has_value() &&
      ShouldSendClientHint(
          ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kUAPlatformVersion,
          hints_preferences)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kUAPlatformVersion)],
        SerializeHeaderString(ua->platform_version));
  }

  if (ua.has_value() &&
      ShouldSendClientHint(ClientHintsMode::kStandard, policy, resource_origin,
                           is_1p_origin,
                           network::mojom::blink::WebClientHintsType::kUAModel,
                           hints_preferences)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kUAModel)],
        SerializeHeaderString(ua->model));
  }

  if (ua.has_value() &&
      ShouldSendClientHint(
          ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kUAFullVersion,
          hints_preferences)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kUAFullVersion)],
        SerializeHeaderString(ua->full_version));
  }
}

void FrameFetchContext::PopulateResourceRequest(
    ResourceType type,
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request,
    const ResourceLoaderOptions& options) {
  if (!GetResourceFetcherProperties().IsDetached())
    probe::SetDevToolsIds(Probe(), request, options.initiator_info);

  ModifyRequestForCSP(request);
  AddClientHintsIfNecessary(hints_preferences, resource_width, request);

  const ContentSecurityPolicy* csp =
      GetContentSecurityPolicyForWorld(options.world_for_csp.get());
  if (csp && csp->ShouldSendCSPHeader(type))
    // TODO(crbug.com/993769): Test if this header returns duplicated values
    // (i.e. "CSP: active, active") on asynchronous "stale-while-revalidate"
    // revalidation requests and if this is unexpected behavior.
    request.AddHttpHeaderField("CSP", "active");
}

void FrameFetchContext::SetFirstPartyCookie(ResourceRequest& request) {
  // Set the first party for cookies url if it has not been set yet (new
  // requests). This value will be updated during redirects, consistent with
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00#section-2.1.1?
  if (!request.SiteForCookiesSet())
    request.SetSiteForCookies(GetSiteForCookies());
}

bool FrameFetchContext::AllowScriptFromSource(const KURL& url) const {
  if (AllowScriptFromSourceWithoutNotifying(url))
    return true;
  WebContentSettingsClient* settings_client = GetContentSettingsClient();
  if (settings_client)
    settings_client->DidNotAllowScript();
  return false;
}

bool FrameFetchContext::AllowScriptFromSourceWithoutNotifying(
    const KURL& url) const {
  Settings* settings = GetSettings();
  bool allow_script = !settings || settings->GetScriptEnabled();
  if (auto* settings_client = GetContentSettingsClient())
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
    const FetchInitiatorInfo& fetch_initiator_info,
    ResourceRequestBlockedReason blocked_reason,
    ResourceType resource_type) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  probe::DidBlockRequest(Probe(), resource_request, document_loader_, Url(),
                         fetch_initiator_info, blocked_reason, resource_type);
}

const ContentSecurityPolicy*
FrameFetchContext::GetContentSecurityPolicyForWorld(
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
  document_loader_->GetUseCounterHelper().Count(feature, GetFrame());
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
  return WebFrame::FromFrame(GetFrame())
      ->ToWebLocalFrame()
      ->Client()
      ->CreateWebSocketHandshakeThrottle();
}

bool FrameFetchContext::ShouldBlockFetchByMixedContentCheck(
    mojom::blink::RequestContextType request_context,
    const base::Optional<ResourceRequest::RedirectInfo>& redirect_info,
    const KURL& url,
    ReportingDisposition reporting_disposition,
    const base::Optional<String>& devtools_id) const {
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
      GetFrame(), request_context, url_before_redirects, redirect_status, url,
      devtools_id, reporting_disposition,
      document_loader_->GetContentSecurityNotifier());
}

bool FrameFetchContext::ShouldBlockFetchAsCredentialedSubresource(
    const ResourceRequest& resource_request,
    const KURL& url) const {
  // URLs with no embedded credentials should load correctly.
  if (url.User().IsEmpty() && url.Pass().IsEmpty())
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

  // TODO(mkwst): Remove the runtime check one way or the other once we're
  // sure it's going to stick (or that it's not).
  return RuntimeEnabledFeatures::BlockCredentialedSubresourcesEnabled();
}

const KURL& FrameFetchContext::Url() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->url;
  return document_->Url();
}

const SecurityOrigin* FrameFetchContext::GetParentSecurityOrigin() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->parent_security_origin.get();
  Frame* parent = GetFrame()->Tree().Parent();
  if (!parent)
    return nullptr;
  return parent->GetSecurityContext()->GetSecurityOrigin();
}

const ContentSecurityPolicy* FrameFetchContext::GetContentSecurityPolicy()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->content_security_policy;
  return document_->domWindow()->GetContentSecurityPolicy();
}

void FrameFetchContext::AddConsoleMessage(ConsoleMessage* message) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  document_->AddConsoleMessage(message);
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

base::Optional<UserAgentMetadata> FrameFetchContext::GetUserAgentMetadata()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->user_agent_metadata;
  return GetLocalFrameClient()->UserAgentMetadata();
}

const FeaturePolicy* FrameFetchContext::GetFeaturePolicy() const {
  return document_
             ? document_->domWindow()->GetSecurityContext().GetFeaturePolicy()
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

float FrameFetchContext::GetDevicePixelRatio() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->device_pixel_ratio;
  return document_->DevicePixelRatio();
}

bool FrameFetchContext::ShouldSendClientHint(
    ClientHintsMode mode,
    const FeaturePolicy* policy,
    const url::Origin& resource_origin,
    bool is_1p_origin,
    network::mojom::blink::WebClientHintsType type,
    const ClientHintsPreferences& hints_preferences) const {
  bool origin_ok;

  if (mode == ClientHintsMode::kLegacy &&
      base::FeatureList::IsEnabled(features::kAllowClientHintsToThirdParty)) {
    origin_ok = true;
  } else if (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled()) {
    origin_ok =
        (policy && policy->IsFeatureEnabledForOrigin(
                       kClientHintsFeaturePolicyMapping[static_cast<int>(type)],
                       resource_origin));
  } else {
    origin_ok = is_1p_origin;
  }

  if (!origin_ok)
    return false;

  // |hints_preferences| is used only in case of the preload scanner;
  // GetClientHintsPreferences() has things parsed for this document
  // by browser (from accept-ch header on this response or previously persisted)
  // with renderer-parsed http-equiv merged in.
  return IsClientHintSentByDefault(type) ||
         GetClientHintsPreferences().ShouldSend(type) ||
         hints_preferences.ShouldSend(type);
}

FetchContext* FrameFetchContext::Detach() {
  if (GetResourceFetcherProperties().IsDetached())
    return this;

  frozen_state_ = MakeGarbageCollected<FrozenState>(
      Url(), GetParentSecurityOrigin(), GetContentSecurityPolicy(),
      GetSiteForCookies(), GetTopFrameOrigin(), GetClientHintsPreferences(),
      GetDevicePixelRatio(), GetUserAgent(), GetUserAgentMetadata(),
      IsSVGImageChromeClient());
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
    const ResourceRequest& resource_request,
    ResourceType type,
    const FetchInitiatorInfo& initiator_info) {
  // Mark the resource as an Ad if the SubresourceFilter thinks it's an ad.
  bool known_ad = BaseFetchContext::CalculateIfAdSubresource(
      resource_request, type, initiator_info);
  if (GetResourceFetcherProperties().IsDetached() ||
      !GetFrame()->GetAdTracker()) {
    return known_ad;
  }

  // The AdTracker needs to know about the request as well, and may also mark it
  // as an ad.
  return GetFrame()->GetAdTracker()->CalculateIfAdSubresource(
      document_->domWindow(), resource_request, type, initiator_info, known_ad);
}

bool FrameFetchContext::SendConversionRequestInsteadOfRedirecting(
    const KURL& url,
    const base::Optional<ResourceRequest::RedirectInfo>& redirect_info,
    ReportingDisposition reporting_disposition) const {
  if (GetResourceFetcherProperties().IsDetached())
    return false;

  if (!RuntimeEnabledFeatures::ConversionMeasurementEnabled(
          document_->domWindow())) {
    return false;
  }

  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  // Only register conversions pings that are redirects in the main frame.
  if (!frame->IsMainFrame() || !redirect_info ||
      !SecurityOrigin::AreSameOrigin(url, redirect_info->previous_url)) {
    return false;
  }

  const char kWellKnownConversionRegsitrationPath[] =
      "/.well-known/register-conversion";
  if (url.GetPath() != kWellKnownConversionRegsitrationPath)
    return false;

  if (!document_->domWindow()->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kConversionMeasurement)) {
    String message =
        "The 'conversion-measurement' feature policy must be enabled to "
        "register a conversion.";
    document_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError, message));
    return false;
  }

  // Only allow conversion registration on secure pages with a secure conversion
  // redirect.
  scoped_refptr<const SecurityOrigin> redirect_origin =
      SecurityOrigin::Create(url);
  if (!GetFrame()
           ->GetSecurityContext()
           ->GetSecurityOrigin()
           ->IsPotentiallyTrustworthy() ||
      !redirect_origin->IsPotentiallyTrustworthy()) {
    return false;
  }

  // Only report conversions for requests with reporting enabled (i.e. do not
  // count preload requests). However, return true.
  if (reporting_disposition == ReportingDisposition::kSuppressReporting)
    return true;

  mojom::blink::ConversionPtr conversion = mojom::blink::Conversion::New();
  conversion->reporting_origin = SecurityOrigin::Create(url);
  conversion->conversion_data = 0UL;

  const char kConversionDataParam[] = "conversion-data";
  URLSearchParams* search_params = URLSearchParams::Create(url.Query());
  if (search_params->has(kConversionDataParam)) {
    bool is_valid_integer = false;
    uint64_t data = search_params->get(kConversionDataParam)
                        .ToUInt64Strict(&is_valid_integer);

    // Default invalid params to 0.
    conversion->conversion_data = is_valid_integer ? data : 0UL;
  }

  mojo::AssociatedRemote<mojom::blink::ConversionHost> conversion_host;
  GetFrame()->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      &conversion_host);
  conversion_host->RegisterConversion(std::move(conversion));

  // Log use counters once we have a conversion.
  UseCounter::Count(document_->domWindow(),
                    mojom::blink::WebFeature::kConversionAPIAll);
  UseCounter::Count(document_->domWindow(),
                    mojom::blink::WebFeature::kConversionRegistration);

  return true;
}

mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
FrameFetchContext::TakePendingWorkerTimingReceiver(int request_id) {
  DCHECK(!GetResourceFetcherProperties().IsDetached());
  return document_loader_->TakePendingWorkerTimingReceiver(request_id);
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

base::Optional<ResourceRequestBlockedReason> FrameFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    const base::Optional<ResourceRequest::RedirectInfo>& redirect_info) const {
  if (!GetResourceFetcherProperties().IsDetached() &&
      document_->IsFreezingInProgress() && !resource_request.GetKeepalive()) {
    AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Only fetch keepalive is allowed during onfreeze: " + url.GetString()));
    return ResourceRequestBlockedReason::kOther;
  }
  return BaseFetchContext::CanRequest(type, resource_request, url, options,
                                      reporting_disposition, redirect_info);
}

CoreProbeSink* FrameFetchContext::Probe() const {
  return probe::ToCoreProbeSink(GetFrame()->GetDocument());
}

}  // namespace blink

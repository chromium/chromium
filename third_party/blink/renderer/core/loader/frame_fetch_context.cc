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
#include <optional>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-blink.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
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
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/back_forward_cache_loader_helper_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/loader_factory_for_frame.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
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
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Creates a serialized AtomicString header value out of the input string, using
// structured headers as described in
// https://www.rfc-editor.org/rfc/rfc8941.html.
const AtomicString SerializeStringHeader(const std::string& str) {
  std::string output;

  // See https://crbug.com/1416925.
  if (str.empty() &&
      !base::FeatureList::IsEnabled(
          blink::features::kQuoteEmptySecChUaStringHeadersConsistently)) {
    return AtomicString(output.c_str());
  }

  output =
      net::structured_headers::SerializeItem(net::structured_headers::Item(str))
          .value_or(std::string());

  return AtomicString(output.c_str());
}

AtomicString GenerateBoolHeaderValue(bool value) {
  const std::string output = net::structured_headers::SerializeItem(
                                 net::structured_headers::Item(value))
                                 .value_or(std::string());
  return AtomicString(output.c_str());
}

// Creates a serialized AtomicString header value out of the input boolean,
// using structured headers as described in
// https://www.rfc-editor.org/rfc/rfc8941.html.
const AtomicString SerializeBoolHeader(const bool value) {
  if (value) {
    DEFINE_STATIC_LOCAL(AtomicString, true_value,
                        (GenerateBoolHeaderValue(true)));
    return true_value;
  }
  DEFINE_STATIC_LOCAL(AtomicString, false_value,
                      (GenerateBoolHeaderValue(false)));
  return false_value;
}

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
    case WebFrameLoadType::kRestore:
      // Mutates the policy for POST requests to avoid form resubmission.
      return mojom::FetchCacheMode::kForceCache;
    case WebFrameLoadType::kReload:
      return mojom::FetchCacheMode::kDefault;
    case WebFrameLoadType::kReloadBypassingCache:
      return mojom::FetchCacheMode::kBypassCache;
  }
  NOTREACHED_IN_MIGRATION();
  return mojom::FetchCacheMode::kDefault;
}

bool ShouldSendClientHint(const PermissionsPolicy& policy,
                          const url::Origin& resource_origin,
                          bool is_1p_origin,
                          network::mojom::blink::WebClientHintsType type,
                          const ClientHintsPreferences& hints_preferences) {
  // For subresource requests, sending the hint in the fetch request based on
  // the permissions policy.
  if (!policy.IsFeatureEnabledForOrigin(
          GetClientHintToPolicyFeatureMap().at(type), resource_origin)) {
    return false;
  }

  return IsClientHintSentByDefault(type) || hints_preferences.ShouldSend(type);
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
              base::optional_ref<const UserAgentMetadata> user_agent_metadata,
              bool is_isolated_svg_chrome_client,
              bool is_prerendering,
              const String& reduced_accept_language)
      : url(url),
        content_security_policy(content_security_policy),
        site_for_cookies(std::move(site_for_cookies)),
        top_frame_origin(std::move(top_frame_origin)),
        client_hints_preferences(client_hints_preferences),
        device_pixel_ratio(device_pixel_ratio),
        user_agent(user_agent),
        user_agent_metadata(user_agent_metadata.CopyAsOptional()),
        is_isolated_svg_chrome_client(is_isolated_svg_chrome_client),
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
  const std::optional<UserAgentMetadata> user_agent_metadata;
  const bool is_isolated_svg_chrome_client;
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

const Vector<KURL>& FrameFetchContext::GetPotentiallyUnusedPreloads() const {
  if (LocalFrame* frame = GetFrame()) {
    if (LCPCriticalPathPredictor* lcpp = frame->GetLCPP()) {
      return lcpp->unused_preloads();
    }
  }
  return empty_unused_preloads_;
}

void FrameFetchContext::AddLcpPredictedCallback(base::OnceClosure callback) {
  if (LocalFrame* frame = FrameFetchContext::GetFrame()) {
    if (LCPCriticalPathPredictor* lcpp = frame->GetLCPP()) {
      lcpp->AddLCPPredictedCallback(std::move(callback));
    }
  }
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
  const bool minimal_prep = RuntimeEnabledFeatures::
      MinimimalResourceRequestPrepBeforeCacheLookupEnabled();

  if (!minimal_prep) {
    SetFirstPartyCookie(request);
  }
  if (request.GetRequestContext() ==
      mojom::blink::RequestContextType::SERVICE_WORKER) {
    // The top frame origin is defined to be null for service worker main
    // resource requests.
    DCHECK(!request.TopFrameOrigin());
  } else {
    request.SetTopFrameOrigin(GetTopFrameOrigin());
  }

  request.SetHTTPUserAgent(AtomicString(GetUserAgent()));

  if (GetResourceFetcherProperties().IsDetached())
    return;

  request.SetUkmSourceId(document_->UkmSourceID());
  request.SetStorageAccessApiStatus(
      document_->GetExecutionContext()->GetStorageAccessApiStatus());

  if (!minimal_prep) {
    if (document_loader_->ForceFetchCacheMode()) {
      request.SetCacheMode(*document_loader_->ForceFetchCacheMode());
    }
    if (const AttributionSrcLoader* attribution_src_loader =
            GetFrame()->GetAttributionSrcLoader()) {
      request.SetAttributionReportingSupport(
          attribution_src_loader->GetSupport());
    }
  }

  // If the original request included the attribute to opt-in to shared storage,
  // then update eligibility for the current (possibly redirected) request. Note
  // that if the original request didn't opt-in, then the original request and
  // any subsequent redirects are ineligible for shared storage writing by
  // response header.
  if (request.GetSharedStorageWritableOptedIn()) {
    auto* policy = GetPermissionsPolicy();
    request.SetSharedStorageWritableEligible(
        policy &&
        request.IsFeatureEnabledForSubresourceRequestAssumingOptIn(
            policy, mojom::blink::PermissionsPolicyFeature::kSharedStorage,
            SecurityOrigin::Create(request.Url())->ToUrlOrigin()));
  }

  request.SetSharedDictionaryWriterEnabled(
      RuntimeEnabledFeatures::CompressionDictionaryTransportEnabled(
          GetExecutionContext()));

  if (!minimal_prep) {
    WillSendRequest(request);
  }
  GetLocalFrameClient()->DispatchFinalizeRequest(request);
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

bool FrameFetchContext::AllowImage() const {
  if (GetResourceFetcherProperties().IsDetached())
    return true;

  bool images_enabled = GetFrame()->ImagesEnabled();
  if (!images_enabled) {
    if (auto* settings_client = GetContentSettingsClient()) {
      settings_client->DidNotAllowImage();
    }
  }
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
    const std::optional<float> resource_width,
    ResourceRequest& request) {
  if (GetResourceFetcherProperties().IsDetached()) {
    return;
  }

  // If the feature is enabled, then client hints are allowed only on secure
  // URLs.
  if (!ClientHintsPreferences::IsClientHintsAllowed(request.Url()))
    return;

  // Check if |url| is allowed to run JavaScript. If not, client hints are not
  // attached to the requests that initiate on the render side.
  if (!GetFrame()->ScriptEnabled()) {
    return;
  }

  // The Permissions policy is used to enable hints for all subresources, based
  // on the policy of the requesting document, and the origin of the resource.
  const PermissionsPolicy* policy =
      document_
          ? document_->domWindow()->GetSecurityContext().GetPermissionsPolicy()
          : nullptr;

  if (!policy) {
    return;
  }

  const scoped_refptr<SecurityOrigin> security_origin =
      SecurityOrigin::Create(request.Url());
  bool is_1p_origin = IsFirstPartyOrigin(security_origin.get());
  const url::Origin resource_origin = security_origin->ToUrlOrigin();

  std::optional<UserAgentMetadata> ua = GetUserAgentMetadata();

  const ClientHintsPreferences& hints_preferences = GetClientHintsPreferences();

  using network::mojom::blink::WebClientHintsType;

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kDeviceMemory_DEPRECATED,
                           hints_preferences)) {
    request.SetHttpHeaderField(
        http_names::kDeviceMemory_DEPRECATED,
        AtomicString(String::Number(
            ApproximatedDeviceMemory::GetApproximatedDeviceMemory())));
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kDeviceMemory,
                           hints_preferences)) {
    request.SetHttpHeaderField(
        http_names::kDeviceMemory,
        AtomicString(String::Number(
            ApproximatedDeviceMemory::GetApproximatedDeviceMemory())));
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kRtt_DEPRECATED,
                           hints_preferences)) {
    std::optional<base::TimeDelta> http_rtt =
        GetNetworkStateNotifier().GetWebHoldbackHttpRtt();
    if (!http_rtt) {
      http_rtt = GetNetworkStateNotifier().HttpRtt();
    }

    uint32_t rtt = GetNetworkStateNotifier().RoundRtt(
        request.Url().Host().ToString(), http_rtt);
    request.SetHttpHeaderField(http_names::kRtt_DEPRECATED,
                               AtomicString(String::Number(rtt)));
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kDownlink_DEPRECATED,
                           hints_preferences)) {
    std::optional<double> throughput_mbps =
        GetNetworkStateNotifier().GetWebHoldbackDownlinkThroughputMbps();
    if (!throughput_mbps) {
      throughput_mbps = GetNetworkStateNotifier().DownlinkThroughputMbps();
    }

    double mbps = GetNetworkStateNotifier().RoundMbps(
        request.Url().Host().ToString(), throughput_mbps);
    request.SetHttpHeaderField(http_names::kDownlink_DEPRECATED,
                               AtomicString(String::Number(mbps)));
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kEct_DEPRECATED,
                           hints_preferences)) {
    std::optional<WebEffectiveConnectionType> holdback_ect =
        GetNetworkStateNotifier().GetWebHoldbackEffectiveType();
    if (!holdback_ect) {
      holdback_ect = GetNetworkStateNotifier().EffectiveType();
    }

    request.SetHttpHeaderField(
        http_names::kEct_DEPRECATED,
        AtomicString(NetworkStateNotifier::EffectiveConnectionTypeToString(
            holdback_ect.value())));
  }

  // Only send User Agent hints if the info is available
  if (ua) {
    // ShouldSendClientHint is called to make sure UA is controlled by
    // Permissions Policy.
    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUA, hints_preferences)) {
      if (last_ua_ != *ua) {
        last_ua_ = *ua;
        last_ua_serialized_brand_major_version_list_ =
            AtomicString(ua->SerializeBrandMajorVersionList().c_str());
      }
      request.SetHttpHeaderField(http_names::kUA,
                                 *last_ua_serialized_brand_major_version_list_);
    }

    // We also send Sec-CH-UA-Mobile to all hints. It is a one-bit header
    // identifying if the browser has opted for a "mobile" experience.
    // ShouldSendClientHint is called to make sure it's controlled by
    // PermissionsPolicy.
    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAMobile,
                             hints_preferences)) {
      request.SetHttpHeaderField(http_names::kUAMobile,
                                 SerializeBoolHeader(ua->mobile));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAArch, hints_preferences)) {
      request.SetHttpHeaderField(http_names::kUAArch,
                                 SerializeStringHeader(ua->architecture));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAPlatform,
                             hints_preferences)) {
      request.SetHttpHeaderField(http_names::kUAPlatform,
                                 SerializeStringHeader(ua->platform));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAPlatformVersion,
                             hints_preferences)) {
      request.SetHttpHeaderField(http_names::kUAPlatformVersion,
                                 SerializeStringHeader(ua->platform_version));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAModel, hints_preferences)) {
      request.SetHttpHeaderField(http_names::kUAModel,
                                 SerializeStringHeader(ua->model));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAFullVersion,
                             hints_preferences)) {
      request.SetHttpHeaderField(http_names::kUAFullVersion,
                                 SerializeStringHeader(ua->full_version));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAFullVersionList,
                             hints_preferences)) {
      request.SetHttpHeaderField(
          http_names::kUAFullVersionList,
          AtomicString(ua->SerializeBrandFullVersionList().c_str()));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUABitness,
                             hints_preferences)) {
      request.SetHttpHeaderField(http_names::kUABitness,
                                 SerializeStringHeader(ua->bitness));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAWoW64, hints_preferences)) {
      request.SetHttpHeaderField(http_names::kUAWoW64,
                                 SerializeBoolHeader(ua->wow64));
    }

    if (ShouldSendClientHint(
            *policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAFormFactors,
            hints_preferences)) {
      request.SetHttpHeaderField(
          http_names::kUAFormFactors,
          AtomicString(ua->SerializeFormFactors().c_str()));
    }
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kSaveData, hints_preferences) &&
      GetNetworkStateNotifier().SaveDataEnabled()) {
    request.SetHttpHeaderField(http_names::kSaveData, http_names::kOn);
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kPrefersReducedTransparency,
                           hints_preferences)) {
    request.SetHttpHeaderField(http_names::kPrefersReducedTransparency,
                               GetSettings()->GetPrefersReducedTransparency()
                                   ? http_names::kReduce
                                   : http_names::kNoPreference);
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kPrefersReducedMotion,
                           hints_preferences)) {
    request.SetHttpHeaderField(http_names::kPrefersReducedMotion,
                               GetSettings()->GetPrefersReducedMotion()
                                   ? http_names::kReduce
                                   : http_names::kNoPreference);
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kPrefersColorScheme,
                           hints_preferences)) {
    request.SetHttpHeaderField(
        http_names::kPrefersColorScheme,
        document_->InDarkMode() ? http_names::kDark : http_names::kLight);
  }

  const float dpr = GetDevicePixelRatio();

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kDpr_DEPRECATED,
                           hints_preferences)) {
    request.SetHttpHeaderField(http_names::kDpr_DEPRECATED,
                               AtomicString(String::Number(dpr)));
  }

  if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kDpr, hints_preferences)) {
    request.SetHttpHeaderField(http_names::kDpr,
                               AtomicString(String::Number(dpr)));
  }

  if (LocalFrameView* frame_view = GetFrame()->View()) {
    const int viewport_width = frame_view->ViewportWidth();
    const int viewport_height = frame_view->ViewportHeight();
    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kViewportWidth_DEPRECATED,
                             hints_preferences)) {
      request.SetHttpHeaderField(http_names::kViewportWidth_DEPRECATED,
                                 AtomicString(String::Number(viewport_width)));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kViewportWidth,
                             hints_preferences)) {
      request.SetHttpHeaderField(http_names::kViewportWidth,
                                 AtomicString(String::Number(viewport_width)));
    }

    if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kViewportHeight,
                             hints_preferences)) {
      request.SetHttpHeaderField(http_names::kViewportHeight,
                                 AtomicString(String::Number(viewport_height)));
    }

    if (resource_width) {
      if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                               WebClientHintsType::kResourceWidth_DEPRECATED,
                               hints_preferences)) {
        float physical_width = resource_width.value() * dpr;
        request.SetHttpHeaderField(
            http_names::kResourceWidth_DEPRECATED,
            AtomicString(String::Number(ceil(physical_width))));
      }

      if (ShouldSendClientHint(*policy, resource_origin, is_1p_origin,
                               WebClientHintsType::kResourceWidth,
                               hints_preferences)) {
        float physical_width = resource_width.value() * dpr;
        request.SetHttpHeaderField(
            http_names::kResourceWidth,
            AtomicString(String::Number(ceil(physical_width))));
      }
    }
  }
}

void FrameFetchContext::AddReducedAcceptLanguageIfNecessary(
    ResourceRequest& request) {
  // If the feature is enabled, then reduce accept language are allowed only on
  // http and https.
  if (!base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage)) {
    return;
  }

  if (!request.Url().ProtocolIsInHTTPFamily())
    return;

  const String& reduced_accept_language = GetReducedAcceptLanguage();
  if (!reduced_accept_language.empty() &&
      request.HttpHeaderField(http_names::kAcceptLanguage).empty()) {
    request.SetHttpHeaderField(
        http_names::kAcceptLanguage,
        AtomicString(reduced_accept_language.Ascii().c_str()));
  }
}

void FrameFetchContext::WillSendRequest(ResourceRequest& resource_request) {
  // Set upstream url based on the request's redirect info.
  KURL upstream_url;
  if (resource_request.GetRedirectInfo().has_value()) {
    upstream_url = KURL(resource_request.GetRedirectInfo()->previous_url);
  }
  std::optional<KURL> overriden_url =
      GetLocalFrameClient()->DispatchWillSendRequest(
          resource_request.Url(), resource_request.RequestorOrigin(),
          resource_request.SiteForCookies(),
          resource_request.GetRedirectInfo().has_value(), upstream_url);
  if (overriden_url.has_value()) {
    resource_request.SetUrl(overriden_url.value());
  }
}

void FrameFetchContext::PopulateResourceRequestBeforeCacheAccess(
    const ResourceLoaderOptions& options,
    ResourceRequest& request) {
  DCHECK(RuntimeEnabledFeatures::
             MinimimalResourceRequestPrepBeforeCacheLookupEnabled());
  if (!GetResourceFetcherProperties().IsDetached()) {
    probe::SetDevToolsIds(Probe(), request, options.initiator_info);
  }

  // CSP may change the url.
  ModifyRequestForCSP(request);
  if (!request.Url().IsValid()) {
    return;
  }
  SetFirstPartyCookie(request);
  if (CoreProbeSink::HasAgentsGlobal(CoreProbeSink::kInspectorEmulationAgent |
                                     CoreProbeSink::kInspectorNetworkAgent)) {
    request.SetRequiresUpgradeForLoader();
  }
  if (document_loader_->ForceFetchCacheMode()) {
    request.SetCacheMode(*document_loader_->ForceFetchCacheMode());
  }
  // ResourceFetcher::DidLoadResourceFromMemoryCache() may call out in such a
  // way that the AttributionSupport is needed.
  if (const AttributionSrcLoader* attribution_src_loader =
          GetFrame()->GetAttributionSrcLoader()) {
    request.SetAttributionReportingSupport(
        attribution_src_loader->GetSupport());
  }
}

void FrameFetchContext::UpgradeResourceRequestForLoader(
    ResourceType type,
    const std::optional<float> resource_width,
    ResourceRequest& request,
    const ResourceLoaderOptions& options) {
  if (!RuntimeEnabledFeatures::
          MinimimalResourceRequestPrepBeforeCacheLookupEnabled()) {
    if (!GetResourceFetcherProperties().IsDetached()) {
      probe::SetDevToolsIds(Probe(), request, options.initiator_info);
    }
    ModifyRequestForCSP(request);
  }
  AddClientHintsIfNecessary(resource_width, request);
  AddReducedAcceptLanguageIfNecessary(request);
}

bool FrameFetchContext::IsPrerendering() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->is_prerendering;
  return document_->IsPrerendering();
}

bool FrameFetchContext::DoesLCPPHaveAnyHintData() {
  if (GetResourceFetcherProperties().IsDetached()) {
    return false;
  }

  LCPCriticalPathPredictor* lcpp = GetFrame()->GetLCPP();
  if (!lcpp) {
    return false;
  }
  return lcpp->HasAnyHintData();
}

bool FrameFetchContext::DoesLCPPHaveLcpElementLocatorHintData() {
  if (GetResourceFetcherProperties().IsDetached()) {
    return false;
  }

  LCPCriticalPathPredictor* lcpp = GetFrame()->GetLCPP();
  if (!lcpp) {
    return false;
  }
  return !lcpp->lcp_element_locators().empty();
}

void FrameFetchContext::SetFirstPartyCookie(ResourceRequest& request) {
  // Set the first party for cookies url if it has not been set yet (new
  // requests). This value will be updated during redirects, consistent with
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00#section-2.1.1?
  if (!request.SiteForCookiesSet())
    request.SetSiteForCookies(GetSiteForCookies());
}

bool FrameFetchContext::AllowScript() const {
  bool script_enabled = GetFrame()->ScriptEnabled();
  if (!script_enabled) {
    WebContentSettingsClient* settings_client = GetContentSettingsClient();
    if (settings_client) {
      settings_client->DidNotAllowScript();
    }
  }
  return script_enabled;
}

bool FrameFetchContext::IsFirstPartyOrigin(
    const SecurityOrigin* resource_origin) const {
  if (GetResourceFetcherProperties().IsDetached())
    return false;

  return GetFrame()
      ->Tree()
      .Top()
      .GetSecurityContext()
      ->GetSecurityOrigin()
      ->IsSameOriginWith(resource_origin);
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
    return frozen_state_->content_security_policy.Get();

  return document_->GetExecutionContext()->GetContentSecurityPolicyForWorld(
      world);
}

bool FrameFetchContext::IsIsolatedSVGChromeClient() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->is_isolated_svg_chrome_client;

  return GetFrame()->GetChromeClient().IsIsolatedSVGChromeClient();
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
    base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info,
    const KURL& url,
    ReportingDisposition reporting_disposition,
    const String& devtools_id) const {
  if (GetResourceFetcherProperties().IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return false;
  }
  const KURL& url_before_redirects =
      redirect_info.has_value() ? redirect_info->original_url : url;
  ResourceRequest::RedirectStatus redirect_status =
      redirect_info.has_value() ? RedirectStatus::kFollowedRedirect
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
    return frozen_state_->content_security_policy.Get();
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

std::optional<UserAgentMetadata> FrameFetchContext::GetUserAgentMetadata()
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
  if (override_accept_language.empty()) {
    String expanded_language = network_utils::ExpandLanguageList(
        frame->GetReducedAcceptLanguage().GetString());
    return network_utils::GenerateAcceptLanguageHeader(expanded_language);
  }
  return network_utils::GenerateAcceptLanguageHeader(override_accept_language);
}

float FrameFetchContext::GetDevicePixelRatio() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->device_pixel_ratio;
  return document_->DevicePixelRatio();
}

FetchContext* FrameFetchContext::Detach() {
  if (GetResourceFetcherProperties().IsDetached())
    return this;

  // As we completed the reduction in the user-agent, the reduced User-Agent
  // string returns from GetUserAgent() should also be set on the User-Agent
  // request header.
  const ClientHintsPreferences& client_hints_prefs =
      GetClientHintsPreferences();
  frozen_state_ = MakeGarbageCollected<FrozenState>(
      Url(), GetContentSecurityPolicy(), GetSiteForCookies(),
      GetTopFrameOrigin(), client_hints_prefs, GetDevicePixelRatio(),
      GetUserAgent(), GetUserAgentMetadata(), IsIsolatedSVGChromeClient(),
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
    base::optional_ref<const KURL> alias_url,
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
  const KURL& url =
      alias_url.has_value() ? alias_url.value() : resource_request.Url();
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

std::optional<ResourceRequestBlockedReason> FrameFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info)
    const {
  const bool detached = GetResourceFetcherProperties().IsDetached();
  if (!detached && document_->IsFreezingInProgress() &&
      !resource_request.GetKeepalive()) {
    GetDetachableConsoleLogger().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kError,
            "Only fetch keepalive is allowed during onfreeze: " +
                url.GetString()));
    return ResourceRequestBlockedReason::kOther;
  }
  std::optional<ResourceRequestBlockedReason> blocked_reason =
      BaseFetchContext::CanRequest(type, resource_request, url, options,
                                   reporting_disposition, redirect_info);
  if (blocked_reason) {
    return blocked_reason;
  }
  if (detached || !RuntimeEnabledFeatures::
                      MinimimalResourceRequestPrepBeforeCacheLookupEnabled()) {
    return std::nullopt;
  }
  if (!resource_request.Url().IsValid()) {
    return ResourceRequestBlockedReason::kOther;
  }
  return std::nullopt;
}

CoreProbeSink* FrameFetchContext::Probe() const {
  return probe::ToCoreProbeSink(GetFrame()->GetDocument());
}

void FrameFetchContext::UpdateSubresourceLoadMetrics(
    const SubresourceLoadMetrics& subresource_load_metrics) {
  document_loader_->UpdateSubresourceLoadMetrics(subresource_load_metrics);
}

}  // namespace blink

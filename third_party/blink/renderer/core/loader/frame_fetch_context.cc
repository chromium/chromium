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
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
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
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"
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
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
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
// device-memory, resource-width, viewport-width and the limited UA hints are
// sent under this model). This feature is enabled by default on Android, and
// disabled by default on all other platforms.
//
// When the runtime flag is enabled, all client hints except UA are controlled
// entirely by feature policy on all platforms. In that case, hints will
// generally be sent for first-party resources, and not for third-party
// resources, unless specifically enabled by policy.

// If kAllowClientHintsToThirdParty is enabled, then device-memory,
// resource-width and viewport-width client hints can be sent to third-party
// origins if the first-party has opted in to receiving client hints.
#if defined(OS_ANDROID)
const base::Feature kAllowClientHintsToThirdParty{
    "AllowClientHintsToThirdParty", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kAllowClientHintsToThirdParty{
    "AllowClientHintsToThirdParty", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

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
const AtomicString AddQuotes(std::string str) {
  if (str.empty())
    return AtomicString("");

  StringBuilder quoted_string;
  quoted_string.Append("\"");
  quoted_string.Append(str.data());
  quoted_string.Append("\"");
  return quoted_string.ToAtomicString();
}

}  // namespace

struct FrameFetchContext::FrozenState final : GarbageCollected<FrozenState> {
  FrozenState(const KURL& url,
              scoped_refptr<const SecurityOrigin> parent_security_origin,
              const ContentSecurityPolicy* content_security_policy,
              KURL site_for_cookies,
              scoped_refptr<const SecurityOrigin> top_frame_origin,
              const ClientHintsPreferences& client_hints_preferences,
              float device_pixel_ratio,
              const String& user_agent,
              const UserAgentMetadata& user_agent_metadata,
              bool is_svg_image_chrome_client)
      : url(url),
        parent_security_origin(std::move(parent_security_origin)),
        content_security_policy(content_security_policy),
        site_for_cookies(site_for_cookies),
        top_frame_origin(std::move(top_frame_origin)),
        client_hints_preferences(client_hints_preferences),
        device_pixel_ratio(device_pixel_ratio),
        user_agent(user_agent),
        user_agent_metadata(user_agent_metadata),
        is_svg_image_chrome_client(is_svg_image_chrome_client) {}

  const KURL url;
  const scoped_refptr<const SecurityOrigin> parent_security_origin;
  const Member<const ContentSecurityPolicy> content_security_policy;
  const KURL site_for_cookies;
  const scoped_refptr<const SecurityOrigin> top_frame_origin;
  const ClientHintsPreferences client_hints_preferences;
  const float device_pixel_ratio;
  const String user_agent;
  const UserAgentMetadata user_agent_metadata;
  const bool is_svg_image_chrome_client;

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(content_security_policy);
  }
};

ResourceFetcher* FrameFetchContext::CreateFetcherForCommittedDocument(
    DocumentLoader& loader,
    Document& document) {
  auto& frame_or_imported_document =
      *MakeGarbageCollected<FrameOrImportedDocument>(loader, document);
  auto& properties = *MakeGarbageCollected<DetachableResourceFetcherProperties>(
      *MakeGarbageCollected<FrameResourceFetcherProperties>(
          frame_or_imported_document));
  LocalFrame& frame = frame_or_imported_document.GetFrame();
  ResourceFetcherInit init(
      properties,
      MakeGarbageCollected<FrameFetchContext>(frame_or_imported_document,
                                              properties),
      frame.GetTaskRunner(TaskType::kNetworking),
      MakeGarbageCollected<LoaderFactoryForFrame>(frame_or_imported_document));
  init.use_counter = MakeGarbageCollected<DetachableUseCounter>(&document);
  init.console_logger =
      MakeGarbageCollected<DetachableConsoleLogger>(&document);
  // Frame loading should normally start with |kTight| throttling, as the
  // frame will be in layout-blocking state until the <body> tag is inserted
  init.initial_throttling_policy =
      ResourceLoadScheduler::ThrottlingPolicy::kTight;
  init.frame_scheduler = frame.GetFrameScheduler();
  init.archive = loader.Archive();
  ResourceFetcher* fetcher = MakeGarbageCollected<ResourceFetcher>(init);
  fetcher->SetResourceLoadObserver(
      MakeGarbageCollected<ResourceLoadObserverForFrame>(
          frame_or_imported_document, fetcher->GetProperties()));
  fetcher->SetImagesEnabled(frame.GetSettings()->GetImagesEnabled());
  fetcher->SetAutoLoadImages(
      frame.GetSettings()->GetLoadsImagesAutomatically());
  return fetcher;
}

ResourceFetcher* FrameFetchContext::CreateFetcherForImportedDocument(
    Document* document) {
  DCHECK(document);
  // |document| is detached.
  DCHECK(!document->GetFrame());
  auto& frame_or_imported_document =
      *MakeGarbageCollected<FrameOrImportedDocument>(*document);
  auto& properties = *MakeGarbageCollected<DetachableResourceFetcherProperties>(
      *MakeGarbageCollected<FrameResourceFetcherProperties>(
          frame_or_imported_document));
  LocalFrame& frame = frame_or_imported_document.GetFrame();
  ResourceFetcherInit init(
      properties,
      MakeGarbageCollected<FrameFetchContext>(frame_or_imported_document,
                                              properties),
      document->GetTaskRunner(blink::TaskType::kNetworking),
      MakeGarbageCollected<LoaderFactoryForFrame>(frame_or_imported_document));
  init.use_counter = MakeGarbageCollected<DetachableUseCounter>(document);
  init.console_logger = MakeGarbageCollected<DetachableConsoleLogger>(document);
  init.frame_scheduler = frame.GetFrameScheduler();
  auto* fetcher = MakeGarbageCollected<ResourceFetcher>(init);
  fetcher->SetResourceLoadObserver(
      MakeGarbageCollected<ResourceLoadObserverForFrame>(
          frame_or_imported_document, fetcher->GetProperties()));
  return fetcher;
}

FrameFetchContext::FrameFetchContext(
    const FrameOrImportedDocument& frame_or_imported_document,
    const DetachableResourceFetcherProperties& properties)
    : BaseFetchContext(properties),
      frame_or_imported_document_(frame_or_imported_document),
      save_data_enabled_(
          GetNetworkStateNotifier().SaveDataEnabled() &&
          !GetFrame()->GetSettings()->GetDataSaverHoldbackWebApi()) {}

KURL FrameFetchContext::GetSiteForCookies() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->site_for_cookies;
  return frame_or_imported_document_->GetDocument().SiteForCookies();
}

scoped_refptr<const SecurityOrigin> FrameFetchContext::GetTopFrameOrigin()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->top_frame_origin;
  return frame_or_imported_document_->GetDocument().TopFrameOrigin();
}

SubresourceFilter* FrameFetchContext::GetSubresourceFilter() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  DocumentLoader* document_loader = MasterDocumentLoader();
  return document_loader ? document_loader->GetSubresourceFilter() : nullptr;
}

PreviewsResourceLoadingHints*
FrameFetchContext::GetPreviewsResourceLoadingHints() const {
  if (GetResourceFetcherProperties().IsDetached())
    return nullptr;
  DocumentLoader* document_loader = MasterDocumentLoader();
  if (!document_loader)
    return nullptr;
  return document_loader->GetPreviewsResourceLoadingHints();
}

WebURLRequest::PreviewsState FrameFetchContext::previews_state() const {
  DocumentLoader* document_loader = MasterDocumentLoader();
  return document_loader ? document_loader->GetPreviewsState()
                         : WebURLRequest::kPreviewsUnspecified;
}

LocalFrame* FrameFetchContext::GetFrame() const {
  return &frame_or_imported_document_->GetFrame();
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
  if (IsReloadLoadType(MasterDocumentLoader()->LoadType()))
    request.ClearHttpHeaderField(http_names::kSaveData);

  if (save_data_enabled_)
    request.SetHttpHeaderField(http_names::kSaveData, "on");
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

DocumentLoader* FrameFetchContext::GetDocumentLoader() const {
  DCHECK(!GetResourceFetcherProperties().IsDetached());
  return frame_or_imported_document_->GetDocumentLoader();
}

inline DocumentLoader* FrameFetchContext::MasterDocumentLoader() const {
  DCHECK(!GetResourceFetcherProperties().IsDetached());
  return &frame_or_imported_document_->GetMasterDocumentLoader();
}

void FrameFetchContext::PrepareRequest(
    ResourceRequest& request,
    const FetchInitiatorInfo& initiator_info,
    WebScopedVirtualTimePauser& virtual_time_pauser,
    ResourceType resource_type) {
  // TODO(yhirano): Clarify which statements are actually needed when
  // this is called during redirect.
  const bool for_redirect =
      (request.GetRedirectStatus() ==
       ResourceRequest::RedirectStatus::kFollowedRedirect);

  SetFirstPartyCookie(request);
  if (request.GetRequestContext() ==
      mojom::RequestContextType::SERVICE_WORKER) {
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

  DocumentLoader* document_loader = MasterDocumentLoader();
  if (document_loader->ForceFetchCacheMode())
    request.SetCacheMode(*document_loader->ForceFetchCacheMode());

  if (request.GetPreviewsState() == WebURLRequest::kPreviewsUnspecified) {
    WebURLRequest::PreviewsState request_previews_state =
        document_loader->GetPreviewsState();
    // The decision of whether or not to enable Client Lo-Fi is made earlier
    // in the request lifetime, in LocalFrame::MaybeAllowImagePlaceholder(),
    // so don't add the Client Lo-Fi bit to the request here.
    request_previews_state &= ~(WebURLRequest::kLazyImageLoadDeferred);
    if (request_previews_state == WebURLRequest::kPreviewsUnspecified)
      request_previews_state = WebURLRequest::kPreviewsOff;
    request.SetPreviewsState(request_previews_state);
  }

  GetLocalFrameClient()->DispatchWillSendRequest(request);
  FrameScheduler* frame_scheduler = GetFrame()->GetFrameScheduler();
  if (!for_redirect && frame_scheduler) {
    virtual_time_pauser = frame_scheduler->CreateWebScopedVirtualTimePauser(
        request.Url().GetString(),
        WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
  }

  probe::PrepareRequest(Probe(), document_loader, request, initiator_info,
                        resource_type);

  // ServiceWorker hook ups.
  if (document_loader->GetServiceWorkerNetworkProvider()) {
    WrappedResourceRequest webreq(request);
    document_loader->GetServiceWorkerNetworkProvider()->WillSendRequest(webreq);
  }
}

void FrameFetchContext::AddResourceTiming(const ResourceTimingInfo& info) {
  // Normally, |document_| is cleared on Document shutdown. However, Documents
  // for HTML imports will also not have a LocalFrame set: in that case, also
  // early return, as there is nothing to report the resource timing to.
  if (GetResourceFetcherProperties().IsDetached())
    return;
  LocalFrame* frame = frame_or_imported_document_->GetDocument().GetFrame();
  if (!frame)
    return;

  // Timing for main resource is handled in DocumentLoader.
  // All other resources are reported to the corresponding Document.
  DOMWindowPerformance::performance(
      *frame_or_imported_document_->GetDocument().domWindow())
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

  // Record the latest requiredCSP value that will be used when sending this
  // request.
  GetFrame()->Loader().RecordLatestRequiredCSP();
  GetFrame()->Loader().ModifyRequestForCSP(
      resource_request,
      &GetResourceFetcherProperties().GetFetchClientSettingsObject(),
      &frame_or_imported_document_->GetDocument(),
      network::mojom::RequestContextFrameType::kNone);
}

void FrameFetchContext::AddClientHintsIfNecessary(
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  WebEnabledClientHints enabled_hints;

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
  const FeaturePolicy* policy = nullptr;
  if (frame_or_imported_document_)
    policy = frame_or_imported_document_->GetDocument().GetFeaturePolicy();
  url::Origin resource_origin =
      SecurityOrigin::Create(request.Url())->ToUrlOrigin();

  // Sec-CH-UA is special: we always send the header to all origins that are
  // eligible for client hints (e.g. secure transport, JavaScript enabled). We
  // alter the header's value based on whether or not the site has opted into
  // additional detail.
  //
  // https://github.com/WICG/ua-client-hints
  blink::UserAgentMetadata ua = GetUserAgentMetadata();
  bool use_full_ua =
      (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() ||
       (policy &&
        policy->IsFeatureEnabledForOrigin(
            mojom::FeaturePolicyFeature::kClientHintUA, resource_origin))) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kUA, hints_preferences,
                           enabled_hints);
  if (RuntimeEnabledFeatures::UserAgentClientHintEnabled()) {
    StringBuilder result;
    result.Append(ua.brand.data());
    const auto& version = use_full_ua ? ua.full_version : ua.major_version;
    if (!version.empty()) {
      result.Append(' ');
      result.Append(version.data());
    }
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kUA)],
        AddQuotes(result.ToString().Ascii()));
  }

  // If the frame is detached, then don't send any hints other than UA.
  if (!policy)
    return;

  bool is_1p_origin = IsFirstPartyOrigin(request.Url());

  if (!RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
      !base::FeatureList::IsEnabled(kAllowClientHintsToThirdParty) &&
      !is_1p_origin) {
    // No client hints for 3p origins.
    return;
  }
  // Persisted client hints preferences should be read for only the first
  // party origins.
  if (is_1p_origin && GetContentSettingsClient()) {
    GetContentSettingsClient()->GetAllowedClientHintsFromSource(request.Url(),
                                                                &enabled_hints);
  }

  // TODO(iclelland): If feature policy control over client hints ships, remove
  // the runtime flag check for the next four hints. Currently, when the
  // kAllowClientHintsToThirdParty feature is enabled, but the runtime flag is
  // *not* set, the behaviour is that these four hints will be sent on all
  // eligible requests. Feature policy control is intended to change that
  // default.

  if ((!RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() ||
       policy->IsFeatureEnabledForOrigin(
           mojom::FeaturePolicyFeature::kClientHintDeviceMemory,
           resource_origin)) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kDeviceMemory,
                           hints_preferences, enabled_hints)) {
    request.SetHttpHeaderField(
        "Device-Memory",
        AtomicString(String::Number(
            ApproximatedDeviceMemory::GetApproximatedDeviceMemory())));
  }

  float dpr = GetDevicePixelRatio();
  if ((!RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() ||
       policy->IsFeatureEnabledForOrigin(
           mojom::FeaturePolicyFeature::kClientHintDPR, resource_origin)) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kDpr, hints_preferences,
                           enabled_hints)) {
    request.SetHttpHeaderField("DPR", AtomicString(String::Number(dpr)));
  }

  if ((!RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() ||
       policy->IsFeatureEnabledForOrigin(
           mojom::FeaturePolicyFeature::kClientHintWidth, resource_origin)) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kResourceWidth,
                           hints_preferences, enabled_hints)) {
    if (resource_width.is_set) {
      float physical_width = resource_width.width * dpr;
      request.SetHttpHeaderField(
          "Width", AtomicString(String::Number(ceil(physical_width))));
    }
  }

  if ((!RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() ||
       policy->IsFeatureEnabledForOrigin(
           mojom::FeaturePolicyFeature::kClientHintViewportWidth,
           resource_origin)) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kViewportWidth,
                           hints_preferences, enabled_hints) &&
      !GetResourceFetcherProperties().IsDetached() && GetFrame()->View()) {
    request.SetHttpHeaderField(
        "Viewport-Width",
        AtomicString(String::Number(GetFrame()->View()->ViewportWidth())));
  }

  // TODO(iclelland): If feature policy control over client hints ships, remove
  // the runtime flag check and the 1p origin requirement for the remaining
  // hints. Currently, even when the kAllowClientHintsToThirdParty feature is
  // (and the runtime flag is *not* set,) these hints are only sent for first-
  // party requests. With feature policy control, these can be sent to third
  // parties as well, if correctly delegated.

  // Note that if both the kAllowClientHintsToThirdParty feature and the runtime
  // flag are disabled, this code will not be reached.

  // True if this is a first-party resource request, and feature policy for
  // client hints is *not* in use.
  bool can_always_send_hints =
      is_1p_origin &&
      !RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled();

  if ((can_always_send_hints ||
       (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
        policy->IsFeatureEnabledForOrigin(
            mojom::FeaturePolicyFeature::kClientHintRTT, resource_origin))) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kRtt, hints_preferences,
                           enabled_hints)) {
    base::Optional<base::TimeDelta> http_rtt =
        GetNetworkStateNotifier().GetWebHoldbackHttpRtt();
    if (!http_rtt) {
      http_rtt = GetNetworkStateNotifier().HttpRtt();
    }

    uint32_t rtt =
        GetNetworkStateNotifier().RoundRtt(request.Url().Host(), http_rtt);
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kRtt)],
        AtomicString(String::Number(rtt)));
  }

  if ((can_always_send_hints ||
       (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
        policy->IsFeatureEnabledForOrigin(
            mojom::FeaturePolicyFeature::kClientHintDownlink,
            resource_origin))) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kDownlink,
                           hints_preferences, enabled_hints)) {
    base::Optional<double> throughput_mbps =
        GetNetworkStateNotifier().GetWebHoldbackDownlinkThroughputMbps();
    if (!throughput_mbps) {
      throughput_mbps = GetNetworkStateNotifier().DownlinkThroughputMbps();
    }

    double mbps = GetNetworkStateNotifier().RoundMbps(request.Url().Host(),
                                                      throughput_mbps);
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kDownlink)],
        AtomicString(String::Number(mbps)));
  }

  if ((can_always_send_hints ||
       (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
        policy->IsFeatureEnabledForOrigin(
            mojom::FeaturePolicyFeature::kClientHintECT, resource_origin))) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kEct, hints_preferences,
                           enabled_hints)) {
    base::Optional<WebEffectiveConnectionType> holdback_ect =
        GetNetworkStateNotifier().GetWebHoldbackEffectiveType();
    if (!holdback_ect)
      holdback_ect = GetNetworkStateNotifier().EffectiveType();

    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kEct)],
        AtomicString(NetworkStateNotifier::EffectiveConnectionTypeToString(
            holdback_ect.value())));
  }

  if ((can_always_send_hints ||
       (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
        policy->IsFeatureEnabledForOrigin(
            mojom::FeaturePolicyFeature::kClientHintLang, resource_origin))) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kLang, hints_preferences,
                           enabled_hints)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kLang)],
        GetFrame()
            ->DomWindow()
            ->navigator()
            ->SerializeLanguagesForClientHintHeader());
  }

  if ((can_always_send_hints ||
       (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
        policy->IsFeatureEnabledForOrigin(
            mojom::FeaturePolicyFeature::kClientHintUAArch,
            resource_origin))) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kUAArch,
                           hints_preferences, enabled_hints)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kUAArch)],
        AddQuotes(ua.architecture));
  }

  if ((can_always_send_hints ||
       (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
        policy->IsFeatureEnabledForOrigin(
            mojom::FeaturePolicyFeature::kClientHintUAPlatform,
            resource_origin))) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kUAPlatform,
                           hints_preferences, enabled_hints)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kUAPlatform)],
        AddQuotes(ua.platform));
  }

  if ((can_always_send_hints ||
       (RuntimeEnabledFeatures::FeaturePolicyForClientHintsEnabled() &&
        policy->IsFeatureEnabledForOrigin(
            mojom::FeaturePolicyFeature::kClientHintUAModel,
            resource_origin))) &&
      ShouldSendClientHint(mojom::WebClientHintsType::kUAModel,
                           hints_preferences, enabled_hints)) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            mojom::WebClientHintsType::kUAModel)],
        AddQuotes(ua.model));
  }
}

void FrameFetchContext::PopulateResourceRequest(
    ResourceType type,
    const ClientHintsPreferences& hints_preferences,
    const FetchParameters::ResourceWidth& resource_width,
    ResourceRequest& request) {
  ModifyRequestForCSP(request);
  AddClientHintsIfNecessary(hints_preferences, resource_width, request);

  const ContentSecurityPolicy* csp = GetContentSecurityPolicy();
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
  if (request.SiteForCookies().IsNull())
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
      ->IsSameSchemeHostPort(SecurityOrigin::Create(url).get());
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
  probe::DidBlockRequest(Probe(), resource_request, MasterDocumentLoader(),
                         Url(), fetch_initiator_info, blocked_reason,
                         resource_type);
}

bool FrameFetchContext::ShouldBypassMainWorldCSP() const {
  if (GetResourceFetcherProperties().IsDetached())
    return false;

  return ContentSecurityPolicy::ShouldBypassMainWorld(
      GetFrame()->GetDocument());
}

bool FrameFetchContext::IsSVGImageChromeClient() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->is_svg_image_chrome_client;

  return GetFrame()->GetChromeClient().IsSVGImageChromeClient();
}

void FrameFetchContext::CountUsage(WebFeature feature) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  if (DocumentLoader* loader = MasterDocumentLoader())
    loader->GetUseCounterHelper().Count(feature, GetFrame());
}

void FrameFetchContext::CountDeprecation(WebFeature feature) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;
  if (MasterDocumentLoader())
    Deprecation::CountDeprecation(MasterDocumentLoader(), feature);
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
    mojom::RequestContextType request_context,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) const {
  if (GetResourceFetcherProperties().IsDetached()) {
    // TODO(yhirano): Implement the detached case.
    return false;
  }
  return MixedContentChecker::ShouldBlockFetch(
      GetFrame(), request_context, redirect_status, url, reporting_policy);
}

bool FrameFetchContext::ShouldBlockFetchAsCredentialedSubresource(
    const ResourceRequest& resource_request,
    const KURL& url) const {
  // URLs with no embedded credentials should load correctly.
  if (url.User().IsEmpty() && url.Pass().IsEmpty())
    return false;

  if (resource_request.GetRequestContext() ==
      mojom::RequestContextType::XML_HTTP_REQUEST) {
    return false;
  }

  // Relative URLs on top-level pages that were loaded with embedded credentials
  // should load correctly.
  // TODO(mkwst): This doesn't work when the subresource is an iframe.
  // See https://crbug.com/756846.
  if (Url().User() == url.User() && Url().Pass() == url.Pass() &&
      SecurityOrigin::Create(url)->IsSameSchemeHostPort(
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
  return frame_or_imported_document_->GetDocument().Url();
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
  return frame_or_imported_document_->GetDocument().GetContentSecurityPolicy();
}

void FrameFetchContext::AddConsoleMessage(ConsoleMessage* message) const {
  if (GetResourceFetcherProperties().IsDetached())
    return;

  // Route the console message through Document if it's attached, so
  // that script line numbers can be included. Otherwise, route directly to the
  // FrameConsole, to ensure we never drop a message.
  if (frame_or_imported_document_->GetDocument().GetFrame())
    frame_or_imported_document_->GetDocument().AddConsoleMessage(message);
  else
    GetFrame()->Console().AddMessage(message);
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

UserAgentMetadata FrameFetchContext::GetUserAgentMetadata() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->user_agent_metadata;
  return GetLocalFrameClient()->UserAgentMetadata();
}

const ClientHintsPreferences FrameFetchContext::GetClientHintsPreferences()
    const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->client_hints_preferences;
  LocalFrame* frame = frame_or_imported_document_->GetDocument().GetFrame();
  return frame ? frame->GetClientHintsPreferences() : ClientHintsPreferences();
}

float FrameFetchContext::GetDevicePixelRatio() const {
  if (GetResourceFetcherProperties().IsDetached())
    return frozen_state_->device_pixel_ratio;
  return frame_or_imported_document_->GetDocument().DevicePixelRatio();
}

bool FrameFetchContext::ShouldSendClientHint(
    mojom::WebClientHintsType type,
    const ClientHintsPreferences& hints_preferences,
    const WebEnabledClientHints& enabled_hints) const {
  return GetClientHintsPreferences().ShouldSend(type) ||
         hints_preferences.ShouldSend(type) || enabled_hints.IsEnabled(type);
}

FetchContext* FrameFetchContext::Detach() {
  if (GetResourceFetcherProperties().IsDetached())
    return this;

  frozen_state_ = MakeGarbageCollected<FrozenState>(
      Url(), GetParentSecurityOrigin(), GetContentSecurityPolicy(),
      GetSiteForCookies(), GetTopFrameOrigin(), GetClientHintsPreferences(),
      GetDevicePixelRatio(), GetUserAgent(), GetUserAgentMetadata(),
      IsSVGImageChromeClient());
  frame_or_imported_document_ = nullptr;
  return this;
}

void FrameFetchContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_or_imported_document_);
  visitor->Trace(frozen_state_);
  BaseFetchContext::Trace(visitor);
}

bool FrameFetchContext::CalculateIfAdSubresource(
    const ResourceRequest& resource_request,
    ResourceType type) {
  // Mark the resource as an Ad if the SubresourceFilter thinks it's an ad.
  bool known_ad =
      BaseFetchContext::CalculateIfAdSubresource(resource_request, type);
  if (GetResourceFetcherProperties().IsDetached() ||
      !GetFrame()->GetAdTracker()) {
    return known_ad;
  }

  // The AdTracker needs to know about the request as well, and may also mark it
  // as an ad.
  return GetFrame()->GetAdTracker()->CalculateIfAdSubresource(
      &frame_or_imported_document_->GetDocument(), resource_request, type,
      known_ad);
}

base::Optional<ResourceRequestBlockedReason> FrameFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (!GetResourceFetcherProperties().IsDetached() &&
      frame_or_imported_document_->GetDocument().IsFreezingInProgress() &&
      !resource_request.GetKeepalive()) {
    AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Only fetch keepalive is allowed during onfreeze: " + url.GetString()));
    return ResourceRequestBlockedReason::kOther;
  }
  return BaseFetchContext::CanRequest(type, resource_request, url, options,
                                      reporting_policy, redirect_status);
}

CoreProbeSink* FrameFetchContext::Probe() const {
  return probe::ToCoreProbeSink(GetFrame()->GetDocument());
}

}  // namespace blink

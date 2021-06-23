// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/base_fetch_context.h"

#include "net/http/structured_headers.h"
#include "services/network/public/cpp/request_mode.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/loader/subresource_redirect_util.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace {

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

namespace blink {

absl::optional<ResourceRequestBlockedReason> BaseFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    const absl::optional<ResourceRequest::RedirectInfo>& redirect_info) const {
  absl::optional<ResourceRequestBlockedReason> blocked_reason =
      CanRequestInternal(type, resource_request, url, options,
                         reporting_disposition, redirect_info);
  if (blocked_reason &&
      reporting_disposition == ReportingDisposition::kReport) {
    DispatchDidBlockRequest(resource_request, options, blocked_reason.value(),
                            type);
  }
  return blocked_reason;
}

absl::optional<ResourceRequestBlockedReason>
BaseFetchContext::CanRequestBasedOnSubresourceFilterOnly(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    const absl::optional<ResourceRequest::RedirectInfo>& redirect_info) const {
  auto* subresource_filter = GetSubresourceFilter();
  if (subresource_filter &&
      !subresource_filter->AllowLoad(url, resource_request.GetRequestContext(),
                                     reporting_disposition)) {
    if (reporting_disposition == ReportingDisposition::kReport) {
      DispatchDidBlockRequest(resource_request, options,
                              ResourceRequestBlockedReason::kSubresourceFilter,
                              type);
    }
    return ResourceRequestBlockedReason::kSubresourceFilter;
  }

  return absl::nullopt;
}

bool BaseFetchContext::CalculateIfAdSubresource(
    const ResourceRequestHead& request,
    const absl::optional<KURL>& alias_url,
    ResourceType type,
    const FetchInitiatorInfo& initiator_info) {
  // A derived class should override this if they have more signals than just
  // the SubresourceFilter.
  SubresourceFilter* filter = GetSubresourceFilter();
  const KURL& url = alias_url ? alias_url.value() : request.Url();

  return request.IsAdResource() ||
         (filter && filter->IsAdResource(url, request.GetRequestContext()));
}

bool BaseFetchContext::SendConversionRequestInsteadOfRedirecting(
    const KURL& url,
    const absl::optional<ResourceRequest::RedirectInfo>& redirect_info,
    ReportingDisposition reporting_disposition,
    const String& devtools_request_id) const {
  return false;
}

void BaseFetchContext::AddClientHintsIfNecessary(
    const ClientHintsPreferences& hints_preferences,
    const url::Origin& resource_origin,
    bool is_1p_origin,
    absl::optional<UserAgentMetadata> ua,
    const PermissionsPolicy* policy,
    const absl::optional<ClientHintImageInfo>& image_info,
    const absl::optional<WTF::AtomicString>& lang,
    const absl::optional<WTF::AtomicString>& prefers_color_scheme,
    ResourceRequest& request) {
  // If the feature is enabled, then client hints are allowed only on secure
  // URLs.
  if (!ClientHintsPreferences::IsClientHintsAllowed(request.Url()))
    return;

  // Sec-CH-UA is special: we always send the header to all origins that are
  // eligible for client hints (e.g. secure transport, JavaScript enabled).
  //
  // https://github.com/WICG/ua-client-hints
  //
  // One exception, however, is that a custom UA is sometimes set without
  // specifying accomponying client hints, in which case we disable sending
  // them.
  if (ClientHintsPreferences::UserAgentClientHintEnabled() && ua) {
    // ShouldSendClientHint is called to make sure UA is controlled by
    // Permissions Policy.
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
    // PermissionsPolicy.
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
  // parties, or if PermissionsPolicy delegation says they are allowed.
  if (ShouldSendClientHint(
          ClientHintsMode::kLegacy, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kDeviceMemory,
          hints_preferences)) {
    request.SetHttpHeaderField(
        "Device-Memory",
        AtomicString(String::Number(
            ApproximatedDeviceMemory::GetApproximatedDeviceMemory())));
  }

  // These hints only make sense if the image info is available
  if (image_info) {
    if (ShouldSendClientHint(ClientHintsMode::kLegacy, policy, resource_origin,
                             is_1p_origin,
                             network::mojom::blink::WebClientHintsType::kDpr,
                             hints_preferences)) {
      request.SetHttpHeaderField("DPR",
                                 AtomicString(String::Number(image_info->dpr)));
    }

    if (ShouldSendClientHint(
            ClientHintsMode::kLegacy, policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kViewportWidth,
            hints_preferences) &&
        image_info->viewport_width) {
      request.SetHttpHeaderField(
          "Viewport-Width",
          AtomicString(String::Number(image_info->viewport_width.value())));
    }

    if (ShouldSendClientHint(
            ClientHintsMode::kLegacy, policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kResourceWidth,
            hints_preferences)) {
      if (image_info->resource_width.is_set) {
        float physical_width =
            image_info->resource_width.width * image_info->dpr;
        request.SetHttpHeaderField(
            "Width", AtomicString(String::Number(ceil(physical_width))));
      }
    }
  }

  if (ShouldSendClientHint(
          ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kRtt, hints_preferences)) {
    absl::optional<base::TimeDelta> http_rtt =
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
    absl::optional<double> throughput_mbps =
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
    absl::optional<WebEffectiveConnectionType> holdback_ect =
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
                           hints_preferences) &&
      lang) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kLang)],
        lang.value());
  }

  // Only send User Agent hints if the info is available
  if (ClientHintsPreferences::UserAgentClientHintEnabled() && ua) {
    if (ShouldSendClientHint(ClientHintsMode::kStandard, policy,
                             resource_origin, is_1p_origin,
                             network::mojom::blink::WebClientHintsType::kUAArch,
                             hints_preferences)) {
      request.SetHttpHeaderField(
          blink::kClientHintsHeaderMapping[static_cast<size_t>(
              network::mojom::blink::WebClientHintsType::kUAArch)],
          SerializeHeaderString(ua->architecture));
    }

    if (ShouldSendClientHint(
            ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAPlatform,
            hints_preferences)) {
      request.SetHttpHeaderField(
          blink::kClientHintsHeaderMapping[static_cast<size_t>(
              network::mojom::blink::WebClientHintsType::kUAPlatform)],
          SerializeHeaderString(ua->platform));
    }

    if (ShouldSendClientHint(
            ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAPlatformVersion,
            hints_preferences)) {
      request.SetHttpHeaderField(
          blink::kClientHintsHeaderMapping[static_cast<size_t>(
              network::mojom::blink::WebClientHintsType::kUAPlatformVersion)],
          SerializeHeaderString(ua->platform_version));
    }

    if (ShouldSendClientHint(
            ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAModel,
            hints_preferences)) {
      request.SetHttpHeaderField(
          blink::kClientHintsHeaderMapping[static_cast<size_t>(
              network::mojom::blink::WebClientHintsType::kUAModel)],
          SerializeHeaderString(ua->model));
    }

    if (ShouldSendClientHint(
            ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAFullVersion,
            hints_preferences)) {
      request.SetHttpHeaderField(
          blink::kClientHintsHeaderMapping[static_cast<size_t>(
              network::mojom::blink::WebClientHintsType::kUAFullVersion)],
          SerializeHeaderString(ua->full_version));
    }

    if (ShouldSendClientHint(
            ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUABitness,
            hints_preferences)) {
      request.SetHttpHeaderField(
          blink::kClientHintsHeaderMapping[static_cast<size_t>(
              network::mojom::blink::WebClientHintsType::kUABitness)],
          SerializeHeaderString(ua->bitness));
    }
  }

  if (ShouldSendClientHint(
          ClientHintsMode::kStandard, policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kPrefersColorScheme,
          hints_preferences) &&
      prefers_color_scheme) {
    request.SetHttpHeaderField(
        blink::kClientHintsHeaderMapping[static_cast<size_t>(
            network::mojom::blink::WebClientHintsType::kPrefersColorScheme)],
        prefers_color_scheme.value());
  }
}

void BaseFetchContext::PrintAccessDeniedMessage(const KURL& url) const {
  if (url.IsNull())
    return;

  String message;
  if (Url().IsNull()) {
    message = "Unsafe attempt to load URL " + url.ElidedString() + '.';
  } else if (url.IsLocalFile() || Url().IsLocalFile()) {
    message = "Unsafe attempt to load URL " + url.ElidedString() +
              " from frame with URL " + Url().ElidedString() +
              ". 'file:' URLs are treated as unique security origins.\n";
  } else {
    message = "Unsafe attempt to load URL " + url.ElidedString() +
              " from frame with URL " + Url().ElidedString() +
              ". Domains, protocols and ports must match.\n";
  }

  AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, message));
}

absl::optional<ResourceRequestBlockedReason>
BaseFetchContext::CheckCSPForRequest(
    mojom::blink::RequestContextType request_context,
    network::mojom::RequestDestination request_destination,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status) const {
  return CheckCSPForRequestInternal(
      request_context, request_destination, url, options, reporting_disposition,
      url_before_redirects, redirect_status,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly);
}

absl::optional<ResourceRequestBlockedReason>
BaseFetchContext::CheckCSPForRequestInternal(
    mojom::blink::RequestContextType request_context,
    network::mojom::RequestDestination request_destination,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    const KURL& url_before_redirects,
    ResourceRequest::RedirectStatus redirect_status,
    ContentSecurityPolicy::CheckHeaderType check_header_type) const {
  if (options.content_security_policy_option ==
      network::mojom::CSPDisposition::DO_NOT_CHECK) {
    return absl::nullopt;
  }

  if (ShouldDisableCSPCheckForLitePageSubresourceRedirectOrigin(
          GetResourceFetcherProperties().GetLitePageSubresourceRedirectOrigin(),
          request_context, redirect_status, url)) {
    return absl::nullopt;
  }

  ContentSecurityPolicy* csp =
      GetContentSecurityPolicyForWorld(options.world_for_csp.get());
  if (csp &&
      !csp->AllowRequest(request_context, request_destination, url,
                         options.content_security_policy_nonce,
                         options.integrity_metadata, options.parser_disposition,
                         url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type)) {
    return ResourceRequestBlockedReason::kCSP;
  }
  return absl::nullopt;
}

absl::optional<ResourceRequestBlockedReason>
BaseFetchContext::CanRequestInternal(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    const absl::optional<ResourceRequest::RedirectInfo>& redirect_info) const {
  if (GetResourceFetcherProperties().IsDetached()) {
    if (!resource_request.GetKeepalive() || !redirect_info) {
      return ResourceRequestBlockedReason::kOther;
    }
  }

  if (ShouldBlockRequestByInspector(resource_request.Url()))
    return ResourceRequestBlockedReason::kInspector;

  scoped_refptr<const SecurityOrigin> origin =
      resource_request.RequestorOrigin();

  const auto request_mode = resource_request.GetMode();
  // On navigation cases, Context().GetSecurityOrigin() may return nullptr, so
  // the request's origin may be nullptr.
  // TODO(yhirano): Figure out if it's actually fine.
  DCHECK(request_mode == network::mojom::RequestMode::kNavigate || origin);
  if (request_mode != network::mojom::RequestMode::kNavigate &&
      !resource_request.CanDisplay(url)) {
    if (reporting_disposition == ReportingDisposition::kReport) {
      AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kError,
          "Not allowed to load local resource: " + url.GetString()));
    }
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::requestResource URL was not "
                                 "allowed by SecurityOrigin::CanDisplay";
    return ResourceRequestBlockedReason::kOther;
  }

  if (request_mode == network::mojom::RequestMode::kSameOrigin &&
      cors::CalculateCorsFlag(url, origin.get(),
                              resource_request.IsolatedWorldOrigin().get(),
                              request_mode)) {
    PrintAccessDeniedMessage(url);
    return ResourceRequestBlockedReason::kOrigin;
  }

  // User Agent CSS stylesheets should only support loading images and should be
  // restricted to data urls.
  if (options.initiator_info.name == fetch_initiator_type_names::kUacss) {
    if (type == ResourceType::kImage && url.ProtocolIsData()) {
      return absl::nullopt;
    }
    return ResourceRequestBlockedReason::kOther;
  }

  mojom::blink::RequestContextType request_context =
      resource_request.GetRequestContext();
  network::mojom::RequestDestination request_destination =
      resource_request.GetRequestDestination();

  const KURL& url_before_redirects =
      redirect_info ? redirect_info->original_url : url;
  const ResourceRequestHead::RedirectStatus redirect_status =
      redirect_info ? ResourceRequestHead::RedirectStatus::kFollowedRedirect
                    : ResourceRequestHead::RedirectStatus::kNoRedirect;
  // We check the 'report-only' headers before upgrading the request (in
  // populateResourceRequest). We check the enforced headers here to ensure we
  // block things we ought to block.
  if (CheckCSPForRequestInternal(
          request_context, request_destination, url, options,
          reporting_disposition, url_before_redirects, redirect_status,
          ContentSecurityPolicy::CheckHeaderType::kCheckEnforce) ==
      ResourceRequestBlockedReason::kCSP) {
    return ResourceRequestBlockedReason::kCSP;
  }

  if (type == ResourceType::kScript) {
    if (!AllowScriptFromSource(url)) {
      // TODO(estark): Use a different ResourceRequestBlockedReason here, since
      // this check has nothing to do with CSP. https://crbug.com/600795
      return ResourceRequestBlockedReason::kCSP;
    }
  }

  // SVG Images have unique security rules that prevent all subresource requests
  // except for data urls.
  if (IsSVGImageChromeClient() && !url.ProtocolIsData())
    return ResourceRequestBlockedReason::kOrigin;

  // Measure the number of legacy URL schemes ('ftp://') and the number of
  // embedded-credential ('http://user:password@...') resources embedded as
  // subresources.
  const FetchClientSettingsObject& fetch_client_settings_object =
      GetResourceFetcherProperties().GetFetchClientSettingsObject();
  const SecurityOrigin* embedding_origin =
      fetch_client_settings_object.GetSecurityOrigin();
  DCHECK(embedding_origin);
  if (SchemeRegistry::ShouldTreatURLSchemeAsLegacy(url.Protocol()) &&
      !SchemeRegistry::ShouldTreatURLSchemeAsLegacy(
          embedding_origin->Protocol())) {
    CountDeprecation(WebFeature::kLegacyProtocolEmbeddedAsSubresource);

    return ResourceRequestBlockedReason::kOrigin;
  }

  if (ShouldBlockFetchAsCredentialedSubresource(resource_request, url))
    return ResourceRequestBlockedReason::kOrigin;

  // Check for mixed content. We do this second-to-last so that when folks block
  // mixed content via CSP, they don't get a mixed content warning, but a CSP
  // warning instead.
  if (ShouldBlockFetchByMixedContentCheck(request_context, redirect_info, url,
                                          reporting_disposition,
                                          resource_request.GetDevToolsId())) {
    return ResourceRequestBlockedReason::kMixedContent;
  }

  if (url.PotentiallyDanglingMarkup() && url.ProtocolIsInHTTPFamily()) {
    CountDeprecation(WebFeature::kCanRequestURLHTTPContainingNewline);
    return ResourceRequestBlockedReason::kOther;
  }

  // Redirect `ResourceRequest`s don't have a DevToolsId set, but are
  // associated with the requestId of the initial request. The right
  // DevToolsId needs to be resolved via the InspectorId.
  const String devtools_request_id =
      IdentifiersFactory::RequestId(nullptr, resource_request.InspectorId());
  if (SendConversionRequestInsteadOfRedirecting(
          url, redirect_info, reporting_disposition, devtools_request_id)) {
    return ResourceRequestBlockedReason::kConversionRequest;
  }

  // Let the client have the final say into whether or not the load should
  // proceed.
  if (GetSubresourceFilter()) {
    if (!GetSubresourceFilter()->AllowLoad(url, request_context,
                                           reporting_disposition)) {
      return ResourceRequestBlockedReason::kSubresourceFilter;
    }
  }

  return absl::nullopt;
}

bool BaseFetchContext::ShouldSendClientHint(
    ClientHintsMode mode,
    const PermissionsPolicy* policy,
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
        (policy &&
         policy->IsFeatureEnabledForOrigin(
             kClientHintsPermissionsPolicyMapping[static_cast<int>(type)],
             resource_origin));
  } else {
    origin_ok = is_1p_origin;
  }

  if (!origin_ok)
    return false;

  return IsClientHintSentByDefault(type) || hints_preferences.ShouldSend(type);
}

void BaseFetchContext::AddBackForwardCacheExperimentHTTPHeaderIfNeeded(
    ExecutionContext* context,
    ResourceRequest& request) {
  if (!RuntimeEnabledFeatures::BackForwardCacheExperimentHTTPHeaderEnabled(
          context)) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kBackForwardCacheABExperimentControl)) {
    return;
  }
  // Send the 'Sec-bfcache-experiment' HTTP header to indicate which
  // BackForwardCacheSameSite experiment group we're in currently.
  UseCounter::Count(context, WebFeature::kBackForwardCacheExperimentHTTPHeader);
  auto experiment_group = base::GetFieldTrialParamValueByFeature(
      features::kBackForwardCacheABExperimentControl,
      features::kBackForwardCacheABExperimentGroup);
  request.SetHttpHeaderField("Sec-bfcache-experiment",
                             experiment_group.c_str());
}

void BaseFetchContext::Trace(Visitor* visitor) const {
  visitor->Trace(fetcher_properties_);
  FetchContext::Trace(visitor);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/base_fetch_context.h"

#include "net/http/structured_headers.h"
#include "services/network/public/cpp/client_hints.h"
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
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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

// Creates a serialized AtomicString header value out of the input string, using
// structured headers as described in
// https://www.rfc-editor.org/rfc/rfc8941.html.
const AtomicString SerializeStringHeader(std::string str) {
  std::string output;
  if (!str.empty()) {
    output = net::structured_headers::SerializeItem(
                 net::structured_headers::Item(str))
                 .value_or(std::string());
  }

  return AtomicString(output.c_str());
}

// Creates a serialized AtomicString header value out of the input boolean,
// using structured headers as described in
// https://www.rfc-editor.org/rfc/rfc8941.html.
const AtomicString SerializeBoolHeader(const bool value) {
  const std::string output = net::structured_headers::SerializeItem(
                                 net::structured_headers::Item(value))
                                 .value_or(std::string());

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

void BaseFetchContext::AddClientHintsIfNecessary(
    const ClientHintsPreferences& hints_preferences,
    const url::Origin& resource_origin,
    bool is_1p_origin,
    absl::optional<UserAgentMetadata> ua,
    const PermissionsPolicy* policy,
    const absl::optional<ClientHintImageInfo>& image_info,
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
  if (RuntimeEnabledFeatures::UserAgentClientHintEnabled() && ua) {
    // ShouldSendClientHint is called to make sure UA is controlled by
    // Permissions Policy.
    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             network::mojom::blink::WebClientHintsType::kUA,
                             hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUA)
              .c_str(),
          ua->SerializeBrandMajorVersionList().c_str());
    }

    // We also send Sec-CH-UA-Mobile to all hints. It is a one-bit header
    // identifying if the browser has opted for a "mobile" experience.
    // ShouldSendClientHint is called to make sure it's controlled by
    // PermissionsPolicy.
    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAMobile,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAMobile)
              .c_str(),
          SerializeBoolHeader(ua->mobile));
    }
  }

  // If the frame is detached, then don't send any hints other than UA.
  if (!policy)
    return;

  // The next 4 hints should be enabled if we're allowing legacy hints to third
  // parties, or if PermissionsPolicy delegation says they are allowed.
  if (ShouldSendClientHint(
          policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kDeviceMemory_DEPRECATED,
          hints_preferences)) {
    request.SetHttpHeaderField(
        network::GetClientHintToNameMap()
            .at(network::mojom::blink::WebClientHintsType::
                    kDeviceMemory_DEPRECATED)
            .c_str(),
        AtomicString(String::Number(
            ApproximatedDeviceMemory::GetApproximatedDeviceMemory())));
  }

  if (ShouldSendClientHint(
          policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kDeviceMemory,
          hints_preferences)) {
    request.SetHttpHeaderField(
        network::GetClientHintToNameMap()
            .at(network::mojom::blink::WebClientHintsType::kDeviceMemory)
            .c_str(),
        AtomicString(String::Number(
            ApproximatedDeviceMemory::GetApproximatedDeviceMemory())));
  }

  // These hints only make sense if the image info is available
  if (image_info) {
    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kDpr_DEPRECATED,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kDpr_DEPRECATED)
              .c_str(),
          AtomicString(String::Number(image_info->dpr)));
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             network::mojom::blink::WebClientHintsType::kDpr,
                             hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kDpr)
              .c_str(),
          AtomicString(String::Number(image_info->dpr)));
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             network::mojom::blink::WebClientHintsType::
                                 kViewportWidth_DEPRECATED,
                             hints_preferences) &&
        image_info->viewport_width) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::
                      kViewportWidth_DEPRECATED)
              .c_str(),
          AtomicString(String::Number(image_info->viewport_width.value())));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kViewportWidth,
            hints_preferences) &&
        image_info->viewport_width) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kViewportWidth)
              .c_str(),
          AtomicString(String::Number(image_info->viewport_width.value())));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kViewportHeight,
            hints_preferences) &&
        image_info->viewport_height) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kViewportHeight)
              .c_str(),
          AtomicString(String::Number(image_info->viewport_height.value())));
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             network::mojom::blink::WebClientHintsType::
                                 kResourceWidth_DEPRECATED,
                             hints_preferences)) {
      if (image_info->resource_width.is_set) {
        float physical_width =
            image_info->resource_width.width * image_info->dpr;
        request.SetHttpHeaderField(
            network::GetClientHintToNameMap()
                .at(network::mojom::blink::WebClientHintsType::
                        kResourceWidth_DEPRECATED)
                .c_str(),
            AtomicString(String::Number(ceil(physical_width))));
      }
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kResourceWidth,
            hints_preferences)) {
      if (image_info->resource_width.is_set) {
        float physical_width =
            image_info->resource_width.width * image_info->dpr;
        request.SetHttpHeaderField(
            network::GetClientHintToNameMap()
                .at(network::mojom::blink::WebClientHintsType::kResourceWidth)
                .c_str(),
            AtomicString(String::Number(ceil(physical_width))));
      }
    }
  }

  if (ShouldSendClientHint(
          policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kRtt_DEPRECATED,
          hints_preferences)) {
    absl::optional<base::TimeDelta> http_rtt =
        GetNetworkStateNotifier().GetWebHoldbackHttpRtt();
    if (!http_rtt) {
      http_rtt = GetNetworkStateNotifier().HttpRtt();
    }

    uint32_t rtt =
        GetNetworkStateNotifier().RoundRtt(request.Url().Host(), http_rtt);
    request.SetHttpHeaderField(
        network::GetClientHintToNameMap()
            .at(network::mojom::blink::WebClientHintsType::kRtt_DEPRECATED)
            .c_str(),
        AtomicString(String::Number(rtt)));
  }

  if (ShouldSendClientHint(
          policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kDownlink_DEPRECATED,
          hints_preferences)) {
    absl::optional<double> throughput_mbps =
        GetNetworkStateNotifier().GetWebHoldbackDownlinkThroughputMbps();
    if (!throughput_mbps) {
      throughput_mbps = GetNetworkStateNotifier().DownlinkThroughputMbps();
    }

    double mbps = GetNetworkStateNotifier().RoundMbps(request.Url().Host(),
                                                      throughput_mbps);
    request.SetHttpHeaderField(
        network::GetClientHintToNameMap()
            .at(network::mojom::blink::WebClientHintsType::kDownlink_DEPRECATED)
            .c_str(),
        AtomicString(String::Number(mbps)));
  }

  if (ShouldSendClientHint(
          policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kEct_DEPRECATED,
          hints_preferences)) {
    absl::optional<WebEffectiveConnectionType> holdback_ect =
        GetNetworkStateNotifier().GetWebHoldbackEffectiveType();
    if (!holdback_ect)
      holdback_ect = GetNetworkStateNotifier().EffectiveType();

    request.SetHttpHeaderField(
        network::GetClientHintToNameMap()
            .at(network::mojom::blink::WebClientHintsType::kEct_DEPRECATED)
            .c_str(),
        AtomicString(NetworkStateNotifier::EffectiveConnectionTypeToString(
            holdback_ect.value())));
  }

  // Only send User Agent hints if the info is available
  if (RuntimeEnabledFeatures::UserAgentClientHintEnabled() && ua) {
    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             network::mojom::blink::WebClientHintsType::kUAArch,
                             hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAArch)
              .c_str(),
          SerializeStringHeader(ua->architecture));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAPlatform,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAPlatform)
              .c_str(),
          SerializeStringHeader(ua->platform));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAPlatformVersion,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAPlatformVersion)
              .c_str(),
          SerializeStringHeader(ua->platform_version));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAModel,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAModel)
              .c_str(),
          SerializeStringHeader(ua->model));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAFullVersion,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAFullVersion)
              .c_str(),
          SerializeStringHeader(ua->full_version));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAFullVersionList,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAFullVersionList)
              .c_str(),
          ua->SerializeBrandFullVersionList().c_str());
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUABitness,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUABitness)
              .c_str(),
          SerializeStringHeader(ua->bitness));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAWoW64,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAWoW64)
              .c_str(),
          SerializeBoolHeader(ua->wow64));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAReduced,
            hints_preferences)) {
      // If the UA-Reduced client hint should be sent according to the hints
      // preferences, it means the Origin Trial token for User-Agent Reduction
      // has already been validated.
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kUAReduced)
              .c_str(),
          SerializeBoolHeader(true));
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kFullUserAgent,
            hints_preferences)) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kFullUserAgent)
              .c_str(),
          SerializeBoolHeader(true));
    }
  }

  if (ShouldSendClientHint(
          policy, resource_origin, is_1p_origin,
          network::mojom::blink::WebClientHintsType::kPrefersColorScheme,
          hints_preferences) &&
      prefers_color_scheme) {
    request.SetHttpHeaderField(
        network::GetClientHintToNameMap()
            .at(network::mojom::blink::WebClientHintsType::kPrefersColorScheme)
            .c_str(),
        prefers_color_scheme.value());
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           network::mojom::blink::WebClientHintsType::kSaveData,
                           hints_preferences)) {
    if (GetNetworkStateNotifier().SaveDataEnabled()) {
      request.SetHttpHeaderField(
          network::GetClientHintToNameMap()
              .at(network::mojom::blink::WebClientHintsType::kSaveData)
              .c_str(),
          "on");
    }
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

  // Measure the number of embedded-credential ('http://user:password@...')
  // resources embedded as subresources.
  const FetchClientSettingsObject& fetch_client_settings_object =
      GetResourceFetcherProperties().GetFetchClientSettingsObject();
  const SecurityOrigin* embedding_origin =
      fetch_client_settings_object.GetSecurityOrigin();
  DCHECK(embedding_origin);
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
    const PermissionsPolicy* policy,
    const url::Origin& resource_origin,
    bool is_1p_origin,
    network::mojom::blink::WebClientHintsType type,
    const ClientHintsPreferences& hints_preferences) const {
  // For subresource requests, if the parent frame has Sec-CH-UA-Reduced,
  // Sec-CH-UA-Full, or Sec-CH-Partitioned-Cookies, then send the hint in the
  // fetch request, regardless of the permissions policy.
  if (type != network::mojom::blink::WebClientHintsType::kUAReduced &&
      type != network::mojom::blink::WebClientHintsType::kFullUserAgent &&
      (!policy ||
       !policy->IsFeatureEnabledForOrigin(
           GetClientHintToPolicyFeatureMap().at(type), resource_origin))) {
    return false;
  }

  return IsClientHintSentByDefault(type) || hints_preferences.ShouldSend(type);
}

void BaseFetchContext::AddBackForwardCacheExperimentHTTPHeaderIfNeeded(
    ResourceRequest& request) {
  if (!RuntimeEnabledFeatures::BackForwardCacheExperimentHTTPHeaderEnabled(
          GetExecutionContext())) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kBackForwardCacheABExperimentControl)) {
    return;
  }
  // Send the 'Sec-bfcache-experiment' HTTP header to indicate which
  // BackForwardCacheSameSite experiment group we're in currently.
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kBackForwardCacheExperimentHTTPHeader);
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

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/base_fetch_context.h"

#include "base/command_line.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/request_mode.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.h"
#include "third_party/blink/renderer/core/loader/idna_util.h"
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

// Creates a serialized AtomicString header value out of the input boolean,
// using structured headers as described in
// https://www.rfc-editor.org/rfc/rfc8941.html.
const AtomicString SerializeBoolHeader(const bool value) {
  const std::string output = net::structured_headers::SerializeItem(
                                 net::structured_headers::Item(value))
                                 .value_or(std::string());

  return AtomicString(output.c_str());
}

void SetHttpHeader(network::mojom::blink::WebClientHintsType hints_type,
                   const AtomicString& value,
                   blink::ResourceRequest& request) {
  std::string header_name = network::GetClientHintToNameMap().at(hints_type);
  request.SetHttpHeaderField(
      AtomicString(reinterpret_cast<const LChar*>(header_name.data()),
                   header_name.size()),
      value);
}

}  // namespace

namespace blink {

absl::optional<ResourceRequestBlockedReason> BaseFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info)
    const {
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
    base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info)
    const {
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
    base::optional_ref<const KURL> alias_url,
    ResourceType type,
    const FetchInitiatorInfo& initiator_info) {
  // A derived class should override this if they have more signals than just
  // the SubresourceFilter.
  SubresourceFilter* filter = GetSubresourceFilter();
  const KURL& url = alias_url.has_value() ? alias_url.value() : request.Url();

  return request.IsAdResource() ||
         (filter && filter->IsAdResource(url, request.GetRequestContext()));
}

// TODO(https://crbug.com/1469830) Refactor the strings into some sort of
// context object
void BaseFetchContext::AddClientHintsIfNecessary(
    const ClientHintsPreferences& hints_preferences,
    const url::Origin& resource_origin,
    bool is_1p_origin,
    absl::optional<UserAgentMetadata> ua,
    const PermissionsPolicy* policy,
    base::optional_ref<const ClientHintImageInfo> image_info,
    base::optional_ref<const WTF::AtomicString> prefers_color_scheme,
    base::optional_ref<const WTF::AtomicString> prefers_reduced_motion,
    base::optional_ref<const WTF::AtomicString> prefers_reduced_transparency,
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
  using network::mojom::blink::WebClientHintsType;
  if (RuntimeEnabledFeatures::UserAgentClientHintEnabled() && ua) {
    // ShouldSendClientHint is called to make sure UA is controlled by
    // Permissions Policy.
    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUA, hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUA,
                    AtomicString(ua->SerializeBrandMajorVersionList().c_str()),
                    request);
    }

    // We also send Sec-CH-UA-Mobile to all hints. It is a one-bit header
    // identifying if the browser has opted for a "mobile" experience.
    // ShouldSendClientHint is called to make sure it's controlled by
    // PermissionsPolicy.
    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAMobile,
                             hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAMobile,
                    SerializeBoolHeader(ua->mobile), request);
    }
  }

  // If the frame is detached, then don't send any hints other than UA.
  if (!policy)
    return;

  // The next 4 hints should be enabled if we're allowing legacy hints to third
  // parties, or if PermissionsPolicy delegation says they are allowed.
  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kDeviceMemory_DEPRECATED,
                           hints_preferences)) {
    SetHttpHeader(WebClientHintsType::kDeviceMemory_DEPRECATED,
                  AtomicString(String::Number(
                      ApproximatedDeviceMemory::GetApproximatedDeviceMemory())),
                  request);
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kDeviceMemory,
                           hints_preferences)) {
    SetHttpHeader(WebClientHintsType::kDeviceMemory,
                  AtomicString(String::Number(
                      ApproximatedDeviceMemory::GetApproximatedDeviceMemory())),
                  request);
  }

  // These hints only make sense if the image info is available
  if (image_info.has_value()) {
    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kDpr_DEPRECATED,
                             hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kDpr_DEPRECATED,
                    AtomicString(String::Number(image_info->dpr)), request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kDpr, hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kDpr,
                    AtomicString(String::Number(image_info->dpr)), request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kViewportWidth_DEPRECATED,
                             hints_preferences) &&
        image_info->viewport_width) {
      SetHttpHeader(
          WebClientHintsType::kViewportWidth_DEPRECATED,
          AtomicString(String::Number(image_info->viewport_width.value())),
          request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kViewportWidth,
                             hints_preferences) &&
        image_info->viewport_width) {
      SetHttpHeader(
          WebClientHintsType::kViewportWidth,
          AtomicString(String::Number(image_info->viewport_width.value())),
          request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kViewportHeight,
                             hints_preferences) &&
        image_info->viewport_height) {
      SetHttpHeader(
          WebClientHintsType::kViewportHeight,
          AtomicString(String::Number(image_info->viewport_height.value())),
          request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kResourceWidth_DEPRECATED,
                             hints_preferences)) {
      if (image_info->resource_width) {
        float physical_width =
            image_info->resource_width.value() * image_info->dpr;
        SetHttpHeader(WebClientHintsType::kResourceWidth_DEPRECATED,
                      AtomicString(String::Number(ceil(physical_width))),
                      request);
      }
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kResourceWidth,
                             hints_preferences)) {
      if (image_info->resource_width) {
        float physical_width =
            image_info->resource_width.value() * image_info->dpr;
        SetHttpHeader(WebClientHintsType::kResourceWidth,
                      AtomicString(String::Number(ceil(physical_width))),
                      request);
      }
    }
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kRtt_DEPRECATED,
                           hints_preferences)) {
    absl::optional<base::TimeDelta> http_rtt =
        GetNetworkStateNotifier().GetWebHoldbackHttpRtt();
    if (!http_rtt) {
      http_rtt = GetNetworkStateNotifier().HttpRtt();
    }

    uint32_t rtt =
        GetNetworkStateNotifier().RoundRtt(request.Url().Host(), http_rtt);
    SetHttpHeader(WebClientHintsType::kRtt_DEPRECATED,
                  AtomicString(String::Number(rtt)), request);
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kDownlink_DEPRECATED,
                           hints_preferences)) {
    absl::optional<double> throughput_mbps =
        GetNetworkStateNotifier().GetWebHoldbackDownlinkThroughputMbps();
    if (!throughput_mbps) {
      throughput_mbps = GetNetworkStateNotifier().DownlinkThroughputMbps();
    }

    double mbps = GetNetworkStateNotifier().RoundMbps(request.Url().Host(),
                                                      throughput_mbps);
    SetHttpHeader(WebClientHintsType::kDownlink_DEPRECATED,
                  AtomicString(String::Number(mbps)), request);
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kEct_DEPRECATED,
                           hints_preferences)) {
    absl::optional<WebEffectiveConnectionType> holdback_ect =
        GetNetworkStateNotifier().GetWebHoldbackEffectiveType();
    if (!holdback_ect)
      holdback_ect = GetNetworkStateNotifier().EffectiveType();

    SetHttpHeader(
        WebClientHintsType::kEct_DEPRECATED,
        AtomicString(NetworkStateNotifier::EffectiveConnectionTypeToString(
            holdback_ect.value())),
        request);
  }

  // Only send User Agent hints if the info is available
  if (RuntimeEnabledFeatures::UserAgentClientHintEnabled() && ua) {
    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAArch, hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAArch,
                    SerializeStringHeader(ua->architecture), request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAPlatform,
                             hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAPlatform,
                    SerializeStringHeader(ua->platform), request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAPlatformVersion,
                             hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAPlatformVersion,
                    SerializeStringHeader(ua->platform_version), request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAModel, hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAModel,
                    SerializeStringHeader(ua->model), request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAFullVersion,
                             hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAFullVersion,
                    SerializeStringHeader(ua->full_version), request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAFullVersionList,
                             hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAFullVersionList,
                    AtomicString(ua->SerializeBrandFullVersionList().c_str()),
                    request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUABitness,
                             hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUABitness,
                    SerializeStringHeader(ua->bitness), request);
    }

    if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                             WebClientHintsType::kUAWoW64, hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAWoW64,
                    SerializeBoolHeader(ua->wow64), request);
    }

    if (ShouldSendClientHint(
            policy, resource_origin, is_1p_origin,
            network::mojom::blink::WebClientHintsType::kUAFormFactor,
            hints_preferences)) {
      SetHttpHeader(WebClientHintsType::kUAFormFactor,
                    AtomicString(ua->SerializeFormFactor().c_str()), request);
    }
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kPrefersColorScheme,
                           hints_preferences) &&
      prefers_color_scheme.has_value()) {
    SetHttpHeader(WebClientHintsType::kPrefersColorScheme,
                  prefers_color_scheme.value(), request);
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kSaveData, hints_preferences)) {
    if (GetNetworkStateNotifier().SaveDataEnabled()) {
      SetHttpHeader(WebClientHintsType::kSaveData, AtomicString("on"), request);
    }
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kPrefersReducedMotion,
                           hints_preferences) &&
      prefers_reduced_motion.has_value()) {
    SetHttpHeader(WebClientHintsType::kPrefersReducedMotion,
                  prefers_reduced_motion.value(), request);
  }

  if (ShouldSendClientHint(policy, resource_origin, is_1p_origin,
                           WebClientHintsType::kPrefersReducedTransparency,
                           hints_preferences) &&
      prefers_reduced_transparency.has_value()) {
    SetHttpHeader(WebClientHintsType::kPrefersReducedTransparency,
                  prefers_reduced_transparency.value(), request);
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

  console_logger_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
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
    base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info)
    const {
  if (GetResourceFetcherProperties().IsDetached()) {
    if (!resource_request.GetKeepalive() || !redirect_info.has_value()) {
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
      console_logger_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
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
      redirect_info.has_value() ? redirect_info->original_url : url;
  const ResourceRequestHead::RedirectStatus redirect_status =
      redirect_info.has_value()
          ? ResourceRequestHead::RedirectStatus::kFollowedRedirect
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

  // data: URL is deprecated in SVGUseElement.
  if (RuntimeEnabledFeatures::RemoveDataUrlInSvgUseEnabled() &&
      options.initiator_info.name == fetch_initiator_type_names::kUse &&
      url.ProtocolIsData() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          blink::switches::kDataUrlInSvgUseEnabled)) {
    PrintAccessDeniedMessage(url);
    return ResourceRequestBlockedReason::kOrigin;
  }

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
  if (ShouldBlockFetchByMixedContentCheck(
          request_context, resource_request.GetTargetAddressSpace(),
          redirect_info, url, reporting_disposition,
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

  // Warn if the resource URL's hostname contains IDNA deviation characters.
  // Only warn if the resource URL's origin is different than its requestor
  // (we don't want to warn for <img src="faß.de/image.img"> on faß.de).
  // TODO(crbug.com/1396475): Remove once Non-Transitional mode is shipped.
  if (!resource_request.RequestorOrigin()->IsSameOriginWith(
          SecurityOrigin::Create(url).get()) &&
      url.HasIDNA2008DeviationCharacter()) {
    String message = GetConsoleWarningForIDNADeviationCharacters(url);
    if (!message.empty()) {
      console_logger_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kSecurity,
          mojom::ConsoleMessageLevel::kWarning, message));
      UseCounter::Count(
          GetExecutionContext(),
          WebFeature::kIDNA2008DeviationCharacterInHostnameOfSubresource);
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
  // For subresource requests, sending the hint in the fetch request based on
  // the permissions policy.
  if ((!policy ||
       !policy->IsFeatureEnabledForOrigin(
           GetClientHintToPolicyFeatureMap().at(type), resource_origin))) {
    return false;
  }

  return IsClientHintSentByDefault(type) || hints_preferences.ShouldSend(type);
}

void BaseFetchContext::Trace(Visitor* visitor) const {
  visitor->Trace(fetcher_properties_);
  visitor->Trace(console_logger_);
  FetchContext::Trace(visitor);
}

}  // namespace blink

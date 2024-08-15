// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/base_fetch_context.h"

#include "base/command_line.h"
#include "services/network/public/cpp/request_mode.h"
#include "third_party/blink/public/common/features.h"
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

namespace blink {

std::optional<ResourceRequestBlockedReason> BaseFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info)
    const {
  std::optional<ResourceRequestBlockedReason> blocked_reason =
      CanRequestInternal(type, resource_request, url, options,
                         reporting_disposition, redirect_info);
  if (blocked_reason &&
      reporting_disposition == ReportingDisposition::kReport) {
    DispatchDidBlockRequest(resource_request, options, blocked_reason.value(),
                            type);
  }
  return blocked_reason;
}

std::optional<ResourceRequestBlockedReason>
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

  return std::nullopt;
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

std::optional<ResourceRequestBlockedReason>
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

std::optional<ResourceRequestBlockedReason>
BaseFetchContext::CheckAndEnforceCSPForRequest(
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
      ContentSecurityPolicy::CheckHeaderType::kCheckAll);
}

std::optional<ResourceRequestBlockedReason>
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
    return std::nullopt;
  }

  ContentSecurityPolicy* csp =
      GetContentSecurityPolicyForWorld(options.world_for_csp.Get());
  if (csp &&
      !csp->AllowRequest(request_context, request_destination, url,
                         options.content_security_policy_nonce,
                         options.integrity_metadata, options.parser_disposition,
                         url_before_redirects, redirect_status,
                         reporting_disposition, check_header_type)) {
    return ResourceRequestBlockedReason::kCSP;
  }
  return std::nullopt;
}

std::optional<ResourceRequestBlockedReason>
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

  if (!(base::FeatureList::IsEnabled(features::kOptimizeLoadingDataUrls) &&
        url.ProtocolIsData())) {
    // CORS is defined only for HTTP(S) requests. See
    // https://fetch.spec.whatwg.org/#http-extensions.
    if (request_mode == network::mojom::RequestMode::kSameOrigin &&
        cors::CalculateCorsFlag(url, origin.get(),
                                resource_request.IsolatedWorldOrigin().get(),
                                request_mode)) {
      PrintAccessDeniedMessage(url);
      return ResourceRequestBlockedReason::kOrigin;
    }
  }

  // User Agent CSS stylesheets should only support loading images and should be
  // restricted to data urls.
  if (options.initiator_info.name == fetch_initiator_type_names::kUacss) {
    if (type == ResourceType::kImage && url.ProtocolIsData()) {
      return std::nullopt;
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
    if (!AllowScript()) {
      // TODO(estark): Use a different ResourceRequestBlockedReason here, since
      // this check has nothing to do with CSP. https://crbug.com/600795
      return ResourceRequestBlockedReason::kCSP;
    }
  }

  // SVG images/resource documents have unique security rules that prevent all
  // subresource requests except for data urls.
  if (IsIsolatedSVGChromeClient() && !url.ProtocolIsData())
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

  // Nothing below this point applies to data: URL images.
  if (base::FeatureList::IsEnabled(features::kOptimizeLoadingDataUrls) &&
      type == ResourceType::kImage && url.ProtocolIsData()) {
    return std::nullopt;
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
  if (url.HasIDNA2008DeviationCharacter() &&
      !resource_request.RequestorOrigin()->IsSameOriginWith(
          SecurityOrigin::Create(url).get())) {
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

  return std::nullopt;
}

void BaseFetchContext::Trace(Visitor* visitor) const {
  visitor->Trace(fetcher_properties_);
  visitor->Trace(console_logger_);
  FetchContext::Trace(visitor);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/base_fetch_context.h"

#include "services/network/public/cpp/request_mode.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"
#include "third_party/blink/renderer/core/loader/private/frame_client_hints_preferences_context.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

base::Optional<ResourceRequestBlockedReason> BaseFetchContext::CanRequest(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  base::Optional<ResourceRequestBlockedReason> blocked_reason =
      CanRequestInternal(type, resource_request, url, options, reporting_policy,
                         redirect_status);
  if (blocked_reason &&
      reporting_policy == SecurityViolationReportingPolicy::kReport) {
    DispatchDidBlockRequest(resource_request, options.initiator_info,
                            blocked_reason.value(), type);
  }
  return blocked_reason;
}

bool BaseFetchContext::CalculateIfAdSubresource(const ResourceRequest& request,
                                                ResourceType type) {
  // A base class should override this is they have more signals than just the
  // SubresourceFilter.
  SubresourceFilter* filter = GetSubresourceFilter();

  return request.IsAdResource() ||
         (filter &&
          filter->IsAdResource(request.Url(), request.GetRequestContext()));
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

  AddConsoleMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                             mojom::ConsoleMessageLevel::kError, message));
}

base::Optional<ResourceRequestBlockedReason>
BaseFetchContext::CheckCSPForRequest(
    mojom::RequestContextType request_context,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  return CheckCSPForRequestInternal(
      request_context, url, options, reporting_policy, redirect_status,
      ContentSecurityPolicy::CheckHeaderType::kCheckReportOnly);
}

base::Optional<ResourceRequestBlockedReason>
BaseFetchContext::CheckCSPForRequestInternal(
    mojom::RequestContextType request_context,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status,
    ContentSecurityPolicy::CheckHeaderType check_header_type) const {
  if (ShouldBypassMainWorldCSP() || options.content_security_policy_option ==
                                        kDoNotCheckContentSecurityPolicy) {
    return base::nullopt;
  }

  const ContentSecurityPolicy* csp = GetContentSecurityPolicy();
  if (csp && !csp->AllowRequest(
                 request_context, url, options.content_security_policy_nonce,
                 options.integrity_metadata, options.parser_disposition,
                 redirect_status, reporting_policy, check_header_type)) {
    return ResourceRequestBlockedReason::kCSP;
  }
  return base::nullopt;
}

base::Optional<ResourceRequestBlockedReason>
BaseFetchContext::CanRequestInternal(
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    SecurityViolationReportingPolicy reporting_policy,
    ResourceRequest::RedirectStatus redirect_status) const {
  if (GetResourceFetcherProperties().IsDetached()) {
    if (!resource_request.GetKeepalive() ||
        redirect_status == ResourceRequest::RedirectStatus::kNoRedirect) {
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
  DCHECK(network::IsNavigationRequestMode(request_mode) || origin);
  if (!network::IsNavigationRequestMode(request_mode) &&
      !resource_request.CanDisplay(url)) {
    if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
      AddConsoleMessage(ConsoleMessage::Create(
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
      return base::nullopt;
    }
    return ResourceRequestBlockedReason::kOther;
  }

  mojom::RequestContextType request_context =
      resource_request.GetRequestContext();

  // We check the 'report-only' headers before upgrading the request (in
  // populateResourceRequest). We check the enforced headers here to ensure we
  // block things we ought to block.
  if (CheckCSPForRequestInternal(
          request_context, url, options, reporting_policy, redirect_status,
          ContentSecurityPolicy::CheckHeaderType::kCheckEnforce) ==
      ResourceRequestBlockedReason::kCSP) {
    return ResourceRequestBlockedReason::kCSP;
  }

  if (type == ResourceType::kScript || type == ResourceType::kImportResource) {
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
  if (ShouldBlockFetchByMixedContentCheck(request_context,
                                          resource_request.GetRedirectStatus(),
                                          url, reporting_policy))
    return ResourceRequestBlockedReason::kMixedContent;

  if (url.PotentiallyDanglingMarkup() && url.ProtocolIsInHTTPFamily()) {
    CountDeprecation(WebFeature::kCanRequestURLHTTPContainingNewline);
    return ResourceRequestBlockedReason::kOther;
  }

  // Loading of a subresource may be blocked by previews resource loading hints.
  if (GetPreviewsResourceLoadingHints() &&
      !GetPreviewsResourceLoadingHints()->AllowLoad(
          type, url, resource_request.Priority())) {
    return ResourceRequestBlockedReason::kOther;
  }

  // Let the client have the final say into whether or not the load should
  // proceed.
  if (GetSubresourceFilter() && type != ResourceType::kImportResource) {
    if (!GetSubresourceFilter()->AllowLoad(url, request_context,
                                           reporting_policy)) {
      return ResourceRequestBlockedReason::kSubresourceFilter;
    }
  }

  return base::nullopt;
}

void BaseFetchContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_properties_);
  FetchContext::Trace(visitor);
}

}  // namespace blink

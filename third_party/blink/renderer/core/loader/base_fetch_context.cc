// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/base_fetch_context.h"

#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
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
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

namespace {

// This function maps from Blink's internal "request context" concept to Fetch's
// notion of a request's "destination":
// https://fetch.spec.whatwg.org/#concept-request-destination.
const char* GetDestinationFromContext(mojom::RequestContextType context) {
  switch (context) {
    case mojom::RequestContextType::UNSPECIFIED:
    case mojom::RequestContextType::BEACON:
    case mojom::RequestContextType::DOWNLOAD:
    case mojom::RequestContextType::EVENT_SOURCE:
    case mojom::RequestContextType::FETCH:
    case mojom::RequestContextType::PING:
    case mojom::RequestContextType::XML_HTTP_REQUEST:
    case mojom::RequestContextType::SUBRESOURCE:
    case mojom::RequestContextType::PREFETCH:
      return "";
    case mojom::RequestContextType::CSP_REPORT:
      return "report";
    case mojom::RequestContextType::AUDIO:
      return "audio";
    case mojom::RequestContextType::EMBED:
      return "embed";
    case mojom::RequestContextType::FONT:
      return "font";
    case mojom::RequestContextType::FRAME:
    case mojom::RequestContextType::HYPERLINK:
    case mojom::RequestContextType::IFRAME:
    case mojom::RequestContextType::LOCATION:
    case mojom::RequestContextType::FORM:
      return "document";
    case mojom::RequestContextType::IMAGE:
    case mojom::RequestContextType::FAVICON:
    case mojom::RequestContextType::IMAGE_SET:
      return "image";
    case mojom::RequestContextType::MANIFEST:
      return "manifest";
    case mojom::RequestContextType::OBJECT:
      return "object";
    case mojom::RequestContextType::SCRIPT:
      return "script";
    case mojom::RequestContextType::SERVICE_WORKER:
      return "serviceworker";
    case mojom::RequestContextType::SHARED_WORKER:
      return "sharedworker";
    case mojom::RequestContextType::STYLE:
      return "style";
    case mojom::RequestContextType::TRACK:
      return "track";
    case mojom::RequestContextType::VIDEO:
      return "video";
    case mojom::RequestContextType::WORKER:
      return "worker";
    case mojom::RequestContextType::XSLT:
      return "xslt";
    case mojom::RequestContextType::IMPORT:
    case mojom::RequestContextType::INTERNAL:
    case mojom::RequestContextType::PLUGIN:
      return "unknown";
  }
  NOTREACHED();
  return "";
}

MessageSource ConvertLogSourceToMessageSource(FetchContext::LogSource source) {
  // When LogSource is extended, this switch statement should be modified to
  // convert LogSource to blink::MessageSource.
  switch (source) {
    case FetchContext::kJSSource:
      return kJSMessageSource;
    case FetchContext::kSecuritySource:
      return kSecurityMessageSource;
    case FetchContext::kOtherSource:
      return kOtherMessageSource;
  }
  NOTREACHED();
  return kOtherMessageSource;
}

}  // namespace

BaseFetchContext::BaseFetchContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : FetchContext(std::move(task_runner)) {}

void BaseFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                   FetchResourceType type) {
  bool is_main_resource = type == kFetchMainResource;
  if (!is_main_resource) {
    // TODO(domfarolino): we can probably *just set* the HTTP `Referer` here
    // no matter what now.
    if (!request.DidSetHTTPReferrer()) {
      String referrer_to_use = request.ReferrerString();
      ReferrerPolicy referrer_policy_to_use = request.GetReferrerPolicy();

      if (referrer_to_use == Referrer::ClientReferrerString())
        referrer_to_use = GetFetchClientSettingsObject()->GetOutgoingReferrer();

      if (referrer_policy_to_use == kReferrerPolicyDefault) {
        referrer_policy_to_use =
            GetFetchClientSettingsObject()->GetReferrerPolicy();
      }

      // TODO(domfarolino): Stop storing ResourceRequest's referrer as a header
      // and store it elsewhere. See https://crbug.com/850813.
      request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
          referrer_policy_to_use, request.Url(), referrer_to_use));
      request.SetHTTPOriginIfNeeded(GetSecurityOrigin());
    } else {
      DCHECK_EQ(SecurityPolicy::GenerateReferrer(request.GetReferrerPolicy(),
                                                 request.Url(),
                                                 request.HttpReferrer())
                    .referrer,
                request.HttpReferrer());
      request.SetHTTPOriginToMatchReferrerIfNeeded();
    }
  }

  auto address_space = GetAddressSpace();
  if (address_space)
    request.SetExternalRequestStateFromRequestorAddressSpace(*address_space);

  if (blink::RuntimeEnabledFeatures::SecMetadataEnabled()) {
    const char* destination_value =
        GetDestinationFromContext(request.GetRequestContext());

    // If the request's destination is the empty string (e.g. `fetch()`), then
    // we'll use the identifier "empty" instead.
    if (strlen(destination_value) == 0)
      destination_value = "empty";

    // We'll handle adding the header to navigations outside of Blink.
    if (strncmp(destination_value, "document", 8) != 0 &&
        request.GetRequestContext() != mojom::RequestContextType::INTERNAL) {
      const char* site_value = "cross-site";
      if (SecurityOrigin::Create(request.Url())
              ->IsSameSchemeHostPort(GetSecurityOrigin())) {
        site_value = "same-origin";
      } else {
        OriginAccessEntry access_entry(
            request.Url().Protocol(), request.Url().Host(),
            network::cors::OriginAccessEntry::kAllowRegisterableDomains);
        if (access_entry.MatchesOrigin(*GetSecurityOrigin()) ==
            network::cors::OriginAccessEntry::kMatchesOrigin) {
          site_value = "same-site";
        }
      }

      String value = String::Format("destination=%s, site=%s",
                                    destination_value, site_value);
      request.AddHTTPHeaderField("Sec-Metadata", AtomicString(value));
    }
  }
}

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

void BaseFetchContext::AddInfoConsoleMessage(const String& message,
                                             LogSource source) const {
  AddConsoleMessage(ConsoleMessage::Create(
      ConvertLogSourceToMessageSource(source), kInfoMessageLevel, message));
}

void BaseFetchContext::AddWarningConsoleMessage(const String& message,
                                                LogSource source) const {
  AddConsoleMessage(ConsoleMessage::Create(
      ConvertLogSourceToMessageSource(source), kWarningMessageLevel, message));
}

void BaseFetchContext::AddErrorConsoleMessage(const String& message,
                                              LogSource source) const {
  AddConsoleMessage(ConsoleMessage::Create(
      ConvertLogSourceToMessageSource(source), kErrorMessageLevel, message));
}

bool BaseFetchContext::IsAdResource(
    const KURL& resource_url,
    ResourceType type,
    mojom::RequestContextType request_context) const {
  SubresourceFilter* filter = GetSubresourceFilter();

  // We do not need main document tagging currently so skipping main resources.
  if (filter && type != ResourceType::kMainResource) {
    return filter->IsAdResource(resource_url, request_context);
  }

  return false;
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

  AddConsoleMessage(ConsoleMessage::Create(kSecurityMessageSource,
                                           kErrorMessageLevel, message));
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
  if (IsDetached()) {
    if (!resource_request.GetKeepalive() ||
        redirect_status == ResourceRequest::RedirectStatus::kNoRedirect) {
      return ResourceRequestBlockedReason::kOther;
    }
  }

  if (ShouldBlockRequestByInspector(resource_request.Url()))
    return ResourceRequestBlockedReason::kInspector;

  scoped_refptr<const SecurityOrigin> origin =
      resource_request.RequestorOrigin();

  const auto request_mode = resource_request.GetFetchRequestMode();
  // On navigation cases, Context().GetSecurityOrigin() may return nullptr, so
  // the request's origin may be nullptr.
  // TODO(yhirano): Figure out if it's actually fine.
  DCHECK(request_mode == network::mojom::FetchRequestMode::kNavigate || origin);
  if (request_mode != network::mojom::FetchRequestMode::kNavigate &&
      !origin->CanDisplay(url)) {
    if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
      AddErrorConsoleMessage(
          "Not allowed to load local resource: " + url.GetString(), kJSSource);
    }
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::requestResource URL was not "
                                 "allowed by SecurityOrigin::CanDisplay";
    return ResourceRequestBlockedReason::kOther;
  }

  if (request_mode == network::mojom::FetchRequestMode::kSameOrigin &&
      CORS::CalculateCORSFlag(url, origin.get(), request_mode)) {
    PrintAccessDeniedMessage(url);
    return ResourceRequestBlockedReason::kOrigin;
  }

  // User Agent CSS stylesheets should only support loading images and should be
  // restricted to data urls.
  if (options.initiator_info.name == FetchInitiatorTypeNames::uacss) {
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
  if (type != ResourceType::kMainResource && IsSVGImageChromeClient() &&
      !url.ProtocolIsData())
    return ResourceRequestBlockedReason::kOrigin;

  network::mojom::RequestContextFrameType frame_type =
      resource_request.GetFrameType();

  // Measure the number of legacy URL schemes ('ftp://') and the number of
  // embedded-credential ('http://user:password@...') resources embedded as
  // subresources.
  if (frame_type != network::mojom::RequestContextFrameType::kTopLevel) {
    bool is_subresource =
        frame_type == network::mojom::RequestContextFrameType::kNone;
    const SecurityOrigin* embedding_origin =
        is_subresource ? GetSecurityOrigin() : GetParentSecurityOrigin();
    DCHECK(embedding_origin);
    if (SchemeRegistry::ShouldTreatURLSchemeAsLegacy(url.Protocol()) &&
        !SchemeRegistry::ShouldTreatURLSchemeAsLegacy(
            embedding_origin->Protocol())) {
      CountDeprecation(WebFeature::kLegacyProtocolEmbeddedAsSubresource);

      return ResourceRequestBlockedReason::kOrigin;
    }

    if (ShouldBlockFetchAsCredentialedSubresource(resource_request, url))
      return ResourceRequestBlockedReason::kOrigin;
  }

  // Check for mixed content. We do this second-to-last so that when folks block
  // mixed content via CSP, they don't get a mixed content warning, but a CSP
  // warning instead.
  if (ShouldBlockFetchByMixedContentCheck(request_context, frame_type,
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
          url, resource_request.Priority())) {
    return ResourceRequestBlockedReason::kOther;
  }

  // Let the client have the final say into whether or not the load should
  // proceed.
  if (GetSubresourceFilter() && type != ResourceType::kMainResource &&
      type != ResourceType::kImportResource) {
    if (!GetSubresourceFilter()->AllowLoad(url, request_context,
                                           reporting_policy)) {
      return ResourceRequestBlockedReason::kSubresourceFilter;
    }
  }

  return base::nullopt;
}

void BaseFetchContext::Trace(blink::Visitor* visitor) {
  FetchContext::Trace(visitor);
}

}  // namespace blink

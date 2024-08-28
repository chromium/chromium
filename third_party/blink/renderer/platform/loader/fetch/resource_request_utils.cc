// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_request_utils.h"

#include "base/feature_list.h"
#include "base/trace_event/common/trace_event_common.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {
namespace {

ReportingDisposition CalculateReportingDisposition(
    const FetchParameters& params) {
  // No CSP reports are sent for:
  //
  // Speculative preload
  // ===================
  // This avoids sending 2 reports for a single resource (preload + real load).
  // Moreover the speculative preload are 'speculative', it might not even be
  // possible to issue a real request.
  //
  // Stale revalidations
  // ===================
  // Web browser should not send violation reports for stale revalidations. The
  // initial request was allowed. In theory, the revalidation request should be
  // allowed as well. However, some <meta> CSP header might have been added in
  // the meantime. See https://crbug.com/1070117.
  //
  // Note: Ideally, stale revalidations should bypass every checks. In practise,
  // they are run and block the request. Bypassing all security checks could be
  // risky and probably doesn't really worth it. They are very rarely blocked.
  return params.IsSpeculativePreload() || params.IsStaleRevalidation()
             ? ReportingDisposition::kSuppressReporting
             : ReportingDisposition::kReport;
}

}  // namespace

// This function corresponds with step 2 substep 7 of
// https://fetch.spec.whatwg.org/#main-fetch.
void SetReferrer(
    ResourceRequest& request,
    const FetchClientSettingsObject& fetch_client_settings_object) {
  String referrer_to_use = request.ReferrerString();
  network::mojom::ReferrerPolicy referrer_policy_to_use =
      request.GetReferrerPolicy();

  if (referrer_to_use == Referrer::ClientReferrerString()) {
    referrer_to_use = fetch_client_settings_object.GetOutgoingReferrer();
  }

  if (referrer_policy_to_use == network::mojom::ReferrerPolicy::kDefault) {
    referrer_policy_to_use = fetch_client_settings_object.GetReferrerPolicy();
  }

  Referrer generated_referrer = SecurityPolicy::GenerateReferrer(
      referrer_policy_to_use, request.Url(), referrer_to_use);

  request.SetReferrerString(generated_referrer.referrer);
  request.SetReferrerPolicy(generated_referrer.referrer_policy);
}

ResourceLoadPriority AdjustPriorityWithPriorityHintAndRenderBlocking(
    ResourceLoadPriority priority,
    ResourceType type,
    mojom::blink::FetchPriorityHint fetch_priority_hint,
    RenderBlockingBehavior render_blocking_behavior) {
  ResourceLoadPriority new_priority = priority;

  switch (fetch_priority_hint) {
    case mojom::blink::FetchPriorityHint::kAuto:
      break;
    case mojom::blink::FetchPriorityHint::kHigh:
      // Boost priority of any request type that supports priority hints.
      if (new_priority < ResourceLoadPriority::kHigh) {
        new_priority = ResourceLoadPriority::kHigh;
      }
      CHECK_LE(priority, new_priority);
      break;
    case mojom::blink::FetchPriorityHint::kLow:
      // Demote priority of any request type that supports priority hints.
      // Most content types go to kLow. The one exception is early
      // render-blocking CSS which defaults to the highest priority but
      // can be lowered to match the "high" priority of everything else
      // to allow for ordering if necessary without causing too much of a
      // foot-gun.
      if (type == ResourceType::kCSSStyleSheet &&
          new_priority == ResourceLoadPriority::kVeryHigh) {
        new_priority = ResourceLoadPriority::kHigh;
      } else if (new_priority > ResourceLoadPriority::kLow) {
        new_priority = ResourceLoadPriority::kLow;
      }

      CHECK_LE(new_priority, priority);
      break;
  }

  // Render-blocking is a signal that the resource is important, so we bump it
  // to at least kHigh.
  if (render_blocking_behavior == RenderBlockingBehavior::kBlocking &&
      new_priority < ResourceLoadPriority::kHigh) {
    new_priority = ResourceLoadPriority::kHigh;
  }

  return new_priority;
}

// This method simply takes in information about a ResourceRequest, and returns
// if the resource should be loaded in parallel (incremental) or sequentially
// for protocols that support multiplexing and HTTP extensible priorities
// (RFC 9218).
// Most content types can be operated on with partial data (document parsing,
// images, media, etc) but a few need to be complete before they can be
// processed.
bool ShouldLoadIncremental(ResourceType type) {
  switch (type) {
    case ResourceType::kCSSStyleSheet:
    case ResourceType::kScript:
    case ResourceType::kFont:
    case ResourceType::kXSLStyleSheet:
    case ResourceType::kManifest:
      return false;
    case ResourceType::kImage:
    case ResourceType::kRaw:
    case ResourceType::kSVGDocument:
    case ResourceType::kLinkPrefetch:
    case ResourceType::kTextTrack:
    case ResourceType::kAudio:
    case ResourceType::kVideo:
    case ResourceType::kSpeculationRules:
    case ResourceType::kMock:
    case ResourceType::kDictionary:
      return true;
  }
  NOTREACHED();
}

std::optional<ResourceRequestBlockedReason> PrepareResourceRequest(
    ResourceType resource_type,
    const FetchClientSettingsObject& fetch_client_settings_object,
    FetchParameters& params,
    FetchContext& context,
    WebScopedVirtualTimePauser& virtual_time_pauser,
    ResourceRequestContext& resource_request_context,
    const KURL& bundle_url_for_uuid_resources) {
  ResourceRequest& resource_request = params.MutableResourceRequest();
  const ResourceLoaderOptions& options = params.Options();
  DCHECK(!RuntimeEnabledFeatures::
             MinimimalResourceRequestPrepBeforeCacheLookupEnabled());
  const ReportingDisposition reporting_disposition =
      CalculateReportingDisposition(params);

  // Note that resource_request.GetRedirectInfo() may be non-null here since
  // e.g. ThreadableLoader may create a new Resource from a ResourceRequest that
  // originates from the ResourceRequest passed to the redirect handling
  // callback.

  // Before modifying the request for CSP, evaluate report-only headers. This
  // allows site owners to learn about requests that are being modified
  // (e.g. mixed content that is being upgraded by upgrade-insecure-requests).
  const std::optional<ResourceRequest::RedirectInfo>& redirect_info =
      resource_request.GetRedirectInfo();
  const KURL& url_before_redirects =
      redirect_info ? redirect_info->original_url : params.Url();
  const ResourceRequestHead::RedirectStatus redirect_status =
      redirect_info ? ResourceRequestHead::RedirectStatus::kFollowedRedirect
                    : ResourceRequestHead::RedirectStatus::kNoRedirect;
  context.CheckCSPForRequest(
      resource_request.GetRequestContext(),
      resource_request.GetRequestDestination(),
      MemoryCache::RemoveFragmentIdentifierIfNeeded(
          bundle_url_for_uuid_resources.IsValid()
              ? bundle_url_for_uuid_resources
              : params.Url()),
      options, reporting_disposition,
      MemoryCache::RemoveFragmentIdentifierIfNeeded(url_before_redirects),
      redirect_status);

  // This may modify params.Url() (via the resource_request argument).
  context.UpgradeResourceRequestForLoader(
      resource_type, params.GetResourceWidth(), resource_request, options);
  if (!params.Url().IsValid()) {
    return ResourceRequestBlockedReason::kOther;
  }

  ResourceLoadPriority computed_load_priority = resource_request.Priority();
  // We should only compute the priority for ResourceRequests whose priority has
  // not already been set.
  if (!resource_request.PriorityHasBeenSet()) {
    computed_load_priority =
        resource_request_context.ComputeLoadPriority(params);
  }
  CHECK_NE(computed_load_priority, ResourceLoadPriority::kUnresolved);
  resource_request.SetPriority(computed_load_priority);
  resource_request.SetPriorityIncremental(ShouldLoadIncremental(resource_type));
  resource_request.SetRenderBlockingBehavior(
      params.GetRenderBlockingBehavior());

  if (resource_request.GetCacheMode() ==
      mojom::blink::FetchCacheMode::kDefault) {
    resource_request.SetCacheMode(context.ResourceRequestCachePolicy(
        resource_request, resource_type, params.Defer()));
  }
  if (resource_request.GetRequestContext() ==
      mojom::blink::RequestContextType::UNSPECIFIED) {
    resource_request.SetRequestContext(ResourceFetcher::DetermineRequestContext(
        resource_type, ResourceFetcher::kImageNotImageSet));
    resource_request.SetRequestDestination(
        ResourceFetcher::DetermineRequestDestination(resource_type));
  }

  if (resource_type == ResourceType::kLinkPrefetch) {
    // Add the "Purpose: prefetch" header to requests for prefetch.
    resource_request.SetPurposeHeader("prefetch");
  } else if (context.IsPrerendering()) {
    // Add the "Sec-Purpose: prefetch;prerender" header to requests issued from
    // prerendered pages. Add "Purpose: prefetch" as well for compatibility
    // concerns (See https://github.com/WICG/nav-speculation/issues/133).
    resource_request.SetHttpHeaderField(http_names::kSecPurpose,
                                        AtomicString("prefetch;prerender"));
    resource_request.SetPurposeHeader("prefetch");
  }

  // Indicate whether the network stack can return a stale resource. If a
  // stale resource is returned a StaleRevalidation request will be scheduled.
  // Explicitly disallow stale responses for fetchers that don't have SWR
  // enabled (via origin trial), and non-GET requests.
  resource_request.SetAllowStaleResponse(resource_request.HttpMethod() ==
                                             http_names::kGET &&
                                         !params.IsStaleRevalidation());

  SetReferrer(resource_request, fetch_client_settings_object);

  context.AddAdditionalRequestHeaders(resource_request);

  resource_request_context.RecordTrace();

  const std::optional<ResourceRequestBlockedReason> blocked_reason =
      context.CanRequest(resource_type, resource_request,
                         MemoryCache::RemoveFragmentIdentifierIfNeeded(
                             bundle_url_for_uuid_resources.IsValid()
                                 ? bundle_url_for_uuid_resources
                                 : params.Url()),
                         options, reporting_disposition,
                         resource_request.GetRedirectInfo());

  if (context.CalculateIfAdSubresource(resource_request,
                                       std::nullopt /* alias_url */,
                                       resource_type, options.initiator_info)) {
    resource_request.SetIsAdResource();
  }

  if (blocked_reason) {
    return blocked_reason;
  }

  // For initial requests, call PrepareRequest() here before revalidation
  // policy is determined.
  context.PrepareRequest(resource_request, params.MutableOptions(),
                         virtual_time_pauser, resource_type);

  if (!params.Url().IsValid()) {
    return ResourceRequestBlockedReason::kOther;
  }

  return blocked_reason;
}

void UpgradeResourceRequestForLoaderNew(
    ResourceType resource_type,
    FetchParameters& params,
    FetchContext& context,
    ResourceRequestContext& resource_request_context,
    WebScopedVirtualTimePauser& virtual_time_pauser) {
  DCHECK(RuntimeEnabledFeatures::
             MinimimalResourceRequestPrepBeforeCacheLookupEnabled());
  ResourceRequest& resource_request = params.MutableResourceRequest();
  const ResourceLoaderOptions& options = params.Options();

  resource_request.SetCanChangeUrl(false);

  // Note that resource_request.GetRedirectInfo() may be non-null here since
  // e.g. ThreadableLoader may create a new Resource from a ResourceRequest that
  // originates from the ResourceRequest passed to the redirect handling
  // callback.
  context.UpgradeResourceRequestForLoader(
      resource_type, params.GetResourceWidth(), resource_request, options);

  DCHECK(params.Url().IsValid());
  resource_request.SetPriorityIncremental(ShouldLoadIncremental(resource_type));
  resource_request.SetRenderBlockingBehavior(
      params.GetRenderBlockingBehavior());

  if (resource_type == ResourceType::kLinkPrefetch) {
    // Add the "Purpose: prefetch" header to requests for prefetch.
    resource_request.SetPurposeHeader("prefetch");
  } else if (context.IsPrerendering()) {
    // Add the "Sec-Purpose: prefetch;prerender" header to requests issued from
    // prerendered pages. Add "Purpose: prefetch" as well for compatibility
    // concerns (See https://github.com/WICG/nav-speculation/issues/133).
    resource_request.SetHttpHeaderField(http_names::kSecPurpose,
                                        AtomicString("prefetch;prerender"));
    resource_request.SetPurposeHeader("prefetch");
  }

  context.AddAdditionalRequestHeaders(resource_request);

  resource_request_context.RecordTrace();

  if (context.CalculateIfAdSubresource(resource_request,
                                       std::nullopt /* alias_url */,
                                       resource_type, options.initiator_info)) {
    resource_request.SetIsAdResource();
  }

  // For initial requests, call PrepareRequest() here before revalidation
  // policy is determined.
  context.PrepareRequest(resource_request, params.MutableOptions(),
                         virtual_time_pauser, resource_type);
  DCHECK(params.Url().IsValid());

  resource_request.SetCanChangeUrl(true);
}

std::optional<ResourceRequestBlockedReason>
PrepareResourceRequestForCacheAccess(
    ResourceType resource_type,
    const FetchClientSettingsObject& fetch_client_settings_object,
    const KURL& bundle_url_for_uuid_resources,
    ResourceRequestContext& resource_request_context,
    FetchContext& context,
    FetchParameters& params) {
  DCHECK(RuntimeEnabledFeatures::
             MinimimalResourceRequestPrepBeforeCacheLookupEnabled());
  ResourceRequest& resource_request = params.MutableResourceRequest();
  const ResourceLoaderOptions& options = params.Options();
  const ReportingDisposition reporting_disposition =
      CalculateReportingDisposition(params);

  // Note that resource_request.GetRedirectInfo() may be non-null here since
  // e.g. ThreadableLoader may create a new Resource from a ResourceRequest that
  // originates from the ResourceRequest passed to the redirect handling
  // callback.

  // Before modifying the request for CSP, evaluate report-only headers. This
  // allows site owners to learn about requests that are being modified
  // (e.g. mixed content that is being upgraded by upgrade-insecure-requests).
  const std::optional<ResourceRequest::RedirectInfo>& redirect_info =
      resource_request.GetRedirectInfo();
  const KURL& url_before_redirects =
      redirect_info ? redirect_info->original_url : params.Url();
  const ResourceRequestHead::RedirectStatus redirect_status =
      redirect_info ? ResourceRequestHead::RedirectStatus::kFollowedRedirect
                    : ResourceRequestHead::RedirectStatus::kNoRedirect;
  context.CheckCSPForRequest(
      resource_request.GetRequestContext(),
      resource_request.GetRequestDestination(),
      MemoryCache::RemoveFragmentIdentifierIfNeeded(
          bundle_url_for_uuid_resources.IsValid()
              ? bundle_url_for_uuid_resources
              : params.Url()),
      options, reporting_disposition,
      MemoryCache::RemoveFragmentIdentifierIfNeeded(url_before_redirects),
      redirect_status);

  context.PopulateResourceRequestBeforeCacheAccess(options, resource_request);
  if (!resource_request.Url().IsValid()) {
    return ResourceRequestBlockedReason::kOther;
  }

  ResourceLoadPriority computed_load_priority = resource_request.Priority();
  // We should only compute the priority for ResourceRequests whose priority has
  // not already been set.
  if (!resource_request.PriorityHasBeenSet()) {
    computed_load_priority =
        resource_request_context.ComputeLoadPriority(params);
  }
  CHECK_NE(computed_load_priority, ResourceLoadPriority::kUnresolved);
  resource_request.SetPriority(computed_load_priority);

  if (resource_request.GetCacheMode() ==
      mojom::blink::FetchCacheMode::kDefault) {
    resource_request.SetCacheMode(context.ResourceRequestCachePolicy(
        resource_request, resource_type, params.Defer()));
  }

  if (resource_request.GetRequestContext() ==
      mojom::blink::RequestContextType::UNSPECIFIED) {
    resource_request.SetRequestContext(ResourceFetcher::DetermineRequestContext(
        resource_type, ResourceFetcher::kImageNotImageSet));
    resource_request.SetRequestDestination(
        ResourceFetcher::DetermineRequestDestination(resource_type));
  }

  // Indicate whether the network stack can return a stale resource. If a
  // stale resource is returned a StaleRevalidation request will be scheduled.
  // Explicitly disallow stale responses for fetchers that don't have SWR
  // enabled (via origin trial), and non-GET requests.
  resource_request.SetAllowStaleResponse(resource_request.HttpMethod() ==
                                             http_names::kGET &&
                                         !params.IsStaleRevalidation());

  SetReferrer(resource_request, fetch_client_settings_object);

  std::optional<ResourceRequestBlockedReason> blocked_reason =
      context.CanRequest(resource_type, resource_request,
                         MemoryCache::RemoveFragmentIdentifierIfNeeded(
                             bundle_url_for_uuid_resources.IsValid()
                                 ? bundle_url_for_uuid_resources
                                 : params.Url()),
                         options, reporting_disposition,
                         resource_request.GetRedirectInfo());
  if (context.CalculateIfAdSubresource(resource_request,
                                       std::nullopt /* alias_url */,
                                       resource_type, options.initiator_info)) {
    resource_request.SetIsAdResource();
  }
  if (blocked_reason) {
    return blocked_reason;
  }
  if (!resource_request.Url().IsValid()) {
    return ResourceRequestBlockedReason::kOther;
  }
  context.WillSendRequest(resource_request);
  if (!resource_request.Url().IsValid()) {
    return ResourceRequestBlockedReason::kOther;
  }

  return std::nullopt;
}

}  // namespace blink

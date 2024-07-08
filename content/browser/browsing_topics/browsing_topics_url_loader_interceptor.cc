// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_url_loader_interceptor.h"

#include "content/browser/browsing_topics/header_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

namespace {

// The topics header outcome for the fetch initial request or redirect.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BrowsingTopicsFetchRequestOrRedirectResult {
  kSuccess = 0,
  kDisallowedByContentClient = 1,
  kNoInitiatorFrame = 2,
  kFromFencedFrame = 3,
  kFromNonPrimaryPage = 4,
  kOpaqueCallerOrigin = 5,
  kNonSecureCallerOrigin = 6,
  kDisallowedByPermissionsPolicy = 7,

  kMaxValue = kDisallowedByPermissionsPolicy,
};

void RecordFetchRequestResultUma(
    BrowsingTopicsFetchRequestOrRedirectResult result,
    bool is_redirect) {
  if (is_redirect) {
    base::UmaHistogramEnumeration(
        "BrowsingTopics.Fetch.RedirectedUrlRequest.Result", result);
  } else {
    base::UmaHistogramEnumeration(
        "BrowsingTopics.Fetch.InitialUrlRequest.Result", result);
  }
}

}  // namespace

BrowsingTopicsURLLoaderInterceptor::BrowsingTopicsURLLoaderInterceptor(
    WeakDocumentPtr document,
    const network::ResourceRequest& resource_request)
    : document_(document),
      resource_request_(resource_request),
      url_(resource_request.url) {
  CHECK(resource_request_->browsing_topics);
}

BrowsingTopicsURLLoaderInterceptor::~BrowsingTopicsURLLoaderInterceptor() =
    default;

void BrowsingTopicsURLLoaderInterceptor::WillStartRequest(
    net::HttpRequestHeaders& headers) {
  PopulateRequestOrRedirectHeaders(/*is_redirect=*/false, headers,
                                   /*removed_headers=*/nullptr);
}

void BrowsingTopicsURLLoaderInterceptor::WillFollowRedirect(
    const std::optional<GURL>& new_url,
    std::vector<std::string>& removed_headers,
    net::HttpRequestHeaders& modified_headers) {
  if (new_url) {
    url_ = new_url.value();
  }

  PopulateRequestOrRedirectHeaders(/*is_redirect=*/true, modified_headers,
                                   &removed_headers);
}

void BrowsingTopicsURLLoaderInterceptor::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr& head) {
  ProcessRedirectOrResponseHeaders(head);

  url_ = redirect_info.new_url;
}

void BrowsingTopicsURLLoaderInterceptor::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr& head) {
  ProcessRedirectOrResponseHeaders(head);
}

void BrowsingTopicsURLLoaderInterceptor::PopulateRequestOrRedirectHeaders(
    bool is_redirect,
    net::HttpRequestHeaders& headers,
    std::vector<std::string>* removed_headers) {
  topics_eligible_ = false;

  if (removed_headers) {
    removed_headers->push_back(kBrowsingTopicsRequestHeaderKey);
  }

  // Due to the race between the subresource requests and navigations, this
  // request may arrive before the commit confirmation is received (i.e.
  // NavigationRequest::DidCommitNavigation()), or after the document is
  // destroyed. We consider those cases to be ineligible for topics.
  RenderFrameHost* request_initiator_frame =
      document_.AsRenderFrameHostIfValid();

  if (!request_initiator_frame) {
    RecordFetchRequestResultUma(
        BrowsingTopicsFetchRequestOrRedirectResult::kNoInitiatorFrame,
        is_redirect);
    return;
  }

  // Fenced frames disallow most permissions policies which would let this
  // function return false regardless, but adding this check to be more
  // explicit.
  if (request_initiator_frame->IsNestedWithinFencedFrame()) {
    RecordFetchRequestResultUma(
        BrowsingTopicsFetchRequestOrRedirectResult::kFromFencedFrame,
        is_redirect);
    return;
  }

  if (!request_initiator_frame->GetPage().IsPrimary()) {
    RecordFetchRequestResultUma(
        BrowsingTopicsFetchRequestOrRedirectResult::kFromNonPrimaryPage,
        is_redirect);
    return;
  }

  url::Origin origin = url::Origin::Create(url_);
  if (origin.opaque()) {
    RecordFetchRequestResultUma(
        BrowsingTopicsFetchRequestOrRedirectResult::kOpaqueCallerOrigin,
        is_redirect);
    return;
  }

  // TODO(yaoxia): should this be `ReportBadMessage`? On the renderer side, the
  // fetch initiator context must be secure. Does it imply that the requested
  // `origin` is always potentially trustworthy?
  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    RecordFetchRequestResultUma(
        BrowsingTopicsFetchRequestOrRedirectResult::kNonSecureCallerOrigin,
        is_redirect);
    return;
  }

  const blink::PermissionsPolicy* permissions_policy =
      static_cast<RenderFrameHostImpl*>(request_initiator_frame)
          ->permissions_policy();

  if (!permissions_policy->IsFeatureEnabledForSubresourceRequest(
          blink::mojom::PermissionsPolicyFeature::kBrowsingTopics, origin,
          *resource_request_) ||
      !permissions_policy->IsFeatureEnabledForSubresourceRequest(
          blink::mojom::PermissionsPolicyFeature::
              kBrowsingTopicsBackwardCompatible,
          origin, *resource_request_)) {
    RecordFetchRequestResultUma(BrowsingTopicsFetchRequestOrRedirectResult::
                                    kDisallowedByPermissionsPolicy,
                                is_redirect);
    return;
  }

  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics_eligible_ = GetContentClient()->browser()->HandleTopicsWebApi(
      origin, request_initiator_frame->GetMainFrame(),
      browsing_topics::ApiCallerSource::kFetch,
      /*get_topics=*/true,
      /*observe=*/false, topics);

  const int num_versions_in_epochs =
      topics_eligible_
          ? GetContentClient()->browser()->NumVersionsInTopicsEpochs(
                request_initiator_frame->GetMainFrame())
          : 0;
  headers.SetHeader(kBrowsingTopicsRequestHeaderKey,
                    DeriveTopicsHeaderValue(topics, num_versions_in_epochs));

  if (topics_eligible_) {
    RecordFetchRequestResultUma(
        BrowsingTopicsFetchRequestOrRedirectResult::kSuccess, is_redirect);
  } else {
    RecordFetchRequestResultUma(
        BrowsingTopicsFetchRequestOrRedirectResult::kDisallowedByContentClient,
        is_redirect);
  }
}

void BrowsingTopicsURLLoaderInterceptor::ProcessRedirectOrResponseHeaders(
    const network::mojom::URLResponseHeadPtr& head) {
  if (topics_eligible_) {
    RenderFrameHost* rfh = document_.AsRenderFrameHostIfValid();
    if (!rfh) {
      return;
    }

    HandleTopicsEligibleResponse(head->parsed_headers,
                                 url::Origin::Create(url_), *rfh,
                                 browsing_topics::ApiCallerSource::kFetch);

    topics_eligible_ = false;
  }
}

}  // namespace content

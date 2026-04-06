// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_resource_loader.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-shared.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace content {

// static
bool ServiceWorkerResourceLoader::IsValidServiceWorkerResponse(
    network::mojom::RequestMode request_mode,
    network::mojom::RedirectMode redirect_mode,
    const blink::mojom::FetchAPIResponsePtr& response) {
  // This validation follows the Fetch spec.
  // 4.4 HTTP fetch, Step 3.5.6.
  // If one of the following is true
  // - response’s type is "error"
  // - request’s mode is "same-origin" and response’s type is "cors"
  // - request’s mode is not "no-cors" and response’s type is "opaque"
  // - request’s redirect mode is not "manual" and response’s type is
  //   "opaqueredirect"
  // - request’s redirect mode is not "follow" and response’s URL list has more
  //   than one item
  // then return a network error.
  // Note: Existing validation functions do not fully follow the spec.
  if (!response) {
    return true;
  }

  if (response->response_type == network::mojom::FetchResponseType::kError) {
    return false;
  }
  if (request_mode == network::mojom::RequestMode::kSameOrigin &&
      response->response_type == network::mojom::FetchResponseType::kCors) {
    return false;
  }
  if (request_mode != network::mojom::RequestMode::kNoCors &&
      response->response_type == network::mojom::FetchResponseType::kOpaque) {
    return false;
  }
  if (redirect_mode != network::mojom::RedirectMode::kManual &&
      response->response_type ==
          network::mojom::FetchResponseType::kOpaqueRedirect) {
    return false;
  }
  if (redirect_mode != network::mojom::RedirectMode::kFollow &&
      response->url_list.size() > 1) {
    return false;
  }

  return true;
}

bool ServiceWorkerResourceLoader::IsValidStaticRouterResponse(
    const network::ResourceRequest& resource_request,
    const blink::mojom::FetchAPIResponsePtr& response) {
  bool is_valid = IsValidServiceWorkerResponse(
      resource_request.mode, resource_request.redirect_mode, response);
  base::UmaHistogramBoolean(
      base::StrCat({"ServiceWorker.StaticRouter.",
                    IsMainResourceLoader() ? "MainResource" : "Subresource",
                    ".ValidResponse"}),
      is_valid);
  return is_valid;
}

ServiceWorkerResourceLoader::ServiceWorkerResourceLoader() = default;
ServiceWorkerResourceLoader::~ServiceWorkerResourceLoader() = default;

void ServiceWorkerResourceLoader::SetCommitResponsibility(
    FetchResponseFrom fetch_response_from) {
  TRACE_EVENT(
      "ServiceWorker", "ServiceWorkerResourceLoader::SetCommitResponsibility",
      perfetto::Flow::FromPointer(this), "commit_responsibility_",
      commit_responsibility_, "fetch_response_from", fetch_response_from);
  switch (fetch_response_from) {
    case FetchResponseFrom::kNoResponseYet:
      NOTREACHED();
    case FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect:
      // kSubresourceLoaderIsHandlingRedirect is called only from subresources.
      CHECK(!IsMainResourceLoader());
      CHECK(commit_responsibility_ == FetchResponseFrom::kServiceWorker ||
            commit_responsibility_ == FetchResponseFrom::kWithoutServiceWorker);
      break;
    case FetchResponseFrom::kAutoPreloadHandlingFallback:
      CHECK(base::FeatureList::IsEnabled(features::kServiceWorkerAutoPreload));
      CHECK_EQ(commit_responsibility_, FetchResponseFrom::kServiceWorker);
      break;
    case FetchResponseFrom::kServiceWorker:
      if (IsMainResourceLoader()) {
        CHECK_EQ(commit_responsibility_, FetchResponseFrom::kNoResponseYet);
      } else {
        CHECK(commit_responsibility_ == FetchResponseFrom::kNoResponseYet ||
              commit_responsibility_ ==
                  FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect);
      }
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      if (IsMainResourceLoader()) {
        CHECK(commit_responsibility_ == FetchResponseFrom::kNoResponseYet ||
              commit_responsibility_ ==
                  FetchResponseFrom::kAutoPreloadHandlingFallback);
      } else {
        CHECK(commit_responsibility_ == FetchResponseFrom::kNoResponseYet ||
              commit_responsibility_ ==
                  FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect ||
              commit_responsibility_ ==
                  FetchResponseFrom::kAutoPreloadHandlingFallback);
      }
      break;
  }
  commit_responsibility_ = fetch_response_from;
}

void ServiceWorkerResourceLoader::RecordFetchResponseFrom() {
  CHECK(commit_responsibility_ == FetchResponseFrom::kServiceWorker ||
        commit_responsibility_ == FetchResponseFrom::kWithoutServiceWorker);
  if (IsMainResourceLoader()) {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.FetchEvent.MainResource.FetchResponseFrom",
        commit_responsibility_);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.FetchEvent.Subresource.FetchResponseFrom",
        commit_responsibility_);
  }
}

void ServiceWorkerResourceLoader::SetDispatchedPreloadType(
    DispatchedPreloadType type) {
  if (!IsMainResourceLoader()) {
    CHECK_NE(type, DispatchedPreloadType::kNavigationPreload);
    CHECK_NE(type, DispatchedPreloadType::kAutoPreload);
  }
  dispatched_preload_type_ = type;
}

bool ServiceWorkerResourceLoader::ShouldRecordServiceWorkerFetchStart() {
  if (!matched_router_source_type_.has_value()) {
    return true;
  }

  switch (*matched_router_source_type_) {
    case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
    case network::mojom::ServiceWorkerRouterSourceType::kCache:
    case network::mojom::ServiceWorkerRouterSourceType::kRaceNetworkAndCache:
      return false;
    case network::mojom::ServiceWorkerRouterSourceType::
        kRaceNetworkAndFetchEvent:
    case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
      // These source should start ServiceWorker and trigger fetch-event.
      return true;
  }
}

bool ServiceWorkerResourceLoader::IsMatchedRouterSourceType(
    network::mojom::ServiceWorkerRouterSourceType type) {
  if (!matched_router_source_type_.has_value()) {
    return false;
  }

  return *matched_router_source_type_ == type;
}

}  // namespace content

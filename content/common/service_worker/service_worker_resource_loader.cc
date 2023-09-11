// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_resource_loader.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "content/common/features.h"

namespace content {
ServiceWorkerResourceLoader::ServiceWorkerResourceLoader() = default;
ServiceWorkerResourceLoader::~ServiceWorkerResourceLoader() = default;

void ServiceWorkerResourceLoader::SetCommitResponsibility(
    FetchResponseFrom fetch_response_from) {
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerResourceLoader::SetCommitResponsibility",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "commit_responsibility_", commit_responsibility_, "fetch_response_from",
      fetch_response_from);
  switch (fetch_response_from) {
    case FetchResponseFrom::kNoResponseYet:
      NOTREACHED_NORETURN();
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
  CHECK_NE(type, DispatchedPreloadType::kNone);
  if (!IsMainResourceLoader()) {
    CHECK_NE(type, DispatchedPreloadType::kNavigationPreload);
  }
  dispatched_preload_type_ = type;
}
}  // namespace content

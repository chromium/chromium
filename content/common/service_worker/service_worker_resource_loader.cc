// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_resource_loader.h"

namespace content {
ServiceWorkerResourceLoader::ServiceWorkerResourceLoader() = default;
ServiceWorkerResourceLoader::~ServiceWorkerResourceLoader() = default;

void ServiceWorkerResourceLoader::SetCommitResponsibility(
    FetchResponseFrom fetch_response_from) {
  switch (fetch_response_from) {
    case FetchResponseFrom::kNoResponseYet:
      NOTREACHED_NORETURN();
    case FetchResponseFrom::kServiceWorker:
    case FetchResponseFrom::kWithoutServiceWorker:
      CHECK_EQ(commit_responsibility_, FetchResponseFrom::kNoResponseYet);
      commit_responsibility_ = fetch_response_from;
      break;
  }
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
}  // namespace content

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_resource_loader.h"

namespace content {
ServiceWorkerResourceLoader::ServiceWorkerResourceLoader() = default;
ServiceWorkerResourceLoader::~ServiceWorkerResourceLoader() = default;

void ServiceWorkerResourceLoader::SetCommitResponsibility(
    FetchResponseFrom fetch_response_from) {
  DCHECK(commit_responsibility_ == FetchResponseFrom::kNoResponseYet);
  commit_responsibility_ = fetch_response_from;
  RecordFetchResponseFrom();
}

void ServiceWorkerResourceLoader::RecordFetchResponseFrom() {
  UMA_HISTOGRAM_ENUMERATION(
      IsMainResourceLoader()
          ? "ServiceWorker.FetchEvent.MainResource.FetchResponseFrom"
          : "ServiceWorker.FetchEvent.Subresource.FetchResponseFrom",
      commit_responsibility_);
}
}  // namespace content

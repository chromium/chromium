// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_resource_loader.h"

namespace content {
ServiceWorkerResourceLoader::ServiceWorkerResourceLoader() = default;
ServiceWorkerResourceLoader::~ServiceWorkerResourceLoader() = default;

void ServiceWorkerResourceLoader::SetFetchResponseFrom(
    FetchResponseFrom fetch_response_from) {
  DCHECK_EQ(fetch_response_from_, FetchResponseFrom::kNoResponseYet);
  UMA_HISTOGRAM_ENUMERATION(
      IsMainResourceLoader()
          ? "ServiceWorker.FetchEvent.MainResource.FetchResponseFrom"
          : "ServiceWorker.FetchEvent.Subresource.FetchResponseFrom",
      fetch_response_from);
  fetch_response_from_ = fetch_response_from;
}
}  // namespace content

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/stale_revalidation_resource_client.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

namespace blink {

StaleRevalidationResourceClient::StaleRevalidationResourceClient(
    Resource* stale_resource)
    : start_time_(base::TimeTicks::Now()), stale_resource_(stale_resource) {}

StaleRevalidationResourceClient::~StaleRevalidationResourceClient() = default;

void StaleRevalidationResourceClient::NotifyFinished(Resource* resource) {
  // After the load is finished
  if (stale_resource_ && IsMainThread())
    GetMemoryCache()->Remove(stale_resource_);
  ClearResource();

  base::TimeTicks response_end = resource->LoadResponseEnd();
  if (!response_end.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Blink.ResourceFetcher.StaleWhileRevalidateDuration",
        response_end - start_time_);
  }
}

void StaleRevalidationResourceClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(stale_resource_);
  RawResourceClient::Trace(visitor);
}

String StaleRevalidationResourceClient::DebugName() const {
  return "StaleRevalidation";
}

}  // namespace blink

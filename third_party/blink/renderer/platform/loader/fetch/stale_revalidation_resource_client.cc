// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/stale_revalidation_resource_client.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

namespace blink {

StaleRevalidationResourceClient::StaleRevalidationResourceClient(
    Resource* stale_resource)
    : stale_resource_(stale_resource) {}

StaleRevalidationResourceClient::~StaleRevalidationResourceClient() = default;

void StaleRevalidationResourceClient::NotifyFinished(Resource* resource) {
  // After the load is finished
  if (stale_resource_ && IsMainThread())
    MemoryCache::Get()->Remove(stale_resource_);
  ClearResource();
}

void StaleRevalidationResourceClient::Trace(Visitor* visitor) const {
  visitor->Trace(stale_resource_);
  RawResourceClient::Trace(visitor);
}

String StaleRevalidationResourceClient::DebugName() const {
  return "StaleRevalidation";
}

}  // namespace blink

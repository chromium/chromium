// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_version_base_info.h"

namespace content {
ServiceWorkerVersionBaseInfo::ServiceWorkerVersionBaseInfo() = default;
ServiceWorkerVersionBaseInfo::ServiceWorkerVersionBaseInfo(
    const GURL& scope,
    const blink::StorageKey& storage_key,
    int64_t registration_id,
    int64_t version_id,
    int process_id,
    blink::mojom::AncestorFrameType ancestor_frame_type)
    : scope(scope),
      storage_key(storage_key),
      registration_id(registration_id),
      version_id(version_id),
      process_id(process_id),
      ancestor_frame_type(ancestor_frame_type) {}
ServiceWorkerVersionBaseInfo::ServiceWorkerVersionBaseInfo(
    const ServiceWorkerVersionBaseInfo& other) = default;
}  // namespace content

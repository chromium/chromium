// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

#include <utility>

namespace content {

ServiceWorkerDiskCache::ServiceWorkerDiskCache()
    : AppCacheDiskCache("DiskCache.ServiceWorker", /*use_simple_cache=*/true) {}

ServiceWorkerResponseReader::ServiceWorkerResponseReader(
    int64_t resource_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseReader(resource_id, std::move(disk_cache)) {}

ServiceWorkerResponseWriter::ServiceWorkerResponseWriter(
    int64_t resource_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseWriter(resource_id, std::move(disk_cache)) {}

ServiceWorkerResponseMetadataWriter::ServiceWorkerResponseMetadataWriter(
    int64_t resource_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseMetadataWriter(resource_id, std::move(disk_cache)) {}

}  // namespace content

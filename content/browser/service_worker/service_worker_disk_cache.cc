// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

#include <utility>

#include "net/base/io_buffer.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

ServiceWorkerDiskCache::ServiceWorkerDiskCache()
    : AppCacheDiskCache(/*use_simple_cache=*/true) {}

}  // namespace content

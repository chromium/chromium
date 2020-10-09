// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_

#include "content/browser/appcache/appcache_disk_cache.h"
#include "content/common/content_export.h"

namespace content {

// Wholesale reusage of the appcache code for response reading,
// writing, and storage. See the corresponding class in that
// library for doc comments and other details.
// TODO(michaeln): If this reuse sticks, refactor/move the
// resused classes to a more common location.

class CONTENT_EXPORT ServiceWorkerDiskCache : public AppCacheDiskCache {
 public:
  ServiceWorkerDiskCache();
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_

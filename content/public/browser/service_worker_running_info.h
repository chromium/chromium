// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_

#include "content/common/content_export.h"
#include "content/public/common/child_process_host.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace content {

// A struct containing information about a running service worker.
struct CONTENT_EXPORT ServiceWorkerRunningInfo {
  ServiceWorkerRunningInfo(const GURL& script_url,
                           const GURL& scope,
                           const blink::StorageKey& key,
                           int64_t render_process_id,
                           const blink::ServiceWorkerToken& token);
  ServiceWorkerRunningInfo(ServiceWorkerRunningInfo&& other) noexcept;
  ServiceWorkerRunningInfo& operator=(
      ServiceWorkerRunningInfo&& other) noexcept;
  ~ServiceWorkerRunningInfo();

  // The service worker script URL.
  GURL script_url;

  // The scope that this service worker handles.
  GURL scope;

  // The key the service worker is stored under.
  blink::StorageKey key;

  // The ID of the render process on which this service worker lives.
  int render_process_id = content::ChildProcessHost::kInvalidUniqueID;

  // The token that uniquely identifies this worker.
  blink::ServiceWorkerToken token;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_

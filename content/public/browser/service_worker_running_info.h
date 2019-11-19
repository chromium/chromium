// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_

#include "content/common/content_export.h"
#include "content/public/common/child_process_host.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "url/gurl.h"

namespace content {

// A struct containing information about a running service worker.
struct CONTENT_EXPORT ServiceWorkerRunningInfo {
  ServiceWorkerRunningInfo(const GURL& script_url,
                           const GURL& scope,
                           int64_t render_process_id);
  ServiceWorkerRunningInfo(ServiceWorkerRunningInfo&& other) noexcept;
  ServiceWorkerRunningInfo& operator=(
      ServiceWorkerRunningInfo&& other) noexcept;
  ~ServiceWorkerRunningInfo();

  // The service worker script URL.
  GURL script_url;

  // The scope that this service worker handles.
  GURL scope;

  // The ID of the render process on which this service worker lives.
  int render_process_id = content::ChildProcessHost::kInvalidUniqueID;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_

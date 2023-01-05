// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_VERSION_BASE_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_VERSION_BASE_INFO_H_

#include "content/common/content_export.h"
#include "content/public/browser/child_process_host.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_ancestor_frame_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"

namespace content {

// Basic information about a service worker.
struct CONTENT_EXPORT ServiceWorkerVersionBaseInfo {
 public:
  ServiceWorkerVersionBaseInfo();
  ServiceWorkerVersionBaseInfo(
      const GURL& scope,
      const blink::StorageKey& storage_key,
      int64_t registration_id,
      int64_t version_id,
      int process_id,
      blink::mojom::AncestorFrameType ancestor_frame_type);
  ServiceWorkerVersionBaseInfo(const ServiceWorkerVersionBaseInfo& other);
  virtual ~ServiceWorkerVersionBaseInfo() = default;

  GURL scope;
  blink::StorageKey storage_key;
  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t version_id = blink::mojom::kInvalidServiceWorkerVersionId;
  int process_id = ChildProcessHost::kInvalidUniqueID;
  // The information of frame type when the service worker is registered.
  // This will be used to check if the frame is fenced frame or not.
  blink::mojom::AncestorFrameType ancestor_frame_type;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_VERSION_BASE_INFO_H_

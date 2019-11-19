// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

enum class EmbeddedWorkerStatus;
struct ServiceWorkerClientInfo;

struct CONTENT_EXPORT ServiceWorkerVersionInfo {
 public:
  ServiceWorkerVersionInfo();
  ServiceWorkerVersionInfo(
      EmbeddedWorkerStatus running_status,
      ServiceWorkerVersion::Status status,
      ServiceWorkerVersion::FetchHandlerExistence fetch_handler_existence,
      const GURL& script_url,
      const url::Origin& origin,
      int64_t registration_id,
      int64_t version_id,
      int process_id,
      int thread_id,
      int devtools_agent_route_id);
  ServiceWorkerVersionInfo(const ServiceWorkerVersionInfo& other);
  ~ServiceWorkerVersionInfo();

  EmbeddedWorkerStatus running_status;
  ServiceWorkerVersion::Status status;
  ServiceWorkerVersion::FetchHandlerExistence fetch_handler_existence;
  blink::mojom::NavigationPreloadState navigation_preload_state;
  GURL script_url;
  url::Origin script_origin;
  int64_t registration_id;
  int64_t version_id;
  int process_id;
  int thread_id;
  int devtools_agent_route_id;
  base::Time script_response_time;
  base::Time script_last_modified;
  std::map<std::string, ServiceWorkerClientInfo> clients;
};

struct CONTENT_EXPORT ServiceWorkerRegistrationInfo {
 public:
  enum DeleteFlag { IS_NOT_DELETED, IS_DELETED };
  ServiceWorkerRegistrationInfo();
  ServiceWorkerRegistrationInfo(const GURL& scope,
                                int64_t registration_id,
                                DeleteFlag delete_flag);
  ServiceWorkerRegistrationInfo(
      const GURL& scope,
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
      int64_t registration_id,
      DeleteFlag delete_flag,
      const ServiceWorkerVersionInfo& active_version,
      const ServiceWorkerVersionInfo& waiting_version,
      const ServiceWorkerVersionInfo& installing_version,
      int64_t stored_version_size_bytes,
      bool navigation_preload_enabled,
      size_t navigation_preload_header_length);
  ServiceWorkerRegistrationInfo(const ServiceWorkerRegistrationInfo& other);
  ~ServiceWorkerRegistrationInfo();

  GURL scope;
  blink::mojom::ServiceWorkerUpdateViaCache update_via_cache;
  int64_t registration_id;
  DeleteFlag delete_flag;
  ServiceWorkerVersionInfo active_version;
  ServiceWorkerVersionInfo waiting_version;
  ServiceWorkerVersionInfo installing_version;

  int64_t stored_version_size_bytes;
  bool navigation_preload_enabled;
  size_t navigation_preload_header_length;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_

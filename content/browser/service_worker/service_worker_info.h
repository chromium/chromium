// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_client_info.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "url/gurl.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

struct CONTENT_EXPORT ServiceWorkerVersionInfo
    : public ServiceWorkerVersionBaseInfo {
 public:
  ServiceWorkerVersionInfo();
  ServiceWorkerVersionInfo(
      blink::EmbeddedWorkerStatus running_status,
      ServiceWorkerVersion::Status status,
      std::optional<ServiceWorkerVersion::FetchHandlerType> fetch_handler_type,
      const GURL& script_url,
      const GURL& scope,
      const blink::StorageKey& storage_key,
      int64_t registration_id,
      int64_t version_id,
      int process_id,
      int thread_id,
      int devtools_agent_route_id,
      ukm::SourceId ukm_source_id,
      blink::mojom::AncestorFrameType ancestor_frame_type,
      std::optional<std::string> router_rules);
  ServiceWorkerVersionInfo(const ServiceWorkerVersionInfo& other);
  ~ServiceWorkerVersionInfo() override;

  blink::EmbeddedWorkerStatus running_status;
  ServiceWorkerVersion::Status status;
  std::optional<ServiceWorkerVersion::FetchHandlerType> fetch_handler_type;
  blink::mojom::NavigationPreloadState navigation_preload_state;
  GURL script_url;
  int thread_id;
  int devtools_agent_route_id;
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  std::optional<std::string> router_rules;
  base::Time script_response_time;
  base::Time script_last_modified;
  std::map<std::string, ServiceWorkerClientInfo> clients;
};

struct CONTENT_EXPORT ServiceWorkerRegistrationInfo {
 public:
  enum DeleteFlag { IS_NOT_DELETED, IS_DELETED };
  ServiceWorkerRegistrationInfo();
  ServiceWorkerRegistrationInfo(const GURL& scope,
                                const blink::StorageKey& key,
                                int64_t registration_id,
                                DeleteFlag delete_flag);
  ServiceWorkerRegistrationInfo(
      const GURL& scope,
      const blink::StorageKey& key,
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
  blink::StorageKey key;
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

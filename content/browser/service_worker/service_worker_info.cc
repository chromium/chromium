// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_info.h"

#include "content/browser/service_worker/service_worker_consts.h"
#include "content/public/browser/child_process_host.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

ServiceWorkerVersionInfo::ServiceWorkerVersionInfo()
    : running_status(blink::EmbeddedWorkerStatus::kStopped),
      status(ServiceWorkerVersion::NEW),
      thread_id(ServiceWorkerConsts::kInvalidEmbeddedWorkerThreadId),
      devtools_agent_route_id(MSG_ROUTING_NONE),
      ukm_source_id(ukm::kInvalidSourceId) {}

ServiceWorkerVersionInfo::ServiceWorkerVersionInfo(
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
    std::optional<std::string> router_rules)
    : ServiceWorkerVersionBaseInfo(scope,
                                   storage_key,
                                   registration_id,
                                   version_id,
                                   process_id,
                                   ancestor_frame_type),
      running_status(running_status),
      status(status),
      fetch_handler_type(fetch_handler_type),
      script_url(script_url),
      thread_id(thread_id),
      devtools_agent_route_id(devtools_agent_route_id),
      ukm_source_id(ukm_source_id),
      router_rules(router_rules) {}

ServiceWorkerVersionInfo::ServiceWorkerVersionInfo(
    const ServiceWorkerVersionInfo& other) = default;

ServiceWorkerVersionInfo::~ServiceWorkerVersionInfo() {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo()
    : registration_id(blink::mojom::kInvalidServiceWorkerRegistrationId),
      delete_flag(IS_NOT_DELETED),
      stored_version_size_bytes(0),
      navigation_preload_enabled(false),
      navigation_preload_header_length(0) {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
    const GURL& scope,
    const blink::StorageKey& key,
    int64_t registration_id,
    DeleteFlag delete_flag)
    : scope(scope),
      key(key),
      registration_id(registration_id),
      delete_flag(delete_flag),
      stored_version_size_bytes(0),
      navigation_preload_enabled(false),
      navigation_preload_header_length(0) {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
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
    size_t navigation_preload_header_length)
    : scope(scope),
      key(key),
      update_via_cache(update_via_cache),
      registration_id(registration_id),
      delete_flag(delete_flag),
      active_version(active_version),
      waiting_version(waiting_version),
      installing_version(installing_version),
      stored_version_size_bytes(stored_version_size_bytes),
      navigation_preload_enabled(navigation_preload_enabled),
      navigation_preload_header_length(navigation_preload_header_length) {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
    const ServiceWorkerRegistrationInfo& other) = default;

ServiceWorkerRegistrationInfo::~ServiceWorkerRegistrationInfo() {}

}  // namespace content

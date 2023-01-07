// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_IDENTIFIABILITY_METRICS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_IDENTIFIABILITY_METRICS_H_

#include <stdint.h>

#include <map>
#include <set>

#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

namespace content {

struct VersionIdentifiabilityInfo {
  VersionIdentifiabilityInfo() = default;
  VersionIdentifiabilityInfo(ukm::SourceId ukm_source_id, const GURL& origin)
      : ukm_source_id(ukm_source_id), origin(origin) {}

  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  GURL origin;
};

// Used to emit a UKM event to associate each worker with each client that it
// has the potential to communicate with (whether or not they actually do). Only
// created when the identifiability study is active.
class ServiceWorkerIdentifiabilityMetrics
    : public ServiceWorkerContextCoreObserver {
 public:
  ServiceWorkerIdentifiabilityMetrics();
  ~ServiceWorkerIdentifiabilityMetrics() override;

 private:
  // ServiceWorkerContextCoreObserver implements
  void OnNewLiveVersion(const ServiceWorkerVersionInfo& version_info) override;
  void OnLiveVersionDestroyed(int64_t version_id) override;
  void OnClientIsExecutionReady(
      ukm::SourceId client_source_id,
      const GURL& url,
      blink::mojom::ServiceWorkerClientType type) override;
  void OnClientDestroyed(ukm::SourceId client_source_id,
                         const GURL& url,
                         blink::mojom::ServiceWorkerClientType type) override;

  void EmitClientAddedEvent(ukm::SourceId version_ukm_source_id,
                            ukm::SourceId client_ukm_source_id);

  // Stores information about ServiceWorkerVersions keyed by version_id.
  std::map<int64_t, VersionIdentifiabilityInfo> version_map_;

  // Stores the UKM source IDs of all clients that are execution ready. They are
  // partitioned by the origin of the client's URL. A GURL, not urL::Origin, is
  // used to avoid unnecessary conversions as GURL::GetOrigin() returns a GURL.
  std::map<GURL, std::set<ukm::SourceId>> client_source_ids_by_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_IDENTIFIABILITY_METRICS_H_

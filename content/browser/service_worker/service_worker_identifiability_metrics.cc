// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_identifiability_metrics.h"

#include "base/containers/contains.h"
#include "content/public/browser/worker_type.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_worker_client_added.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerIdentifiabilityMetrics::ServiceWorkerIdentifiabilityMetrics() =
    default;
ServiceWorkerIdentifiabilityMetrics::~ServiceWorkerIdentifiabilityMetrics() =
    default;

void ServiceWorkerIdentifiabilityMetrics::OnNewLiveVersion(
    const ServiceWorkerVersionInfo& version_info) {
  int64_t version_id = version_info.version_id;
  auto version_it = version_map_.find(version_id);
  if (version_it != version_map_.end()) {
    DCHECK_EQ(version_it->second.ukm_source_id, version_info.ukm_source_id);
    DCHECK_EQ(version_it->second.origin,
              version_info.storage_key.origin().GetURL());
    return;
  }

  GURL origin = version_info.storage_key.origin().GetURL();
  version_map_.emplace(version_id, VersionIdentifiabilityInfo(
                                       version_info.ukm_source_id, origin));

  auto client_it = client_source_ids_by_origin_.find(origin);
  if (client_it == client_source_ids_by_origin_.end()) {
    return;
  }

  for (const auto& client_source_id : client_it->second) {
    EmitClientAddedEvent(version_info.ukm_source_id, client_source_id);
  }
}

void ServiceWorkerIdentifiabilityMetrics::OnLiveVersionDestroyed(
    int64_t version_id) {
  auto version_it = version_map_.find(version_id);
  if (version_it == version_map_.end()) {
    return;
  }

  if (ukm::DelegatingUkmRecorder* ukm_recorder =
          ukm::DelegatingUkmRecorder::Get()) {
    blink::IdentifiabilitySampleCollector::Get()->FlushSource(
        ukm_recorder, version_it->second.ukm_source_id);
  }
  version_map_.erase(version_it);
}

void ServiceWorkerIdentifiabilityMetrics::OnClientIsExecutionReady(
    ukm::SourceId client_source_id,
    const GURL& url,
    blink::mojom::ServiceWorkerClientType type) {
  GURL client_origin = url.DeprecatedGetOriginAsURL();

  // Don't track dedicated workers as they simply inherit the source id of their
  // parents.
  if (type == blink::mojom::ServiceWorkerClientType::kDedicatedWorker) {
    // TODO(crbug.com/40153087): Re-enable once dedicated worker source ids are
    // propagated. Also include dedicated workers in the valid source id DCHECK.
    // DCHECK(base::Contains(client_source_ids_by_origin_, client_origin) &&
    //        base::Contains(client_source_ids_by_origin_[client_origin],
    //                       client_source_id));
    return;
  }
  DCHECK_NE(client_source_id, ukm::kInvalidSourceId);
  client_source_ids_by_origin_[client_origin].insert(client_source_id);

  for (const auto& version : version_map_) {
    if (version.second.origin == client_origin) {
      EmitClientAddedEvent(version.second.ukm_source_id, client_source_id);
    }
  }
}

void ServiceWorkerIdentifiabilityMetrics::OnClientDestroyed(
    ukm::SourceId client_source_id,
    const GURL& url,
    blink::mojom::ServiceWorkerClientType type) {
  GURL client_origin = url.DeprecatedGetOriginAsURL();

  // Don't track dedicated workers as they simply inherit the source id of their
  // parents.
  if (type == blink::mojom::ServiceWorkerClientType::kDedicatedWorker) {
    return;
  }

  auto client_it = client_source_ids_by_origin_.find(client_origin);
  if (client_it != client_source_ids_by_origin_.end()) {
    client_it->second.erase(client_source_id);
    if (client_it->second.empty()) {
      client_source_ids_by_origin_.erase(client_it);
    }
  }
}

void ServiceWorkerIdentifiabilityMetrics::EmitClientAddedEvent(
    ukm::SourceId version_ukm_source_id,
    ukm::SourceId client_ukm_source_id) {
  if (ukm::DelegatingUkmRecorder* ukm_recorder =
          ukm::DelegatingUkmRecorder::Get()) {
    ukm::builders::Worker_ClientAdded(version_ukm_source_id)
        .SetClientSourceId(client_ukm_source_id)
        .SetWorkerType(static_cast<int64_t>(WorkerType::kServiceWorker))
        .Record(ukm_recorder);

    if (blink::IdentifiabilityStudySettings::Get()->IsActive()) {
      blink::IdentifiabilityStudyWorkerClientAdded(version_ukm_source_id)
          .SetClientSourceId(client_ukm_source_id)
          .SetWorkerType(blink::IdentifiableSurface::WorkerType::kServiceWorker)
          .Record(ukm_recorder);
    }
  }
}

}  // namespace content

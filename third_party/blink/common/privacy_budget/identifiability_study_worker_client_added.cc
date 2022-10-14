// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_study_worker_client_added.h"

#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace blink {

IdentifiabilityStudyWorkerClientAdded::IdentifiabilityStudyWorkerClientAdded(
    ukm::SourceId source_id)
    : source_id_(source_id) {}

IdentifiabilityStudyWorkerClientAdded::
    ~IdentifiabilityStudyWorkerClientAdded() = default;

IdentifiabilityStudyWorkerClientAdded&
IdentifiabilityStudyWorkerClientAdded::SetClientSourceId(
    ukm::SourceId client_source_id) {
  client_source_id_ = client_source_id;
  return *this;
}

IdentifiabilityStudyWorkerClientAdded&
IdentifiabilityStudyWorkerClientAdded::SetWorkerType(
    blink::IdentifiableSurface::WorkerType worker_type) {
  worker_type_ = worker_type;
  return *this;
}

void IdentifiabilityStudyWorkerClientAdded::Record(ukm::UkmRecorder* recorder) {
  using Metrics = blink::IdentifiableSurface::ReservedSurfaceMetrics;
  base::flat_map<uint64_t, int64_t> metrics = {
      {
          IdentifiableSurface::FromTypeAndToken(
              blink::IdentifiableSurface::Type::kReservedInternal,
              Metrics::kWorkerClientAdded_ClientSourceId)
              .ToUkmMetricHash(),
          client_source_id_,
      },
      {
          IdentifiableSurface::FromTypeAndToken(
              blink::IdentifiableSurface::Type::kReservedInternal,
              Metrics::kWorkerClientAdded_WorkerType)
              .ToUkmMetricHash(),
          static_cast<int64_t>(worker_type_),
      },
  };

  recorder->AddEntry(ukm::mojom::UkmEntry::New(
      source_id_, ukm::builders::Identifiability::kEntryNameHash, metrics));
}

}  // namespace blink

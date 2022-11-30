// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_study_document_created.h"

#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace blink {

IdentifiabilityStudyDocumentCreated::IdentifiabilityStudyDocumentCreated(
    ukm::SourceIdObj source_id)
    : source_id_(source_id.ToInt64()) {}

IdentifiabilityStudyDocumentCreated::IdentifiabilityStudyDocumentCreated(
    ukm::SourceId source_id)
    : source_id_(source_id) {}

IdentifiabilityStudyDocumentCreated::~IdentifiabilityStudyDocumentCreated() =
    default;

IdentifiabilityStudyDocumentCreated&
IdentifiabilityStudyDocumentCreated::SetNavigationSourceId(
    ukm::SourceId navigation_source_id) {
  navigation_source_id_ = navigation_source_id;
  return *this;
}

IdentifiabilityStudyDocumentCreated&
IdentifiabilityStudyDocumentCreated::SetIsMainFrame(bool is_main_frame) {
  is_main_frame_ = is_main_frame;
  return *this;
}

IdentifiabilityStudyDocumentCreated&
IdentifiabilityStudyDocumentCreated::SetIsCrossSiteFrame(
    bool is_cross_site_frame) {
  is_cross_site_frame_ = is_cross_site_frame;
  return *this;
}

IdentifiabilityStudyDocumentCreated&
IdentifiabilityStudyDocumentCreated::SetIsCrossOriginFrame(
    bool is_cross_origin_frame) {
  is_cross_origin_frame_ = is_cross_origin_frame;
  return *this;
}

void IdentifiabilityStudyDocumentCreated::Record(ukm::UkmRecorder* recorder) {
  using Metrics = blink::IdentifiableSurface::ReservedSurfaceMetrics;
  base::flat_map<uint64_t, int64_t> metrics = {
      {IdentifiableSurface::FromTypeAndToken(
           blink::IdentifiableSurface::Type::kReservedInternal,
           Metrics::kDocumentCreated_IsCrossOriginFrame)
           .ToUkmMetricHash(),
       is_cross_origin_frame_},
      {IdentifiableSurface::FromTypeAndToken(
           blink::IdentifiableSurface::Type::kReservedInternal,
           Metrics::kDocumentCreated_IsCrossSiteFrame)
           .ToUkmMetricHash(),
       is_cross_site_frame_},
      {IdentifiableSurface::FromTypeAndToken(
           blink::IdentifiableSurface::Type::kReservedInternal,
           Metrics::kDocumentCreated_IsMainFrame)
           .ToUkmMetricHash(),
       is_main_frame_},
      {IdentifiableSurface::FromTypeAndToken(
           blink::IdentifiableSurface::Type::kReservedInternal,
           Metrics::kDocumentCreated_NavigationSourceId)
           .ToUkmMetricHash(),
       navigation_source_id_}};

  recorder->AddEntry(ukm::mojom::UkmEntry::New(
      source_id_, ukm::builders::Identifiability::kEntryNameHash, metrics));
}

}  // namespace blink

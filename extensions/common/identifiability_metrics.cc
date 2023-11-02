// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/identifiability_metrics.h"

#include "extensions/common/extension_set.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"

namespace extensions {

blink::IdentifiableSurface SurfaceForExtension(
    blink::IdentifiableSurface::Type type,
    const ExtensionId& extension_id) {
  return blink::IdentifiableSurface::FromTypeAndToken(
      type, base::as_bytes(base::make_span(extension_id)));
}

void RecordExtensionResourceAccessResult(ukm::SourceIdObj ukm_source_id,
                                         const GURL& gurl,
                                         ExtensionResourceAccessResult result) {
  if (ukm_source_id == ukm::kInvalidSourceIdObj)
    return;

  ExtensionId extension_id = ExtensionSet::GetExtensionIdByURL(gurl);
  blink::IdentifiabilityMetricBuilder(ukm_source_id)
      .Add(SurfaceForExtension(
               blink::IdentifiableSurface::Type::kExtensionFileAccess,
               extension_id),
           result)
      .Record(ukm::UkmRecorder::Get());
}

void RecordContentScriptInjection(ukm::SourceIdObj ukm_source_id,
                                  const ExtensionId& extension_id) {
  if (ukm_source_id == ukm::kInvalidSourceIdObj)
    return;
  blink::IdentifiabilityMetricBuilder(ukm_source_id)
      .Add(SurfaceForExtension(
               blink::IdentifiableSurface::Type::kExtensionContentScript,
               extension_id),
           /* Succeeded= */ true)
      .Record(ukm::UkmRecorder::Get());
}

void RecordNetworkRequestBlocked(ukm::SourceIdObj ukm_source_id,
                                 const ExtensionId& extension_id) {
  if (ukm_source_id == ukm::kInvalidSourceIdObj)
    return;
  blink::IdentifiabilityMetricBuilder(ukm_source_id)
      .Add(SurfaceForExtension(
               blink::IdentifiableSurface::Type::kExtensionCancelRequest,
               extension_id),
           /* Succeeded= */ true)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace extensions

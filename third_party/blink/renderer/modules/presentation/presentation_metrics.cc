// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_metrics.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"

namespace blink {

// static
void PresentationMetrics::RecordPresentationConnectionResult(
    PresentationRequest* request,
    bool success) {
  if (!request)
    return;

  // Only record when |request| has at least one Presentation URL with "cast:"
  // scheme.
  bool has_cast_protocol = false;
  for (auto url : request->Urls()) {
    if (url.ProtocolIs("cast")) {
      has_cast_protocol = true;
      break;
    }
  }
  if (!has_cast_protocol)
    return;

  ExecutionContext* execution_context = request->GetExecutionContext();
  auto* ukm_recorder = execution_context->UkmRecorder();
  const ukm::SourceId source_id = execution_context->UkmSourceID();
  ukm::builders::Presentation_StartResult(source_id)
      .SetPresentationRequest(success)
      .Record(ukm_recorder);
}

}  // namespace blink

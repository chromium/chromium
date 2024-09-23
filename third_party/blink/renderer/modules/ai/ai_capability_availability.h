// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CAPABILITY_AVAILABILITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CAPABILITY_AVAILABILITY_H_

#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_capability_availability.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"

namespace blink {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AICapabilityAvailability)
enum class AICapabilityAvailability {
  kReadily = 0,
  kAfterDownload = 1,
  kNo = 2,

  kMaxValue = kNo,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:AICapabilityAvailability)

V8AICapabilityAvailability AICapabilityAvailabilityToV8(
    AICapabilityAvailability availability);

AICapabilityAvailability HandleModelAvailabilityCheckResult(
    ExecutionContext* execution_context,
    AIMetrics::AISessionType session_type,
    mojom::blink::ModelAvailabilityCheckResult result);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CAPABILITY_AVAILABILITY_H_

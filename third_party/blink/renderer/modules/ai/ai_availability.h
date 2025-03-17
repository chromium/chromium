// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_AVAILABILITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_AVAILABILITY_H_

#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_availability.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/platform/language_detection/language_detection_model.h"

namespace blink {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AIAvailability)
enum class AIAvailability {
  kUnavailable = 0,
  kDownloadable = 1,
  kDownloading = 2,
  kAvailable = 3,

  kMaxValue = kAvailable,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:AIAvailability)

V8AIAvailability AIAvailabilityToV8(AIAvailability availability);

AIAvailability HandleModelAvailabilityCheckResult(
    ExecutionContext* execution_context,
    AIMetrics::AISessionType session_type,
    mojom::blink::ModelAvailabilityCheckResult result);

AIAvailability HandleTranslatorAvailabilityCheckResult(
    ExecutionContext* execution_context,
    mojom::blink::CanCreateTranslatorResult result);

AIAvailability HandleLanguageDetectionModelCheckResult(
    ExecutionContext* execution_context,
    language_detection::mojom::blink::LanguageDetectionModelStatus result);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_AVAILABILITY_H_

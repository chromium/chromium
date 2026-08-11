// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AVAILABILITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AVAILABILITY_H_

#include "components/language_detection/content/common/language_detection.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_availability.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(Availability)
enum class Availability {
  kUnavailable = 0,
  kDownloadable = 1,
  kDownloading = 2,
  kAvailable = 3,

  kMaxValue = kAvailable,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:Availability)

MODULES_EXPORT V8Availability AvailabilityToV8(Availability availability);

MODULES_EXPORT Availability ConvertModelAvailabilityCheckResult(
    mojom::blink::ModelAvailabilityCheckResult result);

MODULES_EXPORT Availability HandleModelAvailabilityCheckResult(
    ExecutionContext* execution_context,
    AIMetrics::AISessionType session_type,
    mojom::blink::ModelAvailabilityCheckResult result);

MODULES_EXPORT Availability HandleTranslatorAvailabilityCheckResult(
    ExecutionContext* execution_context,
    mojom::blink::CanCreateTranslatorResult result);

MODULES_EXPORT Availability HandleLanguageDetectionModelCheckResult(
    ExecutionContext* execution_context,
    language_detection::mojom::blink::LanguageDetectionModelStatus result);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AVAILABILITY_H_

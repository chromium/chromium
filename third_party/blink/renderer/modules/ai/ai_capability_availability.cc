// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

namespace blink {

AICapabilityAvailability HandleModelAvailabilityCheckResult(
    ExecutionContext* execution_context,
    AIMetrics::AISessionType session_type,
    mojom::blink::ModelAvailabilityCheckResult result) {
  AICapabilityAvailability availability;
  if (result == mojom::blink::ModelAvailabilityCheckResult::kReadily) {
    availability = AICapabilityAvailability::kReadily;
  } else if (result ==
             mojom::blink::ModelAvailabilityCheckResult::kAfterDownload) {
    // TODO(crbug.com/345357441): Implement the
    // `ontextmodeldownloadprogress` event.
    availability = AICapabilityAvailability::kAfterDownload;
  } else {
    // If the text session cannot be created, logs the error message to
    // the console.
    availability = AICapabilityAvailability::kNo;
    execution_context->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        ConvertModelAvailabilityCheckResultToDebugString(result));
  }
  base::UmaHistogramEnumeration(
      AIMetrics::GetAICapabilityAvailabilityMetricName(session_type),
      availability);
  return availability;
}

V8AICapabilityAvailability AICapabilityAvailabilityToV8(
    AICapabilityAvailability availability) {
  switch (availability) {
    case AICapabilityAvailability::kReadily:
      return V8AICapabilityAvailability(
          V8AICapabilityAvailability::Enum::kReadily);
    case AICapabilityAvailability::kAfterDownload:
      return V8AICapabilityAvailability(
          V8AICapabilityAvailability::Enum::kAfterDownload);
    case AICapabilityAvailability::kNo:
      return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
  }
}

}  // namespace blink

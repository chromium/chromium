// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_availability.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

namespace blink {

using mojom::blink::ModelAvailabilityCheckResult;

AIAvailability HandleModelAvailabilityCheckResult(
    ExecutionContext* execution_context,
    AIMetrics::AISessionType session_type,
    ModelAvailabilityCheckResult result) {
  AIAvailability availability = AIAvailability::kUnavailable;
  switch (result) {
    case ModelAvailabilityCheckResult::kAvailable: {
      availability = AIAvailability::kAvailable;
      break;
    }
    case ModelAvailabilityCheckResult::kDownloadable: {
      availability = AIAvailability::kDownloadable;
      break;
    }
    case ModelAvailabilityCheckResult::kDownloading: {
      availability = AIAvailability::kDownloading;
      break;
    }
    default: {
      // If the text session cannot be created, logs the error message to
      // the console.
      availability = AIAvailability::kUnavailable;
      execution_context->AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          ConvertModelAvailabilityCheckResultToDebugString(result));
    }
  }
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAvailabilityMetricName(session_type), availability);
  return availability;
}

AIAvailability HandleTranslatorAvailabilityCheckResult(
    ExecutionContext* execution_context,
    mojom::blink::CanCreateTranslatorResult result) {
  switch (result) {
    case mojom::blink::CanCreateTranslatorResult::kReadily:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kTranslator,
          mojom::blink::ModelAvailabilityCheckResult::kAvailable);
    case mojom::blink::CanCreateTranslatorResult::kAfterDownloadLibraryNotReady:
    case mojom::blink::CanCreateTranslatorResult::
        kAfterDownloadLanguagePackNotReady:
    case mojom::blink::CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kTranslator,
          mojom::blink::ModelAvailabilityCheckResult::kDownloadable);
    case mojom::blink::CanCreateTranslatorResult::kNoNotSupportedLanguage:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kTranslator,
          mojom::blink::ModelAvailabilityCheckResult::
              kUnavailableUnsupportedLanguage);
    case mojom::blink::CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed:
    case mojom::blink::CanCreateTranslatorResult::
        kNoExceedsLanguagePackCountLimitation:
    case mojom::blink::CanCreateTranslatorResult::kNoServiceCrashed:
    case mojom::blink::CanCreateTranslatorResult::kNoDisallowedByPolicy:
    case mojom::blink::CanCreateTranslatorResult::
        kNoExceedsServiceCountLimitation:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kTranslator,
          mojom::blink::ModelAvailabilityCheckResult::
              kUnavailableTranslationNotEligible);
  }
}

AIAvailability HandleLanguageDetectionModelCheckResult(
    ExecutionContext* execution_context,
    language_detection::mojom::blink::LanguageDetectionModelStatus result) {
  switch (result) {
    case language_detection::mojom::blink::LanguageDetectionModelStatus::
        kReadily:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kLanguageDetector,
          mojom::blink::ModelAvailabilityCheckResult::kAvailable);
    case language_detection::mojom::blink::LanguageDetectionModelStatus::
        kAfterDownload:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kLanguageDetector,
          mojom::blink::ModelAvailabilityCheckResult::kDownloadable);
    case language_detection::mojom::blink::LanguageDetectionModelStatus::
        kNotAvailable:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kLanguageDetector,
          mojom::blink::ModelAvailabilityCheckResult::
              kUnavailableLanguageDetectionModelNotAvailable);
  }
}

V8AIAvailability AIAvailabilityToV8(AIAvailability availability) {
  switch (availability) {
    case AIAvailability::kUnavailable:
      return V8AIAvailability(V8AIAvailability::Enum::kUnavailable);
    case AIAvailability::kDownloadable:
      return V8AIAvailability(V8AIAvailability::Enum::kDownloadable);
    case AIAvailability::kDownloading:
      return V8AIAvailability(V8AIAvailability::Enum::kDownloading);
    case AIAvailability::kAvailable:
      return V8AIAvailability(V8AIAvailability::Enum::kAvailable);
  }
}

}  // namespace blink

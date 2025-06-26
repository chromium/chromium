// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/availability.h"

#include "base/metrics/histogram_functions.h"
#include "components/language_detection/content/common/language_detection.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

namespace blink {

using mojom::blink::ModelAvailabilityCheckResult;

Availability ConvertModelAvailabilityCheckResult(
    ModelAvailabilityCheckResult result) {
  switch (result) {
    case ModelAvailabilityCheckResult::kAvailable:
      return Availability::kAvailable;
    case ModelAvailabilityCheckResult::kDownloadable:
      return Availability::kDownloadable;
    case ModelAvailabilityCheckResult::kDownloading:
      return Availability::kDownloading;
    case ModelAvailabilityCheckResult::kUnavailableServiceNotRunning:
    case ModelAvailabilityCheckResult::kUnavailableUnsupportedLanguage:
    case ModelAvailabilityCheckResult::kUnavailableUnknown:
    case ModelAvailabilityCheckResult::kUnavailableFeatureNotEnabled:
    case ModelAvailabilityCheckResult::kUnavailableConfigNotAvailableForFeature:
    case ModelAvailabilityCheckResult::kUnavailableGpuBlocked:
    case ModelAvailabilityCheckResult::kUnavailableTooManyRecentCrashes:
    case ModelAvailabilityCheckResult::kUnavailableSafetyModelNotAvailable:
    case ModelAvailabilityCheckResult::
        kUnavailableSafetyConfigNotAvailableForFeature:
    case ModelAvailabilityCheckResult::
        kUnavailableLanguageDetectionModelNotAvailable:
    case ModelAvailabilityCheckResult::kUnavailableFeatureExecutionNotEnabled:
    case ModelAvailabilityCheckResult::kUnavailableModelAdaptationNotAvailable:
    case ModelAvailabilityCheckResult::kUnavailableValidationPending:
    case ModelAvailabilityCheckResult::kUnavailableValidationFailed:
    case ModelAvailabilityCheckResult::kUnavailableModelNotEligible:
    case ModelAvailabilityCheckResult::kUnavailableInsufficientDiskSpace:
    case ModelAvailabilityCheckResult::kUnavailableTranslationNotEligible:
    case ModelAvailabilityCheckResult::kUnavailableEnterprisePolicyDisabled:
      return Availability::kUnavailable;
  }
}

Availability HandleModelAvailabilityCheckResult(
    ExecutionContext* execution_context,
    AIMetrics::AISessionType session_type,
    ModelAvailabilityCheckResult result) {
  Availability availability = ConvertModelAvailabilityCheckResult(result);
  if (availability == Availability::kUnavailable) {
    execution_context->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        ConvertModelAvailabilityCheckResultToDebugString(result));
  }
  base::UmaHistogramEnumeration(
      AIMetrics::GetAvailabilityMetricName(session_type), availability);
  return availability;
}

Availability HandleTranslatorAvailabilityCheckResult(
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
    case mojom::blink::CanCreateTranslatorResult::
        kAfterDownloadTranslatorCreationRequired:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kTranslator,
          mojom::blink::ModelAvailabilityCheckResult::kDownloadable);
    case mojom::blink::CanCreateTranslatorResult::kNoNotSupportedLanguage:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kTranslator,
          mojom::blink::ModelAvailabilityCheckResult::
              kUnavailableUnsupportedLanguage);
    case mojom::blink::CanCreateTranslatorResult::kNoServiceCrashed:
    case mojom::blink::CanCreateTranslatorResult::kNoDisallowedByPolicy:
    case mojom::blink::CanCreateTranslatorResult::
        kNoExceedsServiceCountLimitation:
    case mojom::blink::CanCreateTranslatorResult::kNoInvalidStoragePartition:
      return HandleModelAvailabilityCheckResult(
          execution_context, AIMetrics::AISessionType::kTranslator,
          mojom::blink::ModelAvailabilityCheckResult::
              kUnavailableTranslationNotEligible);
  }
}

Availability HandleLanguageDetectionModelCheckResult(
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

V8Availability AvailabilityToV8(Availability availability) {
  switch (availability) {
    case Availability::kUnavailable:
      return V8Availability(V8Availability::Enum::kUnavailable);
    case Availability::kDownloadable:
      return V8Availability(V8Availability::Enum::kDownloadable);
    case Availability::kDownloading:
      return V8Availability(V8Availability::Enum::kDownloading);
    case Availability::kAvailable:
      return V8Availability(V8Availability::Enum::kAvailable);
  }
}

}  // namespace blink

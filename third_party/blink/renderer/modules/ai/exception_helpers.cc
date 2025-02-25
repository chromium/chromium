// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

#include "base/debug/dump_without_crashing.h"
#include "base/notreached.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

const char kExceptionMessageExecutionContextInvalid[] =
    "The execution context is not valid.";
const char kExceptionMessageServiceUnavailable[] =
    "Model execution service is not available.";

const char kExceptionMessagePermissionDenied[] =
    "A user permission error occurred, such as not signed-in or not "
    "allowed to execute model.";
const char kExceptionMessageGenericError[] = "Other generic failures occurred.";
const char kExceptionMessageFiltered[] =
    "The execution yielded an unsafe response.";
const char kExceptionMessageOutputLanguageFiltered[] =
    "The model attempted to output text in an untested language, and was "
    "prevented from doing so.";
const char kExceptionMessageResponseLowQuality[] =
    "The model attempted to output text with low quality, and was prevented "
    "from doing so.";
const char kExceptionMessageDisabled[] = "The response was disabled.";
const char kExceptionMessageCancelled[] = "The request was cancelled.";
const char kExceptionMessageSessionDestroyed[] =
    "The model execution session has been destroyed.";
const char kExceptionMessageRequestAborted[] = "The request has been aborted.";
const char kExceptionRequestTooLarge[] = "The prompt request is too large.";

const char kExceptionMessageInvalidTemperatureAndTopKFormat[] =
    "Initializing a new session must either specify both topK and temperature, "
    "or neither of them.";
const char kExceptionMessageInvalidTopK[] =
    "The topK value provided is invalid.";
const char kExceptionMessageInvalidTemperature[] =
    "The temperature value provided is invalid.";
const char kExceptionMessageUnableToCreateSession[] =
    "The session cannot be created.";
const char kExceptionMessageInitialPromptTooLarge[] =
    "The initial prompts / system prompts are too large to fit in the "
    "context.";
const char kExceptionMessageUnableToCloneSession[] =
    "The session cannot be cloned.";
const char kExceptionMessageSystemPromptIsDefinedMultipleTimes[] =
    "The system prompt should not be defined in both systemPrompt and "
    "initialPrompts.";
const char kExceptionMessageSystemPromptIsNotTheFirst[] =
    "The prompt with 'system' role must be placed at the first entry of "
    "initialPrompts.";
const char kExceptionMessageUnsupportedLanguages[] =
    "The specified languages are not supported.";

void ThrowInvalidContextException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    kExceptionMessageExecutionContextInvalid);
}

void ThrowSessionDestroyedException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    kExceptionMessageSessionDestroyed);
}

void ThrowAbortedException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                    kExceptionMessageRequestAborted);
}

void RejectPromiseWithInternalError(ScriptPromiseResolverBase* resolver) {
  if (resolver) {
    resolver->Reject(CreateInternalErrorException());
  }
}

DOMException* CreateInternalErrorException() {
  return DOMException::Create(
      kExceptionMessageServiceUnavailable,
      DOMException::GetErrorName(DOMExceptionCode::kOperationError));
}

bool HandleAbortSignal(AbortSignal* signal,
                       ScriptState* script_state,
                       ExceptionState& exception_state) {
  if (signal && signal->aborted()) {
    auto reason = signal->reason(script_state);
    if (reason.IsEmpty()) {
      ThrowAbortedException(exception_state);
    } else {
      V8ThrowException::ThrowException(script_state->GetIsolate(),
                                       reason.V8Value());
    }
    return true;
  }

  return false;
}

namespace {
// Create an UnknownError exception, include `error` in the exception
// message. This is intended for handling values of
// `ModelStreamingResponseStatus` that we do not expect to ever see when
// using an on-device model, e.g. errors related to servers.
DOMException* CreateUnknown(const char* error) {
  return DOMException::Create(
      String("An unknown error occurred: ") + error,
      DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
}
}  // namespace

DOMException* ConvertModelStreamingResponseErrorToDOMException(
    ModelStreamingResponseStatus error) {
  switch (error) {
    case ModelStreamingResponseStatus::kErrorUnknown:
      base::debug::DumpWithoutCrashing();
      return CreateUnknown("kErrorUnknown");
    case ModelStreamingResponseStatus::kErrorInvalidRequest:
      base::debug::DumpWithoutCrashing();
      return CreateUnknown("kErrorInvalidRequest");
    case ModelStreamingResponseStatus::kErrorRequestThrottled:
      base::debug::DumpWithoutCrashing();
      return CreateUnknown("kErrorRequestThrottled");
    case ModelStreamingResponseStatus::kErrorPermissionDenied:
      return DOMException::Create(
          kExceptionMessagePermissionDenied,
          DOMException::GetErrorName(DOMExceptionCode::kNotAllowedError));
    case ModelStreamingResponseStatus::kErrorGenericFailure:
      return DOMException::Create(
          kExceptionMessageGenericError,
          DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
    case ModelStreamingResponseStatus::kErrorRetryableError:
      base::debug::DumpWithoutCrashing();
      return CreateUnknown("kErrorRetryableError");
    case ModelStreamingResponseStatus::kErrorNonRetryableError:
      base::debug::DumpWithoutCrashing();
      return CreateUnknown("kErrorNonRetryableError");
    case ModelStreamingResponseStatus::kErrorUnsupportedLanguage:
      return DOMException::Create(
          kExceptionMessageOutputLanguageFiltered,
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError));
    case ModelStreamingResponseStatus::kErrorFiltered:
      return DOMException::Create(
          kExceptionMessageFiltered,
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorDisabled:
      return DOMException::Create(
          kExceptionMessageDisabled,
          DOMException::GetErrorName(DOMExceptionCode::kAbortError));
    case ModelStreamingResponseStatus::kErrorCancelled:
      return DOMException::Create(
          kExceptionMessageCancelled,
          DOMException::GetErrorName(DOMExceptionCode::kAbortError));
    case ModelStreamingResponseStatus::kErrorSessionDestroyed:
      return DOMException::Create(
          kExceptionMessageSessionDestroyed,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError));
    case ModelStreamingResponseStatus::kErrorPromptRequestTooLarge:
      return DOMException::Create(
          kExceptionRequestTooLarge,
          DOMException::GetErrorName(DOMExceptionCode::kQuotaExceededError));
    case ModelStreamingResponseStatus::kErrorResponseLowQuality:
      return DOMException::Create(
          kExceptionMessageResponseLowQuality,
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError));
    case ModelStreamingResponseStatus::kOngoing:
    case ModelStreamingResponseStatus::kComplete:
      NOTREACHED();
  }
  NOTREACHED();
}

// LINT.IfChange(ConvertModelAvailabilityCheckResultToDebugString)
WTF::String ConvertModelAvailabilityCheckResultToDebugString(
    mojom::blink::ModelAvailabilityCheckResult result) {
  switch (result) {
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableServiceNotRunning:
      return "Unable to create a text session because the service is not "
             "running.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableUnsupportedLanguage:
      return "The requested language options are not supported.";
    case mojom::blink::ModelAvailabilityCheckResult::kUnavailableUnknown:
      return "The service is unable to create new session.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableFeatureNotEnabled:
      return "The feature flag gating model execution was disabled.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableConfigNotAvailableForFeature:
      return "The model was available but there was not an execution config "
             "available for the feature.";
    case mojom::blink::ModelAvailabilityCheckResult::kUnavailableGpuBlocked:
      return "The GPU is blocked.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableTooManyRecentCrashes:
      return "The model process crashed too many times for this version.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableSafetyModelNotAvailable:
      return "The safety model was required but not available.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableSafetyConfigNotAvailableForFeature:
      return "The safety model was available but there was not a safety config "
             "available for the feature.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableLanguageDetectionModelNotAvailable:
      return "The language detection model was required but not available.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableFeatureExecutionNotEnabled:
      return "Model execution for this feature was not enabled.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableValidationPending:
      return "Model validation is still pending.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableValidationFailed:
      return "Model validation failed.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableModelNotEligible:
      return "The device is not eligible for running on-device model.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableInsufficientDiskSpace:
      return "The device does not have enough space for downloading the "
             "on-device model";
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableTranslationNotEligible:
      return "The on-device translation is not available.";
    case mojom::blink::ModelAvailabilityCheckResult::kAvailable:
    case mojom::blink::ModelAvailabilityCheckResult::kDownloadable:
    case mojom::blink::ModelAvailabilityCheckResult::kDownloading:
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableModelAdaptationNotAvailable:
      NOTREACHED();
  }
  NOTREACHED();
}
// LINT.ThenChange(//third_party/blink/public/mojom/ai/ai_manager.mojom:ModelAvailabilityCheckResult)

}  // namespace blink

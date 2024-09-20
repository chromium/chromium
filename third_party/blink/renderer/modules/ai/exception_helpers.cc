// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

#include "base/debug/dump_without_crashing.h"
#include "base/notreached.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

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
    "The execution yielded a bad response.";
const char kExceptionMessageDisabled[] = "The response was disabled.";
const char kExceptionMessageCancelled[] = "The request was canceled.";
const char kExceptionMessageSessionDestroyed[] =
    "The model execution session has been destroyed.";
const char kExceptionMessageRequestAborted[] = "The request has been aborted.";

const char kExceptionMessageInvalidTemperatureAndTopKFormat[] =
    "Initializing a new session must either specify both topK and temperature, "
    "or neither of them.";
const char kExceptionMessageUnableToCreateSession[] =
    "The session cannot be created.";
const char kExceptionMessageUnableToCloneSession[] =
    "The session cannot be cloned.";
const char kExceptionMessageSystemPromptAndInitialPromptsExist[] =
    "The systemPrompt and initialPrompts should not present at the same time.";
const char kExceptionMessageSystemPromptIsNotTheFirst[] =
    "The prompt with 'system' role must be placed at the first entry of "
    "initialPrompts.";

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
  resolver->Reject(CreateInternalErrorException());
}

DOMException* CreateInternalErrorException() {
  return DOMException::Create(
      kExceptionMessageServiceUnavailable,
      DOMException::GetErrorName(DOMExceptionCode::kOperationError));
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
      base::debug::DumpWithoutCrashing();
      return CreateUnknown("kErrorUnsupportedLanguage");
    case ModelStreamingResponseStatus::kErrorFiltered:
      return DOMException::Create(
          kExceptionMessageFiltered,
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorDisabled:
      return DOMException::Create(
          kExceptionMessageDisabled,
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorCancelled:
      return DOMException::Create(
          kExceptionMessageCancelled,
          DOMException::GetErrorName(DOMExceptionCode::kAbortError));
    case ModelStreamingResponseStatus::kErrorSessionDestroyed:
      return DOMException::Create(
          kExceptionMessageSessionDestroyed,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError));
    case ModelStreamingResponseStatus::kOngoing:
    case ModelStreamingResponseStatus::kComplete:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED();
}

// LINT.IfChange(ConvertModelAvailabilityCheckResultToDebugString)
WTF::String ConvertModelAvailabilityCheckResultToDebugString(
    mojom::blink::ModelAvailabilityCheckResult result) {
  switch (result) {
    case mojom::blink::ModelAvailabilityCheckResult::kNoServiceNotRunning:
      return "Unable to create a text session because the service is not "
             "running.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoUnknown:
      return "The service is unable to create new session.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoFeatureNotEnabled:
      return "The feature flag gating model execution was disabled.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoModelNotAvailable:
      return "There was no model available.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kNoConfigNotAvailableForFeature:
      return "The model was available but there was not an execution config "
             "available for the feature.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoGpuBlocked:
      return "The GPU is blocked.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoTooManyRecentCrashes:
      return "The model process crashed too many times for this version.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoTooManyRecentTimeouts:
      return "The model took too long too many times for this version.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoSafetyModelNotAvailable:
      return "The safety model was required but not available.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kNoSafetyConfigNotAvailableForFeature:
      return "The safety model was available but there was not a safety config "
             "available for the feature.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kNoLanguageDetectionModelNotAvailable:
      return "The language detection model was required but not available.";
    case mojom::blink::ModelAvailabilityCheckResult::
        kNoFeatureExecutionNotEnabled:
      return "Model execution for this feature was not enabled.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoValidationPending:
      return "Model validation is still pending.";
    case mojom::blink::ModelAvailabilityCheckResult::kNoValidationFailed:
      return "Model validation failed.";
    case mojom::blink::ModelAvailabilityCheckResult::kReadily:
    case mojom::blink::ModelAvailabilityCheckResult::kAfterDownload:
    case mojom::blink::ModelAvailabilityCheckResult::
        kNoModelAdaptationNotAvailable:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED();
}
// LINT.ThenChange(//third_party/blink/public/mojom/ai_manager.mojom:ModelAvailabilityCheckResult)

}  // namespace blink

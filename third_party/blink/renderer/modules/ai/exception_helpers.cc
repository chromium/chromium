// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

#include "base/debug/dump_without_crashing.h"
#include "base/notreached.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_quota_exceeded_error_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/quota_exceeded_error.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

const char kExceptionMessageExecutionContextInvalid[] =
    "The execution context is not valid.";
const char kExceptionMessageServiceUnavailable[] =
    "Model execution service is not available.";
const char kExceptionMessageDocumentNotActive[] = "The document is not active.";

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
const char kExceptionMessageInputTooLarge[] = "The input is too large.";

const char kExceptionMessageInvalidTemperatureAndTopKFormat[] =
    "Initializing a new session must either specify both topK and temperature, "
    "or neither of them.";
const char kExceptionMessageInvalidTopK[] =
    "The topK value provided is invalid.";
const char kExceptionMessageInvalidTemperature[] =
    "The temperature value provided is invalid.";
const char kExceptionMessageUnableToCreateSession[] =
    "The device is unable to create a session to run the model. "
    "Please check the result of availability() first.";
const char kExceptionMessageUnableToCloneSession[] =
    "The session cannot be cloned.";
const char kExceptionMessageUnableToCalculateUsage[] =
    "The usage cannot be calculated.";
const char kExceptionMessagePromptWithSystemRoleIsNotTheFirst[] =
    "The prompt with 'system' role must be placed at the first entry of "
    "initialPrompts.";
const char kExceptionMessageUnsupportedLanguages[] =
    "The specified languages are not supported.";
const char kExceptionMessageInvalidResponseJsonSchema[] =
    "Response json schema is invalid - it should be an object that can be "
    "stringified into a JSON string.";
const char kExceptionMessagePermissionPolicy[] =
    "Access denied because the Permission Policy is not enabled.";
const char kExceptionMessageUserActivationRequired[] =
    "Requires a user gesture when availability is \"downloading\" or "
    "\"downloadable\".";

void ThrowInvalidContextException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    kExceptionMessageExecutionContextInvalid);
}

void ThrowDocumentNotActiveException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    kExceptionMessageDocumentNotActive);
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

DOMException* CreateSessionDestroyedException() {
  return DOMException::Create(
      kExceptionMessageSessionDestroyed,
      DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError));
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

bool ValidateScriptState(ScriptState* script_state,
                         ExceptionState& exception_state,
                         bool permit_workers) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return false;
  }

  ExecutionContext* context = ExecutionContext::From(script_state);

  if (context->IsServiceWorkerGlobalScope()) {
    return permit_workers;
  }

  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);

  // Realm’s global object must be a Window object.
  CHECK(window);

  // If document is not fully active, then return a promise rejected with an
  // "InvalidStateError" DOMException.
  Document* document = window->document();
  CHECK(document);
  if (!document->IsActive()) {
    ThrowDocumentNotActiveException(exception_state);
    return false;
  }

  return true;
}

String ValidateAndStringifyObject(const ScriptValue& input,
                                  ScriptState* script_state,
                                  ExceptionState& exception_state) {
  v8::Local<v8::String> value;
  if (!input.V8Value()->IsObject() ||
      !v8::JSON::Stringify(script_state->GetContext(),
                           input.V8Value().As<v8::Object>())
           .ToLocal(&value)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        kExceptionMessageInvalidResponseJsonSchema);
    return String();
  }
  return ToBlinkString<String>(script_state->GetIsolate(), value,
                               kDoNotExternalize);
}

namespace {
// Create an UnknownError exception, include `error` in the exception
// message. This is intended for handling values of
// `ModelStreamingResponseStatus` that we do not expect to ever see when
// using an on-device model, e.g. errors related to servers.
DOMException* CreateUnknown(const char* error) {
  return DOMException::Create(
      StrCat({"An unknown error occurred: ", error}),
      DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
}
}  // namespace

DOMException* ConvertModelStreamingResponseErrorToDOMException(
    ModelStreamingResponseStatus error,
    mojom::blink::QuotaErrorInfoPtr quota_error_info) {
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
    case ModelStreamingResponseStatus::kErrorInputTooLarge:
      if (RuntimeEnabledFeatures::QuotaExceededErrorUpdateEnabled()) {
        CHECK(quota_error_info);
        auto* options = MakeGarbageCollected<QuotaExceededErrorOptions>();
        options->setQuota(static_cast<double>(quota_error_info->quota));
        options->setRequested(static_cast<double>(quota_error_info->requested));
        return QuotaExceededError::Create(kExceptionMessageInputTooLarge,
                                          std::move(options));
      }
      return DOMException::Create(
          kExceptionMessageInputTooLarge,
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
String ConvertModelAvailabilityCheckResultToDebugString(
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
        kUnavailableModelAdaptationNotAvailable:
      return "Model capability is not available.";
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
    case mojom::blink::ModelAvailabilityCheckResult::
        kUnavailableEnterprisePolicyDisabled:
      return "The on-device model is not available because the enterprise "
             "policy disables the feature.";
    case mojom::blink::ModelAvailabilityCheckResult::kAvailable:
    case mojom::blink::ModelAvailabilityCheckResult::kDownloadable:
    case mojom::blink::ModelAvailabilityCheckResult::kDownloading:
      NOTREACHED();
  }
  NOTREACHED();
}
// LINT.ThenChange(//third_party/blink/public/mojom/ai/ai_manager.mojom:ModelAvailabilityCheckResult)

}  // namespace blink

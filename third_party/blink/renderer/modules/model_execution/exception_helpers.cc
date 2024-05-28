// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/exception_helpers.h"

#include "third_party/blink/public/mojom/model_execution/model_session.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

const char kExceptionMessageExecutionContextInvalid[] =
    "The execution context is not valid.";
const char kExceptionMessageServiceUnavailable[] =
    "Model execution service is not available.";

const char kExceptionMessageUnknown[] = "An unknown error occurred.";
const char kExceptionMessageInvalidRequest[] = "The request was invalid.";
const char kExceptionMessageRequestThrottled[] = "The request was throttled.";
const char kExceptionMessagePermissionDenied[] =
    "A user permission error occurred, such as not signed-in or not "
    "allowed to execute model.";
const char kExceptionMessageGenericError[] = "Other generic failure occurred.";
const char kExceptionMessageRetryableError[] =
    "A retryable error occurred in the server.";
const char kExceptionMessageNonRetryableError[] =
    "A non-retryable error occurred in the server.";
const char kExceptionMessageUnsupportedLanguage[] =
    "The language was unsupported.";
const char kExceptionMessageFiltered[] =
    "The execution yielded a bad response.";
const char kExceptionMessageDisabled[] = "The response was disabled.";
const char kExceptionMessageCancelled[] = "The request was cancelled.";
const char kExceptionMessageSessionDestroyed[] =
    "The model execution session has been destroyed.";

const char kExceptionMessageInvalidTemperatureAndTopKFormat[] =
    "Initializing a new session must either specify both topK and temperature, "
    "or neither of them.";
const char kExceptionMessageUnableToCreateSession[] =
    "The session cannot be created.";

void ThrowInvalidContextException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    kExceptionMessageExecutionContextInvalid);
}

void RejectPromiseWithInternalError(ScriptPromiseResolverBase* resolver) {
  resolver->Reject(DOMException::Create(
      kExceptionMessageServiceUnavailable,
      DOMException::GetErrorName(DOMExceptionCode::kOperationError)));
}

DOMException* ConvertModelStreamingResponseErrorToDOMException(
    ModelStreamingResponseStatus error) {
  switch (error) {
    case ModelStreamingResponseStatus::kErrorUnknown:
      return DOMException::Create(
          kExceptionMessageUnknown,
          DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
    case ModelStreamingResponseStatus::kErrorInvalidRequest:
      return DOMException::Create(
          kExceptionMessageInvalidRequest,
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError));
    case ModelStreamingResponseStatus::kErrorRequestThrottled:
      return DOMException::Create(
          kExceptionMessageRequestThrottled,
          DOMException::GetErrorName(DOMExceptionCode::kQuotaExceededError));
    case ModelStreamingResponseStatus::kErrorPermissionDenied:
      return DOMException::Create(
          kExceptionMessagePermissionDenied,
          DOMException::GetErrorName(DOMExceptionCode::kNotAllowedError));
    case ModelStreamingResponseStatus::kErrorGenericFailure:
      return DOMException::Create(
          kExceptionMessageGenericError,
          DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
    case ModelStreamingResponseStatus::kErrorRetryableError:
      return DOMException::Create(
          kExceptionMessageRetryableError,
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorNonRetryableError:
      return DOMException::Create(
          kExceptionMessageNonRetryableError,
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorUnsupportedLanguage:
      return DOMException::Create(
          kExceptionMessageUnsupportedLanguage,
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError));
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
      return nullptr;
  }
}

}  // namespace blink

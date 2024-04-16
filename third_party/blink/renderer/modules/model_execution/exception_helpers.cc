// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/exception_helpers.h"

#include "third_party/blink/public/mojom/model_execution/model_session.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

void ThrowInvalidContextException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "The execution context is not valid.");
}

void RejectPromiseWithInternalError(ScriptPromiseResolverBase* resolver) {
  resolver->Reject(DOMException::Create(
      "Model execution service is not available.",
      DOMException::GetErrorName(DOMExceptionCode::kOperationError)));
}

DOMException* ConvertModelStreamingResponseErrorToDOMException(
    ModelStreamingResponseStatus error) {
  switch (error) {
    case ModelStreamingResponseStatus::kErrorUnknown:
      return DOMException::Create(
          "An unknown error occurred.",
          DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
    case ModelStreamingResponseStatus::kErrorInvalidRequest:
      return DOMException::Create(
          "The request was invalid.",
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError));
    case ModelStreamingResponseStatus::kErrorRequestThrottled:
      return DOMException::Create(
          "The request was throttled.",
          DOMException::GetErrorName(DOMExceptionCode::kQuotaExceededError));
    case ModelStreamingResponseStatus::kErrorPermissionDenied:
      return DOMException::Create(
          "A user permission error occurred, such as not signed-in or not "
          "allowed to execute model.",
          DOMException::GetErrorName(DOMExceptionCode::kNotAllowedError));
    case ModelStreamingResponseStatus::kErrorGenericFailure:
      return DOMException::Create(
          "Other generic failure occurred.",
          DOMException::GetErrorName(DOMExceptionCode::kUnknownError));
    case ModelStreamingResponseStatus::kErrorRetryableError:
      return DOMException::Create(
          "A retryable error occurred in the server.",
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorNonRetryableError:
      return DOMException::Create(
          "A non-retryable error occurred in the server.",
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorUnsupportedLanguage:
      return DOMException::Create(
          "The language was unsupported.",
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError));
    case ModelStreamingResponseStatus::kErrorFiltered:
      return DOMException::Create(
          "The execution yielded a bad response.",
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorDisabled:
      return DOMException::Create(
          "The response was disabled.",
          DOMException::GetErrorName(DOMExceptionCode::kNotReadableError));
    case ModelStreamingResponseStatus::kErrorCancelled:
      return DOMException::Create(
          "The request was cancelled.",
          DOMException::GetErrorName(DOMExceptionCode::kAbortError));
    case ModelStreamingResponseStatus::kOngoing:
    case ModelStreamingResponseStatus::kComplete:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink

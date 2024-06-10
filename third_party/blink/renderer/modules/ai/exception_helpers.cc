// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

#include "base/debug/dump_without_crashing.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-shared.h"
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
      return nullptr;
  }
}

}  // namespace blink

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_error.h"

#include "base/files/file.h"
#include "third_party/blink/public/common/native_io/native_io_utils.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

using blink::mojom::blink::NativeIOErrorPtr;
using blink::mojom::blink::NativeIOErrorType;

namespace blink {

namespace {

DOMExceptionCode NativeIOErrorToDOMExceptionCode(NativeIOErrorType error) {
  switch (error) {
    case NativeIOErrorType::kSuccess:
      // This function should only be called with an error.
      NOTREACHED();
      return DOMExceptionCode::kNoError;
    case NativeIOErrorType::kUnknown:
      return DOMExceptionCode::kUnknownError;
    case NativeIOErrorType::kInvalidState:
      return DOMExceptionCode::kInvalidStateError;
    case NativeIOErrorType::kNotFound:
      return DOMExceptionCode::kNotFoundError;
    case NativeIOErrorType::kNoModificationAllowed:
      return DOMExceptionCode::kNoModificationAllowedError;
    case NativeIOErrorType::kNoSpace:
      return DOMExceptionCode::kQuotaExceededError;
  }
  NOTREACHED();
  return DOMExceptionCode::kUnknownError;
}

}  // namespace

void RejectNativeIOWithError(ScriptPromiseResolver* resolver,
                             NativeIOErrorPtr error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);

  DOMExceptionCode exception_code =
      NativeIOErrorToDOMExceptionCode(error->type);
  resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
      script_state->GetIsolate(), exception_code, error->message));
  return;
}

void RejectNativeIOWithError(ScriptPromiseResolver* resolver,
                             base::File::Error file_error,
                             const String& message) {
  DCHECK(resolver->GetScriptState()->ContextIsValid())
      << "The resolver's script state must be valid.";
  RejectNativeIOWithError(resolver,
                          FileErrorToNativeIOError(file_error, message));
  return;
}

void ThrowNativeIOWithError(ExceptionState& exception_state,
                            NativeIOErrorPtr error) {
  DOMExceptionCode exception_code =
      NativeIOErrorToDOMExceptionCode(error->type);
  exception_state.ThrowDOMException(exception_code, error->message);
  return;
}

void ThrowNativeIOWithError(ExceptionState& exception_state,
                            base::File::Error file_error,
                            const String& message) {
  ThrowNativeIOWithError(exception_state,
                         FileErrorToNativeIOError(file_error, message));
  return;
}

NativeIOErrorPtr FileErrorToNativeIOError(base::File::Error file_error,
                                          const String& message) {
  NativeIOErrorType native_io_error_type =
      blink::native_io::FileErrorToNativeIOErrorType(file_error);
  String final_message =
      message.empty() ? String::FromUTF8(blink::native_io::GetDefaultMessage(
                                             native_io_error_type)
                                             .c_str())
                      : message;
  return mojom::blink::NativeIOError::New(native_io_error_type, final_message);
}
}  // namespace blink

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"

#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

using blink::WebServiceWorkerError;

namespace blink {

namespace {

struct ExceptionParams {
  ExceptionParams(DOMExceptionCode code,
                  const String& default_message = String(),
                  const String& message = String())
      : code(code), message(message.IsEmpty() ? default_message : message) {}

  DOMExceptionCode code;
  String message;
};

ExceptionParams GetExceptionParams(const WebServiceWorkerError& web_error) {
  switch (web_error.error_type) {
    case mojom::blink::ServiceWorkerErrorType::kAbort:
      return ExceptionParams(DOMExceptionCode::kAbortError,
                             "The Service Worker operation was aborted.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kActivate:
      // Not currently returned as a promise rejection.
      // TODO: Introduce new ActivateError type to ExceptionCodes?
      return ExceptionParams(DOMExceptionCode::kAbortError,
                             "The Service Worker activation failed.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kDisabled:
      return ExceptionParams(DOMExceptionCode::kNotSupportedError,
                             "Service Worker support is disabled.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kInstall:
      // TODO: Introduce new InstallError type to ExceptionCodes?
      return ExceptionParams(DOMExceptionCode::kAbortError,
                             "The Service Worker installation failed.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kScriptEvaluateFailed:
      return ExceptionParams(DOMExceptionCode::kAbortError,
                             "The Service Worker script failed to evaluate.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kNavigation:
      // ErrorTypeNavigation should have bailed out before calling this.
      NOTREACHED();
      return ExceptionParams(DOMExceptionCode::kUnknownError);
    case mojom::blink::ServiceWorkerErrorType::kNetwork:
      return ExceptionParams(DOMExceptionCode::kNetworkError,
                             "The Service Worker failed by network.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kNotFound:
      return ExceptionParams(
          DOMExceptionCode::kNotFoundError,
          "The specified Service Worker resource was not found.",
          web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kSecurity:
      return ExceptionParams(
          DOMExceptionCode::kSecurityError,
          "The Service Worker security policy prevented an action.",
          web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kState:
      return ExceptionParams(DOMExceptionCode::kInvalidStateError,
                             "The Service Worker state was not valid.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kTimeout:
      return ExceptionParams(DOMExceptionCode::kAbortError,
                             "The Service Worker operation timed out.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kUnknown:
      return ExceptionParams(DOMExceptionCode::kUnknownError,
                             "An unknown error occurred within Service Worker.",
                             web_error.message);
    case mojom::blink::ServiceWorkerErrorType::kNone:
    case mojom::blink::ServiceWorkerErrorType::kType:
      // ErrorTypeType should have been handled before reaching this point.
      NOTREACHED();
      return ExceptionParams(DOMExceptionCode::kUnknownError);
  }
  NOTREACHED();
  return ExceptionParams(DOMExceptionCode::kUnknownError);
}

}  // namespace

// static
DOMException* ServiceWorkerError::Take(ScriptPromiseResolver*,
                                       const WebServiceWorkerError& web_error) {
  ExceptionParams params = GetExceptionParams(web_error);
  return MakeGarbageCollected<DOMException>(params.code, params.message);
}

// static
DOMException* ServiceWorkerError::GetException(
    ScriptPromiseResolver* resolver,
    mojom::blink::ServiceWorkerErrorType error,
    const String& error_msg) {
  return Take(resolver, WebServiceWorkerError(error, error_msg));
}

// static
v8::Local<v8::Value> ServiceWorkerErrorForUpdate::Take(
    ScriptPromiseResolver* resolver,
    const WebServiceWorkerError& web_error) {
  ScriptState* script_state = resolver->GetScriptState();
  switch (web_error.error_type) {
    case mojom::blink::ServiceWorkerErrorType::kNetwork:
    case mojom::blink::ServiceWorkerErrorType::kNotFound:
    case mojom::blink::ServiceWorkerErrorType::kScriptEvaluateFailed:
      // According to the spec, these errors during update should result in
      // a TypeError.
      return V8ThrowException::CreateTypeError(
          script_state->GetIsolate(), GetExceptionParams(web_error).message);
    case mojom::blink::ServiceWorkerErrorType::kType:
      return V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                               web_error.message);
    default:
      return ToV8(ServiceWorkerError::Take(resolver, web_error),
                  script_state->GetContext()->Global(),
                  script_state->GetIsolate());
  }
}

}  // namespace blink

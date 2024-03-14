// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_token_provider.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/extensions_webview/v8/v8_media_integrity_error_name.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

MediaIntegrityTokenProvider::MediaIntegrityTokenProvider(
    ExecutionContext* context,
    uint64_t cloud_project_number)
    : cloud_project_number_(cloud_project_number) {}

ScriptPromise MediaIntegrityTokenProvider::requestToken(
    ScriptState* script_state,
    const String& content_binding,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid context");
    return ScriptPromise();
  }
  // TODO(crbug.com/327186031): Add real implementation.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  resolver->Reject(MediaIntegrityError::CreateForName(
      V8MediaIntegrityErrorName::Enum::kNonRecoverableError));
  return promise;
}

void MediaIntegrityTokenProvider::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

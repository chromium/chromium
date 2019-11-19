// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_error.h"

#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {
namespace native_file_system_error {

void Reject(ScriptPromiseResolver* resolver,
            const mojom::blink::NativeFileSystemError& error) {
  DCHECK_NE(error.status, mojom::blink::NativeFileSystemStatus::kOk);
  ResolveOrReject(resolver, error);
}

void ResolveOrReject(ScriptPromiseResolver* resolver,
                     const mojom::blink::NativeFileSystemError& error) {
  // Early exit if the resolver's context has been destroyed already.
  if (!resolver->GetScriptState()->ContextIsValid())
    return;
  auto* const isolate = resolver->GetScriptState()->GetIsolate();
  ScriptState::Scope scope(resolver->GetScriptState());

  // Convert empty message to a null string, to make sure we get the default
  // error message if no custom error message is provided.
  const String message = error.message.IsEmpty() ? String() : error.message;

  switch (error.status) {
    case mojom::blink::NativeFileSystemStatus::kOk:
      resolver->Resolve();
      break;
    case mojom::blink::NativeFileSystemStatus::kPermissionDenied:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kNotAllowedError, message));
      break;
    case mojom::blink::NativeFileSystemStatus::kInvalidState:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kInvalidStateError, message));
      break;
    case mojom::blink::NativeFileSystemStatus::kInvalidArgument:
      resolver->Reject(V8ThrowException::CreateTypeError(
          resolver->GetScriptState()->GetIsolate(), message));
      break;
    case mojom::blink::NativeFileSystemStatus::kOperationFailed:
    case mojom::blink::NativeFileSystemStatus::kOperationAborted:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kAbortError, message));
      break;
    case mojom::blink::NativeFileSystemStatus::kFileError:
      // TODO(mek): We might want to support custom messages for these cases.
      resolver->Reject(file_error::CreateDOMException(error.file_error));
      break;
  }
}

}  // namespace native_file_system_error
}  // namespace blink

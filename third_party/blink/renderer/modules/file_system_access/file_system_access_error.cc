// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"

#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace file_system_access_error {

void Reject(ScriptPromiseResolver* resolver,
            const mojom::blink::FileSystemAccessError& error) {
  DCHECK_NE(error.status, mojom::blink::FileSystemAccessStatus::kOk);
  ResolveOrReject(resolver, error);
}

void ResolveOrReject(ScriptPromiseResolver* resolver,
                     const mojom::blink::FileSystemAccessError& error) {
  // Early exit if the resolver's context has been destroyed already.
  if (!resolver->GetScriptState()->ContextIsValid())
    return;
  auto* const isolate = resolver->GetScriptState()->GetIsolate();
  ScriptState::Scope scope(resolver->GetScriptState());

  // Convert empty message to a null string, to make sure we get the default
  // error message if no custom error message is provided.
  const String message = error.message.empty() ? String() : error.message;

  switch (error.status) {
    case mojom::blink::FileSystemAccessStatus::kOk:
      resolver->Resolve();
      break;
    case mojom::blink::FileSystemAccessStatus::kPermissionDenied:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kNotAllowedError, message));
      break;
    case mojom::blink::FileSystemAccessStatus::kNoModificationAllowedError:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kNoModificationAllowedError, message));
      break;
    case mojom::blink::FileSystemAccessStatus::kInvalidModificationError:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kInvalidModificationError, message));
      break;
    case mojom::blink::FileSystemAccessStatus::kSecurityError:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kSecurityError, message));
      break;
    case mojom::blink::FileSystemAccessStatus::kNotSupportedError:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kNotSupportedError, message));
      break;
    case mojom::blink::FileSystemAccessStatus::kInvalidState:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kInvalidStateError, message));
      break;
    case mojom::blink::FileSystemAccessStatus::kInvalidArgument:
      resolver->Reject(V8ThrowException::CreateTypeError(
          resolver->GetScriptState()->GetIsolate(), message));
      break;
    case mojom::blink::FileSystemAccessStatus::kOperationFailed:
    case mojom::blink::FileSystemAccessStatus::kOperationAborted:
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kAbortError, message));
      break;
    case mojom::blink::FileSystemAccessStatus::kFileError:
      // TODO(mek): We might want to support custom messages for these cases.
      resolver->Reject(file_error::CreateDOMException(error.file_error));
      break;
  }
}

}  // namespace file_system_access_error
}  // namespace blink

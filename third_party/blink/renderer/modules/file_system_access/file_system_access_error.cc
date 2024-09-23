// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"

#include "base/files/file.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"

namespace blink::file_system_access_error {

void ResolveOrReject(ScriptPromiseResolver<IDLUndefined>* resolver,
                     const mojom::blink::FileSystemAccessError& error) {
  if (error.status == mojom::blink::FileSystemAccessStatus::kOk) {
    resolver->Resolve();
  } else {
    Reject(resolver, error);
  }
}

void Reject(ScriptPromiseResolverBase* resolver,
            const mojom::blink::FileSystemAccessError& error) {
  // Convert empty message to a null string, to make sure we get the default
  // error message if no custom error message is provided.
  const String message = error.message.empty() ? String() : error.message;

  switch (error.status) {
    case mojom::blink::FileSystemAccessStatus::kOk:
      NOTREACHED_IN_MIGRATION();
      break;
    case mojom::blink::FileSystemAccessStatus::kPermissionDenied:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                       message);
      break;
    case mojom::blink::FileSystemAccessStatus::kNoModificationAllowedError:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kNoModificationAllowedError, message);
      break;
    case mojom::blink::FileSystemAccessStatus::kInvalidModificationError:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kInvalidModificationError, message);
      break;
    case mojom::blink::FileSystemAccessStatus::kSecurityError:
      resolver->RejectWithSecurityError(message, message);
      break;
    case mojom::blink::FileSystemAccessStatus::kNotSupportedError:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                       message);
      break;
    case mojom::blink::FileSystemAccessStatus::kInvalidState:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       message);
      break;
    case mojom::blink::FileSystemAccessStatus::kInvalidArgument:
      resolver->RejectWithTypeError(message);
      break;
    case mojom::blink::FileSystemAccessStatus::kOperationFailed:
    case mojom::blink::FileSystemAccessStatus::kOperationAborted:
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError, message);
      break;
    case mojom::blink::FileSystemAccessStatus::kFileError:
      // TODO(mek): We might want to support custom messages for these cases.
      resolver->Reject(file_error::CreateDOMException(error.file_error));
      break;
  }
}

}  // namespace blink::file_system_access_error

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"

#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/cache_storage/cache.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

String GetDefaultMessage(mojom::CacheStorageError web_error) {
  switch (web_error) {
    case mojom::CacheStorageError::kSuccess:
      // This function should only be called with an error.
      break;
    case mojom::CacheStorageError::kErrorExists:
      return "Entry already exists.";
    case mojom::CacheStorageError::kErrorStorage:
      return "Unexpected internal error.";
    case mojom::CacheStorageError::kErrorNotFound:
      return "Entry was not found.";
    case mojom::CacheStorageError::kErrorQuotaExceeded:
      return "Quota exceeded.";
    case mojom::CacheStorageError::kErrorCacheNameNotFound:
      return "Cache was not found.";
    case mojom::CacheStorageError::kErrorQueryTooLarge:
      return "Operation too large.";
    case mojom::CacheStorageError::kErrorNotImplemented:
      return "Method is not implemented.";
    case mojom::CacheStorageError::kErrorDuplicateOperation:
      return "Duplicate operation.";
    case mojom::CacheStorageError::kErrorCrossOriginResourcePolicy:
      return "Failed Cross-Origin-Resource-Policy check.";
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

}  // namespace

void RejectCacheStorageWithError(ScriptPromiseResolverBase* resolver,
                                 mojom::blink::CacheStorageError web_error,
                                 const String& message) {
  String final_message =

      !message.empty() ? message : GetDefaultMessage(web_error);
  switch (web_error) {
    case mojom::CacheStorageError::kSuccess:
      // This function should only be called with an error.
      NOTREACHED_IN_MIGRATION();
      return;
    case mojom::CacheStorageError::kErrorExists:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidAccessError,
                                       final_message);
      return;
    case mojom::CacheStorageError::kErrorStorage:
      resolver->RejectWithDOMException(DOMExceptionCode::kUnknownError,
                                       final_message);
      return;
    case mojom::CacheStorageError::kErrorNotFound:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                       final_message);
      return;
    case mojom::CacheStorageError::kErrorQuotaExceeded:
      resolver->RejectWithDOMException(DOMExceptionCode::kQuotaExceededError,
                                       final_message);
      return;
    case mojom::CacheStorageError::kErrorCacheNameNotFound:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotFoundError,
                                       final_message);
      return;
    case mojom::CacheStorageError::kErrorQueryTooLarge:
      resolver->RejectWithDOMException(DOMExceptionCode::kAbortError,
                                       final_message);
      return;
    case mojom::CacheStorageError::kErrorNotImplemented:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                       final_message);
      return;
    case mojom::CacheStorageError::kErrorDuplicateOperation:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       final_message);
      return;
    case mojom::CacheStorageError::kErrorCrossOriginResourcePolicy:
      resolver->RejectWithTypeError(message);
      return;
  }
}

}  // namespace blink

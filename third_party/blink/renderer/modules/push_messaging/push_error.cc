// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_error.h"

#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

DOMException* PushError::CreateException(mojom::PushErrorType error,
                                         const String& message) {
  switch (error) {
    case mojom::PushErrorType::ABORT:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                message);
    case mojom::PushErrorType::INVALID_STATE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, message);
    case mojom::PushErrorType::NETWORK:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                message);
    case mojom::PushErrorType::NONE:
      NOTREACHED();
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError,
                                                message);
    case mojom::PushErrorType::NOT_ALLOWED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, message);
    case mojom::PushErrorType::NOT_FOUND:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, message);
    case mojom::PushErrorType::NOT_SUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, message);
  }
  NOTREACHED();
  return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError);
}

}  // namespace blink

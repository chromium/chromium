// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"

namespace blink {

DOMException* NDEFErrorTypeToDOMException(
    device::mojom::blink::NDEFErrorType error_type,
    const String& error_message) {
  switch (error_type) {
    case device::mojom::blink::NDEFErrorType::NOT_ALLOWED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, error_message);
    case device::mojom::blink::NDEFErrorType::NOT_SUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, error_message);
    case device::mojom::blink::NDEFErrorType::NOT_READABLE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError, error_message);
    case device::mojom::blink::NDEFErrorType::INVALID_MESSAGE:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                                error_message);
    case device::mojom::blink::NDEFErrorType::OPERATION_CANCELLED:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                error_message);
    case device::mojom::blink::NDEFErrorType::IO_ERROR:
      return MakeGarbageCollected<DOMException>(DOMExceptionCode::kNetworkError,
                                                error_message);
  }
  NOTREACHED();
  // Don't need to handle the case after a NOTREACHED().
  return nullptr;
}

}  // namespace blink

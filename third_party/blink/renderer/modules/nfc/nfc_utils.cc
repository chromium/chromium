// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

using device::mojom::blink::NDEFPushTarget;

namespace blink {

size_t GetNDEFMessageSize(const device::mojom::blink::NDEFMessage& message) {
  size_t message_size = message.url.CharactersSizeInBytes();
  for (wtf_size_t i = 0; i < message.data.size(); ++i) {
    message_size += message.data[i]->media_type.CharactersSizeInBytes();
    message_size += message.data[i]->data.size();
  }
  return message_size;
}

bool SetNDEFMessageURL(const String& origin,
                       device::mojom::blink::NDEFMessage* message) {
  KURL origin_url(origin);

  if (!message->url.IsEmpty() && origin_url.CanSetPathname()) {
    origin_url.SetPath(message->url);
  }

  message->url = origin_url;
  return origin_url.IsValid();
}

NDEFPushTarget StringToNDEFPushTarget(const String& target) {
  if (target == "tag")
    return NDEFPushTarget::TAG;

  if (target == "peer")
    return NDEFPushTarget::PEER;

  return NDEFPushTarget::ANY;
}

DOMException* NDEFErrorTypeToDOMException(
    device::mojom::blink::NDEFErrorType error_type) {
  switch (error_type) {
    case device::mojom::blink::NDEFErrorType::NOT_ALLOWED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, "NFC operation not allowed.");
    case device::mojom::blink::NDEFErrorType::NOT_SUPPORTED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "No NFC adapter or cannot establish connection.");
    case device::mojom::blink::NDEFErrorType::NOT_READABLE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotReadableError, "NFC is not enabled.");
    case device::mojom::blink::NDEFErrorType::NOT_FOUND:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError,
          "Provided watch id cannot be found.");
    case device::mojom::blink::NDEFErrorType::INVALID_MESSAGE:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSyntaxError, "Invalid NFC message was provided.");
    case device::mojom::blink::NDEFErrorType::OPERATION_CANCELLED:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "The NFC operation was cancelled.");
    case device::mojom::blink::NDEFErrorType::CANNOT_CANCEL:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNoModificationAllowedError,
          "NFC operation cannot be cancelled.");
    case device::mojom::blink::NDEFErrorType::IO_ERROR:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNetworkError,
          "NFC data transfer error has occurred.");
  }
  NOTREACHED();
  // Don't need to handle the case after a NOTREACHED().
  return nullptr;
}

}  // namespace blink

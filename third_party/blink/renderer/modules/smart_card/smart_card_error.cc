// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_error_options.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
SmartCardError* SmartCardError::Create(String message,
                                       const SmartCardErrorOptions* options) {
  return MakeGarbageCollected<SmartCardError>(std::move(message),
                                              options->responseCode());
}

// static
DOMException* SmartCardError::Create(
    mojom::blink::SmartCardResponseCode mojom_response_code) {
  switch (mojom_response_code) {
    // SmartCardError:
    // The response code messages are mostly from
    // https://learn.microsoft.com/en-us/windows/win32/secauthn/authentication-return-values,
    // which are also used by PCSC lite.
    case mojom::blink::SmartCardResponseCode::kNoService:
      return MakeGarbageCollected<SmartCardError>(
          "No smart card service available in the system.",
          V8SmartCardResponseCode::Enum::kNoService);
    case mojom::blink::SmartCardResponseCode::kNoSmartCard:
      return MakeGarbageCollected<SmartCardError>(
          "The operation requires a smart card, but no smart card is "
          "currently in the device.",
          V8SmartCardResponseCode::Enum::kNoSmartcard);
    case mojom::blink::SmartCardResponseCode::kNotReady:
      return MakeGarbageCollected<SmartCardError>(
          "The reader or smart card is not ready to accept commands.",
          V8SmartCardResponseCode::Enum::kNotReady);
    case mojom::blink::SmartCardResponseCode::kNotTransacted:
      return MakeGarbageCollected<SmartCardError>(
          "An attempt was made to end a non-existent transaction.",
          V8SmartCardResponseCode::Enum::kNotTransacted);
    case mojom::blink::SmartCardResponseCode::kProtoMismatch:
      return MakeGarbageCollected<SmartCardError>(
          "The requested protocols are incompatible with the protocol "
          "currently in use with the smart card.",
          V8SmartCardResponseCode::Enum::kProtoMismatch);
    case mojom::blink::SmartCardResponseCode::kReaderUnavailable:
      return MakeGarbageCollected<SmartCardError>(
          "The specified reader is not currently available for use.",
          V8SmartCardResponseCode::Enum::kReaderUnavailable);
    case mojom::blink::SmartCardResponseCode::kRemovedCard:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card has been removed, so further communication is not "
          "possible.",
          V8SmartCardResponseCode::Enum::kRemovedCard);
    case mojom::blink::SmartCardResponseCode::kResetCard:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card has been reset, so any shared state information "
          "is invalid.",
          V8SmartCardResponseCode::Enum::kResetCard);
    case mojom::blink::SmartCardResponseCode::kServerTooBusy:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card resource manager is too busy to complete this "
          "operation.",
          V8SmartCardResponseCode::Enum::kServerTooBusy);
    case mojom::blink::SmartCardResponseCode::kSharingViolation:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card cannot be accessed because of other connections "
          "outstanding.",
          V8SmartCardResponseCode::Enum::kSharingViolation);
    case mojom::blink::SmartCardResponseCode::kSystemCancelled:
      return MakeGarbageCollected<SmartCardError>(
          "The action was cancelled by the system, presumably to log off or "
          "shut down.",
          V8SmartCardResponseCode::Enum::kSystemCancelled);
    case mojom::blink::SmartCardResponseCode::kUnpoweredCard:
      return MakeGarbageCollected<SmartCardError>(
          "Power has been removed from the smart card, so that further "
          "communication is not possible.",
          V8SmartCardResponseCode::Enum::kUnpoweredCard);
    case mojom::blink::SmartCardResponseCode::kUnresponsiveCard:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card is not responding to a reset.",
          V8SmartCardResponseCode::Enum::kUnresponsiveCard);
    case mojom::blink::SmartCardResponseCode::kUnsupportedCard:
      return MakeGarbageCollected<SmartCardError>(
          "The reader cannot communicate with the card, due to ATR string "
          "configuration conflicts.",
          V8SmartCardResponseCode::Enum::kUnsupportedCard);
    case mojom::blink::SmartCardResponseCode::kUnsupportedFeature:
      return MakeGarbageCollected<SmartCardError>(
          "This smart card does not support the requested feature.",
          V8SmartCardResponseCode::Enum::kUnsupportedFeature);

    // DOMException:
    // "InvalidStateError"
    case mojom::blink::SmartCardResponseCode::kInvalidConnection:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, "Connection is invalid.");
    case mojom::blink::SmartCardResponseCode::kServiceStopped:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "The smart card resource manager has shut down.");
    // "AbortError"
    case mojom::blink::SmartCardResponseCode::kShutdown:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "The operation has been aborted to allow the server application to "
          "exit.");
    // "UnknownError"
    case mojom::blink::SmartCardResponseCode::kCommError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An internal communications error has been detected.");
    case mojom::blink::SmartCardResponseCode::kInternalError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An internal consistency check failed.");
    case mojom::blink::SmartCardResponseCode::kNoMemory:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "Not enough memory available to complete this command.");
    case mojom::blink::SmartCardResponseCode::kUnexpected:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An unexpected card error has occurred.");
    case mojom::blink::SmartCardResponseCode::kUnknownError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An internal error has been detected, but the source is unknown.");
  }
}

SmartCardError::SmartCardError(String message,
                               V8SmartCardResponseCode::Enum response_code_enum)
    : SmartCardError(std::move(message),
                     V8SmartCardResponseCode(response_code_enum)) {}

SmartCardError::SmartCardError(String message,
                               V8SmartCardResponseCode response_code)
    : DOMException(DOMExceptionCode::kSmartCardError, std::move(message)),
      response_code_(std::move(response_code)) {}

SmartCardError::~SmartCardError() = default;

}  // namespace blink

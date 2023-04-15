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
    device::mojom::blink::SmartCardError mojom_error) {
  switch (mojom_error) {
    // SmartCardError:
    // The response code messages are mostly from
    // https://learn.microsoft.com/en-us/windows/win32/secauthn/authentication-return-values,
    // which are also used by PCSC lite.
    case device::mojom::blink::SmartCardError::kNoService:
      return MakeGarbageCollected<SmartCardError>(
          "No smart card service available in the system.",
          V8SmartCardResponseCode::Enum::kNoService);
    case device::mojom::blink::SmartCardError::kNoSmartcard:
      return MakeGarbageCollected<SmartCardError>(
          "The operation requires a smart card, but no smart card is "
          "currently in the device.",
          V8SmartCardResponseCode::Enum::kNoSmartcard);
    case device::mojom::blink::SmartCardError::kNotReady:
      return MakeGarbageCollected<SmartCardError>(
          "The reader or smart card is not ready to accept commands.",
          V8SmartCardResponseCode::Enum::kNotReady);
    case device::mojom::blink::SmartCardError::kNotTransacted:
      return MakeGarbageCollected<SmartCardError>(
          "An attempt was made to end a non-existent transaction.",
          V8SmartCardResponseCode::Enum::kNotTransacted);
    case device::mojom::blink::SmartCardError::kProtoMismatch:
      return MakeGarbageCollected<SmartCardError>(
          "The requested protocols are incompatible with the protocol "
          "currently in use with the smart card.",
          V8SmartCardResponseCode::Enum::kProtoMismatch);
    case device::mojom::blink::SmartCardError::kReaderUnavailable:
      return MakeGarbageCollected<SmartCardError>(
          "The specified reader is not currently available for use.",
          V8SmartCardResponseCode::Enum::kReaderUnavailable);
    case device::mojom::blink::SmartCardError::kRemovedCard:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card has been removed, so further communication is not "
          "possible.",
          V8SmartCardResponseCode::Enum::kRemovedCard);
    case device::mojom::blink::SmartCardError::kResetCard:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card has been reset, so any shared state information "
          "is invalid.",
          V8SmartCardResponseCode::Enum::kResetCard);
    case device::mojom::blink::SmartCardError::kServerTooBusy:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card resource manager is too busy to complete this "
          "operation.",
          V8SmartCardResponseCode::Enum::kServerTooBusy);
    case device::mojom::blink::SmartCardError::kSharingViolation:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card cannot be accessed because of other connections "
          "outstanding.",
          V8SmartCardResponseCode::Enum::kSharingViolation);
    case device::mojom::blink::SmartCardError::kSystemCancelled:
      return MakeGarbageCollected<SmartCardError>(
          "The action was cancelled by the system, presumably to log off or "
          "shut down.",
          V8SmartCardResponseCode::Enum::kSystemCancelled);
    case device::mojom::blink::SmartCardError::kUnpoweredCard:
      return MakeGarbageCollected<SmartCardError>(
          "Power has been removed from the smart card, so that further "
          "communication is not possible.",
          V8SmartCardResponseCode::Enum::kUnpoweredCard);
    case device::mojom::blink::SmartCardError::kUnresponsiveCard:
      return MakeGarbageCollected<SmartCardError>(
          "The smart card is not responding to a reset.",
          V8SmartCardResponseCode::Enum::kUnresponsiveCard);
    case device::mojom::blink::SmartCardError::kUnsupportedCard:
      return MakeGarbageCollected<SmartCardError>(
          "The reader cannot communicate with the card, due to ATR string "
          "configuration conflicts.",
          V8SmartCardResponseCode::Enum::kUnsupportedCard);
    case device::mojom::blink::SmartCardError::kUnsupportedFeature:
      return MakeGarbageCollected<SmartCardError>(
          "This smart card does not support the requested feature.",
          V8SmartCardResponseCode::Enum::kUnsupportedFeature);

    // DOMException:
    // "InvalidStateError"
    case device::mojom::blink::SmartCardError::kInvalidHandle:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, "Connection is invalid.");
    case device::mojom::blink::SmartCardError::kServiceStopped:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "The smart card resource manager has shut down.");
    // "AbortError"
    case device::mojom::blink::SmartCardError::kShutdown:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "The operation has been aborted to allow the server application to "
          "exit.");
    // "UnknownError"
    case device::mojom::blink::SmartCardError::kCommError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An internal communications error has been detected.");
    case device::mojom::blink::SmartCardError::kInternalError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An internal consistency check failed.");
    case device::mojom::blink::SmartCardError::kNoMemory:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "Not enough memory available to complete this command.");
    case device::mojom::blink::SmartCardError::kUnexpected:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An unexpected card error has occurred.");
    case device::mojom::blink::SmartCardError::kUnknownError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An internal error has been detected, but the source is unknown.");
    case device::mojom::blink::SmartCardError::kUnknown:
      // NB: kUnknownError is an actual PC/SC error code returned from the
      // platform's PC/SC stack. kUnknown means that PC/SC returned an error
      // code not yet represented in our enum and therefore is unknown to us.
      LOG(WARNING) << "An unmapped PC/SC error has occurred.";
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "An unknown error has occurred.");
    // Handled internally but listed here for completeness.
    // Also, technically nothing stops the PC/SC stack from spilling those
    // unexpectedly (eg, in unrelated requests).
    case device::mojom::blink::SmartCardError::kCancelled:
    case device::mojom::blink::SmartCardError::kTimeout:
    case device::mojom::blink::SmartCardError::kUnknownReader:
    case device::mojom::blink::SmartCardError::kNoReadersAvailable:
    // Errors that indicate bad usage of the API (ie, a programming
    // error in browser code).
    // Again, technically nothing stops the PC/SC stack from spilling those
    // unexpectedly.
    case device::mojom::blink::SmartCardError::kInsufficientBuffer:
    case device::mojom::blink::SmartCardError::kInvalidParameter:
    case device::mojom::blink::SmartCardError::kInvalidValue:
      LOG(WARNING) << "An unexpected PC/SC error has occurred: " << mojom_error;
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "An unexpected card error has occurred.");
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

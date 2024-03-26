// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "services/device/public/mojom/smart_card.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
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
void SmartCardError::MaybeReject(
    ScriptPromiseResolverBase* resolver,
    device::mojom::blink::SmartCardError mojom_error) {
  ScriptState* script_state = resolver->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  // Enter the associated v8 context.
  // Otherwise a RejectWithDOMException() or RejectWithTypeError() call will
  // abort, as they need it in order to add call site context to the error
  // message text.
  ScriptState::Scope script_state_scope(script_state);

  switch (mojom_error) {
    // SmartCardError:
    // The response code messages are mostly from
    // https://learn.microsoft.com/en-us/windows/win32/secauthn/authentication-return-values,
    // which are also used by PCSC lite.
    case device::mojom::blink::SmartCardError::kNoService:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "No smart card service available in the system.",
          V8SmartCardResponseCode::Enum::kNoService));
      break;
    case device::mojom::blink::SmartCardError::kNoSmartcard:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The operation requires a smart card, but no smart card is "
          "currently in the device.",
          V8SmartCardResponseCode::Enum::kNoSmartcard));
      break;
    case device::mojom::blink::SmartCardError::kNotReady:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The reader or smart card is not ready to accept commands.",
          V8SmartCardResponseCode::Enum::kNotReady));
      break;
    case device::mojom::blink::SmartCardError::kNotTransacted:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "An attempt was made to end a non-existent transaction.",
          V8SmartCardResponseCode::Enum::kNotTransacted));
      break;
    case device::mojom::blink::SmartCardError::kProtoMismatch:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The requested protocols are incompatible with the protocol "
          "currently in use with the smart card.",
          V8SmartCardResponseCode::Enum::kProtoMismatch));
      break;
    case device::mojom::blink::SmartCardError::kReaderUnavailable:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The specified reader is not currently available for use.",
          V8SmartCardResponseCode::Enum::kReaderUnavailable));
      break;
    case device::mojom::blink::SmartCardError::kRemovedCard:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The smart card has been removed, so further communication is not "
          "possible.",
          V8SmartCardResponseCode::Enum::kRemovedCard));
      break;
    case device::mojom::blink::SmartCardError::kResetCard:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The smart card has been reset, so any shared state information "
          "is invalid.",
          V8SmartCardResponseCode::Enum::kResetCard));
      break;
    case device::mojom::blink::SmartCardError::kServerTooBusy:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The smart card resource manager is too busy to complete this "
          "operation.",
          V8SmartCardResponseCode::Enum::kServerTooBusy));
      break;
    case device::mojom::blink::SmartCardError::kSharingViolation:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The smart card cannot be accessed because of other connections "
          "outstanding.",
          V8SmartCardResponseCode::Enum::kSharingViolation));
      break;
    case device::mojom::blink::SmartCardError::kSystemCancelled:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The action was cancelled by the system, presumably to log off or "
          "shut down.",
          V8SmartCardResponseCode::Enum::kSystemCancelled));
      break;
    case device::mojom::blink::SmartCardError::kUnknownReader:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The specified reader name is not recognized.",
          V8SmartCardResponseCode::Enum::kUnknownReader));
      break;
    case device::mojom::blink::SmartCardError::kUnpoweredCard:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "Power has been removed from the smart card, so that further "
          "communication is not possible.",
          V8SmartCardResponseCode::Enum::kUnpoweredCard));
      break;
    case device::mojom::blink::SmartCardError::kUnresponsiveCard:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The smart card is not responding to a reset.",
          V8SmartCardResponseCode::Enum::kUnresponsiveCard));
      break;
    case device::mojom::blink::SmartCardError::kUnsupportedCard:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "The reader cannot communicate with the card, due to ATR string "
          "configuration conflicts.",
          V8SmartCardResponseCode::Enum::kUnsupportedCard));
      break;
    case device::mojom::blink::SmartCardError::kUnsupportedFeature:
      resolver->Reject(MakeGarbageCollected<SmartCardError>(
          "This smart card does not support the requested feature.",
          V8SmartCardResponseCode::Enum::kUnsupportedFeature));
      break;

    // TypeError:
    // This is not only triggered by bad PC/SC API usage (e.g., passing a null
    // context), which would be a browser implementation bug. It can also be
    // returned by the reader driver or card on input that, from a pure PC/SC
    // API perspective, is perfectly valid.
    case device::mojom::blink::SmartCardError::kInvalidParameter:
      resolver->RejectWithTypeError(
          "One or more of the supplied parameters could not be properly "
          "interpreted.");
      break;

    // DOMException:
    // "InvalidStateError"
    case device::mojom::blink::SmartCardError::kInvalidHandle:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       "Connection is invalid.");
      break;
    case device::mojom::blink::SmartCardError::kServiceStopped:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The smart card resource manager has shut down.");
      break;
    // "AbortError"
    case device::mojom::blink::SmartCardError::kShutdown:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kAbortError,
          "The operation has been aborted to allow the server application to "
          "exit.");
      break;
    // "NotAllowedError"
    case device::mojom::blink::SmartCardError::kPermissionDenied:
      resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                       "The user has denied permission.");
      break;
    // "UnknownError"
    case device::mojom::blink::SmartCardError::kCommError:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kUnknownError,
          "An internal communications error has been detected.");
      break;
    case device::mojom::blink::SmartCardError::kInternalError:
      resolver->RejectWithDOMException(DOMExceptionCode::kUnknownError,
                                       "An internal consistency check failed.");
      break;
    case device::mojom::blink::SmartCardError::kNoMemory:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kUnknownError,
          "Not enough memory available to complete this command.");
      break;
    case device::mojom::blink::SmartCardError::kUnexpected:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kUnknownError,
          "An unexpected card error has occurred.");
      break;
    case device::mojom::blink::SmartCardError::kUnknownError:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kUnknownError,
          "An internal error has been detected, but the source is unknown.");
      break;
    case device::mojom::blink::SmartCardError::kUnknown:
      // NB: kUnknownError is an actual PC/SC error code returned from the
      // platform's PC/SC stack. kUnknown means that PC/SC returned an error
      // code not yet represented in our enum and therefore is unknown to us.
      LOG(WARNING) << "An unmapped PC/SC error has occurred.";
      resolver->RejectWithDOMException(DOMExceptionCode::kUnknownError,
                                       "An unknown error has occurred.");
      break;
    // Handled internally but listed here for completeness.
    // Also, technically nothing stops the PC/SC stack from spilling those
    // unexpectedly (eg, in unrelated requests).
    case device::mojom::blink::SmartCardError::kCancelled:
    case device::mojom::blink::SmartCardError::kTimeout:
    case device::mojom::blink::SmartCardError::kNoReadersAvailable:
    // Errors that indicate bad usage of the API (ie, a programming
    // error in browser code).
    // Again, technically nothing stops the PC/SC stack from spilling those
    // unexpectedly.
    case device::mojom::blink::SmartCardError::kInsufficientBuffer:
    case device::mojom::blink::SmartCardError::kInvalidValue:
      LOG(WARNING) << "An unexpected PC/SC error has occurred: " << mojom_error;
      resolver->RejectWithDOMException(
          DOMExceptionCode::kUnknownError,
          "An unexpected card error has occurred.");
      break;
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

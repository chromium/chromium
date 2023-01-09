// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_error_options.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {

// The response code messages are mostly from
// https://learn.microsoft.com/en-us/windows/win32/secauthn/authentication-return-values,
// which are also used by PCSC lite.
String MojomResponseCodeToMessage(
    mojom::SmartCardResponseCode mojom_response_code) {
  switch (mojom_response_code) {
    case mojom::SmartCardResponseCode::kNoService:
      return "No smart card service available in the system.";
    case mojom::SmartCardResponseCode::kNoSmartCard:
      return "The operation requires a smart card, but no smart card is "
             "currently in the device.";
    case mojom::SmartCardResponseCode::kNotReady:
      return "The reader or smart card is not ready to accept commands.";
    case mojom::SmartCardResponseCode::kNotTransacted:
      return "An attempt was made to end a non-existent transaction.";
    case mojom::SmartCardResponseCode::kProtoMismatch:
      return "The requested protocols are incompatible with the protocol "
             "currently in use with the smart card.";
    case mojom::SmartCardResponseCode::kReaderUnavailable:
      return "The specified reader is not currently available for use.";
    case mojom::SmartCardResponseCode::kRemovedCard:
      return "The smart card has been removed, so further communication is not "
             "possible.";
    case mojom::SmartCardResponseCode::kResetCard:
      return "The smart card has been reset, so any shared state information "
             "is invalid.";
    case mojom::SmartCardResponseCode::kSharingViolation:
      return "The smart card cannot be accessed because of other connections "
             "outstanding.";
    case mojom::SmartCardResponseCode::kSystemCancelled:
      return "The action was cancelled by the system, presumably to log off or "
             "shut down.";
    case mojom::SmartCardResponseCode::kUnpoweredCard:
      return "Power has been removed from the smart card, so that further "
             "communication is not possible.";
    case mojom::SmartCardResponseCode::kUnresponsiveCard:
      return "The smart card is not responding to a reset.";
    case mojom::SmartCardResponseCode::kUnsupportedCard:
      return "The reader cannot communicate with the card, due to ATR string "
             "configuration conflicts.";
    case mojom::SmartCardResponseCode::kUnsupportedFeature:
      return "This smart card does not support the requested feature.";
  }
}

V8SmartCardResponseCode::Enum MojomToV8ResponseCode(
    mojom::SmartCardResponseCode mojom_response_code) {
  switch (mojom_response_code) {
    case mojom::SmartCardResponseCode::kNoService:
      return V8SmartCardResponseCode::Enum::kNoService;
    case mojom::SmartCardResponseCode::kNoSmartCard:
      return V8SmartCardResponseCode::Enum::kNoSmartcard;
    case mojom::SmartCardResponseCode::kNotReady:
      return V8SmartCardResponseCode::Enum::kNotReady;
    case mojom::SmartCardResponseCode::kNotTransacted:
      return V8SmartCardResponseCode::Enum::kNotTransacted;
    case mojom::SmartCardResponseCode::kProtoMismatch:
      return V8SmartCardResponseCode::Enum::kProtoMismatch;
    case mojom::SmartCardResponseCode::kReaderUnavailable:
      return V8SmartCardResponseCode::Enum::kReaderUnavailable;
    case mojom::SmartCardResponseCode::kRemovedCard:
      return V8SmartCardResponseCode::Enum::kRemovedCard;
    case mojom::SmartCardResponseCode::kResetCard:
      return V8SmartCardResponseCode::Enum::kResetCard;
    case mojom::SmartCardResponseCode::kSharingViolation:
      return V8SmartCardResponseCode::Enum::kSharingViolation;
    case mojom::SmartCardResponseCode::kSystemCancelled:
      return V8SmartCardResponseCode::Enum::kSystemCancelled;
    case mojom::SmartCardResponseCode::kUnpoweredCard:
      return V8SmartCardResponseCode::Enum::kUnpoweredCard;
    case mojom::SmartCardResponseCode::kUnresponsiveCard:
      return V8SmartCardResponseCode::Enum::kUnresponsiveCard;
    case mojom::SmartCardResponseCode::kUnsupportedCard:
      return V8SmartCardResponseCode::Enum::kUnsupportedCard;
    case mojom::SmartCardResponseCode::kUnsupportedFeature:
      return V8SmartCardResponseCode::Enum::kUnsupportedFeature;
  }
}

}  // namespace

// static
SmartCardError* SmartCardError::Create(String message,
                                       const SmartCardErrorOptions* options) {
  return MakeGarbageCollected<SmartCardError>(std::move(message),
                                              options->responseCode());
}

SmartCardError::SmartCardError(mojom::SmartCardResponseCode mojom_response_code)
    : SmartCardError(
          MojomResponseCodeToMessage(mojom_response_code),
          V8SmartCardResponseCode(MojomToV8ResponseCode(mojom_response_code))) {
}

SmartCardError::SmartCardError(String message,
                               V8SmartCardResponseCode response_code)
    : DOMException(DOMExceptionCode::kSmartCardError, std::move(message)),
      response_code_(std::move(response_code)) {}

SmartCardError::~SmartCardError() = default;

}  // namespace blink

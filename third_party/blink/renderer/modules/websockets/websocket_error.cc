// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_error.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_close_info.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_common.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

WebSocketError* WebSocketError::Create(String message,
                                       const WebSocketCloseInfo* close_info,
                                       ExceptionState& exception_state) {
  return ValidateAndCreate(
      std::move(message),
      close_info->hasCloseCode() ? std::make_optional(close_info->closeCode())
                                 : std::nullopt,
      close_info->hasReason() ? close_info->reason() : String(),
      exception_state);
}

v8::Local<v8::Value> WebSocketError::Create(v8::Isolate* isolate,
                                            String message,
                                            std::optional<uint16_t> close_code,
                                            String reason) {
  if (!reason.empty() && !close_code.has_value()) {
    close_code = WebSocketChannel::kCloseEventCodeNormalClosure;
  }
  auto* error = MakeGarbageCollected<WebSocketError>(
      PassKey(), std::move(message), close_code, std::move(reason));
  return V8ThrowDOMException::AttachStackProperty(isolate, error);
}

WebSocketError::WebSocketError(PassKey,
                               String message,
                               std::optional<uint16_t> close_code,
                               String reason)
    : DOMException(DOMExceptionCode::kWebSocketError, std::move(message)),
      close_code_(close_code),
      reason_(std::move(reason)) {}

WebSocketError::~WebSocketError() = default;

WebSocketError* WebSocketError::ValidateAndCreate(
    String message,
    std::optional<uint16_t> close_code,
    String reason,
    ExceptionState& exception_state) {
  const std::optional<uint16_t> valid_code =
      WebSocketCommon::ValidateCloseCodeAndReason(close_code, reason,
                                                  exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return MakeGarbageCollected<WebSocketError>(PassKey(), std::move(message),
                                              valid_code, std::move(reason));
}

}  // namespace blink

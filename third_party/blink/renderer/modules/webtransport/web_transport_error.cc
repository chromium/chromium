// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

WebTransportError* WebTransportError::Create(
    const WebTransportErrorInit* init) {
  std::optional<uint32_t> stream_error_code =
      init->hasStreamErrorCode() ? std::make_optional(init->streamErrorCode())
                                 : std::nullopt;
  String message = init->hasMessage() ? init->message() : g_empty_string;
  return MakeGarbageCollected<WebTransportError>(
      PassKey(), stream_error_code, std::move(message),
      V8WebTransportErrorSource::Enum::kStream);
}

v8::Local<v8::Value> WebTransportError::Create(
    v8::Isolate* isolate,
    std::optional<uint32_t> stream_error_code,
    String message,
    V8WebTransportErrorSource::Enum source) {
  auto* dom_exception = MakeGarbageCollected<WebTransportError>(
      PassKey(), stream_error_code, std::move(message), source);
  return V8ThrowDOMException::AttachStackProperty(isolate, dom_exception);
}

WebTransportError::WebTransportError(PassKey,
                                     std::optional<uint32_t> stream_error_code,
                                     String message,
                                     V8WebTransportErrorSource::Enum source)
    : DOMException(DOMExceptionCode::kWebTransportError, std::move(message)),
      stream_error_code_(stream_error_code),
      source_(source) {}

WebTransportError::~WebTransportError() = default;

V8WebTransportErrorSource WebTransportError::source() const {
  return V8WebTransportErrorSource(source_);
}

}  // namespace blink

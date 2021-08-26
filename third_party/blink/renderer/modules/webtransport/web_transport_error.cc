// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

WebTransportError* WebTransportError::Create(
    const WebTransportErrorInit* init) {
  absl::optional<uint8_t> application_protocol_code =
      init->hasApplicationProtocolCode()
          ? absl::make_optional(init->applicationProtocolCode())
          : absl::nullopt;
  String message = init->hasMessage() ? init->message() : g_empty_string;
  return MakeGarbageCollected<WebTransportError>(
      PassKey(), application_protocol_code, std::move(message),
      Source::kStream);
}

v8::Local<v8::Value> WebTransportError::Create(
    ScriptState* script_state,
    absl::optional<uint8_t> application_protocol_code,
    String message,
    Source source) {
  auto* dom_exception = MakeGarbageCollected<WebTransportError>(
      PassKey(), application_protocol_code, std::move(message), source);
  return V8ThrowDOMException::AttachStackProperty(
      script_state->GetIsolate(), script_state->GetContext(), dom_exception);
}

WebTransportError::WebTransportError(
    PassKey,
    absl::optional<uint8_t> application_protocol_code,
    String message,
    Source source)
    : DOMException(DOMExceptionCode::kWebTransportError, std::move(message)),
      application_protocol_code_(application_protocol_code),
      source_(source) {}

WebTransportError::~WebTransportError() = default;

String WebTransportError::source() const {
  switch (source_) {
    case Source::kStream:
      return "stream";

    case Source::kSession:
      return "session";
  }
}

}  // namespace blink

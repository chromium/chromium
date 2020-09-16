// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WebTransport::WebTransport(PassKey, QuicTransport* quic_transport)
    : quic_transport_(quic_transport) {}
WebTransport::~WebTransport() = default;

WebTransport* WebTransport::Create(ScriptState* script_state,
                                   const String& url,
                                   QuicTransportOptions* options,
                                   ExceptionState& exception_state) {
  QuicTransport* quic_transport =
      QuicTransport::Create(script_state, url, options, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return MakeGarbageCollected<WebTransport>(PassKey(), quic_transport);
}

}  // namespace blink

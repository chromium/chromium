// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_server_readable_stream_wrapper.h"

#include "base/notreached.h"

namespace blink {

TCPServerReadableStreamWrapper::TCPServerReadableStreamWrapper(
    ScriptState* script_state)
    : ReadableStreamDefaultWrapper(script_state) {}

void TCPServerReadableStreamWrapper::Pull() {
  NOTIMPLEMENTED();
}

void TCPServerReadableStreamWrapper::CloseStream() {
  NOTIMPLEMENTED();
}

void TCPServerReadableStreamWrapper::ErrorStream(int32_t error_code) {
  NOTIMPLEMENTED();
}

void TCPServerReadableStreamWrapper::Trace(Visitor* visitor) const {
  ReadableStreamDefaultWrapper::Trace(visitor);
}

}  // namespace blink

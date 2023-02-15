// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_server_socket.h"

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_server_socket_open_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_server_socket_options.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_server_readable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

TCPServerSocket::TCPServerSocket(ScriptState* script_state)
    : Socket(script_state) {}

TCPServerSocket::~TCPServerSocket() = default;

// static
TCPServerSocket* TCPServerSocket::Create(ScriptState* script_state,
                                         const String& local_address,
                                         const TCPServerSocketOptions* options,
                                         ExceptionState& exception_state) {
  NOTIMPLEMENTED();
  return nullptr;
}

ScriptPromise TCPServerSocket::close(ScriptState*, ExceptionState&) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

void TCPServerSocket::Trace(Visitor* visitor) const {
  visitor->Trace(readable_stream_wrapper_);

  ScriptWrappable::Trace(visitor);
  Socket::Trace(visitor);
}

}  // namespace blink

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_WRITABLE_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_WRITABLE_STREAM_WRAPPER_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ScriptState;

class MODULES_EXPORT UDPWritableStreamWrapper final
    : public GarbageCollected<UDPWritableStreamWrapper>,
      public WritableStreamWrapper {
 public:
  UDPWritableStreamWrapper(ScriptState* script_state,
                           const Member<UDPSocketMojoRemote> udp_socket_);

  void CloseStream(bool error) override;
  bool HasPendingWrite() const override;

  void Trace(Visitor*) const override;

 private:
  class UDPUnderlyingSink;

  ScriptPromise Write(ScriptValue chunk,
                      ExceptionState& exception_state) override;

  // Callback for DirectUDPSocket::Send().
  void OnSend(int32_t result);

  const Member<UDPSocketMojoRemote> udp_socket_;
  Member<ScriptPromiseResolver> send_resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_WRITABLE_STREAM_WRAPPER_H_

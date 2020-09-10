// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_NAVIGATOR_SOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_NAVIGATOR_SOCKET_H_

#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class Navigator;
class ScriptState;
class SocketOptions;

class MODULES_EXPORT NavigatorSocket final
    : public GarbageCollected<NavigatorSocket>,
      public Supplement<ExecutionContext>,
      public ExecutionContextLifecycleStateObserver {
 public:
  static const char kSupplementName[];

  explicit NavigatorSocket(ExecutionContext*);
  ~NavigatorSocket() override = default;

  NavigatorSocket(const NavigatorSocket&) = delete;
  NavigatorSocket& operator=(const NavigatorSocket&) = delete;

  // Gets, or creates, NavigatorSocket supplement on ExecutionContext.
  // See platform/Supplementable.h
  static NavigatorSocket& From(ScriptState*);

  // Navigator partial interface
  static ScriptPromise openTCPSocket(ScriptState*,
                                     Navigator&,
                                     const SocketOptions*,
                                     ExceptionState&);

  static ScriptPromise openUDPSocket(ScriptState*,
                                     Navigator&,
                                     const SocketOptions*,
                                     ExceptionState&);

  // ExecutionContextLifecycleStateObserver:
  void ContextDestroyed() override;
  void ContextLifecycleStateChanged(mojom::blink::FrameLifecycleState) override;

  void Trace(Visitor*) const override;

 private:
  class PendingRequest;

  // Binds service_remote_ if not already bound.
  void EnsureServiceConnected(LocalDOMWindow&);

  static mojom::blink::DirectSocketOptionsPtr CreateSocketOptions(
      const SocketOptions&);

  ScriptPromise openTCPSocket(ScriptState*,
                              const SocketOptions*,
                              ExceptionState&);

  ScriptPromise openUDPSocket(ScriptState*,
                              const SocketOptions*,
                              ExceptionState&);

  // Updates exception state whenever returning false.
  bool OpenSocketPermitted(ScriptState*, const SocketOptions*, ExceptionState&);

  void OnConnectionError();

  HeapMojoRemote<blink::mojom::blink::DirectSocketsService> service_remote_{
      nullptr};

  HeapHashSet<Member<PendingRequest>> pending_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_NAVIGATOR_SOCKET_H_

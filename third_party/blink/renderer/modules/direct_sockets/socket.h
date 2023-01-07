// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_SOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_SOCKET_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/modules/direct_sockets/direct_sockets_service_mojo_remote.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

class ScriptPromise;
class ExceptionState;

// Base class for TCPSocket and UDPSocket.
class MODULES_EXPORT Socket : public ExecutionContextLifecycleStateObserver {
 public:
  // IDL definitions
  virtual ScriptPromise opened(ScriptState*) const;
  virtual ScriptPromise closed(ScriptState*) const;
  // Calls readable.cancel() and writable.abort() given that they're not locked
  // or rejects otherwise.
  virtual ScriptPromise close(ScriptState*, ExceptionState&);

 public:
  explicit Socket(ScriptState*);
  ~Socket() override;

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  static bool CheckContextAndPermissions(ScriptState*, ExceptionState&);
  static DOMException* CreateDOMExceptionFromNetErrorCode(int32_t net_error);

  // ExecutionContextLifecycleStateObserver:
  void ContextDestroyed() override;
  void ContextLifecycleStateChanged(mojom::blink::FrameLifecycleState) override;

  // Connects DirectSocketsServiceMojoRemote.
  void ConnectService();

  bool Closed() const;
  bool Initialized() const;
  bool HasPendingActivity() const;

  // Resolves |closed| promise.
  void ResolveClosed();

  // Rejects |closed| promise with the given |exception|.
  void RejectClosed(ScriptValue exception);

  // Closes |service_| and resets |feature_handle_for_scheduler_|.
  void CloseServiceAndResetFeatureHandle();

  void Trace(Visitor*) const override;

 protected:
  // Handler for |service_| errors.
  virtual void OnServiceConnectionError() = 0;

  const Member<ScriptState> script_state_;

  Member<DirectSocketsServiceMojoRemote> service_;

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  Member<ScriptPromiseResolver> opened_resolver_;
  const TraceWrapperV8Reference<v8::Promise> opened_;

  Member<ScriptPromiseResolver> closed_resolver_;
  const TraceWrapperV8Reference<v8::Promise> closed_;

  Member<ReadableStreamWrapper> readable_stream_wrapper_;
  Member<WritableStreamWrapper> writable_stream_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_SOCKET_H_

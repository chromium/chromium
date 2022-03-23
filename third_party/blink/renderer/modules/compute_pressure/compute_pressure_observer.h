// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_COMPUTE_PRESSURE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_COMPUTE_PRESSURE_OBSERVER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_compute_pressure_record.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_compute_pressure_update_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

namespace {

// https://wicg.github.io/compute-pressure/#ref-for-dfn-max-queued-records-1
constexpr wtf_size_t kMaxQueuedRecords = 10;

}  // namespace

class ExceptionState;
class ScriptState;
class ScriptPromise;
class ScriptPromiseResolver;
class ComputePressureObserverOptions;
class V8ComputePressureSource;

class ComputePressureObserver final
    : public ScriptWrappable,
      public ExecutionContextLifecycleStateObserver,
      public mojom::blink::ComputePressureObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ComputePressureObserver(ExecutionContext* execution_context,
                          V8ComputePressureUpdateCallback* observer_callback,
                          ComputePressureObserverOptions* normalized_options);
  ~ComputePressureObserver() override;

  static ComputePressureObserver* Create(ScriptState*,
                                         V8ComputePressureUpdateCallback*,
                                         ComputePressureObserverOptions*,
                                         ExceptionState&);

  // ComputePressureObserver IDL implementation.
  ScriptPromise observe(ScriptState*, V8ComputePressureSource, ExceptionState&);
  void unobserve(V8ComputePressureSource source);
  void disconnect();
  HeapVector<Member<ComputePressureRecord>> takeRecords();
  static Vector<V8ComputePressureSource> supportedSources();

  ComputePressureObserver(const ComputePressureObserver&) = delete;
  ComputePressureObserver operator=(const ComputePressureObserver&) = delete;

  // GarbageCollected implementation.
  void Trace(blink::Visitor*) const override;

  // mojom::blink::ComputePressureObserver implementation.
  void OnUpdate(mojom::blink::ComputePressureStatePtr state) override;

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override;

  // ExecutionContextLifecycleStateObserver implementation.
  void ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState state) override;

 private:
  // Called when `receiver_` is disconnected.
  void OnReceiverDisconnect();

  void DidAddObserver(ScriptPromiseResolver* resolver,
                      mojom::blink::ComputePressureStatus status);

  // The callback that receives compute pressure state updates.
  Member<V8ComputePressureUpdateCallback> observer_callback_;

  // The quantization scheme sent to the browser-side implementation.
  Member<ComputePressureObserverOptions> normalized_options_;

  // Last received records from the platform collector.
  // The records are only collected when there is a change in the status.
  HeapVector<Member<ComputePressureRecord>, kMaxQueuedRecords> records_;

  // Connection to the browser-side implementation.
  HeapMojoRemote<mojom::blink::ComputePressureHost> compute_pressure_host_;

  // Routes ComputePressureObserver mojo messages to this instance.
  HeapMojoReceiver<mojom::blink::ComputePressureObserver,
                   ComputePressureObserver>
      receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_COMPUTE_PRESSURE_OBSERVER_H_

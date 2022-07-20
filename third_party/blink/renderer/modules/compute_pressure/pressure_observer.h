// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/pressure_state.mojom-blink.h"
#include "third_party/blink/public/mojom/compute_pressure/pressure_service.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_record.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_update_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

namespace {

// https://wicg.github.io/compute-pressure/#dfn-max-queued-records
constexpr wtf_size_t kMaxQueuedRecords = 10;

}  // namespace

class ExceptionState;
class PressureObserverOptions;
class ScriptState;
class ScriptPromise;
class ScriptPromiseResolver;
class V8PressureSource;

class PressureObserver final : public ScriptWrappable,
                               public ExecutionContextLifecycleStateObserver,
                               public mojom::blink::PressureObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PressureObserver(ExecutionContext* execution_context,
                   V8PressureUpdateCallback* observer_callback,
                   PressureObserverOptions* normalized_options);
  ~PressureObserver() override;

  static PressureObserver* Create(ScriptState*,
                                  V8PressureUpdateCallback*,
                                  PressureObserverOptions*,
                                  ExceptionState&);

  // PressureObserver IDL implementation.
  ScriptPromise observe(ScriptState*, V8PressureSource, ExceptionState&);
  void unobserve(V8PressureSource source);
  void disconnect();
  HeapVector<Member<PressureRecord>> takeRecords();
  static Vector<V8PressureSource> supportedSources();

  PressureObserver(const PressureObserver&) = delete;
  PressureObserver operator=(const PressureObserver&) = delete;

  // GarbageCollected implementation.
  void Trace(blink::Visitor*) const override;

  // mojom::blink::PressureObserver implementation.
  void OnUpdate(device::mojom::blink::PressureStatePtr state) override;

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override;

  // ExecutionContextLifecycleStateObserver implementation.
  void ContextLifecycleStateChanged(
      mojom::blink::FrameLifecycleState state) override;

 private:
  // Called when `receiver_` is disconnected.
  void OnReceiverDisconnect();

  void DidAddObserver(ScriptPromiseResolver* resolver,
                      mojom::blink::PressureStatus status);

  // The callback that receives pressure state updates.
  Member<V8PressureUpdateCallback> observer_callback_;

  // The quantization scheme sent to the browser-side implementation.
  Member<PressureObserverOptions> normalized_options_;

  // Last received records from the platform collector.
  // The records are only collected when there is a change in the status.
  HeapVector<Member<PressureRecord>, kMaxQueuedRecords> records_;

  // Connection to the browser-side implementation.
  HeapMojoRemote<mojom::blink::PressureService> pressure_service_;

  // Routes PressureObserver mojo messages to this instance.
  HeapMojoReceiver<mojom::blink::PressureObserver, PressureObserver> receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_

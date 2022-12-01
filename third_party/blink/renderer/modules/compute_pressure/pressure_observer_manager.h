// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_

#include "third_party/blink/public/mojom/compute_pressure/pressure_service.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT PressureObserverManager final
    : public GarbageCollected<PressureObserverManager>,
      public ExecutionContextLifecycleStateObserver,
      public Supplement<LocalDOMWindow>,
      public mojom::blink::PressureObserver {
 public:
  static const char kSupplementName[];

  static PressureObserverManager* From(LocalDOMWindow&);

  explicit PressureObserverManager(LocalDOMWindow&);
  ~PressureObserverManager() override;

  PressureObserverManager(const PressureObserverManager&) = delete;
  PressureObserverManager& operator=(const PressureObserverManager&) = delete;

  ScriptPromise AddObserver(V8PressureSource,
                            blink::PressureObserver*,
                            ScriptState*,
                            ExceptionState&);
  void RemoveObserver(V8PressureSource, blink::PressureObserver*);
  void RemoveObserverFromAllSources(blink::PressureObserver*);

  // ContextLifecycleStateimplementation.
  void ContextDestroyed() override;
  void ContextLifecycleStateChanged(mojom::blink::FrameLifecycleState) override;

  // mojom::blink::PressureObserver implementation.
  void OnUpdate(device::mojom::blink::PressureUpdatePtr) override;

  // GarbageCollected implementation.
  void Trace(Visitor*) const override;

 private:
  void EnsureServiceConnection();

  // Called when `pressure_service_` is disconnected.
  void OnServiceConnectionError();

  // Called when `receiver_` is disconnected.
  void Reset();

  bool IsRegistering(V8PressureSource, blink::PressureObserver*) const;

  bool IsRegistered(V8PressureSource, blink::PressureObserver*) const;

  void DidBindObserver(V8PressureSource,
                       blink::PressureObserver*,
                       ScriptPromiseResolver*,
                       mojom::blink::PressureStatus);

  constexpr static size_t kPressureSourceSize = V8PressureSource::kEnumSize;

  // Connection to the browser-side implementation.
  HeapMojoRemote<mojom::blink::PressureService> pressure_service_;

  // Routes PressureObserver mojo messages to this instance.
  HeapMojoReceiver<mojom::blink::PressureObserver, PressureObserverManager>
      receiver_;

  HeapHashSet<Member<blink::PressureObserver>>
      registering_observers_[kPressureSourceSize];

  HeapHashSet<Member<blink::PressureObserver>>
      registered_observers_[kPressureSourceSize];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_

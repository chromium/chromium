// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_

#include "services/device/public/mojom/pressure_manager.mojom-blink.h"
#include "services/device/public/mojom/pressure_update.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;

// This class implements the "device::mojom::blink::PressureClient"
// interface to receive "device::mojom::blink::PressureUpdate" from
// "device::PressureManagerImpl" and broadcasts the information to active
// PressureObservers.
class MODULES_EXPORT PressureObserverManager final
    : public GarbageCollected<PressureObserverManager>,
      public ExecutionContextLifecycleStateObserver,
      public Supplement<ExecutionContext>,
      public device::mojom::blink::PressureClient {
 public:
  static const char kSupplementName[];

  static PressureObserverManager* From(ExecutionContext*);

  explicit PressureObserverManager(ExecutionContext*);
  ~PressureObserverManager() override;

  PressureObserverManager(const PressureObserverManager&) = delete;
  PressureObserverManager& operator=(const PressureObserverManager&) = delete;

  void AddObserver(V8PressureSource::Enum, blink::PressureObserver*);
  void RemoveObserver(V8PressureSource::Enum, blink::PressureObserver*);
  void RemoveObserverFromAllSources(blink::PressureObserver*);

  // ContextLifecycleStateimplementation.
  void ContextDestroyed() override;
  void ContextLifecycleStateChanged(mojom::blink::FrameLifecycleState) override;

  // device::mojom::blink::PressureClient implementation.
  void OnPressureUpdated(device::mojom::blink::PressureUpdatePtr) override;

  // GarbageCollected implementation.
  void Trace(Visitor*) const override;

 private:
  // kUninitialized: receiver_ is not bound and
  // pressure_manager_->AddClient() must be called.
  // kInitializing: pressure_manager_->AddClient() has been called,
  // but DidAddClient() has not been called yet.
  // kInitialized: DidAddClient() was invoked and succeeded.
  enum class State { kUninitialized, kInitializing, kInitialized };

  void EnsureServiceConnection();

  // Verifies if the data should be delivered according to privacy status.
  bool PassesPrivacyTest() const;

  // Called when `pressure_manager_` is disconnected.
  void OnServiceConnectionError();

  // Called when `receiver_` is disconnected.
  void Reset();

  void DidAddClient(V8PressureSource::Enum,
                    device::mojom::blink::PressureStatus);

  constexpr static size_t kPressureSourceSize = V8PressureSource::kEnumSize;

  // Connection to the browser-side implementation.
  HeapMojoRemote<device::mojom::blink::PressureManager> pressure_manager_;

  // Routes PressureObserver mojo messages to this instance.
  HeapMojoReceiver<device::mojom::blink::PressureClient,
                   PressureObserverManager>
      receiver_;

  State state_ = State::kUninitialized;

  HeapHashSet<Member<blink::PressureObserver>> observers_[kPressureSourceSize];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_MANAGER_H_

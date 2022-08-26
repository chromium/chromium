// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_

#include "services/device/public/mojom/pressure_state.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_record.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_update_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

// https://wicg.github.io/compute-pressure/#dfn-max-queued-records
constexpr wtf_size_t kMaxQueuedRecords = 10;

}  // namespace

class ExceptionState;
class PressureObserverManager;
class PressureObserverOptions;
class ScriptState;
class ScriptPromise;
class V8PressureSource;

class PressureObserver final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PressureObserver(V8PressureUpdateCallback* observer_callback,
                   PressureObserverOptions* normalized_options);
  ~PressureObserver() override;

  static PressureObserver* Create(V8PressureUpdateCallback*,
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

  // Called by PressureObserverManager.
  void OnUpdate(device::mojom::blink::PressureStatePtr state);

  const PressureObserverOptions* normalized_options() const {
    return normalized_options_;
  }

 private:
  // Manages registered observer list for each source.
  WeakMember<PressureObserverManager> manager_;

  // The callback that receives pressure state updates.
  Member<V8PressureUpdateCallback> observer_callback_;

  // The quantization scheme for this observer.
  Member<PressureObserverOptions> normalized_options_;

  // Last received records from the platform collector.
  // The records are only collected when there is a change in the status.
  HeapVector<Member<PressureRecord>, kMaxQueuedRecords> records_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_

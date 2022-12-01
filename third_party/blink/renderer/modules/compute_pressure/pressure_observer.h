// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_

#include "services/device/public/mojom/pressure_manager.mojom-blink.h"
#include "services/device/public/mojom/pressure_update.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_factor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_update_callback.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

// https://wicg.github.io/compute-pressure/#dfn-max-queued-records
constexpr wtf_size_t kMaxQueuedRecords = 10;

}  // namespace

class ExceptionState;
class PressureObserverManager;
class PressureObserverOptions;
class PressureRecord;
class ScriptPromise;
class ScriptState;

class PressureObserver final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PressureObserver(V8PressureUpdateCallback*,
                   PressureObserverOptions*,
                   ExceptionState&);
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
  void OnUpdate(ExecutionContext*,
                V8PressureSource::Enum,
                V8PressureState::Enum,
                const Vector<V8PressureFactor>&,
                DOMHighResTimeStamp);

 private:
  // Verifies if the latest update was at least longer than the sample period.
  bool PassesRateTest(V8PressureSource::Enum, const DOMHighResTimeStamp&) const;

  // Verifies if there is data change in between last update and new one.
  bool HasChangeInData(V8PressureSource::Enum,
                       V8PressureState::Enum,
                       const Vector<V8PressureFactor>&) const;

  // Scheduled method to invoke callback.
  void ReportToCallback(ExecutionContext*);

  // Manages registered observer list for each source.
  WeakMember<PressureObserverManager> manager_;

  // The callback that receives pressure state updates.
  Member<V8PressureUpdateCallback> observer_callback_;

  // Requested sample rate from the user.
  // https://wicg.github.io/compute-pressure/#dfn-samplerate
  double sample_rate_;

  // The last valid record received from the observer manager.
  // Stored to avoid sending updates whenever the new record is the same.
  Member<PressureRecord> last_record_map_[V8PressureSource::kEnumSize];

  // Last received records from the platform collector.
  // The records are only collected when there is a change in the status.
  HeapVector<Member<PressureRecord>, kMaxQueuedRecords> records_;

  // Task handle to check if the posted task is still pending.
  TaskHandle pending_report_to_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COMPUTE_PRESSURE_PRESSURE_OBSERVER_H_

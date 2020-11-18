// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_MEASURE_MEMORY_MEASURE_MEMORY_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_MEASURE_MEMORY_MEASURE_MEMORY_CONTROLLER_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8.h"

namespace blink {

class LocalDOMWindow;
class MeasureMemoryBreakdown;
class ScriptState;
class ExceptionState;

// The implementation of Performance.measureMemory() Web API.
// It is responsible for:
// 1. Starting an asynchronous memory measurement of the main V8 isolate.
// 2. Starting an asynchronous memory measurement of dedicated workers.
// 3. Waiting for measurements to complete.
// 4. Constructing the result and resolving the JS promise.
class MeasureMemoryController final
    : public GarbageCollected<MeasureMemoryController> {
 public:
  using Result = HeapVector<Member<MeasureMemoryBreakdown>>;
  using ResultCallback = base::OnceCallback<void(Result)>;

  // PerformanceManager in blink/renderer/controller uses this interface
  // to provide an implementation of memory measurement for dedicated workers.
  //
  // It will be removed in the future when performance.measureMemory switches
  // to a mojo-based implementation that queries PerformanceManager in the
  // browser process.
  class V8MemoryReporter {
   public:
    virtual void GetMemoryUsage(MeasureMemoryController::ResultCallback,
                                v8::MeasureMemoryExecution) = 0;
  };

  // Private constructor guarded by PassKey. Use the StartMeasurement() wrapper
  // to construct the object and start the measurement.
  MeasureMemoryController(base::PassKey<MeasureMemoryController>,
                          v8::Isolate*,
                          v8::Local<v8::Context>,
                          v8::Local<v8::Promise::Resolver>);

  ~MeasureMemoryController() = default;

  static ScriptPromise StartMeasurement(ScriptState*, ExceptionState&);

  void Trace(Visitor* visitor) const;

  // The entry point for injecting dependency on PerformanceManager.
  CORE_EXPORT static void SetDedicatedWorkerMemoryReporter(V8MemoryReporter*);

 private:
  static bool IsMeasureMemoryAvailable(LocalDOMWindow* window);
  // Invoked when the memory of the main V8 isolate is measured.
  void MainMeasurementComplete(Result);
  // Invoked when the memory of all dedicated workers is measured.
  void WorkerMeasurementComplete(Result);
  // Resolves the JS promise if both pending measurements are done.
  void MaybeResolvePromise();

  v8::Isolate* isolate_;
  ScopedPersistent<v8::Context> context_;
  TraceWrapperV8Reference<v8::Promise::Resolver> promise_resolver_;
  Result main_result_;
  Result worker_result_;
  bool main_measurement_completed_ = false;
  bool worker_measurement_completed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_MEASURE_MEMORY_MEASURE_MEMORY_CONTROLLER_H

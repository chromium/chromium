// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_delegate.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory_breakdown.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {

MeasureMemoryController::MeasureMemoryController(
    util::PassKey<MeasureMemoryController>,
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Promise::Resolver> promise_resolver)
    : isolate_(isolate),
      context_(isolate, context),
      promise_resolver_(isolate, promise_resolver) {
  context_.SetPhantom();
  // TODO(ulan): Currently we keep a strong reference to the promise resolver.
  // This may prolong the lifetime of the context by one more GC in the worst
  // case as JSPromise keeps its context alive.
  // To avoid that we should use an ephemeron context_ => promise_resolver_.
}

void MeasureMemoryController::Trace(Visitor* visitor) const {
  visitor->Trace(promise_resolver_);
}

ScriptPromise MeasureMemoryController::StartMeasurement(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!IsMeasureMemoryAvailable(LocalDOMWindow::From(script_state))) {
    exception_state.ThrowSecurityError(
        "performance.measureMemory is not available in this context");
    return ScriptPromise();
  }
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Promise::Resolver> promise_resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&promise_resolver)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return ScriptPromise();
  }
  v8::MeasureMemoryExecution execution =
      RuntimeEnabledFeatures::ForceEagerMeasureMemoryEnabled(
          ExecutionContext::From(script_state))
          ? v8::MeasureMemoryExecution::kEager
          : v8::MeasureMemoryExecution::kDefault;

  auto* impl = MakeGarbageCollected<MeasureMemoryController>(
      util::PassKey<MeasureMemoryController>(), isolate, context,
      promise_resolver);

  isolate->MeasureMemory(
      std::make_unique<MeasureMemoryDelegate>(
          isolate, context,
          WTF::Bind(&MeasureMemoryController::MeasurementComplete,
                    WrapPersistent(impl))),
      execution);
  return ScriptPromise(script_state, promise_resolver->GetPromise());
}

bool MeasureMemoryController::IsMeasureMemoryAvailable(LocalDOMWindow* window) {
  // TODO(ulan): We should check for window.crossOriginIsolated when it ships.
  // Until then we enable the API only for processes locked to a site
  // similar to the precise mode of the legacy performance.memory API.
  if (!Platform::Current()->IsLockedToSite()) {
    return false;
  }
  // The window.crossOriginIsolated will be true only for the top-level frame.
  // Until the flag is available we check for the top-level condition manually.
  if (!window) {
    return false;
  }
  LocalFrame* local_frame = window->GetFrame();
  if (!local_frame || !local_frame->IsMainFrame()) {
    return false;
  }
  return true;
}

void MeasureMemoryController::MeasurementComplete(
    HeapVector<Member<MeasureMemoryBreakdown>> breakdown) {
  if (context_.IsEmpty()) {
    // The context was garbage collected in the meantime.
    return;
  }
  v8::Local<v8::Context> context = context_.NewLocal(isolate_);
  MeasureMemory* result = MeasureMemory::Create();
  size_t total_size = 0;
  for (auto entry : breakdown) {
    total_size += entry->bytes();
  }
  result->setBytes(total_size);
  result->setBreakdown(breakdown);
  v8::Local<v8::Promise::Resolver> promise_resolver =
      promise_resolver_.NewLocal(isolate_);
  promise_resolver->Resolve(context, ToV8(result, promise_resolver, isolate_))
      .ToChecked();
}

}  // namespace blink

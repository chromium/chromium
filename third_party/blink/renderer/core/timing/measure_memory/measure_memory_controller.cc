// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"

#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_attribution.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_breakdown_entry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_measurement.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

using performance_manager::mojom::blink::WebMemoryAttribution;
using performance_manager::mojom::blink::WebMemoryAttributionPtr;
using performance_manager::mojom::blink::WebMemoryBreakdownEntryPtr;
using performance_manager::mojom::blink::WebMemoryMeasurement;
using performance_manager::mojom::blink::WebMemoryMeasurementPtr;

namespace blink {

MeasureMemoryController::MeasureMemoryController(
    base::PassKey<MeasureMemoryController>,
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
  auto measurement_mode =
      RuntimeEnabledFeatures::ForceEagerMeasureMemoryEnabled(
          ExecutionContext::From(script_state))
          ? WebMemoryMeasurement::Mode::kEager
          : WebMemoryMeasurement::Mode::kDefault;

  auto* impl = MakeGarbageCollected<MeasureMemoryController>(
      base::PassKey<MeasureMemoryController>(), isolate, context,
      promise_resolver);
  Document* document = LocalDOMWindow::From(script_state)->document();
  document->GetResourceCoordinator()->OnWebMemoryMeasurementRequested(
      measurement_mode, WTF::Bind(&MeasureMemoryController::MeasurementComplete,
                                  WrapPersistent(impl)));

  return ScriptPromise(script_state, promise_resolver->GetPromise());
}

bool MeasureMemoryController::IsMeasureMemoryAvailable(LocalDOMWindow* window) {
  if (!base::FeatureList::IsEnabled(
          features::kWebMeasureMemoryViaPerformanceManager)) {
    return false;
  }
  if (!window || !window->CrossOriginIsolatedCapability()) {
    return false;
  }
  // CrossOriginIsolated is also set for same-agent cross-origin iframe.
  // Allow only iframes that have the same origin as the main frame.
  // Note that COOP guarantees that all main frames have the same origin.
  LocalFrame* local_frame = window->GetFrame();
  if (!local_frame || local_frame->IsCrossOriginToMainFrame()) {
    return false;
  }

  // We need DocumentResourceCoordinator to query PerformanceManager.
  if (!window->document() || !window->document()->GetResourceCoordinator()) {
    return false;
  }

  return true;
}

namespace {

// These functions convert WebMemory* mojo structs to IDL and JS values.
WTF::String ConvertScope(WebMemoryAttribution::Scope scope) {
  switch (scope) {
    case WebMemoryAttribution::Scope::kWindow:
      return "Window";
  }
}

MemoryAttribution* ConvertAttribution(
    const WebMemoryAttributionPtr& attribution) {
  auto* result = MemoryAttribution::Create();
  result->setUrl(attribution->url);
  result->setScope(ConvertScope(attribution->scope));
  result->setContainer(nullptr);
  return result;
}

MemoryBreakdownEntry* ConvertBreakdown(
    const WebMemoryBreakdownEntryPtr& breakdown_entry) {
  auto* result = MemoryBreakdownEntry::Create();
  result->setBytes(breakdown_entry->bytes);
  HeapVector<Member<MemoryAttribution>> attribution;
  for (const auto& entry : breakdown_entry->attribution) {
    attribution.push_back(ConvertAttribution(entry));
  }
  result->setAttribution(attribution);
  result->setUserAgentSpecificTypes(Vector<String>());
  return result;
}

MemoryMeasurement* ConvertResult(const WebMemoryMeasurementPtr& measurement) {
  HeapVector<Member<MemoryBreakdownEntry>> breakdown;
  for (const auto& entry : measurement->breakdown) {
    breakdown.push_back(ConvertBreakdown(entry));
  }
  size_t bytes = 0;
  for (auto entry : breakdown) {
    bytes += entry->bytes();
  }
  auto* result = MemoryMeasurement::Create();
  result->setBreakdown(breakdown);
  result->setBytes(bytes);
  return result;
}

}  // anonymous namespace

void MeasureMemoryController::MeasurementComplete(
    WebMemoryMeasurementPtr measurement) {
  if (context_.IsEmpty()) {
    // The context was garbage collected in the meantime.
    return;
  }
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = context_.NewLocal(isolate_);
  v8::Context::Scope context_scope(context);
  auto* result = ConvertResult(measurement);
  v8::Local<v8::Promise::Resolver> promise_resolver =
      promise_resolver_.NewLocal(isolate_);
  promise_resolver->Resolve(context, ToV8(result, promise_resolver, isolate_))
      .ToChecked();
  promise_resolver_.Clear();
}

}  // namespace blink

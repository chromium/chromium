// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"

#include <algorithm>
#include "base/rand_util.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_attribution.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_attribution_container.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_breakdown_entry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_measurement.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
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
using performance_manager::mojom::blink::WebMemoryUsagePtr;

namespace blink {

namespace {

// String constants used for building the result.
constexpr const char* kCrossOriginUrl = "cross-origin-url";
constexpr const char* kMemoryTypeDom = "DOM";
constexpr const char* kMemoryTypeJavaScript = "JavaScript";
constexpr const char* kMemoryTypeShared = "Shared";
constexpr const char* kScopeCrossOriginAggregated = "cross-origin-aggregated";
constexpr const char* kScopeDedicatedWorker = "DedicatedWorker";
constexpr const char* kScopeWindow = "Window";

}  // anonymous namespace

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

namespace {

enum class ApiStatus {
  kAvailable,
  kNotAvailableDueToFlag,
  kNotAvailableDueToDetachedContext,
  kNotAvailableDueToCrossOriginContext,
  kNotAvailableDueToCrossOriginIsolation,
  kNotAvailableDueToResourceCoordinator,
};

ApiStatus CheckMeasureMemoryAvailability(LocalDOMWindow* window) {
  if (!base::FeatureList::IsEnabled(
          features::kWebMeasureMemoryViaPerformanceManager)) {
    return ApiStatus::kNotAvailableDueToFlag;
  }
  if (!window) {
    return ApiStatus::kNotAvailableDueToDetachedContext;
  }
  LocalFrame* local_frame = window->GetFrame();
  if (!local_frame) {
    return ApiStatus::kNotAvailableDueToDetachedContext;
  }
  if (!window->CrossOriginIsolatedCapability() &&
      local_frame->GetSettings()->GetWebSecurityEnabled()) {
    return ApiStatus::kNotAvailableDueToCrossOriginIsolation;
  }

  // We need DocumentResourceCoordinator to query PerformanceManager.
  if (!window->document()) {
    return ApiStatus::kNotAvailableDueToDetachedContext;
  }

  if (!window->document()->GetResourceCoordinator()) {
    return ApiStatus::kNotAvailableDueToResourceCoordinator;
  }

  return ApiStatus::kAvailable;
}

}  // anonymous namespace

ScriptPromise MeasureMemoryController::StartMeasurement(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  switch (auto status = CheckMeasureMemoryAvailability(
              LocalDOMWindow::From(script_state))) {
    case ApiStatus::kAvailable:
      break;
    case ApiStatus::kNotAvailableDueToFlag:
    case ApiStatus::kNotAvailableDueToResourceCoordinator:
      exception_state.ThrowSecurityError(
          "performance.measureUserAgentSpecificMemory is not available.");
      return ScriptPromise();
    case ApiStatus::kNotAvailableDueToDetachedContext:
      exception_state.ThrowSecurityError(
          "performance.measureUserAgentSpecificMemory is not supported"
          " in detached iframes.");
      return ScriptPromise();
    case ApiStatus::kNotAvailableDueToCrossOriginContext:
      exception_state.ThrowSecurityError(
          "performance.measureUserAgentSpecificMemory is not supported"
          " in cross-origin iframes.");
      return ScriptPromise();
    case ApiStatus::kNotAvailableDueToCrossOriginIsolation:
      exception_state.ThrowSecurityError(
          "performance.measureUserAgentSpecificMemory requires"
          " cross-origin isolation.");
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


namespace {

// Satisfies the requirements of UniformRandomBitGenerator from C++ standard.
// It is used in std::shuffle calls below.
struct RandomBitGenerator {
  using result_type = size_t;
  static constexpr size_t min() { return 0; }
  static constexpr size_t max() {
    return static_cast<size_t>(std::numeric_limits<int>::max());
  }
  size_t operator()() {
    return static_cast<size_t>(base::RandInt(min(), max()));
  }
};

// These functions convert WebMemory* mojo structs to IDL and JS values.
WTF::AtomicString ConvertScope(WebMemoryAttribution::Scope scope) {
  using Scope = WebMemoryAttribution::Scope;
  switch (scope) {
    case Scope::kDedicatedWorker:
      return kScopeDedicatedWorker;
    case Scope::kWindow:
      return kScopeWindow;
    case Scope::kCrossOriginAggregated:
      return kScopeCrossOriginAggregated;
  }
}

MemoryAttributionContainer* ConvertContainer(
    const WebMemoryAttributionPtr& attribution) {
  if (!attribution->src && !attribution->id) {
    return nullptr;
  }
  auto* result = MemoryAttributionContainer::Create();
  result->setSrc(attribution->src);
  result->setId(attribution->id);
  return result;
}

MemoryAttribution* ConvertAttribution(
    const WebMemoryAttributionPtr& attribution) {
  auto* result = MemoryAttribution::Create();
  if (attribution->url) {
    result->setUrl(attribution->url);
  } else {
    result->setUrl(kCrossOriginUrl);
  }
  result->setScope(ConvertScope(attribution->scope));
  result->setContainer(ConvertContainer(attribution));
  return result;
}

MemoryBreakdownEntry* ConvertBreakdown(
    const WebMemoryBreakdownEntryPtr& breakdown_entry) {
  auto* result = MemoryBreakdownEntry::Create();
  DCHECK(breakdown_entry->memory);
  result->setBytes(breakdown_entry->memory->bytes);
  HeapVector<Member<MemoryAttribution>> attribution;
  for (const auto& entry : breakdown_entry->attribution) {
    attribution.push_back(ConvertAttribution(entry));
  }
  result->setAttribution(attribution);
  result->setTypes({WTF::AtomicString(kMemoryTypeJavaScript)});
  return result;
}

MemoryBreakdownEntry* CreateUnattributedBreakdown(
    const WebMemoryUsagePtr& memory,
    const WTF::String& memory_type) {
  auto* result = MemoryBreakdownEntry::Create();
  DCHECK(memory);
  result->setBytes(memory->bytes);
  result->setAttribution({});
  Vector<String> types;
  types.push_back(memory_type);
  result->setTypes(types);
  return result;
}

MemoryBreakdownEntry* EmptyBreakdown() {
  auto* result = MemoryBreakdownEntry::Create();
  result->setBytes(0);
  result->setAttribution({});
  result->setTypes({});
  return result;
}

MemoryMeasurement* ConvertResult(const WebMemoryMeasurementPtr& measurement) {
  HeapVector<Member<MemoryBreakdownEntry>> breakdown;
  for (const auto& entry : measurement->breakdown) {
    // Skip breakdowns that didn't get a measurement.
    if (entry->memory)
      breakdown.push_back(ConvertBreakdown(entry));
  }
  // Add breakdowns for memory that isn't attributed to an execution context.
  breakdown.push_back(CreateUnattributedBreakdown(measurement->shared_memory,
                                                  kMemoryTypeShared));
  breakdown.push_back(
      CreateUnattributedBreakdown(measurement->blink_memory, kMemoryTypeDom));
  // TODO(1085129): Report memory usage of detached frames once implemented.
  // Add an empty breakdown entry as required by the spec.
  // See https://github.com/WICG/performance-measure-memory/issues/10.
  breakdown.push_back(EmptyBreakdown());
  // Randomize the order of the entries as required by the spec.
  std::shuffle(breakdown.begin(), breakdown.end(), RandomBitGenerator{});
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

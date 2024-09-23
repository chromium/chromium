// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"

#include <algorithm>
#include "base/rand_util.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_attribution.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_attribution_container.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_breakdown_entry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_measurement.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/timing/measure_memory/local_web_memory_measurer.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
constexpr const char* kMemoryTypeCanvas = "Canvas";
constexpr const char* kMemoryTypeDom = "DOM";
constexpr const char* kMemoryTypeJavaScript = "JavaScript";
constexpr const char* kMemoryTypeShared = "Shared";
constexpr const char* kScopeCrossOriginAggregated = "cross-origin-aggregated";
constexpr const char* kScopeDedicatedWorker = "DedicatedWorkerGlobalScope";
constexpr const char* kScopeServiceWorker = "ServiceWorkerGlobalScope";
constexpr const char* kScopeSharedWorker = "SharedWorkerGlobalScope";
constexpr const char* kScopeWindow = "Window";

}  // anonymous namespace

MeasureMemoryController::MeasureMemoryController(
    base::PassKey<MeasureMemoryController>,
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    ScriptPromiseResolver<MemoryMeasurement>* resolver)
    : context_(isolate, context), resolver_(resolver) {
  context_.SetPhantom();
  // TODO(ulan): Currently we keep a strong reference to the promise resolver.
  // This may prolong the lifetime of the context by one more GC in the worst
  // case as JSPromise keeps its context alive.
  // To avoid that we should use an ephemeron context_ => resolver_.
}

void MeasureMemoryController::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
}

namespace {

enum class ApiStatus {
  kAvailable,
  kNotAvailableDueToFlag,
  kNotAvailableDueToDetachedContext,
  kNotAvailableDueToCrossOriginContext,
  kNotAvailableDueToResourceCoordinator,
};

ApiStatus CheckMeasureMemoryAvailability() {
  if (!RuntimeEnabledFeatures::PerformanceManagerInstrumentationEnabled()) {
    return ApiStatus::kNotAvailableDueToResourceCoordinator;
  }
  return ApiStatus::kAvailable;
}

bool IsAttached(ExecutionContext* execution_context) {
  auto* window = To<LocalDOMWindow>(execution_context);
  return window && window->GetFrame() && window->document();
}

void StartMemoryMeasurement(LocalDOMWindow* window,
                            MeasureMemoryController* controller,
                            WebMemoryMeasurement::Mode mode) {
  Document* document = window->document();
  document->GetResourceCoordinator()->OnWebMemoryMeasurementRequested(
      mode, WTF::BindOnce(&MeasureMemoryController::MeasurementComplete,
                          WrapPersistent(controller)));
}

void StartMemoryMeasurement(WorkerGlobalScope* worker,
                            MeasureMemoryController* controller,
                            WebMemoryMeasurement::Mode mode) {
  DCHECK(worker->IsSharedWorkerGlobalScope() ||
         worker->IsServiceWorkerGlobalScope());
  WebMemoryAttribution::Scope attribution_scope =
      worker->IsServiceWorkerGlobalScope()
          ? WebMemoryAttribution::Scope::kServiceWorker
          : WebMemoryAttribution::Scope::kSharedWorker;
  LocalWebMemoryMeasurer::StartMeasurement(worker->GetIsolate(), mode,
                                           controller, attribution_scope,
                                           worker->Url().GetString());
}

}  // anonymous namespace

ScriptPromise<MemoryMeasurement> MeasureMemoryController::StartMeasurement(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->CrossOriginIsolatedCapability());
  ApiStatus status = CheckMeasureMemoryAvailability();
  if (status == ApiStatus::kAvailable && execution_context->IsWindow() &&
      !IsAttached(execution_context)) {
    status = ApiStatus::kNotAvailableDueToDetachedContext;
  }
  switch (status) {
    case ApiStatus::kAvailable:
      break;
    case ApiStatus::kNotAvailableDueToFlag:
    case ApiStatus::kNotAvailableDueToResourceCoordinator:
      exception_state.ThrowSecurityError(
          "performance.measureUserAgentSpecificMemory is not available.");
      return EmptyPromise();
    case ApiStatus::kNotAvailableDueToDetachedContext:
      exception_state.ThrowSecurityError(
          "performance.measureUserAgentSpecificMemory is not supported"
          " in detached iframes.");
      return EmptyPromise();
    case ApiStatus::kNotAvailableDueToCrossOriginContext:
      exception_state.ThrowSecurityError(
          "performance.measureUserAgentSpecificMemory is not supported"
          " in cross-origin iframes.");
      return EmptyPromise();
  }
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<MemoryMeasurement>>(
          script_state);
  auto promise = resolver->Promise();

  auto measurement_mode =
      RuntimeEnabledFeatures::ForceEagerMeasureMemoryEnabled(
          ExecutionContext::From(script_state))
          ? WebMemoryMeasurement::Mode::kEager
          : WebMemoryMeasurement::Mode::kDefault;

  auto* impl = MakeGarbageCollected<MeasureMemoryController>(
      base::PassKey<MeasureMemoryController>(), isolate, context, resolver);

  if (execution_context->IsWindow()) {
    StartMemoryMeasurement(To<LocalDOMWindow>(execution_context), impl,
                           measurement_mode);
  } else {
    StartMemoryMeasurement(To<WorkerGlobalScope>(execution_context), impl,
                           measurement_mode);
  }
  return promise;
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
String ConvertScope(WebMemoryAttribution::Scope scope) {
  using Scope = WebMemoryAttribution::Scope;
  switch (scope) {
    case Scope::kDedicatedWorker:
      return kScopeDedicatedWorker;
    case Scope::kWindow:
      return kScopeWindow;
    case Scope::kCrossOriginAggregated:
      return kScopeCrossOriginAggregated;
    case Scope::kServiceWorker:
      return kScopeServiceWorker;
    case Scope::kSharedWorker:
      return kScopeSharedWorker;
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
  if (auto* container = ConvertContainer(attribution)) {
    result->setContainer(container);
  }
  return result;
}

MemoryBreakdownEntry* ConvertJavaScriptBreakdown(
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

MemoryBreakdownEntry* ConvertCanvasBreakdown(
    const WebMemoryBreakdownEntryPtr& breakdown_entry) {
  auto* result = MemoryBreakdownEntry::Create();
  DCHECK(breakdown_entry->canvas_memory);
  result->setBytes(breakdown_entry->canvas_memory->bytes);
  HeapVector<Member<MemoryAttribution>> attribution;
  for (const auto& entry : breakdown_entry->attribution) {
    attribution.push_back(ConvertAttribution(entry));
  }
  result->setAttribution(attribution);
  result->setTypes({WTF::AtomicString(kMemoryTypeCanvas)});
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
    if (entry->memory) {
      breakdown.push_back(ConvertJavaScriptBreakdown(entry));
    }
    // Skip breakdowns that didn't get a measurement.
    if (entry->canvas_memory) {
      breakdown.push_back(ConvertCanvasBreakdown(entry));
    }
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

bool IsDedicatedWorkerEntry(const WebMemoryBreakdownEntryPtr& breakdown_entry) {
  for (const auto& entry : breakdown_entry->attribution) {
    if (entry->scope == WebMemoryAttribution::Scope::kDedicatedWorker)
      return true;
  }
  return false;
}

uint64_t GetDedicatedWorkerJavaScriptUkm(
    const WebMemoryMeasurementPtr& measurement) {
  size_t result = 0;
  for (const auto& entry : measurement->breakdown) {
    if (entry->memory && IsDedicatedWorkerEntry(entry)) {
      result += entry->memory->bytes;
    }
  }
  return result;
}

uint64_t GetJavaScriptUkm(const WebMemoryMeasurementPtr& measurement) {
  size_t result = 0;
  for (const auto& entry : measurement->breakdown) {
    if (entry->memory) {
      result += entry->memory->bytes;
    }
  }
  return result;
}

uint64_t GetDomUkm(const WebMemoryMeasurementPtr& measurement) {
  return measurement->blink_memory->bytes;
}

uint64_t GetSharedUkm(const WebMemoryMeasurementPtr& measurement) {
  return measurement->shared_memory->bytes;
}

void RecordWebMemoryUkm(ExecutionContext* execution_context,
                        const WebMemoryMeasurementPtr& measurement) {
  if (!execution_context) {
    // This may happen if the context was detached while the memory
    // measurement was in progress.
    return;
  }
  const uint64_t kBytesInKB = 1024;
  ukm::builders::PerformanceAPI_Memory(execution_context->UkmSourceID())
      .SetJavaScript(GetJavaScriptUkm(measurement) / kBytesInKB)
      .SetJavaScript_DedicatedWorker(
          GetDedicatedWorkerJavaScriptUkm(measurement) / kBytesInKB)
      .SetDom(GetDomUkm(measurement) / kBytesInKB)
      .SetShared(GetSharedUkm(measurement) / kBytesInKB)
      .Record(execution_context->UkmRecorder());
}

}  // anonymous namespace

void MeasureMemoryController::MeasurementComplete(
    WebMemoryMeasurementPtr measurement) {
  resolver_->Resolve(ConvertResult(measurement));
  RecordWebMemoryUkm(resolver_->GetExecutionContext(), measurement);
}

}  // namespace blink

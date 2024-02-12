// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/timing/performance_mark.h"

#include <optional>

#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_mark_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

PerformanceMark::PerformanceMark(
    const AtomicString& name,
    double start_time,
    base::TimeTicks unsafe_time_for_traces,
    scoped_refptr<SerializedScriptValue> serialized_detail,
    ExceptionState& exception_state,
    DOMWindow* source)
    : PerformanceEntry(name, start_time, start_time, source),
      serialized_detail_(std::move(serialized_detail)),
      unsafe_time_for_traces_(unsafe_time_for_traces) {}

// static
PerformanceMark* PerformanceMark::Create(ScriptState* script_state,
                                         const AtomicString& mark_name,
                                         PerformanceMarkOptions* mark_options,
                                         ExceptionState& exception_state) {
  Performance* performance = nullptr;
  bool is_worker_global_scope = false;
  if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
    performance = DOMWindowPerformance::performance(*window);
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(
                 ExecutionContext::From(script_state))) {
    performance = WorkerGlobalScopePerformance::performance(*scope);
    is_worker_global_scope = true;
  }
  DCHECK(performance);

  DOMHighResTimeStamp start = 0.0;
  base::TimeTicks unsafe_start_for_traces;
  std::optional<ScriptValue> detail;
  if (mark_options) {
    if (mark_options->hasStartTime()) {
      start = mark_options->startTime();
      if (start < 0.0) {
        exception_state.ThrowTypeError("'" + mark_name +
                                       "' cannot have a negative start time.");
        return nullptr;
      }
      // |start| is in milliseconds from the start of navigation.
      // GetTimeOrigin() returns seconds from the monotonic clock's origin..
      // Trace events timestamps accept seconds (as a double) based on
      // CurrentTime::monotonicallyIncreasingTime().
      unsafe_start_for_traces =
          performance->GetTimeOriginInternal() + base::Milliseconds(start);
    } else {
      start = performance->now();
      unsafe_start_for_traces = base::TimeTicks::Now();
    }

    if (mark_options->hasDetail())
      detail = mark_options->detail();
  } else {
    start = performance->now();
    unsafe_start_for_traces = base::TimeTicks::Now();
  }

  if (!is_worker_global_scope &&
      PerformanceTiming::IsAttributeName(mark_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + mark_name +
            "' is part of the PerformanceTiming interface, and "
            "cannot be used as a mark name.");
    return nullptr;
  }

  scoped_refptr<SerializedScriptValue> serialized_detail;
  if (!detail) {
    serialized_detail = nullptr;
  } else {
    serialized_detail = SerializedScriptValue::Serialize(
        script_state->GetIsolate(), (*detail).V8Value(),
        SerializedScriptValue::SerializeOptions(), exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }
  }

  return MakeGarbageCollected<PerformanceMark>(
      mark_name, start, unsafe_start_for_traces, std::move(serialized_detail),
      exception_state, LocalDOMWindow::From(script_state));
}

const AtomicString& PerformanceMark::entryType() const {
  return performance_entry_names::kMark;
}

PerformanceEntryType PerformanceMark::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kMark;
}

mojom::blink::PerformanceMarkOrMeasurePtr
PerformanceMark::ToMojoPerformanceMarkOrMeasure() {
  auto mojo_performance_mark_or_measure =
      PerformanceEntry::ToMojoPerformanceMarkOrMeasure();
  if (serialized_detail_) {
    mojo_performance_mark_or_measure->detail =
        serialized_detail_->GetWireData();
  }
  return mojo_performance_mark_or_measure;
}

ScriptValue PerformanceMark::detail(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!serialized_detail_)
    return ScriptValue(isolate, v8::Null(isolate));
  auto result = deserialized_detail_map_.insert(
      script_state, TraceWrapperV8Reference<v8::Value>());
  TraceWrapperV8Reference<v8::Value>& relevant_data =
      result.stored_value->value;
  if (!result.is_new_entry)
    return ScriptValue(isolate, relevant_data.Get(isolate));
  v8::Local<v8::Value> value = serialized_detail_->Deserialize(isolate);
  relevant_data.Reset(isolate, value);
  return ScriptValue(isolate, value);
}

// static
const PerformanceMark::UserFeatureNameToWebFeatureMap&
PerformanceMark::GetUseCounterMapping() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<UserFeatureNameToWebFeatureMap>, map, ());
  if (!map.IsSet()) {
    *map = {
        {"NgOptimizedImage", WebFeature::kUserFeatureNgOptimizedImage},
        {"NgAfterRender", WebFeature::kUserFeatureNgAfterRender},
        {"NgHydration", WebFeature::kUserFeatureNgHydration},
        {"next-third-parties-ga", WebFeature::kUserFeatureNextThirdPartiesGA},
        {"next-third-parties-gtm", WebFeature::kUserFeatureNextThirdPartiesGTM},
        {"next-third-parties-YouTubeEmbed",
         WebFeature::kUserFeatureNextThirdPartiesYouTubeEmbed},
        {"next-third-parties-GoogleMapsEmbed",
         WebFeature::kUserFeatureNextThirdPartiesGoogleMapsEmbed},
        {"nuxt-image", WebFeature::kUserFeatureNuxtImage},
        {"nuxt-picture", WebFeature::kUserFeatureNuxtPicture},
        {"nuxt-third-parties-ga", WebFeature::kUserFeatureNuxtThirdPartiesGA},
        {"nuxt-third-parties-gtm", WebFeature::kUserFeatureNuxtThirdPartiesGTM},
        {"nuxt-third-parties-YouTubeEmbed",
         WebFeature::kUserFeatureNuxtThirdPartiesYouTubeEmbed},
        {"nuxt-third-parties-GoogleMaps",
         WebFeature::kUserFeatureNuxtThirdPartiesGoogleMaps},
    };
  }
  return *map;
}

// static
std::optional<mojom::blink::WebFeature>
PerformanceMark::GetWebFeatureForUserFeatureName(const String& feature_name) {
  auto& feature_map = PerformanceMark::GetUseCounterMapping();
  auto it = feature_map.find(feature_name);
  if (it == feature_map.end()) {
    return std::nullopt;
  }

  return it->value;
}

void PerformanceMark::Trace(Visitor* visitor) const {
  visitor->Trace(deserialized_detail_map_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink

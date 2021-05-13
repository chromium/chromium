// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_metrics.h"

#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::WasmModuleDecoded& event,
    v8::metrics::Recorder::ContextId context_id) {
  auto ukm = GetUkmRecorderAndSourceId(context_id);
  if (!ukm)
    return;
  ukm::builders::V8_Wasm_ModuleDecoded(ukm->source_id)
      .SetStreamed(event.streamed ? 1 : 0)
      .SetSuccess(event.success ? 1 : 0)
      .SetModuleSize(
          ukm::GetExponentialBucketMinForBytes(event.module_size_in_bytes))
      .SetFunctionCount(event.function_count)
      .SetWallClockDuration(event.wall_clock_duration_in_us)
      .Record(ukm->recorder);
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::WasmModuleCompiled& event,
    v8::metrics::Recorder::ContextId context_id) {
  auto ukm = GetUkmRecorderAndSourceId(context_id);
  if (!ukm)
    return;
  ukm::builders::V8_Wasm_ModuleCompiled(ukm->source_id)
      .SetAsync(event.async ? 1 : 0)
      .SetCached(event.cached ? 1 : 0)
      .SetDeserialized(event.deserialized ? 1 : 0)
      .SetLazy(event.lazy ? 1 : 0)
      .SetStreamed(event.streamed ? 1 : 0)
      .SetSuccess(event.success ? 1 : 0)
      .SetCodeSize(
          ukm::GetExponentialBucketMinForBytes(event.code_size_in_bytes))
      .SetLiftoffBailoutCount(event.liftoff_bailout_count)
      .SetWallClockDuration(event.wall_clock_duration_in_us)
      .Record(ukm->recorder);
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::WasmModuleInstantiated& event,
    v8::metrics::Recorder::ContextId context_id) {
  auto ukm = GetUkmRecorderAndSourceId(context_id);
  if (!ukm)
    return;
  ukm::builders::V8_Wasm_ModuleInstantiated(ukm->source_id)
      .SetSuccess(event.success ? 1 : 0)
      .SetImportedFunctionCount(event.imported_function_count)
      .SetWallClockDuration(event.wall_clock_duration_in_us)
      .Record(ukm->recorder);
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::WasmModuleTieredUp& event,
    v8::metrics::Recorder::ContextId context_id) {
  auto ukm = GetUkmRecorderAndSourceId(context_id);
  if (!ukm)
    return;
  ukm::builders::V8_Wasm_ModuleTieredUp(ukm->source_id)
      .SetLazy(event.lazy ? 1 : 0)
      .SetCodeSize(
          ukm::GetExponentialBucketMinForBytes(event.code_size_in_bytes))
      .SetWallClockDuration(event.wall_clock_duration_in_us)
      .Record(ukm->recorder);
}

void V8MetricsRecorder::NotifyIsolateDisposal() {
  v8::metrics::Recorder::NotifyIsolateDisposal();
  isolate_ = nullptr;
}

Document* V8MetricsRecorder::GetDocument(
    v8::metrics::Recorder::ContextId context_id) {
  if (!isolate_)
    return nullptr;
  v8::HandleScope handle_scope(isolate_);
  v8::MaybeLocal<v8::Context> maybe_context =
      v8::metrics::Recorder::GetContext(isolate_, context_id);
  if (maybe_context.IsEmpty())
    return nullptr;
  return To<LocalDOMWindow>(
             ExecutionContext::From(maybe_context.ToLocalChecked()))
      ->document();
}

base::Optional<V8MetricsRecorder::UkmRecorderAndSourceId>
V8MetricsRecorder::GetUkmRecorderAndSourceId(
    v8::metrics::Recorder::ContextId context_id) {
  DCHECK(IsMainThread());
  if (!isolate_)
    return base::Optional<UkmRecorderAndSourceId>();
  Document* document = GetDocument(context_id);
  if (!document)
    return base::Optional<UkmRecorderAndSourceId>();
  ukm::UkmRecorder* ukm_recorder = document->UkmRecorder();
  if (!ukm_recorder)
    return base::Optional<UkmRecorderAndSourceId>();
  return base::Optional<UkmRecorderAndSourceId>(base::in_place, ukm_recorder,
                                                document->UkmSourceID());
}

}  // namespace blink

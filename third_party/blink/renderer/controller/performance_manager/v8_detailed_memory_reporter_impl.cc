// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_detailed_memory_reporter_impl.h"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/controller/performance_manager/v8_worker_memory_reporter.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_resource_tracker.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class FrameAssociatedMeasurementDelegate : public v8::MeasureMemoryDelegate {
 public:
  using ResultCallback =
      base::OnceCallback<void(mojom::blink::PerIsolateV8MemoryUsagePtr)>;

  explicit FrameAssociatedMeasurementDelegate(ResultCallback&& callback)
      : callback_(std::move(callback)) {}

  ~FrameAssociatedMeasurementDelegate() override {
    if (callback_) {
      std::move(callback_).Run(mojom::blink::PerIsolateV8MemoryUsage::New());
    }
  }

 private:
  bool ShouldMeasure(v8::Local<v8::Context> context) override {
    // Measure all contexts.
    return true;
  }

  void MeasurementComplete(v8::MeasureMemoryDelegate::Result result) override {
    DCHECK(IsMainThread());
    mojom::blink::PerIsolateV8MemoryUsagePtr isolate_memory_usage =
        mojom::blink::PerIsolateV8MemoryUsage::New();
    DCHECK_EQ(result.contexts.size(), result.sizes_in_bytes.size());
    for (size_t i = 0; i < result.contexts.size(); ++i) {
      const v8::Local<v8::Context>& context = result.contexts[i];
      const size_t size = result.sizes_in_bytes[i];

      LocalFrame* frame = ToLocalFrameIfNotDetached(context);

      if (!frame) {
        // TODO(crbug.com/1080672): It would be prefereable to count the
        // V8SchemaRegistry context's overhead with unassociated_bytes, but at
        // present there isn't a public API that allows this distinction.
        ++(isolate_memory_usage->num_detached_contexts);
        isolate_memory_usage->detached_bytes_used += size;
        continue;
      }
      v8::Isolate* isolate = context->GetIsolate();
      if (DOMWrapperWorld::World(isolate, context).GetWorldId() !=
          DOMWrapperWorld::kMainWorldId) {
        // TODO(crbug.com/1085129): Handle extension contexts once they get
        // their own V8ContextToken.
        continue;
      }
      auto context_memory_usage = mojom::blink::PerContextV8MemoryUsage::New();
      context_memory_usage->token =
          frame->DomWindow()->GetExecutionContextToken();
      context_memory_usage->bytes_used = size;
#if DCHECK_IS_ON()
      // Check that the token didn't already occur.
      for (const auto& entry : isolate_memory_usage->contexts) {
        DCHECK_NE(entry->token, context_memory_usage->token);
      }
#endif
      isolate_memory_usage->contexts.push_back(std::move(context_memory_usage));
    }
    isolate_memory_usage->shared_bytes_used = result.unattributed_size_in_bytes;
    std::move(callback_).Run(std::move(isolate_memory_usage));
  }

 private:
  ResultCallback callback_;
};

v8::MeasureMemoryExecution ToV8MeasureMemoryExecution(
    V8DetailedMemoryReporterImpl::Mode mode) {
  switch (mode) {
    case V8DetailedMemoryReporterImpl::Mode::DEFAULT:
      return v8::MeasureMemoryExecution::kDefault;
    case V8DetailedMemoryReporterImpl::Mode::EAGER:
      return v8::MeasureMemoryExecution::kEager;
    case V8DetailedMemoryReporterImpl::Mode::LAZY:
      return v8::MeasureMemoryExecution::kLazy;
  }
  NOTREACHED_IN_MIGRATION();
}

ExecutionContextToken ToExecutionContextToken(WorkerToken token) {
  if (token.Is<DedicatedWorkerToken>())
    return ExecutionContextToken(token.GetAs<DedicatedWorkerToken>());
  if (token.Is<SharedWorkerToken>())
    return ExecutionContextToken(token.GetAs<SharedWorkerToken>());
  return ExecutionContextToken(token.GetAs<ServiceWorkerToken>());
}

// A helper class that runs two async functions, combines their
// results, and invokes the given callback. The async functions are:
// - v8::Isolate::MeasureMemory - for the main V8 isolate.
// - V8WorkerMemoryReporter::GetMemoryUsage - for all worker isolates.
class V8ProcessMemoryReporter : public RefCounted<V8ProcessMemoryReporter> {
 public:
  using GetV8MemoryUsageCallback =
      mojom::blink::V8DetailedMemoryReporter::GetV8MemoryUsageCallback;

  explicit V8ProcessMemoryReporter(GetV8MemoryUsageCallback&& callback)
      : callback_(std::move(callback)),
        result_(mojom::blink::PerProcessV8MemoryUsage::New()) {}

  void StartMeasurements(V8DetailedMemoryReporterImpl::Mode mode) {
    DCHECK(IsMainThread());
    DCHECK(!isolate_);
    isolate_ = v8::Isolate::GetCurrent();
    // 1. Start measurement of the main V8 isolate.
    if (!isolate_) {
      // This can happen in tests that do not set up the main V8 isolate
      // or during setup/teardown of the process.
      MainMeasurementComplete(mojom::blink::PerIsolateV8MemoryUsage::New());
    } else {
      auto delegate = std::make_unique<FrameAssociatedMeasurementDelegate>(
          WTF::BindOnce(&V8ProcessMemoryReporter::MainV8MeasurementComplete,
                        scoped_refptr<V8ProcessMemoryReporter>(this)));

      isolate_->MeasureMemory(std::move(delegate),
                              ToV8MeasureMemoryExecution(mode));
    }
    // 2. Start measurement of all worker isolates.
    V8WorkerMemoryReporter::GetMemoryUsage(
        WTF::BindOnce(&V8ProcessMemoryReporter::WorkerMeasurementComplete,
                      scoped_refptr<V8ProcessMemoryReporter>(this)),
        ToV8MeasureMemoryExecution(mode));
  }

 private:
  void MainV8MeasurementComplete(
      mojom::blink::PerIsolateV8MemoryUsagePtr isolate_memory_usage) {
    // At this point measurement of the main V8 isolate is done and we
    // can measure the corresponding Blink memory. Note that the order
    // of the measurements is important because the V8 measurement does
    // a GC and we want to get the Blink memory after the GC.
    // This function and V8ProcessMemoryReporter::StartMeasurements both
    // run on the main thread of the renderer. This means that the Blink
    // heap given by ThreadState::Current() is attached to the main V8
    // isolate given by v8::Isolate::GetCurrent().
    ThreadState::Current()->CollectNodeAndCssStatistics(
        WTF::BindOnce(&V8ProcessMemoryReporter::MainBlinkMeasurementComplete,
                      scoped_refptr<V8ProcessMemoryReporter>(this),
                      std::move(isolate_memory_usage)));
  }

  void MainBlinkMeasurementComplete(
      mojom::blink::PerIsolateV8MemoryUsagePtr isolate_memory_usage,
      size_t node_bytes,
      size_t css_bytes) {
    isolate_memory_usage->blink_bytes_used = node_bytes + css_bytes;
    MeasureCanvasMemory(std::move(isolate_memory_usage));
  }

  void MeasureCanvasMemory(
      mojom::blink::PerIsolateV8MemoryUsagePtr isolate_memory_usage) {
    // We do not use HashMap here because there is no designated deleted value
    // of ExecutionContextToken.
    std::unordered_map<ExecutionContextToken, uint64_t,
                       ExecutionContextToken::Hasher>
        per_context_bytes;
    // Group and accumulate canvas bytes by execution context token.
    for (auto entry : CanvasResourceTracker::For(isolate_)->GetResourceMap()) {
      ExecutionContextToken token = entry.value->GetExecutionContextToken();
      uint64_t bytes_used = entry.key->GetMemoryUsage();
      if (!bytes_used) {
        // Ignore canvas elements that do not have buffers.
        continue;
      }
      auto it = per_context_bytes.find(token);
      if (it == per_context_bytes.end()) {
        per_context_bytes[token] = bytes_used;
      } else {
        it->second += bytes_used;
      }
    }
    for (auto entry : per_context_bytes) {
      auto memory_usage = mojom::blink::PerContextCanvasMemoryUsage::New();
      memory_usage->token = entry.first;
      memory_usage->bytes_used = entry.second;
      isolate_memory_usage->canvas_contexts.push_back(std::move(memory_usage));
    }

    MainMeasurementComplete(std::move(isolate_memory_usage));
  }

  void MainMeasurementComplete(
      mojom::blink::PerIsolateV8MemoryUsagePtr isolate_memory_usage) {
    result_->isolates.push_back(std::move(isolate_memory_usage));
    main_measurement_done_ = true;
    MaybeInvokeCallback();
  }

  void WorkerMeasurementComplete(const V8WorkerMemoryReporter::Result& result) {
    for (auto& worker : result.workers) {
      auto worker_memory_usage = mojom::blink::PerIsolateV8MemoryUsage::New();
      auto context_memory_usage = mojom::blink::PerContextV8MemoryUsage::New();
      context_memory_usage->token = ToExecutionContextToken(worker.token);
      context_memory_usage->bytes_used = worker.bytes;
      if (!worker.url.IsNull()) {
        context_memory_usage->url = worker.url.GetString();
      }
      worker_memory_usage->contexts.push_back(std::move(context_memory_usage));
      result_->isolates.push_back(std::move(worker_memory_usage));
    }
    worker_measurement_done_ = true;
    MaybeInvokeCallback();
  }

  void MaybeInvokeCallback() {
    if (!main_measurement_done_ || !worker_measurement_done_)
      return;

    std::move(callback_).Run(std::move(result_));
  }
  raw_ptr<v8::Isolate> isolate_ = nullptr;
  GetV8MemoryUsageCallback callback_;
  mojom::blink::PerProcessV8MemoryUsagePtr result_;
  bool main_measurement_done_ = false;
  bool worker_measurement_done_ = false;
};

V8DetailedMemoryReporterImpl& GetV8DetailedMemoryReporter() {
  DEFINE_STATIC_LOCAL(V8DetailedMemoryReporterImpl, v8_memory_reporter, ());
  return v8_memory_reporter;
}

}  // namespace

// static
void V8DetailedMemoryReporterImpl::Bind(
    mojo::PendingReceiver<mojom::blink::V8DetailedMemoryReporter> receiver) {
  // This should be called only once per process on RenderProcessWillLaunch.
  DCHECK(!GetV8DetailedMemoryReporter().receiver_.is_bound());
  GetV8DetailedMemoryReporter().receiver_.Bind(std::move(receiver));
}

void V8DetailedMemoryReporterImpl::GetV8MemoryUsage(
    V8DetailedMemoryReporterImpl::Mode mode,
    GetV8MemoryUsageCallback callback) {
  auto v8_process_memory_reporter =
      base::MakeRefCounted<V8ProcessMemoryReporter>(std::move(callback));
  // Start async measurements. The lifetime of the reporter is extended
  // using more shared pointers until the measuremnts complete.
  v8_process_memory_reporter->StartMeasurements(mode);
}

}  // namespace blink

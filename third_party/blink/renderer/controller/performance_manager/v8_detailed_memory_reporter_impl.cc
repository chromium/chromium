// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_detailed_memory_reporter_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/controller/performance_manager/v8_worker_memory_reporter.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
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

  void MeasurementComplete(
      const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
          context_sizes_in_bytes,
      size_t unattributed_size_in_bytes) override {
    mojom::blink::PerIsolateV8MemoryUsagePtr isolate_memory_usage =
        mojom::blink::PerIsolateV8MemoryUsage::New();
    for (const auto& context_and_size : context_sizes_in_bytes) {
      const v8::Local<v8::Context>& context = context_and_size.first;
      const size_t size = context_and_size.second;

      LocalFrame* frame = ToLocalFrameIfNotDetached(context);

      if (!frame) {
        // TODO(crbug.com/1080672): It would be prefereable to count the
        // V8SchemaRegistry context's overhead with unassociated_bytes, but at
        // present there isn't a public API that allows this distinction.
        ++(isolate_memory_usage->num_unassociated_contexts);
        isolate_memory_usage->unassociated_context_bytes_used += size;
        continue;
      }
      if (DOMWrapperWorld::World(context).GetWorldId() !=
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
    isolate_memory_usage->unassociated_bytes_used = unattributed_size_in_bytes;
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
  NOTREACHED();
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
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    // 1. Start measurement of the main V8 isolate.
    if (!isolate) {
      // This can happen in tests that do not set up the main V8 isolate
      // or during setup/teardown of the process.
      MainMeasurementComplete(mojom::blink::PerIsolateV8MemoryUsage::New());
    } else {
      auto delegate = std::make_unique<FrameAssociatedMeasurementDelegate>(
          WTF::Bind(&V8ProcessMemoryReporter::MainMeasurementComplete,
                    scoped_refptr<V8ProcessMemoryReporter>(this)));

      isolate->MeasureMemory(std::move(delegate),
                             ToV8MeasureMemoryExecution(mode));
    }
    // 2. Start measurement of all worker isolates.
    V8WorkerMemoryReporter::GetMemoryUsage(
        WTF::Bind(&V8ProcessMemoryReporter::WorkerMeasurementComplete,
                  scoped_refptr<V8ProcessMemoryReporter>(this)),
        ToV8MeasureMemoryExecution(mode));
  }

 private:
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
  GetV8MemoryUsageCallback callback_;
  mojom::blink::PerProcessV8MemoryUsagePtr result_;
  bool main_measurement_done_ = false;
  bool worker_measurement_done_ = false;
};

}  // namespace

// static
void V8DetailedMemoryReporterImpl::Create(
    mojo::PendingReceiver<mojom::blink::V8DetailedMemoryReporter> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<V8DetailedMemoryReporterImpl>(),
                              std::move(receiver));
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

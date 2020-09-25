// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_worker_memory_reporter.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory_breakdown.h"
#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {
template <>
struct CrossThreadCopier<blink::V8WorkerMemoryReporter::WorkerMemoryUsage>
    : public CrossThreadCopierPassThrough<
          blink::V8WorkerMemoryReporter::WorkerMemoryUsage> {
  STATIC_ONLY(CrossThreadCopier);
};
}  // namespace WTF

namespace blink {

const base::TimeDelta V8WorkerMemoryReporter::kTimeout =
    base::TimeDelta::FromSeconds(60);

namespace {
// This delegate is provided to v8::Isolate::MeasureMemory API.
// V8 calls MeasurementComplete with the measurement result.
//
// All functions of this delegate are called on the worker thread.
class WorkerMeasurementDelegate : public v8::MeasureMemoryDelegate {
 public:
  WorkerMeasurementDelegate(
      base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter,
      WorkerThread* worker_thread)
      : worker_memory_reporter_(std::move(worker_memory_reporter)),
        worker_thread_(worker_thread) {
    DCHECK(worker_thread_->IsCurrentThread());
  }

  ~WorkerMeasurementDelegate() override;

  // v8::MeasureMemoryDelegate overrides.
  bool ShouldMeasure(v8::Local<v8::Context> context) override { return true; }
  void MeasurementComplete(
      const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
          context_sizes,
      size_t unattributed_size) override;

 private:
  void NotifyMeasurementSuccess(
      V8WorkerMemoryReporter::WorkerMemoryUsage memory_usage);
  void NotifyMeasurementFailure();
  base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter_;
  WorkerThread* worker_thread_;
  bool did_notify_ = false;
};

WorkerMeasurementDelegate::~WorkerMeasurementDelegate() {
  DCHECK(worker_thread_->IsCurrentThread());
  if (!did_notify_) {
    // This may happen if the worker shuts down before completing
    // memory measurement.
    NotifyMeasurementFailure();
  }
}

void WorkerMeasurementDelegate::MeasurementComplete(
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>& context_sizes,
    size_t unattributed_size) {
  DCHECK(worker_thread_->IsCurrentThread());
  WorkerOrWorkletGlobalScope* global_scope = worker_thread_->GlobalScope();
  DCHECK(global_scope);
  DCHECK_LE(context_sizes.size(), 1u);
  size_t bytes = unattributed_size;
  for (auto& context_size : context_sizes) {
    bytes += context_size.second;
  }
  NotifyMeasurementSuccess(V8WorkerMemoryReporter::WorkerMemoryUsage{
      To<WorkerGlobalScope>(global_scope)->GetWorkerToken(), bytes});
}

void WorkerMeasurementDelegate::NotifyMeasurementFailure() {
  DCHECK(worker_thread_->IsCurrentThread());
  DCHECK(!did_notify_);
  V8WorkerMemoryReporter::NotifyMeasurementFailure(worker_thread_,
                                                   worker_memory_reporter_);
  did_notify_ = true;
}

void WorkerMeasurementDelegate::NotifyMeasurementSuccess(
    V8WorkerMemoryReporter::WorkerMemoryUsage memory_usage) {
  DCHECK(worker_thread_->IsCurrentThread());
  DCHECK(!did_notify_);
  V8WorkerMemoryReporter::NotifyMeasurementSuccess(
      worker_thread_, worker_memory_reporter_, memory_usage);
  did_notify_ = true;
}

}  // anonymous namespace

// static
void V8WorkerMemoryReporter::GetMemoryUsage(ResultCallback callback,
                                            v8::MeasureMemoryExecution mode) {
  DCHECK(IsMainThread());
  // The private constructor prevents us from using std::make_unique here.
  std::unique_ptr<V8WorkerMemoryReporter> worker_memory_reporter(
      new V8WorkerMemoryReporter(std::move(callback)));
  // Worker tasks get a weak pointer to the instance for passing it back
  // to the main thread in OnMeasurementSuccess and OnMeasurementFailure.
  // Worker tasks never dereference the weak pointer.
  unsigned worker_count = WorkerThread::CallOnAllWorkerThreads(
      &V8WorkerMemoryReporter::StartMeasurement, TaskType::kInternalDefault,
      worker_memory_reporter->GetWeakPtr(), mode);
  if (worker_count == 0) {
    Thread::Current()->GetTaskRunner()->PostTask(
        FROM_HERE, WTF::Bind(&V8WorkerMemoryReporter::InvokeCallback,
                             std::move(worker_memory_reporter)));
    return;
  }
  worker_memory_reporter->SetWorkerCount(worker_count);
  // Transfer the ownership of the instance to the timeout task.
  Thread::Current()->GetTaskRunner()->PostDelayedTask(
      FROM_HERE,
      WTF::Bind(&V8WorkerMemoryReporter::OnTimeout,
                std::move(worker_memory_reporter)),
      kTimeout);
}

// static
void V8WorkerMemoryReporter::StartMeasurement(
    WorkerThread* worker_thread,
    base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter,
    v8::MeasureMemoryExecution measurement_mode) {
  DCHECK(worker_thread->IsCurrentThread());
  WorkerOrWorkletGlobalScope* global_scope = worker_thread->GlobalScope();
  DCHECK(global_scope);
  v8::Isolate* isolate = worker_thread->GetIsolate();
  if (global_scope->IsWorkerGlobalScope()) {
    auto delegate = std::make_unique<WorkerMeasurementDelegate>(
        std::move(worker_memory_reporter), worker_thread);
    isolate->MeasureMemory(std::move(delegate), measurement_mode);
  } else {
    // TODO(ulan): Add support for worklets once we get tokens for them. We
    // need to careful to not trigger GC on a worklet because usually worklets
    // are soft real-time and are written to avoid GC.
    // For now we simply notify a failure so that the main thread doesn't wait
    // for a response from the worklet.
    NotifyMeasurementFailure(worker_thread, worker_memory_reporter);
  }
}

// static
void V8WorkerMemoryReporter::NotifyMeasurementSuccess(
    WorkerThread* worker_thread,
    base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter,
    WorkerMemoryUsage memory_usage) {
  DCHECK(worker_thread->IsCurrentThread());
  PostCrossThreadTask(
      *Thread::MainThread()->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&V8WorkerMemoryReporter::OnMeasurementSuccess,
                          worker_memory_reporter, memory_usage));
}

// static
void V8WorkerMemoryReporter::NotifyMeasurementFailure(
    WorkerThread* worker_thread,
    base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter) {
  DCHECK(worker_thread->IsCurrentThread());
  PostCrossThreadTask(
      *Thread::MainThread()->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&V8WorkerMemoryReporter::OnMeasurementFailure,
                          worker_memory_reporter));
}

void V8WorkerMemoryReporter::OnMeasurementFailure() {
  DCHECK(IsMainThread());
  if (state_ == State::kDone)
    return;
  ++failure_count_;
  if (success_count_ + failure_count_ == worker_count_) {
    InvokeCallback();
    DCHECK_EQ(state_, State::kDone);
  }
}

void V8WorkerMemoryReporter::OnMeasurementSuccess(
    WorkerMemoryUsage memory_usage) {
  DCHECK(IsMainThread());
  if (state_ == State::kDone)
    return;
  result_.workers.emplace_back(memory_usage);
  ++success_count_;
  if (success_count_ + failure_count_ == worker_count_) {
    InvokeCallback();
    DCHECK_EQ(state_, State::kDone);
  }
}

void V8WorkerMemoryReporter::SetWorkerCount(unsigned worker_count) {
  DCHECK(IsMainThread());
  DCHECK_EQ(0u, worker_count_);
  worker_count_ = worker_count;
}

void V8WorkerMemoryReporter::OnTimeout() {
  DCHECK(IsMainThread());
  if (state_ == State::kDone)
    return;
  InvokeCallback();
  DCHECK_EQ(state_, State::kDone);
}

void V8WorkerMemoryReporter::InvokeCallback() {
  DCHECK(IsMainThread());
  DCHECK_EQ(state_, State::kWaiting);
  std::move(callback_).Run(std::move(result_));
  state_ = State::kDone;
}

namespace {

// Used by the performance.measureMemory Web API. It forwards the incoming
// memory measurement request to V8WorkerMemoryReporter and adapts the result
// to match the format of the Web API.
//
// It will be removed in the future when performance.measureMemory switches
// to a mojo-based implementation that queries PerformanceManager in the
// browser process.
class WebMemoryReporter : public MeasureMemoryController::V8MemoryReporter {
  void GetMemoryUsage(MeasureMemoryController::ResultCallback callback,
                      v8::MeasureMemoryExecution execution) override {
    V8WorkerMemoryReporter::GetMemoryUsage(
        WTF::Bind(&WebMemoryReporter::ForwardResults, std::move(callback)),
        execution);
  }

  // Adapts the result to match the format expected by MeasureMemoryController.
  static void ForwardResults(MeasureMemoryController::ResultCallback callback,
                             const V8WorkerMemoryReporter::Result& result) {
    HeapVector<Member<MeasureMemoryBreakdown>> new_result;
    const String kDedicatedWorkerGlobalScope("DedicatedWorkerGlobalScope");
    const String kJS("JS");
    const Vector<String> kWorkerMemoryTypes = {kDedicatedWorkerGlobalScope,
                                               kJS};
    const Vector<String> kEmptyAttribution = {};
    for (const auto& worker : result.workers) {
      if (worker.token.Is<DedicatedWorkerToken>()) {
        MeasureMemoryBreakdown* entry = MeasureMemoryBreakdown::Create();
        entry->setBytes(worker.bytes);
        entry->setUserAgentSpecificTypes(kWorkerMemoryTypes);
        entry->setAttribution(kEmptyAttribution);
        new_result.push_back(entry);
      }
    }
    std::move(callback).Run(new_result);
  }
};

}  // anonymous namespace

void V8WorkerMemoryReporter::RegisterWebMemoryReporter() {
  MeasureMemoryController::SetDedicatedWorkerMemoryReporter(
      new WebMemoryReporter());
}

}  // namespace blink

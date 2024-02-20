// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_worker_memory_reporter.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
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

const base::TimeDelta V8WorkerMemoryReporter::kTimeout = base::Seconds(60);

namespace {

// TODO(906991): Remove this once PlzDedicatedWorker ships. Until then
// the browser does not know URLs of dedicated workers, so we pass them
// together with the measurement result. We limit the max length of the
// URLs to reduce memory allocations and the traffic between the renderer
// and the browser processes.
constexpr size_t kMaxReportedUrlLength = 2000;

// This delegate is provided to v8::Isolate::MeasureMemory API.
// V8 calls MeasurementComplete with the measurement result.
//
// All functions of this delegate are called on the worker thread.
class WorkerMeasurementDelegate : public v8::MeasureMemoryDelegate {
 public:
  WorkerMeasurementDelegate(
      base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter,
      WorkerThread* worker_thread,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : worker_memory_reporter_(std::move(worker_memory_reporter)),
        worker_thread_(worker_thread),
        task_runner_(task_runner) {
    DCHECK(worker_thread_->IsCurrentThread());
  }

  ~WorkerMeasurementDelegate() override;

  // v8::MeasureMemoryDelegate overrides.
  bool ShouldMeasure(v8::Local<v8::Context> context) override { return true; }
  void MeasurementComplete(v8::MeasureMemoryDelegate::Result result) override;

 private:
  void NotifyMeasurementSuccess(
      std::unique_ptr<V8WorkerMemoryReporter::WorkerMemoryUsage> memory_usage);
  void NotifyMeasurementFailure();
  base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter_;
  raw_ptr<WorkerThread> worker_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
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
    v8::MeasureMemoryDelegate::Result result) {
  DCHECK(worker_thread_->IsCurrentThread());
  WorkerOrWorkletGlobalScope* global_scope = worker_thread_->GlobalScope();
  DCHECK(global_scope);
  DCHECK_LE(result.contexts.size(), 1u);
  DCHECK_LE(result.sizes_in_bytes.size(), 1u);
  size_t bytes = result.unattributed_size_in_bytes;
  for (size_t size : result.sizes_in_bytes) {
    bytes += size;
  }
  auto* worker_global_scope = To<WorkerGlobalScope>(global_scope);
  auto memory_usage =
      std::make_unique<V8WorkerMemoryReporter::WorkerMemoryUsage>();
  memory_usage->token = worker_global_scope->GetWorkerToken();
  memory_usage->bytes = bytes;
  if (worker_global_scope->IsUrlValid() &&
      worker_global_scope->Url().GetString().length() < kMaxReportedUrlLength) {
    memory_usage->url = worker_global_scope->Url();
  }
  NotifyMeasurementSuccess(std::move(memory_usage));
}

void WorkerMeasurementDelegate::NotifyMeasurementFailure() {
  DCHECK(worker_thread_->IsCurrentThread());
  DCHECK(!did_notify_);
  V8WorkerMemoryReporter::NotifyMeasurementFailure(worker_thread_, task_runner_,
                                                   worker_memory_reporter_);
  did_notify_ = true;
}

void WorkerMeasurementDelegate::NotifyMeasurementSuccess(
    std::unique_ptr<V8WorkerMemoryReporter::WorkerMemoryUsage> memory_usage) {
  DCHECK(worker_thread_->IsCurrentThread());
  DCHECK(!did_notify_);
  V8WorkerMemoryReporter::NotifyMeasurementSuccess(worker_thread_, task_runner_,
                                                   worker_memory_reporter_,
                                                   std::move(memory_usage));
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
  auto main_thread_task_runner =
      Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted());
  // Worker tasks get a weak pointer to the instance for passing it back
  // to the main thread in OnMeasurementSuccess and OnMeasurementFailure.
  // Worker tasks never dereference the weak pointer.
  unsigned worker_count = WorkerThread::CallOnAllWorkerThreads(
      &V8WorkerMemoryReporter::StartMeasurement, TaskType::kInternalDefault,
      main_thread_task_runner, worker_memory_reporter->GetWeakPtr(), mode);
  if (worker_count == 0) {
    main_thread_task_runner->PostTask(
        FROM_HERE, WTF::BindOnce(&V8WorkerMemoryReporter::InvokeCallback,
                                 std::move(worker_memory_reporter)));
    return;
  }
  worker_memory_reporter->SetWorkerCount(worker_count);
  // Transfer the ownership of the instance to the timeout task.
  main_thread_task_runner->PostDelayedTask(
      FROM_HERE,
      WTF::BindOnce(&V8WorkerMemoryReporter::OnTimeout,
                    std::move(worker_memory_reporter)),
      kTimeout);
}

// static
void V8WorkerMemoryReporter::StartMeasurement(
    WorkerThread* worker_thread,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter,
    v8::MeasureMemoryExecution measurement_mode) {
  DCHECK(worker_thread->IsCurrentThread());
  WorkerOrWorkletGlobalScope* global_scope = worker_thread->GlobalScope();
  DCHECK(global_scope);
  v8::Isolate* isolate = worker_thread->GetIsolate();
  if (global_scope->IsWorkerGlobalScope()) {
    auto delegate = std::make_unique<WorkerMeasurementDelegate>(
        std::move(worker_memory_reporter), worker_thread,
        std::move(task_runner));
    isolate->MeasureMemory(std::move(delegate), measurement_mode);
  } else {
    // TODO(ulan): Add support for worklets once we get tokens for them. We
    // need to careful to not trigger GC on a worklet because usually worklets
    // are soft real-time and are written to avoid GC.
    // For now we simply notify a failure so that the main thread doesn't wait
    // for a response from the worklet.
    NotifyMeasurementFailure(worker_thread, std::move(task_runner),
                             worker_memory_reporter);
  }
}

// static
void V8WorkerMemoryReporter::NotifyMeasurementSuccess(
    WorkerThread* worker_thread,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter,
    std::unique_ptr<WorkerMemoryUsage> memory_usage) {
  DCHECK(worker_thread->IsCurrentThread());
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(&V8WorkerMemoryReporter::OnMeasurementSuccess,
                          worker_memory_reporter, std::move(memory_usage)));
}

// static
void V8WorkerMemoryReporter::NotifyMeasurementFailure(
    WorkerThread* worker_thread,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<V8WorkerMemoryReporter> worker_memory_reporter) {
  DCHECK(worker_thread->IsCurrentThread());
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
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
    std::unique_ptr<WorkerMemoryUsage> memory_usage) {
  DCHECK(IsMainThread());
  if (state_ == State::kDone)
    return;
  result_.workers.emplace_back(*memory_usage);
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

}  // namespace blink

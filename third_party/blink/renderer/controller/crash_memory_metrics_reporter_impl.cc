// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"

#include <utility>

#include "base/atomicops.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "partition_alloc/oom_callback.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// static
void CrashMemoryMetricsReporterImpl::Bind(
    mojo::PendingReceiver<mojom::blink::CrashMemoryMetricsReporter> receiver) {
  // This should be called only once per process on RenderProcessWillLaunch.
  DCHECK(!CrashMemoryMetricsReporterImpl::Instance().receiver_.is_bound());
  CrashMemoryMetricsReporterImpl::Instance().receiver_.Bind(
      std::move(receiver));
}

CrashMemoryMetricsReporterImpl& CrashMemoryMetricsReporterImpl::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CrashMemoryMetricsReporterImpl,
                                  crash_memory_metrics_reporter_impl, ());
  return crash_memory_metrics_reporter_impl;
}

CrashMemoryMetricsReporterImpl::CrashMemoryMetricsReporterImpl()
#if BUILDFLAG(IS_ANDROID)
    : timer_(Thread::MainThread()
                 ->Scheduler()
                 ->ToMainThreadScheduler()
                 ->NonWakingTaskRunner(),
             this,
             &CrashMemoryMetricsReporterImpl::SampleMemoryState)
#endif
{
  ::partition_alloc::SetPartitionAllocOomCallback(
      CrashMemoryMetricsReporterImpl::OnOOMCallback);
}

CrashMemoryMetricsReporterImpl::~CrashMemoryMetricsReporterImpl() = default;

void CrashMemoryMetricsReporterImpl::SetSharedMemory(
    base::UnsafeSharedMemoryRegion shared_metrics_buffer) {
  // This method should be called only once per process.
  base::WritableSharedMemoryMapping mapping = shared_metrics_buffer.Map();
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!shared_metrics_mapping_.IsValid());
    shared_metrics_mapping_ = std::move(mapping);
  }
#if BUILDFLAG(IS_ANDROID)
  timer_.StartRepeating(base::Seconds(1), FROM_HERE);
#endif
}

void CrashMemoryMetricsReporterImpl::ResetForTesting() {
  base::AutoLock auto_lock(lock_);
  shared_metrics_mapping_ = base::WritableSharedMemoryMapping();
  last_reported_metrics_ = OomInterventionMetrics();
}

void CrashMemoryMetricsReporterImpl::WriteIntoSharedMemory() {
  if (!shared_metrics_mapping_.IsValid())
    return;
  // TODO(crbug.com/388844091): Consider using std::atomic.
  base::subtle::RelaxedAtomicWriteMemcpy(
      shared_metrics_mapping_.GetMemoryAsSpan<uint8_t>(),
      base::byte_span_from_ref(last_reported_metrics_));
}

#if BUILDFLAG(IS_ANDROID)
void CrashMemoryMetricsReporterImpl::SampleMemoryState(TimerBase*) {
  base::SystemMemoryInfo meminfo;
  base::GetSystemMemoryInfo(&meminfo);
  base::AutoLock auto_lock(lock_);
  last_reported_metrics_.current_available_memory = meminfo.available;
  last_reported_metrics_.current_swap_free = meminfo.swap_free;
  WriteIntoSharedMemory();
}
#endif

void CrashMemoryMetricsReporterImpl::OnOOMCallback() {
  CrashMemoryMetricsReporterImpl& instance =
      CrashMemoryMetricsReporterImpl::Instance();
  // None of the code executed while holding the`lock_` should allocate memory;
  // if it does, it may trigger an OOM and end up in a deadlock.
  base::AutoLock auto_lock(instance.lock_);

  // If shared_metrics_mapping_ is not set, it means OnNoMemory happened before
  // initializing render process host sets the shared memory.
  if (!instance.shared_metrics_mapping_.IsValid())
    return;
  // Else, we can send the allocation_failed bool.
  instance.last_reported_metrics_.allocation_failed = 1;  // true
  instance.WriteIntoSharedMemory();
}

}  // namespace blink

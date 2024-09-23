// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "partition_alloc/oom_callback.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

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
  DEFINE_STATIC_LOCAL(CrashMemoryMetricsReporterImpl,
                      crash_memory_metrics_reporter_impl, ());
  return crash_memory_metrics_reporter_impl;
}

CrashMemoryMetricsReporterImpl::CrashMemoryMetricsReporterImpl() {
  ::partition_alloc::SetPartitionAllocOomCallback(
      CrashMemoryMetricsReporterImpl::OnOOMCallback);
}

CrashMemoryMetricsReporterImpl::~CrashMemoryMetricsReporterImpl() {
  MemoryUsageMonitor::Instance().RemoveObserver(this);
}

void CrashMemoryMetricsReporterImpl::SetSharedMemory(
    base::UnsafeSharedMemoryRegion shared_metrics_buffer) {
  // This method should be called only once per process.
  DCHECK(!shared_metrics_mapping_.IsValid());
  shared_metrics_mapping_ = shared_metrics_buffer.Map();
  MemoryUsageMonitor::Instance().AddObserver(this);
}

void CrashMemoryMetricsReporterImpl::OnMemoryPing(MemoryUsage usage) {
  DCHECK(IsMainThread());
  last_reported_metrics_ =
      CrashMemoryMetricsReporterImpl::MemoryUsageToMetrics(usage);
  WriteIntoSharedMemory();
}

void CrashMemoryMetricsReporterImpl::WriteIntoSharedMemory() {
  if (!shared_metrics_mapping_.IsValid())
    return;
  auto* metrics_shared =
      shared_metrics_mapping_.GetMemoryAs<OomInterventionMetrics>();
  *metrics_shared = last_reported_metrics_;
}

void CrashMemoryMetricsReporterImpl::OnOOMCallback() {
  // TODO(yuzus: Support allocation failures on other threads as well.
  if (!IsMainThread())
    return;
  CrashMemoryMetricsReporterImpl& instance =
      CrashMemoryMetricsReporterImpl::Instance();
  // If shared_metrics_mapping_ is not set, it means OnNoMemory happened before
  // initializing render process host sets the shared memory.
  if (!instance.shared_metrics_mapping_.IsValid())
    return;
  // Else, we can send the allocation_failed bool.
  // TODO(yuzus): Report this UMA on all the platforms. Currently this is only
  // reported on Android.
  instance.last_reported_metrics_.allocation_failed = 1;  // true
  instance.WriteIntoSharedMemory();
}

// static
OomInterventionMetrics CrashMemoryMetricsReporterImpl::MemoryUsageToMetrics(
    MemoryUsage usage) {
  OomInterventionMetrics metrics;

  DCHECK(!std::isnan(usage.private_footprint_bytes));
  DCHECK(!std::isnan(usage.swap_bytes));
  DCHECK(!std::isnan(usage.vm_size_bytes));
  metrics.current_blink_usage_kb =
      (usage.v8_bytes + usage.blink_gc_bytes + usage.partition_alloc_bytes) /
      1024;

  DCHECK(!std::isnan(usage.private_footprint_bytes));
  DCHECK(!std::isnan(usage.swap_bytes));
  DCHECK(!std::isnan(usage.vm_size_bytes));
  metrics.current_private_footprint_kb = usage.private_footprint_bytes / 1024;
  metrics.current_swap_kb = usage.swap_bytes / 1024;
  metrics.current_vm_size_kb = usage.vm_size_bytes / 1024;
  metrics.allocation_failed = 0;  // false
  return metrics;
}

}  // namespace blink

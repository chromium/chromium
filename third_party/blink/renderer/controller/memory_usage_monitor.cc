// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor.h"

#include "base/test/test_mock_time_task_runner.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

namespace {
constexpr base::TimeDelta kPingInterval = base::TimeDelta::FromSeconds(1);
}

MemoryUsageMonitor::MemoryUsageMonitor() {
  timer_.SetTaskRunner(Thread::MainThread()->GetTaskRunner());
}

MemoryUsageMonitor::MemoryUsageMonitor(
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner_for_testing,
    const base::TickClock* clock_for_testing)
    : timer_(clock_for_testing) {
  timer_.SetTaskRunner(task_runner_for_testing);
}

void MemoryUsageMonitor::AddObserver(Observer* observer) {
  StartMonitoringIfNeeded();
  observers_.AddObserver(observer);
}

void MemoryUsageMonitor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MemoryUsageMonitor::HasObserver(Observer* observer) {
  return observers_.HasObserver(observer);
}

void MemoryUsageMonitor::StartMonitoringIfNeeded() {
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, kPingInterval,
               WTF::BindRepeating(&MemoryUsageMonitor::TimerFired,
                                  WTF::Unretained(this)));
}

void MemoryUsageMonitor::StopMonitoring() {
  timer_.Stop();
}

MemoryUsage MemoryUsageMonitor::GetCurrentMemoryUsage() {
  MemoryUsage usage;
  GetV8MemoryUsage(usage);
  GetBlinkMemoryUsage(usage);
  GetProcessMemoryUsage(usage);
  return usage;
}

void MemoryUsageMonitor::GetV8MemoryUsage(MemoryUsage& usage) {
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  DCHECK(isolate);
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  // TODO: Add memory usage for worker threads.
  usage.v8_bytes =
      heap_statistics.total_heap_size() + heap_statistics.malloced_memory();
}

void MemoryUsageMonitor::GetBlinkMemoryUsage(MemoryUsage& usage) {
  usage.blink_gc_bytes = ProcessHeap::TotalAllocatedObjectSize();
  usage.partition_alloc_bytes = WTF::Partitions::TotalSizeOfCommittedPages();
}

void MemoryUsageMonitor::TimerFired() {
  MemoryUsage usage = GetCurrentMemoryUsage();
  for (auto& observer : observers_)
    observer.OnMemoryPing(usage);
  if (!observers_.might_have_observers())
    StopMonitoring();
}

}  // namespace blink

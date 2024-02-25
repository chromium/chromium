// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor.h"

#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
constexpr base::TimeDelta kPingInterval = base::Seconds(1);
}

MemoryUsageMonitor::MemoryUsageMonitor() {
  MainThreadScheduler* scheduler =
      Thread::MainThread()->Scheduler()->ToMainThreadScheduler();
  DCHECK(scheduler);
  timer_.SetTaskRunner(scheduler->NonWakingTaskRunner());
}

MemoryUsageMonitor::MemoryUsageMonitor(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing,
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
  usage.v8_bytes = 0;
  // TODO: Add memory usage for worker threads.
  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(WTF::BindRepeating(
          [](MemoryUsage& usage, v8::Isolate* isolate) {
            v8::HeapStatistics heap_statistics;
            isolate->GetHeapStatistics(&heap_statistics);
            usage.v8_bytes += heap_statistics.total_heap_size() +
                              heap_statistics.malloced_memory();
          },
          std::ref(usage)));
}

void MemoryUsageMonitor::GetBlinkMemoryUsage(MemoryUsage& usage) {
  usage.blink_gc_bytes = ProcessHeap::TotalAllocatedObjectSize();
  usage.partition_alloc_bytes = WTF::Partitions::TotalSizeOfCommittedPages();
}

void MemoryUsageMonitor::TimerFired() {
  MemoryUsage usage = GetCurrentMemoryUsage();
  for (auto& observer : observers_)
    observer.OnMemoryPing(usage);
  if (observers_.empty())
    StopMonitoring();
}

}  // namespace blink

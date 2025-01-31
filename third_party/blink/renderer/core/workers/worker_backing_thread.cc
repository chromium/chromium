// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"

#include <memory>

#include "base/location.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

namespace {

base::Lock& IsolatesLock() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());
  return lock;
}

HashSet<v8::Isolate*>& Isolates() EXCLUSIVE_LOCKS_REQUIRED(IsolatesLock()) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<v8::Isolate*>, isolates, ());
  return isolates;
}

HashSet<v8::Isolate*>& ForegroundedIsolates()
    EXCLUSIVE_LOCKS_REQUIRED(IsolatesLock()) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<v8::Isolate*>, foregrounded_isolates,
                                  ());
  return foregrounded_isolates;
}

v8::Isolate::Priority& IsolateCurrentPriority()
    EXCLUSIVE_LOCKS_REQUIRED(IsolatesLock()) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(v8::Isolate::Priority,
                                  isolate_current_priority,
                                  (v8::Isolate::Priority::kUserBlocking));
  return isolate_current_priority;
}

bool& BatterySaverModeEnabled() EXCLUSIVE_LOCKS_REQUIRED(IsolatesLock()) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(bool, battery_saver_mode_enabled, ());
  return battery_saver_mode_enabled;
}

void AddWorkerIsolate(v8::Isolate* isolate) {
  base::AutoLock locker(IsolatesLock());
  isolate->SetPriority(IsolateCurrentPriority());
  if (BatterySaverModeEnabled()) {
    isolate->SetBatterySaverMode(true);
  }
  Isolates().insert(isolate);
}

void RemoveWorkerIsolate(v8::Isolate* isolate) {
  base::AutoLock locker(IsolatesLock());
  Isolates().erase(isolate);
}

void AddForegroundedWorkerIsolate(v8::Isolate* isolate) {
  base::AutoLock locker(IsolatesLock());
  ForegroundedIsolates().insert(isolate);
}

void RemoveForegroundedWorkerIsolate(v8::Isolate* isolate) {
  base::AutoLock locker(IsolatesLock());
  ForegroundedIsolates().erase(isolate);
}

}  // namespace

// Wrapper functions defined in third_party/blink/public/web/blink.h
void MemoryPressureNotificationToAllIsolates(v8::MemoryPressureLevel level) {
  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(WTF::BindRepeating(
          [](v8::MemoryPressureLevel level, v8::Isolate* isolate) {
            isolate->MemoryPressureNotification(level);
          },
          level));
  WorkerBackingThread::MemoryPressureNotificationToWorkerThreadIsolates(level);
}

void SetBatterySaverModeForAllIsolates(bool battery_saver_mode_enabled) {
  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(WTF::BindRepeating(
          [](bool battery_saver_mode_enabled, v8::Isolate* isolate) {
            isolate->SetBatterySaverMode(battery_saver_mode_enabled);
          },
          battery_saver_mode_enabled));
  WorkerBackingThread::SetBatterySaverModeForWorkerThreadIsolates(
      battery_saver_mode_enabled);
}

WorkerBackingThread::WorkerBackingThread(const ThreadCreationParams& params)
    : backing_thread_(blink::NonMainThread::CreateThread(
          ThreadCreationParams(params).SetSupportsGC(true))) {}

WorkerBackingThread::~WorkerBackingThread() = default;

void WorkerBackingThread::InitializeOnBackingThread(
    const WorkerBackingThreadStartupData& startup_data) {
  DCHECK(backing_thread_->IsCurrentThread());

  DCHECK(!isolate_);
  ThreadScheduler* scheduler = BackingThread().Scheduler();
  isolate_ = V8PerIsolateData::Initialize(
      scheduler->V8TaskRunner(), scheduler->V8UserVisibleTaskRunner(),
      scheduler->V8BestEffortTaskRunner(),
      V8PerIsolateData::V8ContextSnapshotMode::kDontUseSnapshot, nullptr,
      nullptr);
  scheduler->SetV8Isolate(isolate_);
  AddWorkerIsolate(isolate_);
  V8Initializer::InitializeWorker(isolate_);

  if (RuntimeEnabledFeatures::V8IdleTasksEnabled()) {
    V8PerIsolateData::EnableIdleTasks(
        isolate_, std::make_unique<V8IdleTaskRunner>(scheduler));
  }
  Platform::Current()->DidStartWorkerThread();

  V8PerIsolateData::From(isolate_)->SetThreadDebugger(
      std::make_unique<WorkerThreadDebugger>(isolate_));

  if (startup_data.heap_limit_mode ==
      WorkerBackingThreadStartupData::HeapLimitMode::kIncreasedForDebugging) {
    isolate_->IncreaseHeapLimitForDebugging();
  }
  isolate_->SetAllowAtomicsWait(
      startup_data.atomics_wait_mode ==
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow);
}

void WorkerBackingThread::ShutdownOnBackingThread() {
  DCHECK(backing_thread_->IsCurrentThread());
  BackingThread().Scheduler()->SetV8Isolate(nullptr);
  Platform::Current()->WillStopWorkerThread();

  V8PerIsolateData::WillBeDestroyed(isolate_);
  backing_thread_->ShutdownOnThread();

  RemoveForegroundedWorkerIsolate(isolate_);
  RemoveWorkerIsolate(isolate_);
  V8PerIsolateData::Destroy(isolate_);
  isolate_ = nullptr;
}

void WorkerBackingThread::SetForegrounded() {
  AddForegroundedWorkerIsolate(isolate_);
  isolate_->SetPriority(v8::Isolate::Priority::kUserBlocking);
}

// static
void WorkerBackingThread::MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel level) {
  base::AutoLock locker(IsolatesLock());
  for (v8::Isolate* isolate : Isolates())
    isolate->MemoryPressureNotification(level);
}

// static
void WorkerBackingThread::SetWorkerThreadIsolatesPriority(
    v8::Isolate::Priority priority) {
  base::AutoLock locker(IsolatesLock());
  IsolateCurrentPriority() = priority;
  for (v8::Isolate* isolate : Isolates()) {
    if (!ForegroundedIsolates().Contains(isolate)) {
      isolate->SetPriority(priority);
    }
  }
}

// static
void WorkerBackingThread::SetBatterySaverModeForWorkerThreadIsolates(
    bool battery_saver_mode_enabled) {
  base::AutoLock locker(IsolatesLock());

  for (v8::Isolate* isolate : Isolates()) {
    isolate->SetBatterySaverMode(battery_saver_mode_enabled);
  }
  BatterySaverModeEnabled() = battery_saver_mode_enabled;
}

}  // namespace blink

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_isolate_data.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/public/gin_embedders.h"
#include "gin/v8_foreground_task_runner.h"
#include "gin/v8_foreground_task_runner_with_locker.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-isolate.h"

using v8::ArrayBuffer;
using v8::Isolate;

namespace {
std::shared_ptr<gin::V8ForegroundTaskRunnerBase> CreateV8ForegroundTaskRunner(
    v8::Isolate* isolate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gin::IsolateHolder::AccessMode access_mode) {
  if (access_mode == gin::IsolateHolder::kUseLocker) {
    return std::make_shared<gin::V8ForegroundTaskRunnerWithLocker>(
        isolate, std::move(task_runner));
  } else {
    return std::make_shared<gin::V8ForegroundTaskRunner>(
        std::move(task_runner));
  }
}
}  // namespace

namespace gin {

// Ensure Gin's external pointer tags do not collide with V8's internal tags.
static_assert(
    static_cast<uint16_t>(kLastExternalPointerTypeTag) <
        static_cast<uint16_t>(v8::kFirstInternalExternalPointerTypeTag),
    "Gin embedder tags are colliding with V8 internal tags.");

PerIsolateData::PerIsolateData(
    Isolate* isolate,
    ArrayBuffer::Allocator* allocator,
    IsolateHolder::AccessMode access_mode,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner)
    : isolate_(isolate), allocator_(allocator) {
  isolate_->SetData(kEmbedderNativeGin, this);

  DCHECK(task_runner);
  task_runner_ = CreateV8ForegroundTaskRunner(isolate_, std::move(task_runner),
                                              access_mode);
  if (user_visible_task_runner) {
    user_visible_task_runner_ = CreateV8ForegroundTaskRunner(
        isolate_, std::move(user_visible_task_runner), access_mode);
  }
  if (best_effort_task_runner) {
    best_effort_task_runner_ = CreateV8ForegroundTaskRunner(
        isolate_, std::move(best_effort_task_runner), access_mode);
  }
}

PerIsolateData::~PerIsolateData() = default;

PerIsolateData* PerIsolateData::From(Isolate* isolate) {
  return static_cast<PerIsolateData*>(isolate->GetData(kEmbedderNativeGin));
}

void PerIsolateData::AddDisposeObserver(DisposeObserver* observer) {
  dispose_observers_.AddObserver(observer);
}

void PerIsolateData::RemoveDisposeObserver(DisposeObserver* observer) {
  dispose_observers_.RemoveObserver(observer);
}

void PerIsolateData::NotifyBeforeDispose() {
  for (auto& observer : dispose_observers_) {
    observer.OnBeforeDispose(isolate_.get());
  }
}

void PerIsolateData::NotifyDisposed() {
  for (auto& observer : dispose_observers_) {
    observer.OnDisposed();
  }
}

void PerIsolateData::EnableIdleTasks(
    std::unique_ptr<V8IdleTaskRunner> idle_task_runner) {
  task_runner_->EnableIdleTasks(std::move(idle_task_runner));
}

}  // namespace gin

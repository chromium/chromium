// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_CONTEXT_H_

#include "third_party/blink/renderer/core/probe/async_task_id.h"

namespace v8 {
class Isolate;
}  // namespace v8

namespace WTF {
class StringView;
}  // namespace WTF

namespace blink {
class ExecutionContext;
namespace probe {

// Tracks scheduling and cancelation of a single async task.
// An async task scheduled via `AsyncTaskContext` is guaranteed to be
// canceled.
class CORE_EXPORT AsyncTaskContext {
 public:
  AsyncTaskContext() = default;
  ~AsyncTaskContext();

  // Not copyable or movable. The address of the async_task_id_ is used
  // to identify this task and corresponding runs/invocations via `AsyncTask`.
  AsyncTaskContext(const AsyncTaskContext&) = delete;
  AsyncTaskContext& operator=(const AsyncTaskContext&) = delete;

  // Schedules this async task with the ThreadDebugger. `Schedule` can be called
  // once and only once per AsyncTaskContext instance.
  void Schedule(ExecutionContext* context, const WTF::StringView& name);

  // Explicitly cancel this async task. No `AsyncTasks`s must be created with
  // this context after `Cancel` was called.
  void Cancel();

 private:
  friend class AsyncTask;

  AsyncTaskId async_task_id_;
  v8::Isolate* isolate_ = nullptr;
};

}  // namespace probe
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_CONTEXT_H_

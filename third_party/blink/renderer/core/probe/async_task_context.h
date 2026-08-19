// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_CONTEXT_H_

#include <array>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "third_party/blink/renderer/core/ad_tracker/script_ancestry_tracker.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/loader/fetch/ad_tagging_utils.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace v8 {
class Isolate;
}  // namespace v8

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

  enum class StackOptions {
    kDoNotScan,
    kScan,
  };

  // Not copyable or movable. The address of `AsyncTaskContext` is used
  // to identify this task and corresponding runs/invocations via `AsyncTask`.
  AsyncTaskContext(const AsyncTaskContext&) = delete;
  AsyncTaskContext& operator=(const AsyncTaskContext&) = delete;

  // Schedules this async task with the ThreadDebugger. `Schedule` can be called
  // once and only once per AsyncTaskContext instance. Set `stack_options` to
  // `kScan` only in cases where blink runs an internal operation
  // asynchronously, and we call `ScriptAncestryTracker::GetMarkedScriptInStack`
  // on the other side while the async task is running. Generally should be
  // `kDoNotScan`.
  void Schedule(ExecutionContext* context,
                const StringView& name,
                StackOptions stack_options = StackOptions::kDoNotScan);

  // Explicitly cancel this async task. No `AsyncTasks`s must be created with
  // this context after `Cancel` was called.
  void Cancel();

  // Associates a marked script id with this async task for a given tracker
  // type.
  void SetMarkedScript(ScriptAncestryTrackerType type, V8ScriptId script_id);

  std::optional<V8ScriptId> GetMarkedScript(
      ScriptAncestryTrackerType type) const;

  // The Id uniquely identifies this task with the V8 debugger. The Id is
  // calculated based on the address of `AsyncTaskContext`.
  void* Id() const;

 private:
  friend class AsyncTask;

  static constexpr size_t kNumTrackerTypes =
      static_cast<size_t>(ScriptAncestryTrackerType::kMaxValue) + 1;
  std::array<std::optional<V8ScriptId>, kNumTrackerTypes> marked_scripts_;

  raw_ptr<v8::Isolate, UnprotectedInRelease | DanglingUntriaged> isolate_ =
      nullptr;
};

}  // namespace probe
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_CONTEXT_H_

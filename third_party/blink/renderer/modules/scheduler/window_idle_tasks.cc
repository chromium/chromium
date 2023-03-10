// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/window_idle_tasks.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

namespace {

// `V8IdleTask` is the adapter class for the conversion from
// `V8IdleRequestCallback` to `IdleTask`.
class V8IdleTask : public IdleTask {
 public:
  static V8IdleTask* Create(V8IdleRequestCallback* callback) {
    return MakeGarbageCollected<V8IdleTask>(callback);
  }

  explicit V8IdleTask(V8IdleRequestCallback* callback) : callback_(callback) {}

  ~V8IdleTask() override = default;

  void invoke(IdleDeadline* deadline) override {
    callback_->InvokeAndReportException(nullptr, deadline);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(callback_);
    IdleTask::Trace(visitor);
  }

 private:
  Member<V8IdleRequestCallback> callback_;
};

}  // namespace

int WindowIdleTasks::requestIdleCallback(LocalDOMWindow& window,
                                         V8IdleRequestCallback* callback,
                                         const IdleRequestOptions* options) {
  if (!window.GetFrame()) {
    return 0;
  }
  ScriptState* script_state = callback->CallbackRelevantScriptState();
  auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker();
  if (tracker && script_state->World().IsMainWorld()) {
    callback->SetParentTaskId(tracker->RunningTaskAttributionId(script_state));
  }
  return window.document()->RequestIdleCallback(V8IdleTask::Create(callback),
                                                options);
}

void WindowIdleTasks::cancelIdleCallback(LocalDOMWindow& window, int id) {
  window.document()->CancelIdleCallback(id);
}

}  // namespace blink

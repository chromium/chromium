// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/probe/async_task_context.h"

#include "base/check.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/core/ad_tracker/script_ancestry_tracker.h"
#include "third_party/blink/renderer/core/ad_tracker/script_initiation_monitor.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"

namespace blink {
namespace probe {

AsyncTaskContext::~AsyncTaskContext() {
  Cancel();
}

void AsyncTaskContext::Schedule(ExecutionContext* context,
                                const StringView& name,
                                StackOptions stack_options) {
  // TODO(crbug.com/1275875): Verify that this context was not already
  // scheduled or has already been canceled. Currently we don't have enough
  // confidence that such a CHECK wouldn't break blink.
  isolate_ = context ? context->GetIsolate() : nullptr;

  TRACE_EVENT("blink", "AsyncTask Scheduled",
              perfetto::Flow::FromPointer(this));

  if (!context)
    return;

  if (ThreadDebugger* debugger = ThreadDebugger::From(context->GetIsolate()))
    debugger->AsyncTaskScheduled(name, Id(), true);

  if (stack_options == StackOptions::kScan) {
    blink::ScriptInitiationMonitor* script_initiation_monitor =
        ScriptInitiationMonitor::FromExecutionContext(context);
    if (script_initiation_monitor) {
      script_initiation_monitor->DidCreateAsyncTask(this);
    }
  }
}

void AsyncTaskContext::Cancel() {
  if (ThreadDebugger* debugger = ThreadDebugger::From(isolate_))
    debugger->AsyncTaskCanceled(Id());
  isolate_ = nullptr;  // No need to cancel the task a second time.
}

void AsyncTaskContext::SetMarkedScript(ScriptAncestryTrackerType type,
                                       V8ScriptId script_id) {
  marked_scripts_.at(static_cast<size_t>(type)) = script_id;
}

std::optional<V8ScriptId> AsyncTaskContext::GetMarkedScript(
    ScriptAncestryTrackerType type) const {
  return marked_scripts_.at(static_cast<size_t>(type));
}

void* AsyncTaskContext::Id() const {
  // Blink uses odd ids for network requests and even ids for everything else.
  // We should make all of them even before reporting to V8 to avoid collisions
  // with internal V8 async events.
  return reinterpret_cast<void*>(reinterpret_cast<intptr_t>(this) << 1);
}

}  // namespace probe
}  // namespace blink

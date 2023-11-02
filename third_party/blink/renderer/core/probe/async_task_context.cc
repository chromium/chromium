// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/probe/async_task_context.h"

#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"

namespace blink {
namespace probe {

AsyncTaskContext::~AsyncTaskContext() {
  Cancel();
}

void AsyncTaskContext::Schedule(ExecutionContext* context,
                                const StringView& name) {
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

  blink::AdTracker* ad_tracker = AdTracker::FromExecutionContext(context);
  if (ad_tracker)
    ad_tracker->DidCreateAsyncTask(this);
}

void AsyncTaskContext::Cancel() {
  if (ThreadDebugger* debugger = ThreadDebugger::From(isolate_))
    debugger->AsyncTaskCanceled(Id());
  isolate_ = nullptr;  // No need to cancel the task a second time.
}

void* AsyncTaskContext::Id() const {
  // Blink uses odd ids for network requests and even ids for everything else.
  // We should make all of them even before reporting to V8 to avoid collisions
  // with internal V8 async events.
  return reinterpret_cast<void*>(reinterpret_cast<intptr_t>(this) << 1);
}

}  // namespace probe
}  // namespace blink

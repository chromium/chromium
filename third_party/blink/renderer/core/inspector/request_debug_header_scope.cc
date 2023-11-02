// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/request_debug_header_scope.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {
// static
String RequestDebugHeaderScope::CaptureStackIdForCurrentLocation(
    ExecutionContext* context) {
  ThreadDebugger* debugger = nullptr;
  if (auto* scope = DynamicTo<WorkerGlobalScope>(context))
    debugger = WorkerThreadDebugger::From(scope->GetThread()->GetIsolate());
  else if (LocalDOMWindow* dom_window = DynamicTo<LocalDOMWindow>(context))
    debugger = MainThreadDebugger::Instance();
  if (!debugger)
    return String();
  auto stack = debugger->StoreCurrentStackTrace("network request").ToString();
  return stack ? ToCoreString(std::move(stack)) : String();
}

RequestDebugHeaderScope::RequestDebugHeaderScope(ExecutionContext* context,
                                                 const String& header) {
  if (header.empty())
    return;
  stack_trace_id_ =
      v8_inspector::V8StackTraceId(ToV8InspectorStringView(header));
  if (stack_trace_id_.IsInvalid())
    return;
  if (auto* scope = DynamicTo<WorkerGlobalScope>(context))
    debugger_ = WorkerThreadDebugger::From(scope->GetThread()->GetIsolate());
  else if (auto* window = DynamicTo<LocalDOMWindow>(context))
    debugger_ = MainThreadDebugger::Instance();
  if (debugger_)
    debugger_->ExternalAsyncTaskStarted(stack_trace_id_);
}

RequestDebugHeaderScope::~RequestDebugHeaderScope() {
  if (debugger_)
    debugger_->ExternalAsyncTaskFinished(stack_trace_id_);
}

}  // namespace blink

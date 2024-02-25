// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/request_debug_header_scope.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"

namespace blink {
// static
String RequestDebugHeaderScope::CaptureStackIdForCurrentLocation(
    ExecutionContext* context) {
  if (!context) {
    return String();
  }
  ThreadDebugger* debugger = ThreadDebugger::From(context->GetIsolate());
  if (!debugger)
    return String();
  auto stack = debugger->StoreCurrentStackTrace("network request").ToString();
  return stack ? ToCoreString(std::move(stack)) : String();
}

RequestDebugHeaderScope::RequestDebugHeaderScope(ExecutionContext* context,
                                                 const String& header) {
  if (header.empty() || !context) {
    return;
  }
  stack_trace_id_ =
      v8_inspector::V8StackTraceId(ToV8InspectorStringView(header));
  if (stack_trace_id_.IsInvalid())
    return;
  debugger_ = ThreadDebugger::From(context->GetIsolate());
  if (debugger_)
    debugger_->ExternalAsyncTaskStarted(stack_trace_id_);
}

RequestDebugHeaderScope::~RequestDebugHeaderScope() {
  if (debugger_)
    debugger_->ExternalAsyncTaskFinished(stack_trace_id_);
}

}  // namespace blink

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

std::unique_ptr<SourceLocation> CaptureSourceLocation(
    ExecutionContext* execution_context) {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      SourceLocation::CaptureStackTraceInternal(false);
  if (stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTraceInternal(
        std::move(stack_trace));
  }

  if (LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    Document* document = window->document();
    unsigned line_number = 0;
    if (document->GetScriptableDocumentParser() &&
        !document->IsInDocumentWrite()) {
      if (document->GetScriptableDocumentParser()->IsParsingAtLineNumber()) {
        line_number =
            document->GetScriptableDocumentParser()->LineNumber().OneBasedInt();
      }
    }
    return std::make_unique<SourceLocation>(document->Url().GetString(),
                                            String(), line_number, 0,
                                            std::move(stack_trace));
  }

  return std::make_unique<SourceLocation>(
      execution_context ? execution_context->Url().GetString() : String(),
      String(), 0, 0, std::move(stack_trace));
}

std::unique_ptr<SourceLocation> CaptureSourceLocation(
    v8::Isolate* isolate,
    v8::Local<v8::Message> message,
    ExecutionContext* execution_context) {
  v8::Local<v8::StackTrace> stack = message->GetStackTrace();
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace;
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (debugger) {
    stack_trace = debugger->GetV8Inspector()->createStackTrace(stack);
  }

  int script_id = message->GetScriptOrigin().ScriptId();
  if (!stack.IsEmpty() && stack->GetFrameCount() > 0) {
    int top_script_id = stack->GetFrame(isolate, 0)->GetScriptId();
    if (top_script_id == script_id) {
      script_id = 0;
    }
  }

  int line_number = 0;
  int column_number = 0;
  if (message->GetLineNumber(isolate->GetCurrentContext()).To(&line_number) &&
      message->GetStartColumn(isolate->GetCurrentContext())
          .To(&column_number)) {
    ++column_number;
  }

  if ((!script_id || !line_number) && stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTraceInternal(
        std::move(stack_trace));
  }

  String url = ToCoreStringWithUndefinedOrNullCheck(
      isolate, message->GetScriptOrigin().ResourceName());
  if (url.empty()) {
    url = execution_context->Url();
  }
  return std::make_unique<SourceLocation>(url, String(), line_number,
                                          column_number, std::move(stack_trace),
                                          script_id);
}

}  // namespace blink

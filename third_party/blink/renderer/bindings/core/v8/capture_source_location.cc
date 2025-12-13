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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

String CaptureCurrentScriptUrl(v8::Isolate* isolate) {
  DCHECK(isolate);
  if (!isolate->InContext()) {
    return String();
  }

  v8::Local<v8::String> script_name =
      v8::StackTrace::CurrentScriptNameOrSourceURL(isolate);
  return ToCoreStringWithNullCheck(isolate, script_name);
}

Vector<String> CaptureScriptUrlsFromCurrentStack(v8::Isolate* isolate,
                                                 wtf_size_t unique_url_count) {
  Vector<String> unique_urls;

  if (!isolate || !isolate->InContext()) {
    return unique_urls;
  }

  // CurrentStackTrace is 10x faster than CaptureStackTrace if all that you
  // need is the url of the script at the top of the stack. See
  // crbug.com/1057211 for more detail.
  // Get at most 10 frames, regardless of the requested url count, to minimize
  // the performance impact.
  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate, /*frame_limit=*/10);

  int frame_count = stack_trace->GetFrameCount();
  for (int i = 0; i < frame_count; ++i) {
    v8::Local<v8::StackFrame> frame = stack_trace->GetFrame(isolate, i);
    v8::Local<v8::String> script_name = frame->GetScriptName();
    if (script_name.IsEmpty() || !script_name->Length()) {
      continue;
    }
    String url = ToCoreString(isolate, script_name);
    if (!unique_urls.Contains(url)) {
      unique_urls.push_back(std::move(url));
    }
    if (unique_urls.size() == unique_url_count) {
      break;
    }
  }
  return unique_urls;
}

SourceLocation* CaptureSourceLocation(ExecutionContext* execution_context) {
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
    return MakeGarbageCollected<SourceLocation>(document->Url().GetString(),
                                                String(), line_number, 0,
                                                std::move(stack_trace));
  }

  return MakeGarbageCollected<SourceLocation>(
      execution_context ? execution_context->Url().GetString() : String(),
      String(), 0, 0, std::move(stack_trace));
}

SourceLocation* CaptureSourceLocation(v8::Isolate* isolate,
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
  return MakeGarbageCollected<SourceLocation>(
      url, String(), line_number, column_number, std::move(stack_trace),
      script_id);
}

}  // namespace blink

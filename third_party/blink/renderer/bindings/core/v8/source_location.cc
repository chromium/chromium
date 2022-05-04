// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

namespace {

std::unique_ptr<v8_inspector::V8StackTrace> CaptureStackTrace(bool full) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (!debugger || !isolate->InContext())
    return nullptr;
  ScriptForbiddenScope::AllowUserAgentScript allow_scripting;
  return debugger->GetV8Inspector()->captureStackTrace(full);
}
}

// static
std::unique_ptr<SourceLocation> SourceLocation::Capture(
    const String& url,
    unsigned line_number,
    unsigned column_number) {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      CaptureStackTrace(false);
  if (stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTrace(
        std::move(stack_trace));
  }
  return std::make_unique<SourceLocation>(
      url, String(), line_number, column_number, std::move(stack_trace));
}

// static
std::unique_ptr<SourceLocation> SourceLocation::Capture(
    ExecutionContext* execution_context) {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      CaptureStackTrace(false);
  if (stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTrace(
        std::move(stack_trace));
  }

  if (LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    Document* document = window->document();
    unsigned line_number = 0;
    if (document->GetScriptableDocumentParser() &&
        !document->IsInDocumentWrite()) {
      if (document->GetScriptableDocumentParser()->IsParsingAtLineNumber())
        line_number =
            document->GetScriptableDocumentParser()->LineNumber().OneBasedInt();
    }
    return std::make_unique<SourceLocation>(document->Url().GetString(),
                                            String(), line_number, 0,
                                            std::move(stack_trace));
  }

  return std::make_unique<SourceLocation>(
      execution_context ? execution_context->Url().GetString() : String(),
      String(), 0, 0, std::move(stack_trace));
}

// static
std::unique_ptr<SourceLocation> SourceLocation::FromMessage(
    v8::Isolate* isolate,
    v8::Local<v8::Message> message,
    ExecutionContext* execution_context) {
  v8::Local<v8::StackTrace> stack = message->GetStackTrace();
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace;
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (debugger)
    stack_trace = debugger->GetV8Inspector()->createStackTrace(stack);

  int script_id = message->GetScriptOrigin().ScriptId();
  if (!stack.IsEmpty() && stack->GetFrameCount() > 0) {
    int top_script_id = stack->GetFrame(isolate, 0)->GetScriptId();
    if (top_script_id == script_id)
      script_id = 0;
  }

  int line_number = 0;
  int column_number = 0;
  if (message->GetLineNumber(isolate->GetCurrentContext()).To(&line_number) &&
      message->GetStartColumn(isolate->GetCurrentContext()).To(&column_number))
    ++column_number;

  if ((!script_id || !line_number) && stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTrace(
        std::move(stack_trace));
  }

  String url = ToCoreStringWithUndefinedOrNullCheck(
      message->GetScriptOrigin().ResourceName());
  if (url.IsEmpty())
    url = execution_context->Url();
  return std::make_unique<SourceLocation>(url, String(), line_number,
                                          column_number, std::move(stack_trace),
                                          script_id);
}

// static
std::unique_ptr<SourceLocation> SourceLocation::CreateFromNonEmptyV8StackTrace(
    std::unique_ptr<v8_inspector::V8StackTrace> stack_trace) {
  // Retrieve the data before passing the ownership to SourceLocation.
  String url = ToCoreString(stack_trace->topSourceURL());
  String function = ToCoreString(stack_trace->topFunctionName());
  unsigned line_number = stack_trace->topLineNumber();
  unsigned column_number = stack_trace->topColumnNumber();
  int script_id = stack_trace->topScriptId();
  return base::WrapUnique(
      new SourceLocation(url, function, line_number, column_number,
                         std::move(stack_trace), script_id));
}

// static
std::unique_ptr<SourceLocation> SourceLocation::FromFunction(
    v8::Local<v8::Function> function) {
  if (!function.IsEmpty())
    return std::make_unique<SourceLocation>(
        ToCoreStringWithUndefinedOrNullCheck(
            function->GetScriptOrigin().ResourceName()),
        ToCoreStringWithUndefinedOrNullCheck(function->GetName()),
        function->GetScriptLineNumber() + 1,
        function->GetScriptColumnNumber() + 1, nullptr, function->ScriptId());
  return std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr, 0);
}

// static
std::unique_ptr<SourceLocation> SourceLocation::CaptureWithFullStackTrace() {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      CaptureStackTrace(true);
  if (stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTrace(
        std::move(stack_trace));
  }
  return std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr, 0);
}

SourceLocation::SourceLocation(
    const String& url,
    const String& function,
    unsigned line_number,
    unsigned column_number,
    std::unique_ptr<v8_inspector::V8StackTrace> stack_trace,
    int script_id)
    : url_(url),
      function_(function),
      line_number_(line_number),
      column_number_(column_number),
      stack_trace_(std::move(stack_trace)),
      script_id_(script_id) {}

SourceLocation::~SourceLocation() = default;

void SourceLocation::ToTracedValue(TracedValue* value, const char* name) const {
  if (!stack_trace_ || stack_trace_->isEmpty())
    return;
  value->BeginArray(name);
  value->BeginDictionary();
  value->SetString("functionName",
                   ToCoreString(stack_trace_->topFunctionName()));
  value->SetInteger("scriptId", stack_trace_->topScriptId());
  value->SetString("url", ToCoreString(stack_trace_->topSourceURL()));
  value->SetInteger("lineNumber", stack_trace_->topLineNumber());
  value->SetInteger("columnNumber", stack_trace_->topColumnNumber());
  value->EndDictionary();
  value->EndArray();
}

void SourceLocation::WriteIntoTrace(perfetto::TracedValue context) const {
  // TODO(altimin): Consider replacing nested dict-inside-array with just an
  // array here.
  auto array = std::move(context).WriteArray();
  auto dict = array.AppendDictionary();
  // TODO(altimin): Add TracedValue support to v8::StringView and remove
  // ToCoreString calls.
  dict.Add("functionName", ToCoreString(stack_trace_->topFunctionName()));
  dict.Add("scriptId", String::Number(stack_trace_->topScriptId()));
  dict.Add("url", ToCoreString(stack_trace_->topSourceURL()));
  dict.Add("lineNumber", stack_trace_->topLineNumber());
  dict.Add("columnNumber", stack_trace_->topColumnNumber());
}

std::unique_ptr<SourceLocation> SourceLocation::Clone() const {
  return base::WrapUnique(new SourceLocation(
      url_.IsolatedCopy(), function_.IsolatedCopy(), line_number_,
      column_number_, stack_trace_ ? stack_trace_->clone() : nullptr,
      script_id_));
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
SourceLocation::BuildInspectorObject() const {
  return BuildInspectorObject(std::numeric_limits<int>::max());
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
SourceLocation::BuildInspectorObject(int max_async_depth) const {
  return stack_trace_ ? stack_trace_->buildInspectorObject(max_async_depth)
                      : nullptr;
}

String SourceLocation::ToString() const {
  if (!stack_trace_)
    return String();
  return ToCoreString(stack_trace_->toString());
}

}  // namespace blink

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
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

std::unique_ptr<v8_inspector::V8StackTrace> CaptureStackTraceForReporting(
    const ExecutionContext* execution_context) {
  if (!RuntimeEnabledFeatures::CaptureSourceLocationSameOriginCheckEnabled()) {
    return SourceLocation::CaptureStackTraceInternal(false);
  }
  if (!execution_context) {
    return nullptr;
  }
  v8::Isolate* isolate = execution_context->GetIsolate();
  if (!isolate || !isolate->InContext()) {
    return nullptr;
  }
  const SecurityOrigin* target_origin = execution_context->GetSecurityOrigin();
  ExecutionContext* current_context = CurrentExecutionContext(isolate);
  const SecurityOrigin* current_origin =
      current_context ? current_context->GetSecurityOrigin() : nullptr;
  if (!target_origin || !current_origin ||
      !target_origin->CanAccess(current_origin)) {
    return nullptr;
  }
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (!debugger) {
    return nullptr;
  }
  ScriptForbiddenScope::AllowUserAgentScript allow_scripting;
  v8::HandleScope handle_scope(isolate);
  // Unlike V8Inspector::captureStackTrace (which sets
  // kExposeFramesAcrossSecurityOrigins), v8::StackTrace::CurrentStackTrace with
  // kDetailed skips frames that are not same-origin with `current_context`.
  return debugger->GetV8Inspector()->createStackTrace(
      v8::StackTrace::CurrentStackTrace(isolate, /*frame_limit=*/1,
                                        v8::StackTrace::kDetailed));
}

SourceLocation* CreateSourceLocationFromStackTraceOrExecutionContext(
    std::unique_ptr<v8_inspector::V8StackTrace> stack_trace,
    const ExecutionContext* execution_context) {
  if (stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTraceInternal(
        std::move(stack_trace));
  }

  if (const LocalDOMWindow* window =
          DynamicTo<LocalDOMWindow>(execution_context)) {
    Document* document = window->document();
    TextPosition position = TextPosition::BelowRangePosition();
    // Only fall back to the HTML parser position when no script is executing;
    // if a cross-origin script stack was rejected while `document`'s parser is
    // paused, we must not misattribute the action to the parser line.
    v8::Isolate* isolate = execution_context->GetIsolate();
    if ((!isolate || !isolate->InContext()) &&
        document->GetScriptableDocumentParser() &&
        !document->IsInDocumentWrite() &&
        document->GetScriptableDocumentParser()->IsParsingAtLineNumber()) {
      position = document->GetScriptableDocumentParser()->GetTextPosition();
    }
    return MakeGarbageCollected<SourceLocation>(
        document->Url().GetString(), String(), position.line_.OneBasedInt(),
        position.column_.OneBasedInt(), std::move(stack_trace));
  }

  return MakeGarbageCollected<SourceLocation>(
      execution_context ? execution_context->Url().GetString() : String(),
      String(), 0, 0, std::move(stack_trace));
}

}  // namespace

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

SourceLocation* CaptureSourceLocation(
    const ExecutionContext* execution_context) {
  return CreateSourceLocationFromStackTraceOrExecutionContext(
      SourceLocation::CaptureStackTraceInternal(false), execution_context);
}

SourceLocation* CaptureSourceLocationForReporting(
    const ExecutionContext* execution_context) {
  return CreateSourceLocationFromStackTraceOrExecutionContext(
      CaptureStackTraceForReporting(execution_context), execution_context);
}

SourceLocation* CaptureSourceLocationForReporting(
    const ExecutionContext* execution_context,
    const String& url,
    unsigned line_number,
    unsigned column_number) {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      CaptureStackTraceForReporting(execution_context);
  if (stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTraceInternal(
        std::move(stack_trace));
  }
  return MakeGarbageCollected<SourceLocation>(
      url, String(), line_number, column_number, std::move(stack_trace));
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

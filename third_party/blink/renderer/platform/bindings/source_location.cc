// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/source_location.h"

#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "v8/include/v8-inspector-protocol.h"

namespace blink {

namespace {

String ToPlatformString(const v8_inspector::StringView& string) {
  if (string.is8Bit()) {
    // SAFETY: v8_inspector::StringView guarantees characters8() and length()
    // are safe.
    return String(
        UNSAFE_BUFFERS(base::span(string.characters8(), string.length())));
  }
  // SAFETY: v8_inspector::StringView guarantees characters16() and length()
  // are safe.
  return String(UNSAFE_BUFFERS(base::span(
      reinterpret_cast<const UChar*>(string.characters16()), string.length())));
}

String ToPlatformString(std::unique_ptr<v8_inspector::StringBuffer> buffer) {
  if (!buffer)
    return String();
  return ToPlatformString(buffer->string());
}

}  // namespace

// static
SourceLocation* SourceLocation::CaptureWithFullStackTrace() {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      CaptureStackTraceInternal(true);
  if (stack_trace && !stack_trace->isEmpty()) {
    return CreateFromNonEmptyV8StackTraceInternal(std::move(stack_trace));
  }
  return MakeGarbageCollected<SourceLocation>(String(), String(), 0, 0, nullptr,
                                              0);
}

// static
std::unique_ptr<v8_inspector::V8StackTrace>
SourceLocation::CaptureStackTraceInternal(bool full) {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (!debugger || !isolate->InContext())
    return nullptr;
  ScriptForbiddenScope::AllowUserAgentScript allow_scripting;
  return debugger->GetV8Inspector()->captureStackTrace(full);
}

// static
SourceLocation* SourceLocation::CreateFromNonEmptyV8StackTraceInternal(
    std::unique_ptr<v8_inspector::V8StackTrace> stack_trace) {
  // Retrieve the data before passing the ownership to SourceLocation.
  String url = ToPlatformString(stack_trace->topSourceURL());
  String function = ToPlatformString(stack_trace->topFunctionName());
  unsigned line_number = stack_trace->topLineNumber();
  unsigned column_number = stack_trace->topColumnNumber();
  int script_id = stack_trace->topScriptId();
  return MakeGarbageCollected<SourceLocation>(
      url, function, line_number, column_number, std::move(stack_trace),
      script_id);
}

SourceLocation::SourceLocation(const String& url, int char_position)
    : url_(url),
      line_number_(0),
      column_number_(0),
      char_position_(char_position),
      script_id_(0) {}

SourceLocation::SourceLocation(const String& url,
                               int char_position,
                               unsigned line_number,
                               unsigned column_number)
    : url_(url),
      line_number_(line_number),
      column_number_(column_number),
      char_position_(char_position),
      script_id_(0) {}

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
      char_position_(-1),
      stack_trace_(std::move(stack_trace)),
      script_id_(script_id) {}

SourceLocation::~SourceLocation() = default;

SourceLocation* SourceLocation::Clone() const {
  return MakeGarbageCollected<SourceLocation>(
      url_, function_, line_number_, column_number_,
      stack_trace_ ? stack_trace_->clone() : nullptr, script_id_);
}

void SourceLocation::WriteIntoTrace(perfetto::TracedValue context) const {
  if (!stack_trace_ || stack_trace_->isEmpty()) {
    return;
  }
  // TODO(altimin): Add TracedValue support to v8::StringView and remove
  // ToPlatformString calls.
  auto array = std::move(context).WriteArray();
  for (const auto& frame : stack_trace_->frames()) {
    auto dict = array.AppendDictionary();
    dict.Add("functionName", ToPlatformString(frame.functionName).Utf8());
    dict.Add("scriptId", String::Number(frame.scriptId));
    dict.Add("url", ToPlatformString(frame.sourceURL).Utf8());
    dict.Add("lineNumber", frame.lineNumber);
    dict.Add("columnNumber", frame.columnNumber);
  }
}

void SourceLocation::ToTracedValue(TracedValue* value, const char* name) const {
  if (!stack_trace_ || stack_trace_->isEmpty())
    return;
  value->BeginArray(name);
  value->BeginDictionary();
  value->SetString("functionName",
                   ToPlatformString(stack_trace_->topFunctionName()));
  value->SetInteger("scriptId", stack_trace_->topScriptId());
  value->SetString("url", ToPlatformString(stack_trace_->topSourceURL()));
  value->SetInteger("lineNumber", stack_trace_->topLineNumber());
  value->SetInteger("columnNumber", stack_trace_->topColumnNumber());

  value->BeginArray("stackFrames");
  for (const auto& frame : stack_trace_->frames()) {
    value->BeginDictionary();
    value->SetString("functionName", ToPlatformString(frame.functionName));

    value->BeginDictionary("scriptLocation");
    value->SetString("sourceURL", ToPlatformString(frame.sourceURL));
    value->SetInteger("lineNumber", frame.lineNumber);
    value->SetInteger("columnNumber", frame.columnNumber);
    value->EndDictionary(/*scriptLocation*/);

    value->EndDictionary();
  }
  value->EndArray(/*stackFrames*/);

  value->EndDictionary();
  value->EndArray();
}

String SourceLocation::ToString() const {
  if (!stack_trace_)
    return String();
  return ToPlatformString(stack_trace_->toString());
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

SourceLocation* CaptureSourceLocation(const String& url,
                                      unsigned line_number,
                                      unsigned column_number) {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      SourceLocation::CaptureStackTraceInternal(false);
  if (stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTraceInternal(
        std::move(stack_trace));
  }
  return MakeGarbageCollected<SourceLocation>(
      url, String(), line_number, column_number, std::move(stack_trace));
}

SourceLocation* CaptureSourceLocation() {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      SourceLocation::CaptureStackTraceInternal(false);
  if (stack_trace && !stack_trace->isEmpty()) {
    return SourceLocation::CreateFromNonEmptyV8StackTraceInternal(
        std::move(stack_trace));
  }

  return MakeGarbageCollected<SourceLocation>(String(), String(), 0, 0,
                                              std::move(stack_trace));
}

SourceLocation* CaptureSourceLocation(v8::Isolate* isolate,
                                      v8::Local<v8::Function> function) {
  if (!function.IsEmpty()) {
    v8::Location location = function->GetScriptLocation();
    return MakeGarbageCollected<SourceLocation>(
        ToCoreStringWithUndefinedOrNullCheck(
            isolate, function->GetScriptOrigin().ResourceName()),
        ToCoreStringWithUndefinedOrNullCheck(isolate, function->GetName()),
        location.GetLineNumber() + 1, location.GetColumnNumber() + 1, nullptr,
        function->ScriptId());
  }
  return MakeGarbageCollected<SourceLocation>(String(), String(), 0, 0, nullptr,
                                              0);
}

}  // namespace blink

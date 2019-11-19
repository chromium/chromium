// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SOURCE_LOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SOURCE_LOCATION_H_

#include <v8-inspector-protocol.h>
#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class TracedValue;

class CORE_EXPORT SourceLocation {
  USING_FAST_MALLOC(SourceLocation);

 public:
  // Zero lineNumber and columnNumber mean unknown. Captures current stack
  // trace.
  static std::unique_ptr<SourceLocation> Capture(const String& url,
                                                 unsigned line_number,
                                                 unsigned column_number);

  // Shortcut when location is unknown. Tries to capture call stack or parsing
  // location if available.
  static std::unique_ptr<SourceLocation> Capture(ExecutionContext* = nullptr);

  static std::unique_ptr<SourceLocation> FromMessage(v8::Isolate*,
                                                     v8::Local<v8::Message>,
                                                     ExecutionContext*);

  static std::unique_ptr<SourceLocation> FromFunction(v8::Local<v8::Function>);

  // Forces full stack trace.
  static std::unique_ptr<SourceLocation> CaptureWithFullStackTrace();

  SourceLocation(const String& url,
                 unsigned line_number,
                 unsigned column_number,
                 std::unique_ptr<v8_inspector::V8StackTrace>,
                 int script_id = 0);
  ~SourceLocation();

  bool IsUnknown() const {
    return url_.IsNull() && !script_id_ && !line_number_;
  }
  const String& Url() const { return url_; }
  unsigned LineNumber() const { return line_number_; }
  unsigned ColumnNumber() const { return column_number_; }
  int ScriptId() const { return script_id_; }
  std::unique_ptr<v8_inspector::V8StackTrace> TakeStackTrace() {
    return std::move(stack_trace_);
  }

  // Safe to pass between threads, drops async chain in stack trace.
  std::unique_ptr<SourceLocation> Clone() const;

  // No-op when stack trace is unknown.
  void ToTracedValue(TracedValue*, const char* name) const;

  // Could be null string when stack trace is unknown.
  String ToString() const;

  // Could be null when stack trace is unknown.
  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
  BuildInspectorObject() const;

  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
  BuildInspectorObject(int max_async_depth) const;

 private:
  static std::unique_ptr<SourceLocation> CreateFromNonEmptyV8StackTrace(
      std::unique_ptr<v8_inspector::V8StackTrace>,
      int script_id);

  String url_;
  unsigned line_number_;
  unsigned column_number_;
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace_;
  int script_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SOURCE_LOCATION_H_

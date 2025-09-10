// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/lazy_source_location.h"

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

LazySourceLocation* LazySourceLocation::FromCurrentStack(v8::Isolate* isolate) {
  DCHECK(isolate);

  if (!isolate->InContext()) {
    return MakeGarbageCollected<LazySourceLocation>();
  }

  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate, /*frame_limit=*/1);

  if (stack_trace->GetFrameCount() == 0) {
    return MakeGarbageCollected<LazySourceLocation>();
  }

  v8::Local<v8::StackFrame> stack_frame = stack_trace->GetFrame(isolate, 0);
  int script_position = stack_frame->GetSourcePosition();
  v8::Local<v8::String> script_name_v8 =
      stack_frame->GetScriptNameOrSourceURL();

  if (RuntimeEnabledFeatures::LongAnimationFrameSourceLineColumnEnabled()) {
    v8::Location location = stack_frame->GetLocation();
    return MakeGarbageCollected<LazySourceLocation>(
        isolate, script_name_v8, script_position, location.GetLineNumber(),
        location.GetColumnNumber());
  }

  return MakeGarbageCollected<LazySourceLocation>(isolate, script_name_v8,
                                                  script_position);
}

LazySourceLocation::LazySourceLocation() = default;

LazySourceLocation::LazySourceLocation(const String& url) : url_(url) {}

LazySourceLocation::LazySourceLocation(v8::Isolate* isolate,
                                       v8::Local<v8::String> url,
                                       int char_position)
    : char_position_(char_position) {
  url_.emplace<TraceWrapperV8Reference<v8::String>>(isolate, url);
}

LazySourceLocation::LazySourceLocation(v8::Isolate* isolate,
                                       v8::Local<v8::String> url,
                                       int char_position,
                                       int line_number,
                                       int column_number)
    : char_position_(char_position),
      line_number_(line_number),
      column_number_(column_number) {
  url_.emplace<TraceWrapperV8Reference<v8::String>>(isolate, url);
}

LazySourceLocation::~LazySourceLocation() = default;

// The core method that handles the lazy conversion.
// The URL string is converted from a V8 handle to a Blink string
// only when this method is called for the first time.
const String& LazySourceLocation::Url(v8::Isolate* isolate) const {
  if (std::holds_alternative<TraceWrapperV8Reference<v8::String>>(url_)) {
    const auto& persistent =
        std::get<TraceWrapperV8Reference<v8::String>>(url_);
    if (!persistent.IsEmpty()) {
      url_ = ToCoreStringWithNullCheck(isolate, persistent.Get(isolate));
    } else {
      url_ = String();
    }
  }

  return std::get<String>(url_);
}

void LazySourceLocation::Trace(Visitor* visitor) const {
  if (std::holds_alternative<TraceWrapperV8Reference<v8::String>>(url_)) {
    visitor->Trace(std::get<TraceWrapperV8Reference<v8::String>>(url_));
  }
}

}  // namespace blink

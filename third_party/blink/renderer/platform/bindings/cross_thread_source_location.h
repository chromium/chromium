// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CROSS_THREAD_SOURCE_LOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CROSS_THREAD_SOURCE_LOCATION_H_

#include "third_party/blink/renderer/platform/bindings/source_location.h"

namespace blink {

class PLATFORM_EXPORT CrossThreadSourceLocation {
  friend struct CrossThreadCopier<CrossThreadSourceLocation>;

 public:
  CrossThreadSourceLocation(
      const String& url,
      const String& function,
      unsigned line_number,
      unsigned column_number,
      std::unique_ptr<v8_inspector::V8StackTrace> stack_trace_clone,
      int script_id)
      : url(url),
        function(function),
        line_number(line_number),
        column_number(column_number),
        stack_trace(std::move(stack_trace_clone)),
        script_id(script_id) {}

  explicit CrossThreadSourceLocation(SourceLocation* source_location) {
    if (!source_location) {
      return;
    }

    url = source_location->Url();
    function = source_location->Function();
    line_number = source_location->LineNumber();
    column_number = source_location->ColumnNumber();
    stack_trace = source_location->HasStackTrace()
                      ? source_location->StackTrace()->clone()
                      : nullptr;
    script_id = source_location->ScriptId();
  }

  SourceLocation* CreateSourceLocation() const {
    return MakeGarbageCollected<SourceLocation>(
        url, function, line_number, column_number,
        stack_trace ? stack_trace->clone() : nullptr, script_id);
  }

 private:
  String url;
  String function;
  unsigned line_number = 0;
  unsigned column_number = 0;
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace;
  int script_id = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CROSS_THREAD_SOURCE_LOCATION_H_

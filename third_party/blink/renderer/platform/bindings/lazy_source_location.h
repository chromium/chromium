// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_LAZY_SOURCE_LOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_LAZY_SOURCE_LOCATION_H_

#include <variant>

#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

// A location in JS source code that performs a lazy conversion of the URL
// string from a v8::String to a Blink String. This class is designed to be
// used specifically in scenarios where the URL may not be retrieved, improving
// performance by deferring the string conversion and allocation.
class PLATFORM_EXPORT LazySourceLocation final
    : public GarbageCollected<LazySourceLocation> {
 public:
  LazySourceLocation();

  LazySourceLocation(const String& url);

  LazySourceLocation(v8::Isolate* isolate,
                     v8::Local<v8::String> url,
                     int char_position);

  LazySourceLocation(v8::Isolate* isolate,
                     v8::Local<v8::String> url,
                     int char_position,
                     int line_number,
                     int column_number);
  ~LazySourceLocation();

  // This enables lazy conversion of script URLs for performance.
  static LazySourceLocation* FromCurrentStack(v8::Isolate* isolate);
  const String& Url(v8::Isolate* isolate);
  int CharPosition() const { return char_position_; }
  int LineNumber() const { return line_number_; }
  int ColumnNumber() const { return column_number_; }
  void Trace(Visitor* visitor) const;

 private:
  TraceWrapperV8Reference<v8::String> v8_url_;
  String url_;
  int char_position_ = -1;
  int line_number_ = 0;
  int column_number_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_LAZY_SOURCE_LOCATION_H_

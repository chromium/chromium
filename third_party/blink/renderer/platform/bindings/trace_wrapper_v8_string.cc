// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_string.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void TraceWrapperV8String::Concat(v8::Isolate* isolate, const String& string) {
  DCHECK(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::String> target_string =
      (string_.IsEmpty()) ? V8String(isolate, string)
                          : v8::String::Concat(isolate, string_.Get(isolate),
                                               V8String(isolate, string));
  string_.Reset(isolate, target_string);
}

String TraceWrapperV8String::Flatten(v8::Isolate* isolate) const {
  if (IsEmpty())
    return String();
  DCHECK(isolate);
  v8::HandleScope handle_scope(isolate);
  return ToBlinkString<String>(string_.Get(isolate), kExternalize);
}

}  // namespace blink

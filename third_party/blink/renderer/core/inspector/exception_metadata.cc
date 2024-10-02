// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_EXCEPTION_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_EXCEPTION_METADATA_H_

#include "third_party/blink/renderer/core/inspector/exception_metadata.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

void MaybeAssociateExceptionMetaData(v8::Local<v8::Value> exception,
                                     const String& key,
                                     const String& value) {
  if (exception.IsEmpty()) {
    // Should only happen in tests.
    return;
  }
  // Associating meta-data is only supported for exception that are objects.
  if (!exception->IsObject()) {
    return;
  }
  v8::Local<v8::Object> object = exception.As<v8::Object>();
  v8::Isolate* isolate = object->GetIsolate();
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  debugger->GetV8Inspector()->associateExceptionData(
      v8::Local<v8::Context>(), exception, V8String(isolate, key),
      V8String(isolate, value));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_EXCEPTION_METADATA_H_

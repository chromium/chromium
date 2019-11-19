// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_ITERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ExecutionContext;

class CORE_EXPORT ScriptIterator {
  STACK_ALLOCATED();

 public:
  ScriptIterator(std::nullptr_t) : isolate_(nullptr), done_(true) {}

  ScriptIterator(v8::Local<v8::Object> iterator, v8::Isolate*);

  bool IsNull() const { return iterator_.IsEmpty(); }

  // Returns true if the iterator is still not done.
  bool Next(ExecutionContext*,
            ExceptionState&,
            v8::Local<v8::Value> next_value = v8::Local<v8::Value>());

  v8::MaybeLocal<v8::Value> GetValue() { return value_; }

 private:
  v8::Isolate* isolate_;
  v8::Local<v8::Object> iterator_;
  v8::Local<v8::String> next_key_;
  v8::Local<v8::String> done_key_;
  v8::Local<v8::String> value_key_;
  bool done_;
  v8::MaybeLocal<v8::Value> value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_ITERATOR_H_

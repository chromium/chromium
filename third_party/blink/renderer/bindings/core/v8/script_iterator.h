// Copyright 2016 The Chromium Authors
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

// This class provides a wrapper for iterating over any ES object that
// implements the iterable and iterator protocols. Namely:
// * The object or an object in its prototype chain has an @@iterator property
//   that is a function that returns an iterator object.
// * The iterator object has a next() method that returns an object with at
//   least two properties:
//   1. done: A boolean indicating whether iteration should stop. Can be
//      omitted when false.
//   2. value: Any object. Can be omitted when |done| is true.
//
// In general, this class should be preferred over using the
// GetEsIteratorMethod() and GetEsIteratorWithMethod() functions directly.
//
// Usage:
//   v8::Local<v8::Object> es_object = ...;
//   auto script_iterator = ScriptIterator::FromIterable(
//       isolate, es_object, exception_state,
//       ScriptIterator::ConversionFailureMode::kDoNotThrowTypeError);
//   if (exception_state.HadException())
//     return;
//   if (!script_iterator.IsNull()) {
//     while (script_iterator.Next(execution_context, exception_state)) {
//       // V8 may have thrown an exception.
//       if (exception_state.HadException())
//         return;
//       v8::Local<v8::Value> value =
//           script_iterator.GetValue().ToLocalChecked();
//       // Do something with |value|.
//     }
//   }
//   // If the very first call to Next() throws, the loop above will not be
//   // entered, so we need to catch any exceptions here.
//   if (exception_state.HadException())
//     return;
class CORE_EXPORT ScriptIterator {
  STACK_ALLOCATED();

 public:
  // Creates a ScriptIterator out of an ES object that implements the iterable
  // and iterator protocols.
  // Both the return value and the ExceptionState should be checked:
  // - The ExceptionState will contain an exception if V8 throws one, or if the
  //   ES objects do not conform to the expected protocols. In this case, the
  //   returned ScriptIterator will be null.
  // - ScriptIterator can be null even if there is no exception. In this case,
  //   it indicates that the given ES object does not have an @@iterator
  //   property.
  static ScriptIterator FromIterable(v8::Isolate* isolate,
                                     v8::Local<v8::Object> iterable,
                                     ExceptionState& exception_state);

  ScriptIterator(ScriptIterator&&) noexcept = default;
  ScriptIterator& operator=(ScriptIterator&&) noexcept = default;

  ScriptIterator(const ScriptIterator&) = delete;
  ScriptIterator& operator=(const ScriptIterator&) = delete;

  bool IsNull() const { return iterator_.IsEmpty(); }

  // Returns true if the iterator is still not done.
  bool Next(ExecutionContext* execution_context,
            ExceptionState& exception_state,
            v8::Local<v8::Value> value = v8::Local<v8::Value>());

  v8::MaybeLocal<v8::Value> GetValue() { return value_; }

 private:
  // Constructs a ScriptIterator from an ES object that implements the iterator
  // protocol: |iterator| is supposed to have a next() method that returns an
  // object with two properties, "done" and "value".
  ScriptIterator(v8::Isolate*,
                 v8::Local<v8::Object> iterator,
                 v8::Local<v8::Value> next_method);

  ScriptIterator() = default;

  v8::Isolate* isolate_ = nullptr;
  v8::Local<v8::Object> iterator_;
  v8::Local<v8::Value> next_method_;
  v8::Local<v8::String> done_key_;
  v8::Local<v8::String> value_key_;
  bool done_ = true;
  v8::MaybeLocal<v8::Value> value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_ITERATOR_H_

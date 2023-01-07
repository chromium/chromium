// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
ScriptIterator ScriptIterator::FromIterable(v8::Isolate* isolate,
                                            v8::Local<v8::Object> iterable,
                                            ExceptionState& exception_state) {
  // 7.4.1 GetIterator ( obj [ , hint [ , method ] ] )
  // https://tc39.es/ecma262/#sec-getiterator
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();

  // 3.b. Otherwise, set method to ? GetMethod(obj, @@iterator).
  v8::Local<v8::Value> method;
  if (!iterable->Get(current_context, v8::Symbol::GetIterator(isolate))
           .ToLocal(&method)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return ScriptIterator();
  }
  if (method->IsNullOrUndefined()) {
    // Some algorithms in Web IDL want to change their behavior when `method` is
    // undefined, so give them a choice.
    return ScriptIterator();  // Return without an exception.
  }
  if (!method->IsFunction()) {
    exception_state.ThrowTypeError("@@iterator must be a callable.");
    return ScriptIterator();
  }

  // 4. Let iterator be ? Call(method, obj).
  v8::Local<v8::Value> iterator;
  if (!V8ScriptRunner::CallFunction(method.As<v8::Function>(),
                                    ToExecutionContext(current_context),
                                    iterable, 0, nullptr, isolate)
           .ToLocal(&iterator)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return ScriptIterator();
  }
  // 5. If Type(iterator) is not Object, throw a TypeError exception.
  if (!iterator->IsObject()) {
    exception_state.ThrowTypeError("Iterator object must be an object.");
    return ScriptIterator();
  }

  // 6. Let nextMethod be ? GetV(iterator, "next").
  v8::Local<v8::Value> next_method;
  if (!iterator.As<v8::Object>()
           ->Get(current_context, V8AtomicString(isolate, "next"))
           .ToLocal(&next_method)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return ScriptIterator();
  }

  // 7. Let iteratorRecord be the Record { [[Iterator]]: iterator,
  //   [[NextMethod]]: nextMethod, [[Done]]: false }.
  // 8. Return iteratorRecord.
  return ScriptIterator(isolate, iterator.As<v8::Object>(), next_method);
}

ScriptIterator::ScriptIterator(v8::Isolate* isolate,
                               v8::Local<v8::Object> iterator,
                               v8::Local<v8::Value> next_method)
    : isolate_(isolate),
      iterator_(iterator),
      next_method_(next_method),
      done_key_(V8AtomicString(isolate, "done")),
      value_key_(V8AtomicString(isolate, "value")),
      done_(false) {
  DCHECK(!IsNull());
}

bool ScriptIterator::Next(ExecutionContext* execution_context,
                          ExceptionState& exception_state,
                          v8::Local<v8::Value> value) {
  DCHECK(!IsNull());

  if (!next_method_->IsFunction()) {
    exception_state.ThrowTypeError("Expected next() function on iterator.");
    done_ = true;
    return false;
  }

  v8::TryCatch try_catch(isolate_);
  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::CallFunction(next_method_.As<v8::Function>(),
                                    execution_context, iterator_,
                                    value.IsEmpty() ? 0 : 1, &value, isolate_)
           .ToLocal(&result)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }
  if (!result->IsObject()) {
    exception_state.ThrowTypeError(
        "Expected iterator.next() to return an Object.");
    done_ = true;
    return false;
  }
  v8::Local<v8::Object> result_object = result.As<v8::Object>();

  v8::Local<v8::Context> context = isolate_->GetCurrentContext();
  value_ = result_object->Get(context, value_key_);
  if (value_.IsEmpty()) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }

  v8::Local<v8::Value> done;
  if (!result_object->Get(context, done_key_).ToLocal(&done)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }
  done_ = done->BooleanValue(isolate_);
  return !done_;
}

}  // namespace blink

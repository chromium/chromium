// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

namespace {

class AsyncIteratorCloseFulfillFunction final
    : public ScriptFunction::Callable {
 public:
  explicit AsyncIteratorCloseFulfillFunction(ExceptionContext exception_context)
      : exception_context_(exception_context) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
    // In a detached context, we shouldn't proceed to do things that can run
    // script.
    if (!script_state->ContextIsValid()) {
      return ScriptValue();
    }

    // 9.1. If Type(returnPromiseResult) is not Object, throw a TypeError.
    if (!value.V8Value()->IsObject()) {
      auto error = V8ThrowException::CreateTypeError(
          script_state->GetIsolate(),
          "Expected return() to resolve to an Object.");
      ApplyContextToException(script_state, error, exception_context_);
      V8ThrowException::ThrowException(script_state->GetIsolate(), error);
      return ScriptValue();
    }

    // 9.2. Return undefined.
    return ScriptValue();
  }

  void Trace(Visitor* visitor) const final {
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  ExceptionContext exception_context_;
};

}  // namespace

// static
ScriptIterator ScriptIterator::FromIterable(v8::Isolate* isolate,
                                            v8::Local<v8::Object> iterable,
                                            ExceptionState& exception_state,
                                            Kind kind) {
  // 7.4.3 GetIterator(obj, kind).
  // https://tc39.es/ecma262/#sec-getiterator
  TryRethrowScope rethrow_scope(isolate, exception_state);
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();

  v8::Local<v8::Value> method;

  // 1. If kind is ASYNC, then.
  if (kind == Kind::kAsync) {
    // 1.a. Let method be ? GetMethod(obj, @@asyncIterator).
    if (!iterable->Get(current_context, v8::Symbol::GetAsyncIterator(isolate))
             .ToLocal(&method)) {
      DCHECK(rethrow_scope.HasCaught());
      return ScriptIterator();
    }

    // 1.b. If method is undefined, then
    //
    // We use `IsNullOrUndefined()` here instead of `IsUndefined()`, because
    // ECMAScript's GetMethod() abstract operation returns undefined for methods
    // that are either null or undefined.
    // https://github.com/tc39/ecma262/issues/3417.
    if (method->IsNullOrUndefined()) {
      // TODO(crbug.com/356891478): Match ECMAScript by falling back to creating
      // an async iterator out of an @@iterable implementation, if such an
      // implementation exists:
      //
      // 1.b.i Let syncMethod be ? GetMethod(obj, %Symbol.iterator%).
      // 1.b.ii. If syncMethod is undefined, throw a TypeError exception.
      // 1.b.iii. Let syncIteratorRecord be ? GetIteratorFromMethod(obj,
      //          syncMethod).
      // 1.b.iv. Return CreateAsyncFromSyncIterator(syncIteratorRecord).
      //
      // For now, we just return an `IsNull()` iterator with no exception.
      DCHECK(!rethrow_scope.HasCaught());
      return ScriptIterator();
    }
  } else {
    // 2. Else, let method be ? GetMethod(obj, @@iterator).
    if (!iterable->Get(current_context, v8::Symbol::GetIterator(isolate))
             .ToLocal(&method)) {
      DCHECK(rethrow_scope.HasCaught());
      return ScriptIterator();
    }
    // 3. If method is undefined, throw a TypeError exception.
    //
    // Note we deviate from the spec here! Some algorithms in Web IDL want to
    // change their behavior when `method` is undefined, so give them a choice.
    // They can detect this case by seeing that `IsNull()` is true and there is
    // no exception on the stack.
    //
    // See documentation above about why we use `IsNullOrUndefined()` instead of
    // `IsUndefined()`.
    if (method->IsNullOrUndefined()) {
      DCHECK(!rethrow_scope.HasCaught());
      return ScriptIterator();
    }
  }

  // GetMethod(V, P):
  // https://tc39.es/ecma262/#sec-getmethod.
  //
  // 3. If IsCallable(func) is false, throw a TypeError exception.
  if (!method->IsFunction()) {
    if (kind == Kind::kAsync) {
      exception_state.ThrowTypeError("@@asyncIterator must be a callable.");
    } else {
      exception_state.ThrowTypeError("@@iterator must be a callable.");
    }
    return ScriptIterator();
  }

  // 4. Return ? GetIteratorFromMethod(obj, method).
  //
  // The rest of this algorithm quotes the GetIteratorFromMethod(obj, method)
  // abstract algorithm spec text:
  // https://tc39.es/ecma262/#sec-getiteratorfrommethod
  //
  // 1. Let iterator be ? Call(method, obj).
  v8::Local<v8::Value> iterator;
  if (!V8ScriptRunner::CallFunction(method.As<v8::Function>(),
                                    ToExecutionContext(current_context),
                                    iterable, 0, nullptr, isolate)
           .ToLocal(&iterator)) {
    DCHECK(rethrow_scope.HasCaught());
    return ScriptIterator();
  }

  // 2. If iterator is not Object, throw a TypeError exception.
  if (!iterator->IsObject()) {
    exception_state.ThrowTypeError("Iterator object must be an object.");
    return ScriptIterator();
  }

  // 3. Let nextMethod be ? Get(iterator, "next").
  v8::Local<v8::Value> next_method;
  if (!iterator.As<v8::Object>()
           ->Get(current_context, V8AtomicString(isolate, "next"))
           .ToLocal(&next_method)) {
    return ScriptIterator();
  }

  // 4. Let iteratorRecord be the Iterator Record { [[Iterator]]: iterator,
  //    [[NextMethod]]: nextMethod, [[Done]]: false }.
  // 5. Return iteratorRecord.
  return ScriptIterator(isolate, iterator.As<v8::Object>(), next_method, kind);
}

ScriptIterator::ScriptIterator(v8::Isolate* isolate,
                               v8::Local<v8::Object> iterator,
                               v8::Local<v8::Value> next_method,
                               Kind kind)
    : isolate_(isolate),
      iterator_(isolate, iterator),
      next_method_(isolate, next_method),
      done_key_(V8AtomicString(isolate, "done")),
      value_key_(V8AtomicString(isolate, "value")),
      done_(false),
      kind_(kind) {
  DCHECK(!IsNull());
}

bool ScriptIterator::Next(ExecutionContext* execution_context,
                          ExceptionState& exception_state) {
  DCHECK(!IsNull());

  ScriptState* script_state = ScriptState::ForCurrentRealm(isolate_);
  v8::Local<v8::Value> next_method = next_method_.Get(script_state);
  if (!next_method->IsFunction()) {
    exception_state.ThrowTypeError("Expected next() function on iterator.");
    done_ = true;
    return false;
  }

  TryRethrowScope rethrow_scope(isolate_, exception_state);
  v8::Local<v8::Value> next_return_value;
  if (!V8ScriptRunner::CallFunction(
           next_method.As<v8::Function>(), execution_context,
           iterator_.Get(script_state), 0, nullptr, isolate_)
           .ToLocal(&next_return_value)) {
    done_ = true;
    return false;
  }
  if (!next_return_value->IsObject()) {
    exception_state.ThrowTypeError(
        "Expected iterator.next() to return an Object.");
    done_ = true;
    return false;
  }
  v8::Local<v8::Object> next_return_value_object =
      next_return_value.As<v8::Object>();

  v8::Local<v8::Context> context = script_state->GetContext();
  if (kind_ == Kind::kAsync) {
    value_ = WorldSafeV8Reference(isolate_, next_return_value);
    // Unlike synchronous iterators, in the async case, we don't know whether
    // the iteration is "done" yet, since `value_` is NOT expected to be
    // directly an `IteratorResult` object, but rather a Promise that resolves
    // to one. See [1]. In that case, we'll return true here since we have no
    // indication that the iterator is exhausted yet.
    //
    // [1]: https://tc39.es/ecma262/#table-async-iterator-required.
    return true;
  } else {
    v8::MaybeLocal<v8::Value> maybe_value =
        next_return_value_object->Get(context, value_key_);
    value_ = WorldSafeV8Reference(
        isolate_, maybe_value.FromMaybe(v8::Local<v8::Value>()));
    if (maybe_value.IsEmpty()) {
      done_ = true;
      return false;
    }

    v8::Local<v8::Value> done;
    if (!next_return_value_object->Get(context, done_key_).ToLocal(&done)) {
      done_ = true;
      return false;
    }
    done_ = done->BooleanValue(isolate_);
    return !done_;
  }
}

ScriptValue ScriptIterator::CloseSync(ScriptState* script_state,
                                      ExceptionState& exception_state,
                                      v8::Local<v8::Value> reason) {
  DCHECK_EQ(kind_, Kind::kSync);
  DCHECK(!IsNull());

  v8::Local<v8::Context> current_context = script_state->GetContext();
  v8::Local<v8::Object> iterator = iterator_.Get(script_state);

  TryRethrowScope rethrow_scope(isolate_, exception_state);

  // 7.4.9 IteratorClose().
  //
  // 3. Let innerResult be Completion(GetMethod(iterator, "return")).
  v8::Local<v8::Value> return_method;
  if (!iterator->Get(current_context, V8AtomicString(isolate_, "return"))
           .ToLocal(&return_method)) {
    DCHECK(rethrow_scope.HasCaught());
    // 6. If innerResult is a throw completion, return ? innerResult.
    return ScriptValue();
  }

  // 7.3.10 GetMethod(V, P):
  //
  // 3. If IsCallable(func) is false, throw a TypeError exception.
  if (!return_method->IsFunction()) {
    exception_state.ThrowTypeError("return() function must be callable.");
    return ScriptValue();
  }

  // 4. If innerResult is a normal completion, then
  //   a. Let return be innerResult.[[Value]].
  //   b. If return is undefined, return ? completion.
  if (return_method->IsNullOrUndefined()) {
    return ScriptValue();
  }

  // 4.c. Set innerResult to Completion(Call(return, iterator)).
  v8::Local<v8::Value> return_value;
  if (!V8ScriptRunner::CallFunction(return_method.As<v8::Function>(),
                                    ExecutionContext::From(script_state),
                                    iterator, reason.IsEmpty() ? 0 : 1, &reason,
                                    isolate_)
           .ToLocal(&return_value)) {
    DCHECK(rethrow_scope.HasCaught());
    // 6. If innerResult is a throw completion, return ? innerResult.
    return ScriptValue();
  }

  // If innerResult.[[Value]] is not an Object, throw a TypeError exception.
  if (!return_value->IsObject()) {
    exception_state.ThrowTypeError("Expected return() to return an Object.");
    return ScriptValue();
  }

  // 8. Return ? completion.
  return ScriptValue(isolate_, reason);
}

ScriptPromise<IDLAny> ScriptIterator::CloseAsync(
    ScriptState* script_state,
    const ExceptionContext& exception_context,
    v8::Local<v8::Value> reason) {
  DCHECK_EQ(kind_, Kind::kAsync);
  DCHECK(!IsNull());

  v8::Local<v8::Context> current_context = script_state->GetContext();
  v8::Local<v8::Object> iterator = iterator_.Get(script_state);

  // To close an async iterator<T> |iterator|, with reason |reason|:
  // https://whatpr.org/webidl/1397.html#async-iterator-close.
  v8::TryCatch try_catch(script_state->GetIsolate());

  // 3. Let returnMethod be GetMethod(iteratorObj, "return").
  v8::Local<v8::Value> return_method;
  if (!iterator->Get(current_context, V8AtomicString(isolate_, "return"))
           .ToLocal(&return_method)) {
    // 4. If returnMethod is an abrupt completion, return a promise rejected
    // with returnMethod.[[Value]].
    DCHECK(try_catch.HasCaught());
    ScriptPromise<IDLAny> rejected_promise =
        ScriptPromise<IDLAny>::Reject(script_state, try_catch.Exception());
    try_catch.Reset();
    return rejected_promise;
  }

  // 7.3.10 GetMethod(V, P):
  //
  // 3. If IsCallable(func) is false, throw a TypeError exception.
  if (!return_method->IsFunction()) {
    V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                      "return() function must be callable");
    ScriptPromise<IDLAny> rejected_promise =
        ScriptPromise<IDLAny>::Reject(script_state, try_catch.Exception());
    try_catch.Reset();
    return rejected_promise;
  }

  // 5. If returnMethod is undefined, return a promise resolved with
  //    undefined.
  if (return_method->IsNullOrUndefined()) {
    return EmptyPromise();
  }

  // 6. Let returnResult be
  //    Call(returnMethod.[[Value]], iteratorObj, « reason »).
  v8::Local<v8::Value> return_result;
  if (!V8ScriptRunner::CallFunction(return_method.As<v8::Function>(),
                                    ExecutionContext::From(script_state),
                                    iterator, reason.IsEmpty() ? 0 : 1, &reason,
                                    isolate_)
           .ToLocal(&return_result)) {
    // 7. If returnResult is an abrupt completion, return a promise rejected
    //    with returnResult.[[Value]].
    DCHECK(try_catch.HasCaught());
    ScriptPromise<IDLAny> rejected_promise =
        ScriptPromise<IDLAny>::Reject(script_state, try_catch.Exception());
    try_catch.Reset();
    return rejected_promise;
  }

  // 8. Let returnPromise be a promise resolved with returnResult.[[Value]].
  ScriptPromise<IDLAny> return_promise =
      ToResolvedPromise<IDLAny>(script_state, return_result);

  // 9. Return the result of reacting to returnPromise with the following
  //    fulfillment steps, given returnPromiseResult:
  //
  // (See documentation in `AsyncIteratorCloseFulfillFunction` for remaining
  // documentation).
  ScriptFunction* on_fulfilled = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<AsyncIteratorCloseFulfillFunction>(
                        exception_context));
  return return_promise.Then(on_fulfilled);
}

}  // namespace blink

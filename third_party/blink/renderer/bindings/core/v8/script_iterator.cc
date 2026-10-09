// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

namespace {

// 7.3.10 GetMethod(V, P).
// https://tc39.es/ecma262/#sec-getmethod
//
// Returns false, with an exception on `exception_state`, if the property
// getter throws or the value is neither callable nor nullish. Otherwise
// returns true, leaving `*method` empty when the value is null or undefined.
// `not_callable_message` is the text of the TypeError thrown in step 3.
bool GetMethod(v8::Isolate* isolate,
               v8::Local<v8::Context> context,
               v8::Local<v8::Object> object,
               v8::Local<v8::Value> key,
               const char* not_callable_message,
               v8::Local<v8::Function>* method,
               ExceptionState& exception_state) {
  TryRethrowScope rethrow_scope(isolate, exception_state);
  // 1. Let func be ? GetV(V, P).
  v8::Local<v8::Value> value;
  if (!object->Get(context, key).ToLocal(&value)) {
    CHECK(rethrow_scope.HasCaught());
    return false;
  }
  // 2. If func is either undefined or null, return undefined.
  if (value->IsNullOrUndefined()) {
    *method = v8::Local<v8::Function>();
    return true;
  }
  // 3. If IsCallable(func) is false, throw a TypeError exception.
  //
  // v8::Value::IsFunction() is V8's IsCallable().
  if (!value->IsFunction()) {
    exception_state.ThrowTypeError(not_callable_message);
    return false;
  }
  // 4. Return func.
  *method = value.As<v8::Function>();
  return true;
}

// 7.4.11 IteratorClose(iteratorRecord, completion), for a normal completion
// whose value is `reason`. Returns `reason` once return() has been called
// successfully, or an empty value when the iterator has no return() method,
// or when an exception was thrown and set on `exception_state`.
// https://tc39.es/ecma262/#sec-iteratorclose
ScriptValue IteratorClose(ScriptState* script_state,
                          v8::Local<v8::Object> iterator,
                          v8::Local<v8::Value> reason,
                          ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();

  // 3. Let innerResult be Completion(GetMethod(iterator, "return")).
  // 4. If innerResult is a normal completion, then
  //    a. Let return be innerResult.[[Value]].
  v8::Local<v8::Function> return_method;
  if (!GetMethod(isolate, script_state->GetContext(), iterator,
                 V8AtomicString(isolate, "return"),
                 "return() function must be callable.", &return_method,
                 exception_state)) {
    CHECK(exception_state.HadException());
    // 6. If innerResult is a throw completion, return ? innerResult.
    return ScriptValue();
  }
  //    b. If return is undefined, return ? completion.
  if (return_method.IsEmpty()) {
    return ScriptValue();
  }

  //    c. Set innerResult to Completion(Call(return, iterator)).
  TryRethrowScope rethrow_scope(isolate, exception_state);
  v8::Local<v8::Value> return_value;
  if (!V8ScriptRunner::CallFunction(
           return_method, ExecutionContext::From(script_state), iterator,
           reason.IsEmpty() ? 0 : 1, &reason, isolate)
           .ToLocal(&return_value)) {
    CHECK(rethrow_scope.HasCaught());
    // 6. If innerResult is a throw completion, return ? innerResult.
    return ScriptValue();
  }

  // 7. If innerResult.[[Value]] is not an Object, throw a TypeError exception.
  if (!return_value->IsObject()) {
    exception_state.ThrowTypeError("Expected return() to return an Object.");
    return ScriptValue();
  }

  // 8. Return ? completion.
  return ScriptValue(isolate, reason);
}

// IteratorClose(iteratorRecord, completion) for a throw completion. Its step
// 5, "If completion is a throw completion, return ? completion", runs before
// steps 6-7 look at innerResult, so whatever the return() lookup and call do
// is discarded; only the original completion survives, and the caller
// propagates it.
void IteratorCloseForThrowCompletion(ScriptState* script_state,
                                     v8::Local<v8::Object> iterator) {
  IteratorClose(script_state, iterator, v8::Local<v8::Value>(),
                IGNORE_EXCEPTION);
}

class AsyncIteratorCloseFulfillFunction final
    : public ThenCallable<IDLAny, AsyncIteratorCloseFulfillFunction> {
 public:
  explicit AsyncIteratorCloseFulfillFunction(
      const ExceptionContext& exception_context) {
    SetExceptionContext(exception_context);
  }

  void React(ScriptState* script_state, ScriptValue value) {
    // In a detached context, we shouldn't proceed to do things that can run
    // script.
    if (!script_state->ContextIsValid()) {
      return;
    }

    // 9.1. If Type(returnPromiseResult) is not Object, throw a TypeError.
    if (!value.V8Value()->IsObject()) {
      V8ThrowException::ThrowTypeError(
          script_state->GetIsolate(),
          "Expected return() to resolve to an Object.");
      return;
    }
    // 9.2. Return undefined.
  }
};

// The onFulfilled steps of AsyncFromSyncIteratorContinuation(): wrap the
// awaited value back into an iterator result object.
// https://tc39.es/ecma262/#sec-asyncfromsynciteratorcontinuation
class AsyncFromSyncIteratorFulfillFunction final : public ScriptFunction {
 public:
  explicit AsyncFromSyncIteratorFulfillFunction(bool done) : done_(done) {}

  void CallRaw(ScriptState* script_state,
               const v8::FunctionCallbackInfo<v8::Value>& info) override {
    // In a detached context, we shouldn't proceed to do things that can run
    // script.
    if (!script_state->ContextIsValid()) {
      return;
    }
    // 9. Let unwrap be a new Abstract Closure with parameters (value) that
    //    captures done and performs the following steps when called:
    //    a. Return CreateIteratorResultObject(value, done).
    info.GetReturnValue().Set(
        bindings::ESCreateIterResultObject(script_state, done_, info[0]));
  }

 private:
  const bool done_;
};

// The onRejected steps of AsyncFromSyncIteratorContinuation() when
// closeOnRejection is true: IteratorClose(syncIteratorRecord,
// ThrowCompletion(error)), i.e. close the sync iterator, then rethrow the
// reason. This is a plain ScriptFunction rather than a ThenCallable, which
// would rewrite the reason through ApplyContextToException(); the spec
// requires it to propagate unchanged.
// https://tc39.es/ecma262/#sec-asyncfromsynciteratorcontinuation
class AsyncFromSyncIteratorRejectFunction final : public ScriptFunction {
 public:
  AsyncFromSyncIteratorRejectFunction(v8::Isolate* isolate,
                                      v8::Local<v8::Object> sync_iterator)
      : sync_iterator_(isolate, sync_iterator) {}

  void CallRaw(ScriptState* script_state,
               const v8::FunctionCallbackInfo<v8::Value>& info) override {
    // 13.a. Let closeIterator be a new Abstract Closure with parameters
    //       (error) that captures syncIteratorRecord and performs the
    //       following steps when called:
    //       i. Return ? IteratorClose(syncIteratorRecord,
    //          ThrowCompletion(error)).
    //
    // In a detached context, we shouldn't proceed to do things that can run
    // script, but the rejection is propagated either way.
    if (script_state->ContextIsValid()) {
      IteratorCloseForThrowCompletion(script_state,
                                      sync_iterator_.Get(script_state));
    }
    V8ThrowException::ThrowException(script_state->GetIsolate(), info[0]);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(sync_iterator_);
    ScriptFunction::Trace(visitor);
  }

 private:
  WorldSafeV8Reference<v8::Object> sync_iterator_;
};

}  // namespace

// static
ScriptIterator ScriptIterator::FromIterable(v8::Isolate* isolate,
                                            v8::Local<v8::Object> iterable,
                                            Kind kind,
                                            ExceptionState& exception_state) {
  CHECK(kind == Kind::kSync || kind == Kind::kAsync);

  // 7.4.3 GetIterator(obj, kind).
  // https://tc39.es/ecma262/#sec-getiterator
  v8::Local<v8::Function> method;

  // 1. If kind is ASYNC, then
  //    a. Let method be ? GetMethod(obj, %Symbol.asyncIterator%).
  //    b. If method is undefined, then
  //       i. Let syncMethod be ? GetMethod(obj, %Symbol.iterator%).
  //       ii. If syncMethod is undefined, throw a TypeError exception.
  //       iii. Let syncIteratorRecord be ? GetIteratorFromMethod(obj,
  //            syncMethod).
  //       iv. Return CreateAsyncFromSyncIterator(syncIteratorRecord).
  //
  // The lookups are shared with Web IDL's async_sequence<T> conversion, which
  // sets `kind` to `kAsyncFromSync` when step 1.b applies.
  if (kind == Kind::kAsync) {
    if (!LookUpAsyncIterableMethod(isolate, iterable, &method, &kind,
                                   exception_state)) {
      CHECK(exception_state.HadException());
      return ScriptIterator();
    }
  } else {
    // 2. Else, let method be ? GetMethod(obj, @@iterator).
    if (!GetMethod(isolate, isolate->GetCurrentContext(), iterable,
                   v8::Symbol::GetIterator(isolate),
                   "@@iterator must be a callable.", &method,
                   exception_state)) {
      CHECK(exception_state.HadException());
      return ScriptIterator();
    }
  }

  // 3. If method is undefined, throw a TypeError exception.
  //
  // Deliberate deviation, here and in step 1.b.ii: leave it to the caller to
  // decide whether a non-iterable object should throw.
  if (method.IsEmpty()) {
    CHECK(!exception_state.HadException());
    return ScriptIterator();
  }

  // 4. Return ? GetIteratorFromMethod(obj, method).
  return FromIteratorMethod(isolate, iterable, method, kind, exception_state);
}

// static
ScriptIterator ScriptIterator::FromIteratorMethod(
    v8::Isolate* isolate,
    v8::Local<v8::Object> iterable,
    v8::Local<v8::Function> method,
    Kind kind,
    ExceptionState& exception_state) {
  CHECK_NE(kind, Kind::kNull);

  // 7.4.4 GetIteratorFromMethod(obj, method).
  // https://tc39.es/ecma262/#sec-getiteratorfrommethod
  TryRethrowScope rethrow_scope(isolate, exception_state);
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();

  // 1. Let iterator be ? Call(method, obj).
  v8::Local<v8::Value> iterator;
  if (!V8ScriptRunner::CallFunction(method, ToExecutionContext(current_context),
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
  //
  // For `kAsyncFromSync` this record is the syncIteratorRecord of
  // CreateAsyncFromSyncIterator(); the async behaviour is layered on top in
  // Next() and CloseAsync().
  return ScriptIterator(isolate, iterator.As<v8::Object>(), next_method, kind);
}

// static
bool ScriptIterator::LookUpAsyncIterableMethod(
    v8::Isolate* isolate,
    v8::Local<v8::Object> object,
    v8::Local<v8::Function>* method,
    Kind* kind,
    ExceptionState& exception_state) {
  // Converting a JavaScript value to an IDL async_sequence<T> value, steps
  // 2-4, minus the TypeError for the case where neither method exists:
  // https://webidl.spec.whatwg.org/#js-async-sequence
  //
  // 2. Let method be ? GetMethod(obj, %Symbol.asyncIterator%).
  if (!GetMethod(isolate, isolate->GetCurrentContext(), object,
                 v8::Symbol::GetAsyncIterator(isolate),
                 "@@asyncIterator must be a callable.", method,
                 exception_state)) {
    CHECK(exception_state.HadException());
    return false;
  }
  // 4. Return an IDL async sequence value with object set to V, method set to
  //    method, and type set to "async".
  if (!method->IsEmpty()) {
    *kind = Kind::kAsync;
    return true;
  }

  // 3. If method is undefined:
  //    1. Set syncMethod to ? GetMethod(obj, %Symbol.iterator%).
  if (!GetMethod(isolate, isolate->GetCurrentContext(), object,
                 v8::Symbol::GetIterator(isolate),
                 "@@iterator must be a callable.", method, exception_state)) {
    CHECK(exception_state.HadException());
    return false;
  }
  //    2. If syncMethod is undefined, throw a TypeError.
  //
  // Left to the caller, which may prefer another conversion; `*method` stays
  // empty.
  //
  //    3. Return an IDL async sequence value with object set to V, method
  //       set to syncMethod, and type set to "sync".
  if (!method->IsEmpty()) {
    *kind = Kind::kAsyncFromSync;
  }
  return true;
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
  } else if (kind_ == Kind::kAsyncFromSync) {
    // 27.1.5.2.1 %AsyncFromSyncIteratorPrototype%.next(), step 8, with the
    // object returned by next() above as |result|:
    // https://tc39.es/ecma262/#sec-%asyncfromsynciteratorprototype%.next
    //
    // 8. Return AsyncFromSyncIteratorContinuation(result, promiseCapability,
    //    syncIteratorRecord, true).
    //
    // 27.1.5.4 AsyncFromSyncIteratorContinuation(), steps 2-5:
    // https://tc39.es/ecma262/#sec-asyncfromsynciteratorcontinuation
    //
    // 2. Let done be Completion(IteratorComplete(result)).
    // 3. IfAbruptRejectPromise(done, promiseCapability).
    // 4. Let value be Completion(IteratorValue(result)).
    // 5. IfAbruptRejectPromise(value, promiseCapability).
    //
    // Where the spec rejects the promise, this reports the exception through
    // `exception_state` like every other failure of Next(); consumers turn
    // that into a rejected promise, which is observably the same.
    bool done = true;
    v8::Local<v8::Value> value;
    if (!bindings::ESUnpackIterResultObject(
            script_state, next_return_value_object, &done, &value)) {
      CHECK(rethrow_scope.HasCaught());
      done_ = true;
      return false;
    }
    // Steps 6-16 of the continuation:
    v8::Local<v8::Promise> promise;
    if (!AsyncFromSyncIteratorContinuation(script_state, done, value,
                                           /*close_on_rejection=*/true)
             .ToLocal(&promise)) {
      done_ = true;
      return false;
    }
    value_ = WorldSafeV8Reference<v8::Value>(isolate_, promise);
    // As in the `kAsync` case, the consumer learns about exhaustion from the
    // iterator result object that the Promise resolves to.
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
                                      v8::Local<v8::Value> reason,
                                      ExceptionState& exception_state) {
  DCHECK_EQ(kind_, Kind::kSync);
  DCHECK(!IsNull());
  return IteratorClose(script_state, iterator_.Get(script_state), reason,
                       exception_state);
}

ScriptPromise<IDLUndefined> ScriptIterator::CloseAsync(
    ScriptState* script_state,
    const ExceptionContext& exception_context,
    v8::Local<v8::Value> reason) {
  CHECK(kind_ == Kind::kAsync || kind_ == Kind::kAsyncFromSync);
  DCHECK(!IsNull());

  // To close an async iterator<T> |iterator|, with reason |reason|:
  // https://webidl.spec.whatwg.org/#async-iterator-close
  v8::TryCatch try_catch(isolate_);
  auto reject_with_caught_exception = [&]() {
    DCHECK(try_catch.HasCaught());
    return ScriptPromise<IDLUndefined>::Reject(
        script_state, TryRethrowScope::TakeException(try_catch));
  };

  v8::Local<v8::Object> iterator = iterator_.Get(script_state);

  // 3. Let returnMethod be GetMethod(iteratorObj, "return").
  // 4. If returnMethod is an abrupt completion, return a promise rejected
  //    with returnMethod.[[Value]].
  //
  // For `kAsyncFromSync`, iteratorObj would be the %AsyncFromSyncIterator-
  // Prototype% object of CreateAsyncFromSyncIterator(), whose built-in
  // return() always exists, so steps 3-5 cannot fail and step 6 calls that
  // built-in: 27.1.5.2.2 %AsyncFromSyncIteratorPrototype%.return(). Its steps
  // 5-10 look the sync iterator's return() up and call it exactly as steps
  // 3-7 here do on an async iterator, so both kinds share the code up to
  // |return_result| and part ways there.
  // https://tc39.es/ecma262/#sec-%asyncfromsynciteratorprototype%.return
  v8::Local<v8::Function> return_method;
  if (!GetMethod(isolate_, script_state->GetContext(), iterator,
                 V8AtomicString(isolate_, "return"),
                 "return() function must be callable", &return_method,
                 PassThroughException(isolate_))) {
    return reject_with_caught_exception();
  }

  // 5. If returnMethod is undefined, return a promise resolved with
  //    undefined.
  //
  // For `kAsyncFromSync`, .return() step 8 resolves with
  // CreateIteratorResultObject(value, true) instead, which step 9 below would
  // turn into undefined as well.
  if (return_method.IsEmpty()) {
    return ToResolvedUndefinedPromise(script_state);
  }

  // 6. Let returnResult be
  //    Call(returnMethod.[[Value]], iteratorObj, « reason »).
  // 7. If returnResult is an abrupt completion, return a promise rejected
  //    with returnResult.[[Value]].
  v8::Local<v8::Value> return_result;
  if (!V8ScriptRunner::CallFunction(
           return_method, ExecutionContext::From(script_state), iterator,
           reason.IsEmpty() ? 0 : 1, &reason, isolate_)
           .ToLocal(&return_result)) {
    return reject_with_caught_exception();
  }

  if (kind_ == Kind::kAsyncFromSync) {
    // 27.1.5.2.2 %AsyncFromSyncIteratorPrototype%.return(), steps 11-12:
    //
    // 11. If result is not an Object, then
    //     a. Perform ! Call(promiseCapability.[[Reject]], undefined, « a
    //        newly created TypeError object »).
    //     b. Return promiseCapability.[[Promise]].
    if (!return_result->IsObject()) {
      return ScriptPromise<IDLUndefined>::Reject(
          script_state,
          V8ThrowException::CreateTypeError(
              isolate_, "Expected return() to return an Object."));
    }
    // 12. Return AsyncFromSyncIteratorContinuation(result, promiseCapability,
    //     syncIteratorRecord, false).
    //
    // Its steps 2-5 read the sync result (see Next()); the rest produces the
    // Promise that the built-in return() returns, which is the |returnResult|
    // the Web IDL steps below react to.
    bool done = true;
    v8::Local<v8::Value> value;
    if (!bindings::ESUnpackIterResultObject(
            script_state, return_result.As<v8::Object>(), &done, &value)) {
      return reject_with_caught_exception();
    }
    if (!AsyncFromSyncIteratorContinuation(script_state, done, value,
                                           /*close_on_rejection=*/false)
             .ToLocal(&return_result)) {
      // Script execution is being terminated.
      return ScriptPromise<IDLUndefined>();
    }
  }

  // 8. Let returnPromise be a promise resolved with returnResult.[[Value]].
  ScriptPromise<IDLAny> return_promise =
      ToResolvedPromise<IDLAny>(script_state, return_result);

  // 9. Return the result of reacting to returnPromise with the following
  //    fulfillment steps, given returnPromiseResult:
  //
  // (See documentation in `AsyncIteratorCloseFulfillFunction` for remaining
  // documentation).
  auto* on_fulfilled = MakeGarbageCollected<AsyncIteratorCloseFulfillFunction>(
      exception_context);
  return return_promise.Then(script_state, on_fulfilled);
}

v8::MaybeLocal<v8::Promise> ScriptIterator::AsyncFromSyncIteratorContinuation(
    ScriptState* script_state,
    bool done,
    v8::Local<v8::Value> value,
    bool close_on_rejection) {
  CHECK_EQ(kind_, Kind::kAsyncFromSync);
  // 27.1.5.4 AsyncFromSyncIteratorContinuation(), from step 6; steps 2-5,
  // which read |result|, run in the caller:
  // https://tc39.es/ecma262/#sec-asyncfromsynciteratorcontinuation
  //
  // 6. Let valueWrapper be Completion(PromiseResolve(%Promise%, value)).
  //
  // Steps 7 and 8 do not apply: Blink's promise wrapping never reads the
  // `constructor` getter, the only way PromiseResolve() can throw.
  ScriptPromise<IDLAny> value_wrapper =
      ToResolvedPromise<IDLAny>(script_state, value);

  // 9-10. Let onFulfilled be CreateBuiltinFunction(unwrap, 1, "", « »), where
  //       unwrap returns CreateIteratorResultObject(value, done).
  auto* on_fulfilled =
      MakeGarbageCollected<AsyncFromSyncIteratorFulfillFunction>(done);
  v8::Local<v8::Function> on_fulfilled_function =
      on_fulfilled->ToV8Function(script_state);

  v8::Local<v8::Context> context = script_state->GetContext();
  v8::MaybeLocal<v8::Promise> result;
  if (done || !close_on_rejection) {
    // 12. If done is true or closeOnRejection is false, then
    //     a. Let onRejected be undefined.
    result = value_wrapper.V8Promise()->Then(context, on_fulfilled_function);
  } else {
    // 13. Else,
    //     a-b. Let onRejected be a function that closes the sync iterator and
    //          rethrows its argument.
    auto* on_rejected =
        MakeGarbageCollected<AsyncFromSyncIteratorRejectFunction>(
            isolate_, iterator_.Get(script_state));
    result = value_wrapper.V8Promise()->Then(
        context, on_fulfilled_function,
        on_rejected->ToV8Function(script_state));
  }
  // 15. Perform PerformPromiseThen(valueWrapper, onFulfilled, onRejected,
  //     promiseCapability).
  // 16. Return promiseCapability.[[Promise]].
  //
  // v8::Promise::Then() only fails when script execution is being terminated.
  return result;
}

}  // namespace blink

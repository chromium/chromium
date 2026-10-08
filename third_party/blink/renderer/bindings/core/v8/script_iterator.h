// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_ITERATOR_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionContext;
class ExceptionState;
class ExecutionContext;
template <typename IDLResolvedType>
class ScriptPromise;

// This class provides a wrapper for iterating over any ES object that
// implements either the async iterable and async iterator protocols, or the
// iterable and iterator protocols. Namely:
// * The object or an object in its prototype chain has an @@asyncIterator or
//   @@iterator property that is a function that returns an Iterator Record [1].
// * The Iterator Record has a next() method that returns either:
//   1. Async iterable case: An object (which should be a Promise, although that
//      isn't enforced [2])
//   2. Sync iterable case: An object with at least two properties:
//      a. done: A boolean indicating whether iteration should stop. Can be
//      omitted when false.
//      b. value: Any object. Can be omitted when `done` is true.
//
// This class resembles ECMAScript's `GetIterator(obj, kind)` [3] abstract
// operation, whose `kind` argument is either ASYNC or SYNC, directing the
// operation as to which iterator type to try and obtain from the ES object.
//
// Web IDL's async_sequence<T> type [4] looks the iteration method up when a
// value is converted and only calls it later, when the sequence is "opened";
// see `LookUpAsyncIterableMethod()` and `FromIteratorMethod()` for that split,
// and `Kind::kAsyncFromSync` for consuming an @@iterator-only object
// asynchronously.
//
// [1]: https://tc39.es/ecma262/#sec-iterator-records
// [2]: https://tc39.es/ecma262/#table-async-iterator-required
// [3]: https://tc39.es/ecma262/#sec-getiterator
// [4]: https://webidl.spec.whatwg.org/#idl-async-iterable-type
//
//
// Async iterable usage:
//   class SubscriptionManager {
//    public:
//     SubscriptionManager(v8::Local<v8::Object> obj) {
//       ExceptionState exception_state = ...;
//       iterator_ = ScriptIterator::FromIterable(
//           script_state->GetIsolate(), obj, exception_state,
//           ScriptIterator::Kind::kAsync);
//
//       if (exception_state.HadException()) {
//         return;
//       }
//
//       // When `iterator_.IsNull()` is true but no exception is on the stack,
//       // then `obj` is neither async iterable nor iterable.
//       if (iterator_.IsNull()) {
//         DCHECK(!exception_state.HadException());
//         return;
//       }
//
//       GetNextValue();
//     }
//
//     // Run repeatedly after every async value resolves, to fetch the next
//     // one.
//     void GetNextValue() {
//       DCHECK(!iterator_.IsNull());
//       v8::Isolate* isolate = script_state->GetIsolate();
//       v8::TryCatch try_catch(sisolate);
//       ExecutionContext* execution_context = ...;
//
//       iterator_.Next(execution_context, PassThroughException();
//
//       if (try_catch.HasCaught()) {
//         next_promise_ = ScriptPromise<IDLAny>::Reject(
//             script_state, try_catch.Exception());
//       } else {
//         next_promise_ = ToResolvedPromise<IDLAny>(
//             script_state, iterator_.GetValue().ToLocalChecked());
//       }
//
//       // `on_fulfilled` fulfills to an Iterator Result, and calls
//       // `GetNextValue()` again if the result is not done.
//       ThenCallableDerived* on_fulfilled = ...;
//       ThenCallableDerived* on_rejected = ...;
//       next_promise_.Then(script_State, on_fulfilled, on_rejected);
//     }
//
//    private:
//     ScriptIterator iterator_;
//     ScriptPromise<IDLAny> next_promise_;
//   };
//
// Sync iterable usage:
//   v8::Local<v8::Object> es_object = ...;
//   auto script_iterator = ScriptIterator::FromIterable(
//       isolate, es_object, exception_state, ScriptIterable::Kind::kSync);
//   if (exception_state.HadException()) {
//     return;
//   }
//   if (!script_iterator.IsNull()) {
//     while (script_iterator.Next(execution_context, exception_state)) {
//       // When `Next()` puts an exception on the stack, it always returns
//       // false, thus breaking out of this loop.
//       DCHECK(!exception_state.HadException());
//       v8::Local<v8::Value> value =
//           script_iterator.GetValue().ToLocalChecked();
//       // Do something with `value`.
//     }
//   }
//
//   // See documentation above.
//   if (exception_state.HadException()) {
//     return;
//   }
class CORE_EXPORT ScriptIterator {
  DISALLOW_NEW();

 public:
  enum class Kind {
    // `kNull` is not a real kind per se; it is just the default for
    // `ScriptIterator`s whose `IsNull()` returns true.
    kNull = 0,
    kSync = 1,
    kAsync = 2,
    // A sync iterator driven through the async protocol. ECMAScript's
    // CreateAsyncFromSyncIterator() models this with a wrapper object whose
    // next() and return() delegate to the sync iterator; here the
    // ScriptIterator itself stands in for that object, holding the sync
    // iterator record while Next() and CloseAsync() implement the wrapper's
    // next() and return(): Next() yields (via GetValue()) a Promise for an
    // iterator result object whose value has been awaited, and CloseAsync()
    // forwards to the sync iterator's return(). Consumers drive this kind
    // exactly like `kAsync`. GetIterator(obj, ASYNC) and Web IDL's
    // async_sequence<T> conversion produce it when an object has an @@iterator
    // method but no @@asyncIterator method.
    // https://tc39.es/ecma262/#sec-createasyncfromsynciterator
    kAsyncFromSync = 3,
  };

  // Creates a ScriptIterator out of an ES object that implements the iterable
  // and iterator protocols, following GetIterator(obj, kind). With `kAsync`,
  // an object that only has an @@iterator method is iterated through the
  // async protocol, as CreateAsyncFromSyncIterator() specifies; the returned
  // iterator then has `Kind::kAsyncFromSync`, which consumers drive exactly
  // like `kAsync`.
  // Both the return value and the ExceptionState should be checked:
  // - The ExceptionState will contain an exception if V8 throws one, or if the
  //   ES objects do not conform to the expected protocols. In this case, the
  //   returned ScriptIterator will be null.
  // - ScriptIterator can be null even if there is no exception. In this case,
  //   it indicates that the given ES object has no @@iterator method (nor,
  //   with `kAsync`, an @@asyncIterator method).
  // TODO(crbug.com/356891478): the 'exception_state' output argument must be
  // declared after the last input argument.
  // https://google.github.io/styleguide/cppguide.html#Inputs_and_Outputs
  static ScriptIterator FromIterable(v8::Isolate* isolate,
                                     v8::Local<v8::Object> iterable,
                                     ExceptionState& exception_state,
                                     ScriptIterator::Kind kind);

  // Implements GetIteratorFromMethod(obj, method):
  // https://tc39.es/ecma262/#sec-getiteratorfrommethod
  //
  // `method` is called on `iterable`, and the result must be an object;
  // otherwise a TypeError is thrown and a null ScriptIterator is returned.
  // `kind` says how the resulting iterator is driven: pass `kAsyncFromSync`
  // when `method` is an @@iterator method whose iterator must be consumed
  // asynchronously.
  // TODO(crbug.com/356891478): the 'exception_state' output argument must be
  // declared after the last input argument.
  // https://google.github.io/styleguide/cppguide.html#Inputs_and_Outputs
  static ScriptIterator FromIteratorMethod(v8::Isolate* isolate,
                                           v8::Local<v8::Object> iterable,
                                           v8::Local<v8::Function> method,
                                           ExceptionState& exception_state,
                                           ScriptIterator::Kind kind);

  // GetMethod(obj, @@asyncIterator), then, if that is undefined,
  // GetMethod(obj, @@iterator): the lookups of Web IDL's async_sequence<T>
  // conversion, and of GetIterator(obj, ASYNC).
  // https://webidl.spec.whatwg.org/#js-async-sequence
  //
  // Returns false, with an exception on `exception_state`, if a property
  // getter throws or a present method is not callable. Otherwise returns true
  // and sets `*method` and `*kind` (`kAsync` or `kAsyncFromSync`); `*method`
  // is left empty, and `*kind` untouched, when neither method exists, so that
  // callers can choose between throwing a TypeError and trying another
  // conversion.
  // TODO(crbug.com/356891478): the 'exception_state' output argument must be
  // declared after the last input argument.
  // https://google.github.io/styleguide/cppguide.html#Inputs_and_Outputs
  static bool LookUpAsyncIterableMethod(v8::Isolate* isolate,
                                        v8::Local<v8::Object> object,
                                        ExceptionState& exception_state,
                                        v8::Local<v8::Function>* method,
                                        ScriptIterator::Kind* kind);

  // Returns a `ScriptIterator` whose `IsNull()` is true. This is only needed
  // when storing a bare `ScriptIterator` in a class, which is useful in the
  // async iterator case, when you need to reference `this` asynchronously after
  // its creation, to get subsequent values as they are emitted.
  ScriptIterator() = default;

  ScriptIterator(ScriptIterator&&) noexcept = default;
  ScriptIterator& operator=(ScriptIterator&&) noexcept = default;

  ScriptIterator(const ScriptIterator&) = delete;
  ScriptIterator& operator=(const ScriptIterator&) = delete;

  bool IsNull() const { return iterator_.IsEmpty(); }

  // Returns true if the iterator is still not done.
  bool Next(ExecutionContext* execution_context,
            ExceptionState& exception_state);

  // These methods implement:
  //  - https://tc39.es/ecma262/#sec-iteratorclose (CloseSync)
  //  - https://webidl.spec.whatwg.org/#async-iterator-close (CloseAsync)
  //
  // They should be called when the consumer of an iterator needs to signal to
  // the iterator that it will stop consuming values before the iterator is
  // exhausted. Specifically, they call the `return()` method on the underlying
  // `iterator_`.
  //
  //   1. `CloseSync()` requires `kind_` to be `kSync`. It returns `reason`,
  //      per the ECMAScript Standard, and throws an error if `return()` throws
  //      or fails to return an Object.
  //   2. `CloseAsync()` requires `kind_` to be `kAsync` or `kAsyncFromSync`.
  //      Any errors are swallowed on the stack and returned in the form of a
  //      rejected Promise. It returns a Promise that resolves to undefined,
  //      also when there is no `return()` method, unless `return()` throws, is
  //      not callable, or fails to (eventually) produce an Object, in which
  //      case the returned Promise is rejected. For `kAsyncFromSync` the sync
  //      iterator's `return()` is called and its result's value is awaited, as
  //      %AsyncFromSyncIteratorPrototype%.return does.
  // TODO(crbug.com/356891478): the 'exception_state' output argument must be
  // declared after the last input argument.
  // https://google.github.io/styleguide/cppguide.html#Inputs_and_Outputs
  ScriptValue CloseSync(ScriptState* script_state,
                        ExceptionState& exception_state,
                        v8::Local<v8::Value> reason);
  ScriptPromise<IDLUndefined> CloseAsync(
      ScriptState* script_state,
      const ExceptionContext& exception_context,
      v8::Local<v8::Value> reason = v8::Local<v8::Value>());

  v8::MaybeLocal<v8::Value> GetValue() {
    return value_.Get(ScriptState::ForCurrentRealm(isolate_));
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(iterator_);
    visitor->Trace(next_method_);
    visitor->Trace(value_);
  }

 private:
  // Constructs a ScriptIterator from an ES object that implements the iterator
  // protocol: |iterator| is supposed to have a next() method that returns an
  // object with two properties, "done" and "value".
  ScriptIterator(v8::Isolate*,
                 v8::Local<v8::Object> iterator,
                 v8::Local<v8::Value> next_method,
                 Kind kind);

  // Steps 6-16 of AsyncFromSyncIteratorContinuation(result,
  // promiseCapability, syncIteratorRecord, closeOnRejection) for the
  // `kAsyncFromSync` kind, given the `done` and `value` that its steps 2-5
  // read from the sync iterator result:
  // https://tc39.es/ecma262/#sec-asyncfromsynciteratorcontinuation
  //
  // Returns a Promise for an iterator result object whose value is the awaited
  // `value`. If `value` rejects and `close_on_rejection` is true, the sync
  // iterator is closed before the returned Promise is rejected. Returns an
  // empty MaybeLocal only if script execution is being terminated.
  v8::MaybeLocal<v8::Promise> AsyncFromSyncIteratorContinuation(
      ScriptState* script_state,
      bool done,
      v8::Local<v8::Value> value,
      bool close_on_rejection);

  v8::Isolate* isolate_ = nullptr;
  WorldSafeV8Reference<v8::Object> iterator_;
  WorldSafeV8Reference<v8::Value> next_method_;
  v8::Local<v8::String> done_key_;
  v8::Local<v8::String> value_key_;
  bool done_ = true;
  WorldSafeV8Reference<v8::Value> value_;
  Kind kind_ = Kind::kNull;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_ITERATOR_H_

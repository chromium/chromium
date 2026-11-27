// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ASYNC_SEQUENCE_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ASYNC_SEQUENCE_ITERATOR_H_

#include <utility>

#include "base/check.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/async_sequence.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// Reads the values of an IDL async_sequence<T>, one at a time.
//
// The values arrive through a `Client` rather than a returned promise. A
// ScriptPromise<> resolves with a value of a single IDL type, and no IDL type
// can express "a T or the end of iteration" without ambiguity. A Client is
// also more efficient: it receives each element converted to T once, rather
// than converted back to JavaScript and then to T again.
// Each Next() call invokes exactly one Client method, always asynchronously,
// unless the context is detached first.
// https://webidl.spec.whatwg.org/#js-async-iterable
template <typename T>
class AsyncSequenceIterator final
    : public GarbageCollected<AsyncSequenceIterator<T>> {
 public:
  using ValueType = typename NativeValueTraits<T>::ImplType;
  using PassKey = base::PassKey<AsyncSequenceIterator<T>>;

  // Receives the outcome of one Next() call: a value, the end of the sequence
  // or a failure. It is to Next() what a ReadRequest is to a stream read: the
  // request's chunk, close and error steps are the three methods below.
  class Client : public GarbageCollectedMixin {
   public:
    virtual void OnNextValue(ScriptState* script_state, ValueType value) = 0;
    virtual void OnEndOfIteration(ScriptState* script_state) = 0;
    // next() rejected, threw, returned a non-object, or its value could not be
    // converted to T.
    virtual void OnNextRejected(ScriptState* script_state,
                                ScriptValue reason) = 0;
  };

  static AsyncSequenceIterator* Open(ScriptState* script_state,
                                     const AsyncSequence<T>& async_sequence,
                                     ExceptionState& exception_state) {
    ScriptIterator iterator =
        async_sequence.Open(script_state, exception_state);
    if (iterator.IsNull()) {
      DCHECK(exception_state.HadException());
      return nullptr;
    }
    return MakeGarbageCollected<AsyncSequenceIterator>(PassKey(),
                                                       std::move(iterator));
  }

  AsyncSequenceIterator(PassKey, ScriptIterator iterator)
      : iterator_(std::move(iterator)) {}

  // https://webidl.spec.whatwg.org/#async-iterator-get-next-value
  // The client is passed per call, as a ReadRequest is passed per read.
  void Next(ScriptState* script_state, Client* client) {
    v8::Isolate* isolate = script_state->GetIsolate();

    // 1. Let nextResult be IteratorNext(iterator's underlying record).
    v8::TryCatch try_catch(isolate);
    ScriptPromise<IDLAny> next_promise;
    if (!iterator_.Next(ExecutionContext::From(script_state),
                        PassThroughException(isolate))) {
      // 2. If nextResult is an abrupt completion, return a promise rejected
      //    with nextResult.[[Value]].
      CHECK(try_catch.HasCaught());
      next_promise = ScriptPromise<IDLAny>::Reject(
          script_state, TryRethrowScope::TakeException(try_catch));
    } else {
      // 3. Let nextPromise be a promise resolved with nextResult.[[Value]].
      //    The value is usually a pending promise, which is used as is.
      next_promise = ToResolvedPromise<IDLAny>(
          script_state, iterator_.GetValue().ToLocalChecked());
    }

    // 4. Return the result of reacting to nextPromise with the following
    //    fulfillment steps, given iterResult: (NextReaction::Fulfilled())
    // V8 keeps the reactions, and with them the client, alive for as long as
    // the promise can still settle, so nothing here needs to own them.
    next_promise.Then(
        script_state,
        MakeGarbageCollected<NextReaction<Outcome::kFulfilled>>(client),
        MakeGarbageCollected<NextReaction<Outcome::kRejected>>(client));
  }

  // https://webidl.spec.whatwg.org/#async-iterator-close
  // `exception_context` names the caller's operation in the message of a
  // rejection caused by return().
  ScriptPromise<IDLUndefined> Close(
      ScriptState* script_state,
      const ExceptionContext& exception_context,
      v8::Local<v8::Value> reason = v8::Local<v8::Value>()) {
    return iterator_.CloseAsync(script_state, exception_context, reason);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(iterator_); }

 private:
  enum class Outcome { kFulfilled, kRejected };

  template <Outcome outcome>
  class NextReaction final
      : public ThenCallable<IDLAny, NextReaction<outcome>> {
   public:
    explicit NextReaction(Client* client) : client_(client) {}

    void React(ScriptState* script_state, ScriptValue value) {
      // In a detached context, we shouldn't proceed to do things that can run
      // script.
      if (!script_state->ContextIsValid()) {
        return;
      }
      if constexpr (outcome == Outcome::kRejected) {
        client_->OnNextRejected(script_state, value);
      } else {
        Fulfilled(script_state, value);
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(client_);
      ThenCallable<IDLAny, NextReaction>::Trace(visitor);
    }

   private:
    // 4. The fulfillment steps, given iterResult.
    void Fulfilled(ScriptState* script_state, ScriptValue value) {
      v8::Isolate* isolate = script_state->GetIsolate();
      auto reject = [&](v8::Local<v8::Value> reason) {
        client_->OnNextRejected(script_state, ScriptValue(isolate, reason));
      };

      // 4.1. If iterResult is not an Object, throw a TypeError.
      if (!value.V8Value()->IsObject()) {
        reject(V8ThrowException::CreateTypeError(
            isolate, "Expected next() to resolve to an Object."));
        return;
      }

      // 4.2. Let done be ? IteratorComplete(iterResult).
      // 4.4.1. Let V be ? IteratorValue(iterResult).
      v8::TryCatch try_catch(isolate);
      bool done = true;
      v8::Local<v8::Value> next_value;
      if (!bindings::ESUnpackIterResultObject(script_state,
                                              value.V8Value().As<v8::Object>(),
                                              &done, &next_value)) {
        reject(TryRethrowScope::TakeException(try_catch));
        return;
      }

      // 4.3. If done is true, return end of iteration.
      if (done) {
        client_->OnEndOfIteration(script_state);
        return;
      }

      // 4.4.2. Let value be the result of converting V to an IDL value of type
      //        iterator's type parameter.
      auto&& converted = NativeValueTraits<T>::NativeValue(
          isolate, next_value, PassThroughException(isolate));
      if (try_catch.HasCaught()) {
        reject(TryRethrowScope::TakeException(try_catch));
        return;
      }

      // 4.4.3. Return value.
      client_->OnNextValue(script_state, std::move(converted));
    }

    Member<Client> client_;
  };

  ScriptIterator iterator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_ASYNC_SEQUENCE_ITERATOR_H_

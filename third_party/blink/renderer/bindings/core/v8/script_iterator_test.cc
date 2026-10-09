// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class ScriptIteratorTest : public testing::Test {
 protected:
  // Compiles and runs `source`, which must evaluate to an object.
  v8::Local<v8::Object> EvalObject(V8TestingScope& scope, const char* source) {
    v8::Local<v8::Script> script =
        v8::Script::Compile(scope.GetContext(),
                            V8String(scope.GetIsolate(), source))
            .ToLocalChecked();
    v8::Local<v8::Value> result =
        script->Run(scope.GetContext()).ToLocalChecked();
    CHECK(result->IsObject());
    return result.As<v8::Object>();
  }

  v8::Local<v8::Value> GlobalValue(V8TestingScope& scope, const char* name) {
    return scope.GetContext()
        ->Global()
        ->Get(scope.GetContext(), V8String(scope.GetIsolate(), name))
        .ToLocalChecked();
  }

  int32_t ReadGlobalInt(V8TestingScope& scope, const char* name) {
    return GlobalValue(scope, name)->Int32Value(scope.GetContext()).FromJust();
  }

  String ToString(const ScriptValue& value) {
    String result;
    CHECK(value.ToString(result));
    return result;
  }

  ExceptionContext TestExceptionContext() {
    return ExceptionContext(v8::ExceptionContext::kOperation, "Test",
                            "iterate");
  }

  struct LookUp {
    bool ok = false;
    v8::Local<v8::Function> method;
    ScriptIterator::Kind kind = ScriptIterator::Kind::kNull;
  };

  LookUp LookUpMethod(V8TestingScope& scope,
                      v8::Local<v8::Object> object,
                      ExceptionState& exception_state) {
    LookUp result;
    result.ok = ScriptIterator::LookUpAsyncIterableMethod(
        scope.GetIsolate(), object, &result.method, &result.kind,
        exception_state);
    return result;
  }

  // Opens `iterable` the way Web IDL's async_sequence<T> does: look the method
  // up, then call it.
  ScriptIterator OpenAsAsyncSequence(V8TestingScope& scope,
                                     v8::Local<v8::Object> iterable,
                                     ExceptionState& exception_state) {
    LookUp look_up = LookUpMethod(scope, iterable, exception_state);
    if (!look_up.ok) {
      return ScriptIterator();
    }
    CHECK(!look_up.method.IsEmpty());
    return ScriptIterator::FromIteratorMethod(scope.GetIsolate(), iterable,
                                              look_up.method, look_up.kind,
                                              exception_state);
  }

  ScriptIterator OpenSync(V8TestingScope& scope,
                          v8::Local<v8::Object> iterable,
                          ExceptionState& exception_state) {
    return ScriptIterator::FromIterable(scope.GetIsolate(), iterable,
                                        ScriptIterator::Kind::kSync,
                                        exception_state);
  }

  // Calls Next() on an async-style iterator and returns the Promise it
  // produced, settled.
  ScriptValue SettleNext(V8TestingScope& scope,
                         ScriptIterator& iterator,
                         bool* fulfilled) {
    DummyExceptionStateForTesting exception_state;
    CHECK(iterator.Next(scope.GetExecutionContext(), exception_state));
    CHECK(!exception_state.HadException());
    v8::Local<v8::Value> value = iterator.GetValue().ToLocalChecked();
    CHECK(value->IsPromise());
    ScriptPromiseTester tester(
        scope.GetScriptState(),
        ScriptPromise<IDLAny>::FromV8Promise(scope.GetIsolate(),
                                             value.As<v8::Promise>()));
    tester.WaitUntilSettled();
    *fulfilled = tester.IsFulfilled();
    return tester.Value();
  }

  // Opens `iterable` and returns the settled result of CloseAsync(reason).
  ScriptValue SettleClose(
      V8TestingScope& scope,
      v8::Local<v8::Object> iterable,
      bool* fulfilled,
      v8::Local<v8::Value> reason = v8::Local<v8::Value>()) {
    DummyExceptionStateForTesting exception_state;
    ScriptIterator iterator =
        OpenAsAsyncSequence(scope, iterable, exception_state);
    CHECK(!iterator.IsNull());
    ScriptPromiseTester tester(
        scope.GetScriptState(),
        iterator.CloseAsync(scope.GetScriptState(), TestExceptionContext(),
                            reason));
    tester.WaitUntilSettled();
    *fulfilled = tester.IsFulfilled();
    return tester.Value();
  }

  // Reads `done` and `value` from an iterator result object, returning the
  // value as a string.
  String ReadIterResult(V8TestingScope& scope,
                        const ScriptValue& result,
                        bool* done) {
    CHECK(result.V8Value()->IsObject());
    v8::Local<v8::Value> value;
    CHECK(bindings::ESUnpackIterResultObject(scope.GetScriptState(),
                                             result.V8Value().As<v8::Object>(),
                                             done, &value));
    return ToString(ScriptValue(scope.GetIsolate(), value));
  }

  // Asserts that a test script's `return()` recorded exactly `reason` in
  // `globalThis.returnArgs`.
  void ExpectReturnCalledWith(V8TestingScope& scope,
                              v8::Local<v8::Value> reason) {
    v8::Local<v8::Object> args = EvalObject(scope, "globalThis.returnArgs");
    ASSERT_TRUE(args->IsArray());
    EXPECT_EQ(1u, args.As<v8::Array>()->Length());
    EXPECT_TRUE(args.As<v8::Array>()
                    ->Get(scope.GetContext(), 0)
                    .ToLocalChecked()
                    ->StrictEquals(reason));
  }

  test::TaskEnvironment task_environment_;
};

TEST_F(ScriptIteratorTest, LookUpPrefersAsyncIteratorOverIterator) {
  V8TestingScope scope;
  v8::Local<v8::Object> object = EvalObject(scope, R"JS(
      globalThis.asyncMethod = function() {};
      ({ [Symbol.asyncIterator]: globalThis.asyncMethod,
         [Symbol.iterator]() { throw new Error("must not be read"); } }))JS");

  DummyExceptionStateForTesting exception_state;
  LookUp look_up = LookUpMethod(scope, object, exception_state);
  ASSERT_TRUE(look_up.ok);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(ScriptIterator::Kind::kAsync, look_up.kind);
  ASSERT_FALSE(look_up.method.IsEmpty());
  EXPECT_TRUE(look_up.method->StrictEquals(GlobalValue(scope, "asyncMethod")));
}

TEST_F(ScriptIteratorTest, LookUpFallsBackToIterator) {
  V8TestingScope scope;
  v8::Local<v8::Object> array = EvalObject(scope, "[1, 2, 3]");

  DummyExceptionStateForTesting exception_state;
  LookUp look_up = LookUpMethod(scope, array, exception_state);
  ASSERT_TRUE(look_up.ok);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(ScriptIterator::Kind::kAsyncFromSync, look_up.kind);
  EXPECT_FALSE(look_up.method.IsEmpty());
}

TEST_F(ScriptIteratorTest, LookUpTreatsNullAsyncIteratorAsAbsent) {
  V8TestingScope scope;
  v8::Local<v8::Object> object = EvalObject(
      scope, "({ [Symbol.asyncIterator]: null, [Symbol.iterator]() {} })");

  DummyExceptionStateForTesting exception_state;
  LookUp look_up = LookUpMethod(scope, object, exception_state);
  ASSERT_TRUE(look_up.ok);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(ScriptIterator::Kind::kAsyncFromSync, look_up.kind);
  EXPECT_FALSE(look_up.method.IsEmpty());
}

TEST_F(ScriptIteratorTest, LookUpReportsNoMethodWithoutThrowing) {
  V8TestingScope scope;
  v8::Local<v8::Object> object = EvalObject(scope, "({})");

  DummyExceptionStateForTesting exception_state;
  LookUp look_up = LookUpMethod(scope, object, exception_state);
  ASSERT_TRUE(look_up.ok);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(look_up.method.IsEmpty());
  EXPECT_EQ(ScriptIterator::Kind::kNull, look_up.kind);
}

TEST_F(ScriptIteratorTest, LookUpThrowsForNonCallableMethods) {
  V8TestingScope scope;
  for (const char* source :
       {"({ [Symbol.asyncIterator]: 42 })", "({ [Symbol.iterator]: 42 })"}) {
    SCOPED_TRACE(source);
    DummyExceptionStateForTesting exception_state;
    LookUp look_up =
        LookUpMethod(scope, EvalObject(scope, source), exception_state);
    EXPECT_FALSE(look_up.ok);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  }
}

TEST_F(ScriptIteratorTest, LookUpRethrowsFromGetters) {
  V8TestingScope scope;
  v8::Local<v8::Object> object = EvalObject(scope, R"JS(
      globalThis.theError = new Error('boom');
      ({ get [Symbol.asyncIterator]() { throw globalThis.theError; } }))JS");

  // Rethrown user exceptions are not recorded as a message; observe them
  // through V8.
  v8::TryCatch try_catch(scope.GetIsolate());
  EXPECT_FALSE(
      LookUpMethod(scope, object, PassThroughException(scope.GetIsolate())).ok);
  ASSERT_TRUE(try_catch.HasCaught());
  // The user's exception is rethrown untouched.
  EXPECT_TRUE(
      try_catch.Exception()->StrictEquals(GlobalValue(scope, "theError")));
}

TEST_F(ScriptIteratorTest, FromIteratorMethodRequiresAnObjectIterator) {
  V8TestingScope scope;
  v8::Local<v8::Object> object =
      EvalObject(scope, "({ [Symbol.iterator]() { return 42; } })");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = OpenAsAsyncSequence(scope, object, exception_state);
  EXPECT_TRUE(iterator.IsNull());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
}

TEST_F(ScriptIteratorTest, FromIteratorMethodRethrowsFromTheMethod) {
  V8TestingScope scope;
  v8::Local<v8::Object> object = EvalObject(scope, R"JS(
      globalThis.theError = new Error('boom');
      ({ [Symbol.asyncIterator]() { throw globalThis.theError; } }))JS");

  v8::TryCatch try_catch(scope.GetIsolate());
  ScriptIterator iterator = OpenAsAsyncSequence(
      scope, object, PassThroughException(scope.GetIsolate()));
  EXPECT_TRUE(iterator.IsNull());
  ASSERT_TRUE(try_catch.HasCaught());
  // The user's exception is rethrown untouched.
  EXPECT_TRUE(
      try_catch.Exception()->StrictEquals(GlobalValue(scope, "theError")));
}

TEST_F(ScriptIteratorTest, FromIterableAsyncFallsBackToSyncIterator) {
  V8TestingScope scope;
  v8::Local<v8::Object> array = EvalObject(scope, "['a']");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = ScriptIterator::FromIterable(
      scope.GetIsolate(), array, ScriptIterator::Kind::kAsync, exception_state);
  ASSERT_FALSE(iterator.IsNull());
  ASSERT_FALSE(exception_state.HadException());

  // Driven like any async iterator: Next() hands out a Promise for an iterator
  // result object.
  bool fulfilled = false;
  bool done = true;
  ScriptValue result = SettleNext(scope, iterator, &fulfilled);
  ASSERT_TRUE(fulfilled);
  EXPECT_EQ("a", ReadIterResult(scope, result, &done));
  EXPECT_FALSE(done);
}

TEST_F(ScriptIteratorTest, FromIterableAsyncReturnsNullWithoutAnyMethod) {
  V8TestingScope scope;
  v8::Local<v8::Object> object = EvalObject(scope, "({})");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = ScriptIterator::FromIterable(
      scope.GetIsolate(), object, ScriptIterator::Kind::kAsync,
      exception_state);
  EXPECT_TRUE(iterator.IsNull());
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(ScriptIteratorTest, AsyncFromSyncNextYieldsIteratorResultPromises) {
  V8TestingScope scope;
  v8::Local<v8::Object> array = EvalObject(scope, "['a', 'b']");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = OpenAsAsyncSequence(scope, array, exception_state);
  ASSERT_FALSE(iterator.IsNull());
  ASSERT_FALSE(exception_state.HadException());

  bool fulfilled = false;
  bool done = true;
  ScriptValue result = SettleNext(scope, iterator, &fulfilled);
  ASSERT_TRUE(fulfilled);
  EXPECT_EQ("a", ReadIterResult(scope, result, &done));
  EXPECT_FALSE(done);

  result = SettleNext(scope, iterator, &fulfilled);
  ASSERT_TRUE(fulfilled);
  EXPECT_EQ("b", ReadIterResult(scope, result, &done));
  EXPECT_FALSE(done);

  result = SettleNext(scope, iterator, &fulfilled);
  ASSERT_TRUE(fulfilled);
  EXPECT_EQ("undefined", ReadIterResult(scope, result, &done));
  EXPECT_TRUE(done);
}

TEST_F(ScriptIteratorTest, AsyncFromSyncNextAwaitsPromiseValues) {
  V8TestingScope scope;
  v8::Local<v8::Object> array =
      EvalObject(scope, "[Promise.resolve('awaited')]");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = OpenAsAsyncSequence(scope, array, exception_state);
  ASSERT_FALSE(iterator.IsNull());

  bool fulfilled = false;
  bool done = true;
  ScriptValue result = SettleNext(scope, iterator, &fulfilled);
  ASSERT_TRUE(fulfilled);
  EXPECT_EQ("awaited", ReadIterResult(scope, result, &done));
  EXPECT_FALSE(done);
}

TEST_F(ScriptIteratorTest, AsyncFromSyncNextClosesSyncIteratorOnRejection) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      globalThis.returnCalls = 0;
      globalThis.theError = new Error('rejected value');
      ({ [Symbol.iterator]() {
           return {
             next() {
               return { value: Promise.reject(globalThis.theError),
                        done: false };
             },
             return() { globalThis.returnCalls++; return {}; },
           };
         } }))JS");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator =
      OpenAsAsyncSequence(scope, iterable, exception_state);
  ASSERT_FALSE(iterator.IsNull());

  bool fulfilled = true;
  ScriptValue result = SettleNext(scope, iterator, &fulfilled);
  EXPECT_FALSE(fulfilled);
  // The reason is passed through untouched.
  EXPECT_TRUE(result.V8Value()->StrictEquals(GlobalValue(scope, "theError")));
  EXPECT_EQ(1, ReadGlobalInt(scope, "returnCalls"));
}

// IteratorClose() with a throw completion discards whatever the return()
// lookup and call do, so the rejection reason survives each of these.
TEST_F(ScriptIteratorTest, AsyncFromSyncRejectionIgnoresReturnFailures) {
  const char* const kReturnProperties[] = {
      "return() { throw new Error('from return'); }",
      "return: 42,",
      "return() { return 42; }",
      "get return() { throw new Error('from getter'); }",
  };
  for (const char* return_property : kReturnProperties) {
    SCOPED_TRACE(return_property);
    V8TestingScope scope;
    String source = StrCat({R"JS(
        globalThis.theError = new Error('rejected value');
        ({ [Symbol.iterator]() {
             return {
               next() {
                 return { value: Promise.reject(globalThis.theError),
                          done: false };
               },
               )JS",
                            return_property, "}; } })"});
    v8::Local<v8::Object> iterable = EvalObject(scope, source.Utf8().c_str());

    DummyExceptionStateForTesting exception_state;
    ScriptIterator iterator =
        OpenAsAsyncSequence(scope, iterable, exception_state);
    ASSERT_FALSE(iterator.IsNull());

    bool fulfilled = true;
    ScriptValue result = SettleNext(scope, iterator, &fulfilled);
    EXPECT_FALSE(fulfilled);
    EXPECT_TRUE(result.V8Value()->StrictEquals(GlobalValue(scope, "theError")));
  }
}

TEST_F(ScriptIteratorTest, AsyncFromSyncNextDoesNotCloseWhenDone) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      globalThis.returnCalls = 0;
      ({ [Symbol.iterator]() {
           return {
             next() {
               return { value: Promise.reject(new Error('x')), done: true };
             },
             return() { globalThis.returnCalls++; return {}; },
           };
         } }))JS");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator =
      OpenAsAsyncSequence(scope, iterable, exception_state);
  ASSERT_FALSE(iterator.IsNull());

  bool fulfilled = true;
  SettleNext(scope, iterator, &fulfilled);
  EXPECT_FALSE(fulfilled);
  EXPECT_EQ(0, ReadGlobalInt(scope, "returnCalls"));
}

TEST_F(ScriptIteratorTest, CloseSyncCallsReturnAndReturnsReason) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      globalThis.returnArgs = null;
      ({ [Symbol.iterator]() {
           return { next() { return { value: 1, done: false }; },
                    return(...args) { globalThis.returnArgs = args; return {}; }
                  };
         } }))JS");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = OpenSync(scope, iterable, exception_state);
  ASSERT_FALSE(iterator.IsNull());

  v8::Local<v8::Value> reason = V8String(scope.GetIsolate(), "why");
  ScriptValue result =
      iterator.CloseSync(scope.GetScriptState(), reason, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(result.V8Value()->StrictEquals(reason));
  ExpectReturnCalledWith(scope, reason);
}

TEST_F(ScriptIteratorTest, CloseSyncWithoutReturnDoesNotThrow) {
  V8TestingScope scope;
  v8::Local<v8::Object> array = EvalObject(scope, "[1]");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = OpenSync(scope, array, exception_state);
  ASSERT_FALSE(iterator.IsNull());

  ScriptValue result = iterator.CloseSync(
      scope.GetScriptState(), v8::Local<v8::Value>(), exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(result.IsEmpty());
}

TEST_F(ScriptIteratorTest, CloseSyncThrowsForNonObjectReturnResult) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      ({ [Symbol.iterator]() {
           return { next() { return { value: 1, done: false }; },
                    return() { return 42; } };
         } }))JS");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = OpenSync(scope, iterable, exception_state);
  ASSERT_FALSE(iterator.IsNull());

  iterator.CloseSync(scope.GetScriptState(), v8::Local<v8::Value>(),
                     exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
}

TEST_F(ScriptIteratorTest, CloseSyncRethrowsWhenReturnThrows) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      globalThis.theError = new Error('boom');
      ({ [Symbol.iterator]() {
           return { next() { return { value: 1, done: false }; },
                    return() { throw globalThis.theError; } };
         } }))JS");

  DummyExceptionStateForTesting exception_state;
  ScriptIterator iterator = OpenSync(scope, iterable, exception_state);
  ASSERT_FALSE(iterator.IsNull());

  v8::TryCatch try_catch(scope.GetIsolate());
  iterator.CloseSync(scope.GetScriptState(), v8::Local<v8::Value>(),
                     PassThroughException(scope.GetIsolate()));
  ASSERT_TRUE(try_catch.HasCaught());
  // The user's exception is rethrown untouched.
  EXPECT_TRUE(
      try_catch.Exception()->StrictEquals(GlobalValue(scope, "theError")));
}

TEST_F(ScriptIteratorTest, CloseAsyncWithoutReturnResolvesUndefined) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(
      scope, "({ [Symbol.asyncIterator]() { return { next() {} }; } })");

  bool fulfilled = false;
  ScriptValue result = SettleClose(scope, iterable, &fulfilled);
  EXPECT_TRUE(fulfilled);
  EXPECT_TRUE(result.V8Value()->IsUndefined());
}

TEST_F(ScriptIteratorTest, CloseAsyncRejectsForNonCallableReturn) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(
      scope,
      "({ [Symbol.asyncIterator]() { return { next() {}, return: 42 }; } })");

  bool fulfilled = true;
  ScriptValue result = SettleClose(scope, iterable, &fulfilled);
  EXPECT_FALSE(fulfilled);
  EXPECT_EQ("TypeError: return() function must be callable", ToString(result));
}

TEST_F(ScriptIteratorTest, CloseAsyncRejectsWhenReturnThrows) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      globalThis.theError = new Error('boom');
      ({ [Symbol.asyncIterator]() {
           return { next() {}, return() { throw globalThis.theError; } };
         } }))JS");

  bool fulfilled = true;
  ScriptValue result = SettleClose(scope, iterable, &fulfilled);
  EXPECT_FALSE(fulfilled);
  EXPECT_TRUE(result.V8Value()->StrictEquals(GlobalValue(scope, "theError")));
}

TEST_F(ScriptIteratorTest, CloseAsyncRejectsForNonObjectReturnResult) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      ({ [Symbol.asyncIterator]() {
           return { next() {}, return() { return Promise.resolve(42); } };
         } }))JS");

  bool fulfilled = true;
  ScriptValue result = SettleClose(scope, iterable, &fulfilled);
  EXPECT_FALSE(fulfilled);
  EXPECT_TRUE(result.V8Value()->IsNativeError());
}

TEST_F(ScriptIteratorTest, CloseAsyncPassesReasonToReturn) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      globalThis.returnArgs = null;
      ({ [Symbol.asyncIterator]() {
           return { next() {},
                    return(...args) { globalThis.returnArgs = args; return {}; }
                  };
         } }))JS");

  v8::Local<v8::Value> reason = V8String(scope.GetIsolate(), "why");
  bool fulfilled = false;
  SettleClose(scope, iterable, &fulfilled, reason);
  EXPECT_TRUE(fulfilled);
  ExpectReturnCalledWith(scope, reason);
}

TEST_F(ScriptIteratorTest, CloseAsyncFromSyncForwardsToSyncReturn) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      globalThis.returnArgs = null;
      ({ [Symbol.iterator]() {
           return { next() { return { value: 1, done: false }; },
                    return(...args) {
                      globalThis.returnArgs = args;
                      return { value: Promise.resolve('ignored'), done: true };
                    } };
         } }))JS");

  v8::Local<v8::Value> reason = V8String(scope.GetIsolate(), "why");
  bool fulfilled = false;
  ScriptValue result = SettleClose(scope, iterable, &fulfilled, reason);
  EXPECT_TRUE(fulfilled);
  EXPECT_TRUE(result.V8Value()->IsUndefined());
  ExpectReturnCalledWith(scope, reason);
}

TEST_F(ScriptIteratorTest, CloseAsyncFromSyncRejectsForNonObjectReturnResult) {
  V8TestingScope scope;
  v8::Local<v8::Object> iterable = EvalObject(scope, R"JS(
      ({ [Symbol.iterator]() {
           return { next() { return { value: 1, done: false }; },
                    return() { return 42; } };
         } }))JS");

  bool fulfilled = true;
  ScriptValue result = SettleClose(scope, iterable, &fulfilled);
  EXPECT_FALSE(fulfilled);
  EXPECT_EQ("TypeError: Expected return() to return an Object.",
            ToString(result));
}

TEST_F(ScriptIteratorTest, CloseAsyncFromSyncWithoutReturnResolvesUndefined) {
  V8TestingScope scope;
  v8::Local<v8::Object> array = EvalObject(scope, "[1]");

  bool fulfilled = false;
  ScriptValue result = SettleClose(scope, array, &fulfilled);
  EXPECT_TRUE(fulfilled);
  EXPECT_TRUE(result.V8Value()->IsUndefined());
}

}  // namespace

}  // namespace blink

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/async_sequence.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/async_sequence_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

class RecordingClient final : public GarbageCollected<RecordingClient>,
                              public AsyncSequenceIterator<IDLString>::Client {
 public:
  void OnNextValue(ScriptState*, String value) override {
    values.push_back(value);
    ++calls;
  }
  void OnEndOfIteration(ScriptState*) override {
    done = true;
    ++calls;
  }
  void OnNextRejected(ScriptState*, ScriptValue reason) override {
    rejected = true;
    String text;
    CHECK(reason.ToString(text));
    rejection = text;
    ++calls;
  }

  Vector<String> values;
  bool done = false;
  bool rejected = false;
  String rejection;
  int calls = 0;
};

class AsyncSequenceTest : public testing::Test {
 protected:
  v8::Local<v8::Value> Eval(V8TestingScope& scope, const char* source) {
    v8::Local<v8::Script> script =
        v8::Script::Compile(scope.GetContext(),
                            V8String(scope.GetIsolate(), source))
            .ToLocalChecked();
    return script->Run(scope.GetContext()).ToLocalChecked();
  }

  v8::Local<v8::Value> GlobalValue(V8TestingScope& scope, const char* name) {
    return scope.GetContext()
        ->Global()
        ->Get(scope.GetContext(), V8String(scope.GetIsolate(), name))
        .ToLocalChecked();
  }

  AsyncSequence<IDLString>* Convert(V8TestingScope& scope, const char* source) {
    return NativeValueTraits<IDLAsyncSequence<IDLString>>::NativeValue(
        scope.GetIsolate(), Eval(scope, source), scope.GetExceptionState());
  }

  AsyncSequenceIterator<IDLString>* Open(V8TestingScope& scope,
                                         const char* source) {
    AsyncSequence<IDLString>* sequence = Convert(scope, source);
    CHECK(sequence) << "conversion failed";
    AsyncSequenceIterator<IDLString>* iterator =
        AsyncSequenceIterator<IDLString>::Open(
            scope.GetScriptState(), *sequence, scope.GetExceptionState());
    CHECK(iterator);
    return iterator;
  }

  RecordingClient* ReadAll(V8TestingScope& scope, const char* source) {
    AsyncSequenceIterator<IDLString>* iterator = Open(scope, source);
    auto* client = MakeGarbageCollected<RecordingClient>();
    while (!client->done && !client->rejected) {
      const int calls_before = client->calls;
      iterator->Next(scope.GetScriptState(), client);
      for (int pumps = 0; client->calls == calls_before; ++pumps) {
        CHECK_LT(pumps, 100) << "the step never settled";
        scope.PerformMicrotaskCheckpoint();
        test::RunPendingTasks();
      }
    }
    return client;
  }

  bool CloseFulfills(V8TestingScope& scope,
                     AsyncSequenceIterator<IDLString>* iterator) {
    ScriptPromiseTester tester(
        scope.GetScriptState(),
        iterator->Close(scope.GetScriptState(),
                        ExceptionContext(v8::ExceptionContext::kOperation,
                                         "AsyncSequenceTest", "Close")));
    tester.WaitUntilSettled();
    return tester.IsFulfilled();
  }

  test::TaskEnvironment task_environment_;
};

// --- Conversion -------------------------------------------------------------

TEST_F(AsyncSequenceTest, AsyncGeneratorIsAsyncKind) {
  V8TestingScope scope;
  AsyncSequence<IDLString>* sequence =
      Convert(scope, "(async function* () { yield 'a'; })()");
  ASSERT_TRUE(sequence);
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ScriptIterator::Kind::kAsync, sequence->Kind());
}

TEST_F(AsyncSequenceTest, ArrayIsAsyncFromSyncKind) {
  V8TestingScope scope;
  AsyncSequence<IDLString>* sequence = Convert(scope, "['a', 'b']");
  ASSERT_TRUE(sequence);
  EXPECT_EQ(ScriptIterator::Kind::kAsyncFromSync, sequence->Kind());
}

TEST_F(AsyncSequenceTest, AsyncIteratorWinsOverIterator) {
  V8TestingScope scope;
  AsyncSequence<IDLString>* sequence = Convert(scope, R"JS(({
        [Symbol.asyncIterator]() { return {}; },
        [Symbol.iterator]() { return {}; },
      }))JS");
  ASSERT_TRUE(sequence);
  EXPECT_EQ(ScriptIterator::Kind::kAsync, sequence->Kind());
}

TEST_F(AsyncSequenceTest, NonCallableAsyncIteratorThrowsTypeError) {
  V8TestingScope scope;
  EXPECT_FALSE(Convert(scope, "({ [Symbol.asyncIterator]: 1 })"));
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

TEST_F(AsyncSequenceTest, ThrowingGetterPropagates) {
  V8TestingScope scope;
  // Rethrown user exceptions are not reclassified, so observe them through V8
  // rather than through the ExceptionState's code.
  v8::TryCatch try_catch(scope.GetIsolate());
  EXPECT_FALSE(NativeValueTraits<IDLAsyncSequence<IDLString>>::NativeValue(
      scope.GetIsolate(), Eval(scope, R"JS(
      globalThis.theError = new RangeError('boom');
      ({ get [Symbol.asyncIterator]() { throw globalThis.theError; } }))JS"),
      PassThroughException(scope.GetIsolate())));
  ASSERT_TRUE(try_catch.HasCaught());
  EXPECT_TRUE(
      try_catch.Exception()->StrictEquals(GlobalValue(scope, "theError")));
}

// --- Iteration --------------------------------------------------------------

TEST_F(AsyncSequenceTest, ReadsAsyncGeneratorToEnd) {
  V8TestingScope scope;
  RecordingClient* client =
      ReadAll(scope, "(async function* () { yield 'a'; yield 'b'; })()");
  EXPECT_TRUE(client->done);
  EXPECT_FALSE(client->rejected);
  ASSERT_EQ(2u, client->values.size());
  EXPECT_EQ("a", client->values[0]);
  EXPECT_EQ("b", client->values[1]);
}

TEST_F(AsyncSequenceTest, ReadsArrayThroughAsyncFromSync) {
  V8TestingScope scope;
  RecordingClient* client = ReadAll(scope, "['a', 'b', 'c']");
  EXPECT_TRUE(client->done);
  ASSERT_EQ(3u, client->values.size());
  EXPECT_EQ("c", client->values[2]);
}

TEST_F(AsyncSequenceTest, AwaitsPromisesYieldedBySyncIterator) {
  V8TestingScope scope;
  RecordingClient* client = ReadAll(scope, "[Promise.resolve('awaited')]");
  EXPECT_TRUE(client->done);
  ASSERT_EQ(1u, client->values.size());
  EXPECT_EQ("awaited", client->values[0]);
}

TEST_F(AsyncSequenceTest, RejectedNextRejectsTheStep) {
  V8TestingScope scope;
  RecordingClient* client =
      ReadAll(scope, "(async function* () { throw new TypeError('nope'); })()");
  EXPECT_TRUE(client->rejected);
  EXPECT_FALSE(client->done);
  EXPECT_EQ("TypeError: nope", client->rejection);
}

TEST_F(AsyncSequenceTest, NonObjectIteratorResultRejectsTheStep) {
  V8TestingScope scope;
  RecordingClient* client = ReadAll(scope, R"JS(({
        [Symbol.asyncIterator]() {
          return { next() { return Promise.resolve(42); } };
        },
      }))JS");
  EXPECT_TRUE(client->rejected);
  EXPECT_EQ("TypeError: Expected next() to resolve to an Object.",
            client->rejection);
}

TEST_F(AsyncSequenceTest, SynchronousThrowFromNextRejectsTheStep) {
  V8TestingScope scope;
  RecordingClient* client = ReadAll(scope, R"JS(({
        [Symbol.asyncIterator]() {
          return { next() { throw new TypeError('sync throw'); } };
        },
      }))JS");
  EXPECT_TRUE(client->rejected);
  EXPECT_EQ("TypeError: sync throw", client->rejection);
}

TEST_F(AsyncSequenceTest, ElementConversionFailureRejectsTheStep) {
  V8TestingScope scope;
  RecordingClient* client = ReadAll(scope, R"JS(({
        [Symbol.asyncIterator]() {
          let sent = false;
          return {
            next() {
              if (sent) {
                return Promise.resolve({value: undefined, done: true});
              }
              sent = true;
              return Promise.resolve({
                value: {toString() { throw new TypeError('bad element'); }},
                done: false,
              });
            },
          };
        },
      }))JS");
  EXPECT_TRUE(client->rejected);
  EXPECT_EQ("TypeError: bad element", client->rejection);
}

// A platform object's async iterator (an AsyncIteratorBase) is only reachable
// through its JavaScript next(), so this covers the round trip between the two.
TEST_F(AsyncSequenceTest, ReadsReadableStreamToEnd) {
  V8TestingScope scope;
  RecordingClient* client = ReadAll(scope, R"JS(
      new ReadableStream({
        start(c) {
          c.enqueue('a');
          c.enqueue('b');
          c.close();
        },
      }))JS");
  EXPECT_TRUE(client->done);
  EXPECT_FALSE(client->rejected);
  ASSERT_EQ(2u, client->values.size());
  EXPECT_EQ("a", client->values[0]);
  EXPECT_EQ("b", client->values[1]);
}

// --- Close ------------------------------------------------------------------

TEST_F(AsyncSequenceTest, CloseResolvesWhenReturnSucceeds) {
  V8TestingScope scope;
  EXPECT_TRUE(CloseFulfills(
      scope, Open(scope, "(async function* () { yield 'a'; })()")));
}

TEST_F(AsyncSequenceTest, CloseResolvesWhenReturnIsMissing) {
  V8TestingScope scope;
  EXPECT_TRUE(CloseFulfills(scope, Open(scope, R"JS(({
        [Symbol.asyncIterator]() {
          return {
            next() { return Promise.resolve({value: 'a', done: false}); },
          };
        },
      }))JS")));
}

TEST_F(AsyncSequenceTest, CloseCancelsReadableStream) {
  V8TestingScope scope;
  EXPECT_TRUE(CloseFulfills(scope, Open(scope, R"JS(
      globalThis.cancelled = false;
      new ReadableStream({
        start(c) { c.enqueue('a'); },
        cancel() { globalThis.cancelled = true; },
      }))JS")));
  EXPECT_TRUE(GlobalValue(scope, "cancelled")->IsTrue());
}

TEST_F(AsyncSequenceTest, CloseRejectsWhenReturnThrows) {
  V8TestingScope scope;
  EXPECT_FALSE(CloseFulfills(scope, Open(scope, R"JS(({
        [Symbol.asyncIterator]() {
          return {
            next() { return Promise.resolve({value: 'a', done: false}); },
            return() { throw new TypeError('return threw'); },
          };
        },
      }))JS")));
}

// --- ToV8 -------------------------------------------------------------------

TEST_F(AsyncSequenceTest, ToV8ReturnsTheOriginalObject) {
  V8TestingScope scope;
  v8::Local<v8::Value> original = Eval(scope, "['a']");
  AsyncSequence<IDLString>* sequence =
      NativeValueTraits<IDLAsyncSequence<IDLString>>::NativeValue(
          scope.GetIsolate(), original, scope.GetExceptionState());
  ASSERT_TRUE(sequence);
  EXPECT_TRUE(ToV8Traits<IDLAsyncSequence<IDLString>>::ToV8(
                  scope.GetScriptState(), sequence)
                  ->StrictEquals(original));
}

TEST_F(AsyncSequenceTest, ToV8NullableReturnsNullForNullptr) {
  V8TestingScope scope;
  EXPECT_TRUE(ToV8Traits<IDLNullable<IDLAsyncSequence<IDLString>>>::ToV8(
                  scope.GetScriptState(), nullptr)
                  ->IsNull());
}

}  // namespace

}  // namespace blink

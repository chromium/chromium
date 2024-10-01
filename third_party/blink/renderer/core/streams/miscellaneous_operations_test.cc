// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"

#include <math.h>

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/test_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Tests in this file are named MiscellaneousOperations* so that it is easy to
// select them with gtest_filter.

v8::MaybeLocal<v8::Value> EmptyExtraArg() {
  return v8::MaybeLocal<v8::Value>();
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmNoMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto underlying_object = v8::Object::New(scope.GetIsolate());
  auto* algo = CreateAlgorithmFromUnderlyingMethod(
      scope.GetScriptState(), underlying_object, "pull",
      "underlyingSource.pull", EmptyExtraArg(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(algo);
  auto promise = algo->Run(scope.GetScriptState(), 0, nullptr);
  ASSERT_FALSE(promise.IsEmpty());
  ASSERT_EQ(promise->State(), v8::Promise::kFulfilled);
  EXPECT_TRUE(promise->Result()->IsUndefined());
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmUndefinedMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto underlying_object = v8::Object::New(scope.GetIsolate());
  underlying_object
      ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "pull"),
            v8::Undefined(scope.GetIsolate()))
      .Check();
  auto* algo = CreateAlgorithmFromUnderlyingMethod(
      scope.GetScriptState(), underlying_object, "pull",
      "underlyingSource.pull", EmptyExtraArg(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(algo);
  auto promise = algo->Run(scope.GetScriptState(), 0, nullptr);
  ASSERT_FALSE(promise.IsEmpty());
  ASSERT_EQ(promise->State(), v8::Promise::kFulfilled);
  EXPECT_TRUE(promise->Result()->IsUndefined());
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmNullMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto underlying_object = v8::Object::New(scope.GetIsolate());
  underlying_object
      ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "pull"),
            v8::Null(scope.GetIsolate()))
      .Check();
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kOperation, "", "");
  auto* algo = CreateAlgorithmFromUnderlyingMethod(
      scope.GetScriptState(), underlying_object, "pull",
      "underlyingSource.pull", EmptyExtraArg(), exception_state);
  EXPECT_FALSE(algo);
  EXPECT_TRUE(exception_state.HadException());
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmThrowingGetter) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue underlying_value = EvalWithPrintingError(
      &scope, "({ get pull() { throw new TypeError(); } })");
  ASSERT_TRUE(underlying_value.IsObject());
  auto underlying_object = underlying_value.V8Value().As<v8::Object>();
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kOperation, "", "");
  auto* algo = CreateAlgorithmFromUnderlyingMethod(
      scope.GetScriptState(), underlying_object, "pull",
      "underlyingSource.pull", EmptyExtraArg(), exception_state);
  EXPECT_FALSE(algo);
  EXPECT_TRUE(exception_state.HadException());
}

v8::Local<v8::Value> CreateFromFunctionAndGetResult(
    V8TestingScope* scope,
    const char* function_definition,
    v8::MaybeLocal<v8::Value> extra_arg = v8::MaybeLocal<v8::Value>(),
    int argc = 0,
    v8::Local<v8::Value> argv[] = nullptr) {
  String js = String("({start: ") + function_definition + "})" + '\0';
  ScriptValue underlying_value =
      EvalWithPrintingError(scope, js.Utf8().c_str());
  auto underlying_object = underlying_value.V8Value().As<v8::Object>();
  auto* algo = CreateAlgorithmFromUnderlyingMethod(
      scope->GetScriptState(), underlying_object, "start",
      "underlyingSource.start", extra_arg, ASSERT_NO_EXCEPTION);
  auto promise = algo->Run(scope->GetScriptState(), argc, argv);
  EXPECT_EQ(promise->State(), v8::Promise::kFulfilled);
  return promise->Result();
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmReturnsInteger) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto result = CreateFromFunctionAndGetResult(&scope, "() => 5");
  ASSERT_TRUE(result->IsNumber());
  EXPECT_EQ(result.As<v8::Number>()->Value(), 5);
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmReturnsPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto result =
      CreateFromFunctionAndGetResult(&scope, "() => Promise.resolve(2)");
  ASSERT_TRUE(result->IsNumber());
  EXPECT_EQ(result.As<v8::Number>()->Value(), 2);
}

bool CreateFromFunctionAndGetSuccess(
    V8TestingScope* scope,
    const char* function_definition,
    v8::MaybeLocal<v8::Value> extra_arg = v8::MaybeLocal<v8::Value>(),
    int argc = 0,
    v8::Local<v8::Value> argv[] = nullptr) {
  auto result = CreateFromFunctionAndGetResult(scope, function_definition,
                                               extra_arg, argc, argv);
  if (!result->IsBoolean()) {
    return false;
  }
  return result.As<v8::Boolean>()->Value();
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmNoArgs) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_TRUE(CreateFromFunctionAndGetSuccess(
      &scope, "(...args) => args.length === 0"));
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmExtraArg) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::Number> extra_arg = v8::Number::New(scope.GetIsolate(), 7);
  EXPECT_TRUE(CreateFromFunctionAndGetSuccess(
      &scope, "(...args) => args.length === 1 && args[0] === 7", extra_arg));
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmPassOneArg) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::MaybeLocal<v8::Value> extra_arg;
  v8::Local<v8::Value> argv[] = {v8::Number::New(scope.GetIsolate(), 10)};
  EXPECT_TRUE(CreateFromFunctionAndGetSuccess(
      &scope, "(...args) => args.length === 1 && args[0] === 10",
      EmptyExtraArg(), 1, argv));
}

TEST(MiscellaneousOperationsTest, CreateAlgorithmPassBoth) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::MaybeLocal<v8::Value> extra_arg = v8::Number::New(scope.GetIsolate(), 5);
  v8::Local<v8::Value> argv[] = {v8::Number::New(scope.GetIsolate(), 10)};
  EXPECT_TRUE(CreateFromFunctionAndGetSuccess(
      &scope,
      "(...args) => args.length === 2 && args[0] === 10 && args[1] === 5",
      extra_arg, 1, argv));
}

TEST(MiscellaneousOperationsTest, CreateStartAlgorithmNoMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto underlying_object = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Value> controller = v8::Undefined(scope.GetIsolate());
  auto* algo = CreateStartAlgorithm(scope.GetScriptState(), underlying_object,
                                    "underlyingSink.start", controller);
  ASSERT_TRUE(algo);
  auto maybe_result = algo->Run(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(maybe_result.IsEmpty());
  auto result = maybe_result.ToLocalChecked();
  ASSERT_EQ(result->State(), v8::Promise::kFulfilled);
  EXPECT_TRUE(result->Result()->IsUndefined());
}

TEST(MiscellaneousOperationsTest, CreateStartAlgorithmNullMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto underlying_object = v8::Object::New(scope.GetIsolate());
  underlying_object
      ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "start"),
            v8::Null(scope.GetIsolate()))
      .Check();
  v8::Local<v8::Value> controller = v8::Undefined(scope.GetIsolate());
  auto* algo = CreateStartAlgorithm(scope.GetScriptState(), underlying_object,
                                    "underlyingSink.start", controller);
  ASSERT_TRUE(algo);
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kOperation, "", "");
  auto maybe_result = algo->Run(scope.GetScriptState(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_TRUE(maybe_result.IsEmpty());
}

TEST(MiscellaneousOperationsTest, CreateStartAlgorithmThrowingMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue underlying_value = EvalWithPrintingError(&scope,
                                                       R"(({
  start() {
    throw new Error();
  }
}))");
  ASSERT_TRUE(underlying_value.IsObject());
  auto underlying_object = underlying_value.V8Value().As<v8::Object>();
  v8::Local<v8::Value> controller = v8::Undefined(scope.GetIsolate());
  auto* algo = CreateStartAlgorithm(scope.GetScriptState(), underlying_object,
                                    "underlyingSink.start", controller);
  ASSERT_TRUE(algo);
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kOperation, "", "");
  auto maybe_result = algo->Run(scope.GetScriptState(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_TRUE(maybe_result.IsEmpty());
}

TEST(MiscellaneousOperationsTest, CreateStartAlgorithmReturningController) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue underlying_value = EvalWithPrintingError(&scope,
                                                       R"(({
  start(controller) {
    return controller;
  }
}))");
  ASSERT_TRUE(underlying_value.IsObject());
  auto underlying_object = underlying_value.V8Value().As<v8::Object>();
  // In a real stream, |controller| is never a promise, but nothing in
  // CreateStartAlgorithm() requires this. By making it a promise, we can verify
  // that a promise returned from start is passed through as-is.
  v8::Local<v8::Value> controller =
      v8::Promise::Resolver::New(scope.GetContext())
          .ToLocalChecked()
          ->GetPromise();
  auto* algo = CreateStartAlgorithm(scope.GetScriptState(), underlying_object,
                                    "underlyingSink.start", controller);
  ASSERT_TRUE(algo);
  auto maybe_result = algo->Run(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(maybe_result.IsEmpty());
  v8::Local<v8::Value> result = maybe_result.ToLocalChecked();
  ASSERT_TRUE(result->IsPromise());
  ASSERT_EQ(result, controller);
}

TEST(MiscellaneousOperationsTest, CallOrNoop1NoMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto underlying_object = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Value> arg0 = v8::Number::New(scope.GetIsolate(), 0);
  auto maybe_result =
      CallOrNoop1(scope.GetScriptState(), underlying_object, "transform",
                  "transformer.transform", arg0, ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(maybe_result.IsEmpty());
  EXPECT_TRUE(maybe_result.ToLocalChecked()->IsUndefined());
}

TEST(MiscellaneousOperationsTest, CallOrNoop1NullMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto underlying_object = v8::Object::New(scope.GetIsolate());
  v8::Local<v8::Value> arg0 = v8::Number::New(scope.GetIsolate(), 0);
  underlying_object
      ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "transform"),
            v8::Null(scope.GetIsolate()))
      .Check();
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kOperation, "", "");
  auto maybe_result =
      CallOrNoop1(scope.GetScriptState(), underlying_object, "transform",
                  "transformer.transform", arg0, exception_state);
  ASSERT_TRUE(maybe_result.IsEmpty());
  ASSERT_TRUE(exception_state.HadException());
}

TEST(MiscellaneousOperationsTest, CallOrNoop1CheckCalled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue underlying_value = EvalWithPrintingError(&scope,
                                                       R"(({
  transform(...args) {
    return args.length === 1 && args[0] === 17;
  }
}))");
  ASSERT_TRUE(underlying_value.IsObject());
  auto underlying_object = underlying_value.V8Value().As<v8::Object>();
  v8::Local<v8::Value> arg0 = v8::Number::New(scope.GetIsolate(), 17);
  auto maybe_result =
      CallOrNoop1(scope.GetScriptState(), underlying_object, "transform",
                  "transformer.transform", arg0, ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(maybe_result.IsEmpty());
  auto result = maybe_result.ToLocalChecked();
  ASSERT_TRUE(result->IsBoolean());
  EXPECT_TRUE(result.As<v8::Boolean>()->Value());
}

TEST(MiscellaneousOperationsTest, CallOrNoop1ThrowingMethod) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue underlying_value = EvalWithPrintingError(&scope,
                                                       R"(({
  transform(...args) {
    throw false;
  }
}))");
  ASSERT_TRUE(underlying_value.IsObject());
  auto underlying_object = underlying_value.V8Value().As<v8::Object>();
  v8::Local<v8::Value> arg0 = v8::Number::New(scope.GetIsolate(), 17);
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kUnknown, "", "");
  auto maybe_result =
      CallOrNoop1(scope.GetScriptState(), underlying_object, "transform",
                  "transformer.transform", arg0, exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_TRUE(maybe_result.IsEmpty());
  EXPECT_TRUE(exception_state.GetException()->IsBoolean());
}

v8::Local<v8::Promise> PromiseCallFromText(V8TestingScope* scope,
                                           const char* function_definition,
                                           const char* object_definition,
                                           int argc,
                                           v8::Local<v8::Value> argv[]) {
  ScriptValue function_value =
      EvalWithPrintingError(scope, function_definition);
  EXPECT_TRUE(function_value.IsFunction());
  ScriptValue object_value = EvalWithPrintingError(scope, object_definition);
  EXPECT_TRUE(object_value.IsObject());
  return PromiseCall(scope->GetScriptState(),
                     function_value.V8Value().As<v8::Function>(),
                     object_value.V8Value().As<v8::Object>(), argc, argv);
}

TEST(MiscellaneousOperationsTest, PromiseCalledWithObject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::Promise> promise =
      PromiseCallFromText(&scope, "(function() { return this.value === 15; })",
                          "({ value: 15 })", 0, nullptr);
  ASSERT_EQ(promise->State(), v8::Promise::kFulfilled);
  ASSERT_TRUE(promise->Result()->IsBoolean());
  EXPECT_TRUE(promise->Result().As<v8::Boolean>()->Value());
}

TEST(MiscellaneousOperationsTest, PromiseCallThrowing) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::Promise> promise = PromiseCallFromText(
      &scope, "(function() { throw new TypeError(); })", "({})", 0, nullptr);
  ASSERT_EQ(promise->State(), v8::Promise::kRejected);
  EXPECT_TRUE(promise->Result()->IsNativeError());
}

TEST(MiscellaneousOperationsTest, PromiseCallRejecting) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::Promise> promise = PromiseCallFromText(
      &scope, "(function() { return Promise.reject(16) })", "({})", 0, nullptr);
  ASSERT_EQ(promise->State(), v8::Promise::kRejected);
  ASSERT_TRUE(promise->Result()->IsNumber());
  EXPECT_EQ(promise->Result().As<v8::Number>()->Value(), 16);
}

TEST(MiscellaneousOperationsTest, ValidatePositiveHighWaterMark) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_EQ(ValidateAndNormalizeHighWaterMark(23, ASSERT_NO_EXCEPTION), 23.0);
}

TEST(MiscellaneousOperationsTest, ValidateInfiniteHighWaterMark) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_FALSE(isfinite(ValidateAndNormalizeHighWaterMark(
      std::numeric_limits<double>::infinity(), ASSERT_NO_EXCEPTION)));
}

TEST(MiscellaneousOperationsTest, NegativeHighWaterMarkInvalid) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kOperation, "", "");
  ValidateAndNormalizeHighWaterMark(-1, exception_state);
  EXPECT_TRUE(exception_state.HadException());
}

TEST(MiscellaneousOperationsTest, NaNHighWaterMarkInvalid) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kOperation, "", "");
  ValidateAndNormalizeHighWaterMark(std::numeric_limits<double>::quiet_NaN(),
                                    exception_state);
  EXPECT_TRUE(exception_state.HadException());
}

TEST(MiscellaneousOperationsTest, UndefinedSizeFunction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* algo = MakeSizeAlgorithmFromSizeFunction(
      scope.GetScriptState(), v8::Undefined(scope.GetIsolate()),
      ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(algo);
  auto optional = algo->Run(scope.GetScriptState(),
                            v8::Number::New(scope.GetIsolate(), 97));
  ASSERT_TRUE(optional.has_value());
  EXPECT_EQ(optional.value(), 1.0);
}

TEST(MiscellaneousOperationsTest, NullSizeFunction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ExceptionState exception_state(scope.GetIsolate(),
                                 v8::ExceptionContext::kOperation, "", "");
  EXPECT_EQ(MakeSizeAlgorithmFromSizeFunction(scope.GetScriptState(),
                                              v8::Null(scope.GetIsolate()),

                                              exception_state),
            nullptr);
  EXPECT_TRUE(exception_state.HadException());
}

StrategySizeAlgorithm* IdentitySizeAlgorithm(V8TestingScope* scope) {
  ScriptValue function_value = EvalWithPrintingError(scope, "i => i");
  EXPECT_TRUE(function_value.IsFunction());
  return MakeSizeAlgorithmFromSizeFunction(
      scope->GetScriptState(), function_value.V8Value(), ASSERT_NO_EXCEPTION);
}

TEST(MiscellaneousOperationsTest, SizeAlgorithmWorks) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* algo = IdentitySizeAlgorithm(&scope);
  ASSERT_TRUE(algo);
  auto optional = algo->Run(scope.GetScriptState(),
                            v8::Number::New(scope.GetIsolate(), 41));
  ASSERT_TRUE(optional.has_value());
  EXPECT_EQ(optional.value(), 41.0);
}

TEST(MiscellaneousOperationsTest, SizeAlgorithmConvertsToNumber) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* algo = IdentitySizeAlgorithm(&scope);
  ASSERT_TRUE(algo);
  auto optional =
      algo->Run(scope.GetScriptState(), V8String(scope.GetIsolate(), "79"));
  ASSERT_TRUE(optional.has_value());
  EXPECT_EQ(optional.value(), 79.0);
}

TEST(MiscellaneousOperationsTest, ThrowingSizeAlgorithm) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue function_value =
      EvalWithPrintingError(&scope, "() => { throw new TypeError(); }");
  EXPECT_TRUE(function_value.IsFunction());
  auto* algo = MakeSizeAlgorithmFromSizeFunction(
      scope.GetScriptState(), function_value.V8Value(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(algo);
  v8::TryCatch try_catch(scope.GetIsolate());
  auto optional =
      algo->Run(scope.GetScriptState(), V8String(scope.GetIsolate(), "79"));

  ASSERT_FALSE(optional.has_value());
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST(MiscellaneousOperationsTest, UnconvertibleSize) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* algo = IdentitySizeAlgorithm(&scope);
  ASSERT_TRUE(algo);
  ScriptValue unconvertible_value =
      EvalWithPrintingError(&scope, "({ toString() { throw new Error(); }})");
  EXPECT_TRUE(unconvertible_value.IsObject());
  v8::TryCatch try_catch(scope.GetIsolate());
  auto optional =
      algo->Run(scope.GetScriptState(), unconvertible_value.V8Value());

  ASSERT_FALSE(optional.has_value());
}

TEST(MiscellaneousOperationsTest, PromiseResolve) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto promise = PromiseResolve(scope.GetScriptState(),
                                v8::Number::New(scope.GetIsolate(), 19));
  ASSERT_EQ(promise->State(), v8::Promise::kFulfilled);
  ASSERT_TRUE(promise->Result()->IsNumber());
  EXPECT_EQ(promise->Result().As<v8::Number>()->Value(), 19);
}

TEST(MiscellaneousOperationsTest, PromiseResolveWithPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto original_promise = v8::Promise::Resolver::New(scope.GetContext())
                              .ToLocalChecked()
                              ->GetPromise();
  auto resolved_promise =
      PromiseResolve(scope.GetScriptState(), original_promise);
  EXPECT_EQ(original_promise, resolved_promise);
}

TEST(MiscellaneousOperationsTest, PromiseResolveWithUndefined) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto promise = PromiseResolveWithUndefined(scope.GetScriptState());
  ASSERT_EQ(promise->State(), v8::Promise::kFulfilled);
  EXPECT_TRUE(promise->Result()->IsUndefined());
}

TEST(MiscellaneousOperationsTest, PromiseReject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto promise = PromiseReject(scope.GetScriptState(),
                               v8::Number::New(scope.GetIsolate(), 43));
  ASSERT_EQ(promise->State(), v8::Promise::kRejected);
  ASSERT_TRUE(promise->Result()->IsNumber());
  EXPECT_EQ(promise->Result().As<v8::Number>()->Value(), 43);
}

}  // namespace

}  // namespace blink

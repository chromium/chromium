// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/exception_state_matchers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class V8DictionaryTest : public testing::Test {
 protected:
  static Dictionary CreateDictionary(ScriptState* script_state, const char* s) {
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(script_state->GetIsolate(), s,
                                v8::NewStringType::kNormal)
            .ToLocalChecked();
    v8::Local<v8::Script> script =
        v8::Script::Compile(script_state->GetContext(), source)
            .ToLocalChecked();
    v8::Local<v8::Value> value =
        script->Run(script_state->GetContext()).ToLocalChecked();
    DCHECK(!value.IsEmpty());
    DCHECK(value->IsObject());
    NonThrowableExceptionState exception_state;
    Dictionary dictionary(script_state->GetIsolate(), value, exception_state);
    return dictionary;
  }

  test::TaskEnvironment task_environment_;
};

TEST_F(V8DictionaryTest, Get_Empty) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(scope.GetScriptState(), "({})");

  auto r = dictionary.Get<IDLByteString>("key", scope.GetExceptionState());

  ASSERT_THAT(scope.GetExceptionState(), HadNoException());
  EXPECT_FALSE(r.has_value());
}

TEST_F(V8DictionaryTest, Get_NonPresentForNonEmpty) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: 3})");

  auto r = dictionary.Get<IDLByteString>("key", scope.GetExceptionState());

  ASSERT_THAT(scope.GetExceptionState(), HadNoException());
  EXPECT_FALSE(r.has_value());
}

TEST_F(V8DictionaryTest, Get_UndefinedValue) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: undefined})");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_THAT(scope.GetExceptionState(), HadNoException());
  EXPECT_FALSE(r.has_value());
}

TEST_F(V8DictionaryTest, Get_Found) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: 3})");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_THAT(scope.GetExceptionState(), HadNoException());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "3");
}

TEST_F(V8DictionaryTest, Get_Found2) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: '3'})");

  auto r = dictionary.Get<IDLLong>("foo", scope.GetExceptionState());

  ASSERT_THAT(scope.GetExceptionState(), HadNoException());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 3);
}

TEST_F(V8DictionaryTest, Get_Getter) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(scope.GetScriptState(),
                                           "({get foo() { return 'xy'; }})");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_THAT(scope.GetExceptionState(), HadNoException());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "xy");
}

TEST_F(V8DictionaryTest, Get_ExceptionOnAccess) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(scope.GetScriptState(),
                                           "({get foo() { throw Error(2); }})");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  ASSERT_FALSE(r.has_value());
}

// TODO(bashi,yukishiino): Should rethrow the exception.
// http://crbug.com/666661
TEST_F(V8DictionaryTest, Get_ExceptionOnAccess2) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(scope.GetScriptState(),
                                           "({get foo() { throw Error(2); }})");

  v8::Local<v8::Value> value;
  v8::TryCatch try_catch(scope.GetIsolate());
  ASSERT_FALSE(dictionary.Get("foo", value));
  ASSERT_FALSE(try_catch.HasCaught());
}

// TODO(bashi,yukishiino): Should rethrow the exception.
// http://crbug.com/666661
TEST_F(V8DictionaryTest, Get_InvalidInnerDictionary) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: 4})");

  v8::TryCatch try_catch(scope.GetIsolate());
  Dictionary inner_dictionary;
  ASSERT_TRUE(dictionary.Get("foo", inner_dictionary));
  ASSERT_FALSE(try_catch.HasCaught());

  EXPECT_TRUE(inner_dictionary.IsUndefinedOrNull());
}

TEST_F(V8DictionaryTest, Get_TypeConversion) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(
      scope.GetScriptState(), "({foo: { toString() { return 'hello'; } } })");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_THAT(scope.GetExceptionState(), HadNoException());
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "hello");
}

TEST_F(V8DictionaryTest, Get_ConversionError) {
  V8TestingScope scope;
  Dictionary dictionary = CreateDictionary(
      scope.GetScriptState(),
      "({get foo() { return { toString() { throw Error(88); } };} })");

  auto r = dictionary.Get<IDLByteString>("foo", scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  ASSERT_FALSE(r.has_value());
}

TEST_F(V8DictionaryTest, Get_ConversionError2) {
  V8TestingScope scope;
  Dictionary dictionary =
      CreateDictionary(scope.GetScriptState(), "({foo: NaN})");

  auto r = dictionary.Get<IDLDouble>("foo", scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  ASSERT_FALSE(r.has_value());
}

}  // namespace

}  // namespace blink

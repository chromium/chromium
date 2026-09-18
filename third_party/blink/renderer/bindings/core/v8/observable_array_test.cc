// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Helper to execute a JavaScript string expression in the test scope and return
// the string result.
String RunScript(V8TestingScope& scope, const char* source) {
  v8::Local<v8::Value> result =
      ClassicScript::CreateUnspecifiedScript(source)
          ->RunScriptAndReturnValue(&scope.GetWindow())
          .GetSuccessValueOrEmpty();
  if (result.IsEmpty() || !result->IsString()) {
    return String();
  }
  return ToCoreString(scope.GetIsolate(), result.As<v8::String>());
}

// Helper to evaluate a descriptor expression and run
// V8ObjectToPropertyDescriptor.
void ConvertDescriptor(V8TestingScope& scope,
                       const char* descriptor_expression,
                       bool& has_caught,
                       bindings::V8PropertyDescriptorBag& desc_bag) {
  v8::Local<v8::Value> desc_value =
      ClassicScript::CreateUnspecifiedScript(descriptor_expression)
          ->RunScriptAndReturnValue(&scope.GetWindow())
          .GetSuccessValueOrEmpty();
  ASSERT_FALSE(desc_value.IsEmpty());
  v8::TryCatch try_catch(scope.GetIsolate());
  bindings::V8ObjectToPropertyDescriptor(scope.GetIsolate(), desc_value,
                                         desc_bag);
  has_caught = try_catch.HasCaught();
}

}  // namespace

// Tests that V8ObjectToPropertyDescriptor throws a TypeError when the 'get'
// property is not callable and not undefined.
TEST(V8ObjectToPropertyDescriptorTest, NonCallableGetter) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  bool has_caught = false;
  bindings::V8PropertyDescriptorBag desc_bag;
  ConvertDescriptor(scope, "({get: 1})", has_caught, desc_bag);
  EXPECT_TRUE(has_caught);
}

// Tests that V8ObjectToPropertyDescriptor throws a TypeError when the 'set'
// property is not callable and not undefined.
TEST(V8ObjectToPropertyDescriptorTest, NonCallableSetter) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  bool has_caught = false;
  bindings::V8PropertyDescriptorBag desc_bag;
  ConvertDescriptor(scope, "({set: 'text'})", has_caught, desc_bag);
  EXPECT_TRUE(has_caught);
}

// Tests that V8ObjectToPropertyDescriptor throws a TypeError when an inherited
// 'get' property is not callable.
TEST(V8ObjectToPropertyDescriptorTest, InheritedNonCallableGetter) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  bool has_caught = false;
  bindings::V8PropertyDescriptorBag desc_bag;
  ConvertDescriptor(scope, "Object.create({get: 1})", has_caught, desc_bag);
  EXPECT_TRUE(has_caught);
}

// Tests that V8ObjectToPropertyDescriptor accepts undefined accessors.
TEST(V8ObjectToPropertyDescriptorTest, UndefinedAccessors) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  bool has_caught = false;
  bindings::V8PropertyDescriptorBag desc_bag;
  ConvertDescriptor(scope, "({get: undefined, set: undefined})", has_caught,
                    desc_bag);
  EXPECT_FALSE(has_caught);
  EXPECT_TRUE(desc_bag.has_get);
  EXPECT_TRUE(desc_bag.has_set);
}

// Tests that V8ObjectToPropertyDescriptor accepts valid callable accessors.
TEST(V8ObjectToPropertyDescriptorTest, CallableAccessors) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  bool has_caught = false;
  bindings::V8PropertyDescriptorBag desc_bag;
  ConvertDescriptor(scope, "({get: () => 1, set: () => {}})", has_caught,
                    desc_bag);
  EXPECT_FALSE(has_caught);
  EXPECT_TRUE(desc_bag.has_get);
  EXPECT_TRUE(desc_bag.has_set);
  EXPECT_TRUE(desc_bag.get->IsFunction());
  EXPECT_TRUE(desc_bag.set->IsFunction());
}

// The property descriptor of a defineProperty trap must be rejected when its
// accessors are made non-callable during reentrant descriptor conversion.
TEST(ObservableArrayTest, DefinePropertyRejectsNonCallableAccessor) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const char* source = R"JS((() => {
    const observable = document.adoptedStyleSheets;
    const numeric = Object.create(null);
    numeric.value = 1;
    numeric.writable = true;
    numeric.configurable = true;
    const descriptor = new Proxy(Object.create(null), {
      has(_target, key) {
        if (key === 'set') {
          Reflect.defineProperty(Object.prototype, 'get', numeric);
          Reflect.defineProperty(Object.prototype, 'set', numeric);
        }
        return false;
      },
    });
    let outcome;
    try {
      outcome = 'defined:' +
          Reflect.defineProperty(observable, 'testProperty', descriptor);
    } catch (e) {
      outcome = e instanceof TypeError ? 'TypeError' : 'unexpected:' + e;
    } finally {
      Reflect.deleteProperty(Object.prototype, 'get');
      Reflect.deleteProperty(Object.prototype, 'set');
    }
    if (outcome !== 'TypeError')
      return outcome;
    if (Reflect.getOwnPropertyDescriptor(observable, 'testProperty'))
      return 'property-installed';
    return 'TypeError';
  })())JS";
  EXPECT_EQ("TypeError", RunScript(scope, source));
}

}  // namespace blink

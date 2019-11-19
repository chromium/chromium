// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

#include <limits>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/garbage_collected_script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#define TEST_TOV8(expected, value) \
  TestToV8(&scope, expected, value, __FILE__, __LINE__)

namespace blink {

namespace {

template <typename T>
void TestToV8(V8TestingScope* scope,
              const char* expected,
              T value,
              const char* path,
              int line_number) {
  v8::Local<v8::Value> actual =
      ToV8(value, scope->GetContext()->Global(), scope->GetIsolate());
  if (actual.IsEmpty()) {
    ADD_FAILURE_AT(path, line_number) << "toV8 returns an empty value.";
    return;
  }
  String actual_string =
      ToCoreString(actual->ToString(scope->GetContext()).ToLocalChecked());
  if (String(expected) != actual_string) {
    ADD_FAILURE_AT(path, line_number)
        << "toV8 returns an incorrect value.\n  Actual: "
        << actual_string.Utf8() << "\nExpected: " << expected;
    return;
  }
}

class GarbageCollectedHolderForToV8Test
    : public GarbageCollected<GarbageCollectedHolderForToV8Test> {
 public:
  GarbageCollectedHolderForToV8Test(
      GarbageCollectedScriptWrappable* script_wrappable)
      : script_wrappable_(script_wrappable) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(script_wrappable_); }

  // This should be public in order to access a Member<X> object.
  Member<GarbageCollectedScriptWrappable> script_wrappable_;
};

class OffHeapGarbageCollectedHolder {
  STACK_ALLOCATED();

 public:
  OffHeapGarbageCollectedHolder(
      GarbageCollectedScriptWrappable* script_wrappable)
      : script_wrappable_(script_wrappable) {}

  // This should be public in order to access a Persistent<X> object.
  Persistent<GarbageCollectedScriptWrappable> script_wrappable_;
};

TEST(ToV8Test, garbageCollectedScriptWrappable) {
  V8TestingScope scope;
  GarbageCollectedScriptWrappable* object =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("world");
  GarbageCollectedHolderForToV8Test holder(object);
  OffHeapGarbageCollectedHolder off_heap_holder(object);

  TEST_TOV8("world", object);
  TEST_TOV8("world", holder.script_wrappable_);
  TEST_TOV8("world", off_heap_holder.script_wrappable_);

  object = nullptr;
  holder.script_wrappable_ = nullptr;
  off_heap_holder.script_wrappable_ = nullptr;

  TEST_TOV8("null", object);
  TEST_TOV8("null", holder.script_wrappable_);
  TEST_TOV8("null", off_heap_holder.script_wrappable_);
}

TEST(ToV8Test, string) {
  V8TestingScope scope;
  char array_string[] = "arrayString";
  const char kConstArrayString[] = "constArrayString";
  TEST_TOV8("arrayString", array_string);
  TEST_TOV8("constArrayString", kConstArrayString);
  TEST_TOV8("pointer", const_cast<char*>("pointer"));
  TEST_TOV8("constPointer", static_cast<const char*>("constPointer"));
  TEST_TOV8("coreString", String("coreString"));
  TEST_TOV8("atomicString", AtomicString("atomicString"));

  // Null strings are converted to empty strings.
  TEST_TOV8("", static_cast<char*>(nullptr));
  TEST_TOV8("", static_cast<const char*>(nullptr));
  TEST_TOV8("", String());
  TEST_TOV8("", AtomicString());
}

TEST(ToV8Test, numeric) {
  V8TestingScope scope;
  TEST_TOV8("0", static_cast<int32_t>(0));
  TEST_TOV8("1", static_cast<int32_t>(1));
  TEST_TOV8("-1", static_cast<int32_t>(-1));
  TEST_TOV8("2", static_cast<uint32_t>(2));

  TEST_TOV8("-2147483648", std::numeric_limits<int32_t>::min());
  TEST_TOV8("2147483647", std::numeric_limits<int32_t>::max());
  TEST_TOV8("4294967295", std::numeric_limits<uint32_t>::max());
  // v8::Number can represent exact numbers in [-(2^53-1), 2^53-1].
  TEST_TOV8("-9007199254740991",
            static_cast<int64_t>(-9007199254740991));  // -(2^53-1)
  TEST_TOV8("9007199254740991",
            static_cast<int64_t>(9007199254740991));  // 2^53-1
  TEST_TOV8("9007199254740991",
            static_cast<uint64_t>(9007199254740991));  // 2^53-1

  TEST_TOV8("0.5", static_cast<double>(0.5));
  TEST_TOV8("-0.5", static_cast<float>(-0.5));
  TEST_TOV8("NaN", std::numeric_limits<double>::quiet_NaN());
  TEST_TOV8("Infinity", std::numeric_limits<double>::infinity());
  TEST_TOV8("-Infinity", -std::numeric_limits<double>::infinity());
}

TEST(ToV8Test, boolean) {
  V8TestingScope scope;
  TEST_TOV8("true", true);
  TEST_TOV8("false", false);
}

TEST(ToV8Test, v8Value) {
  V8TestingScope scope;
  v8::Local<v8::Value> local_value(v8::Number::New(scope.GetIsolate(), 1234));
  v8::Local<v8::Value> handle_value(v8::Number::New(scope.GetIsolate(), 5678));

  TEST_TOV8("1234", local_value);
  TEST_TOV8("5678", handle_value);
}

TEST(ToV8Test, undefinedType) {
  V8TestingScope scope;
  TEST_TOV8("undefined", ToV8UndefinedGenerator());
}

TEST(ToV8Test, scriptValue) {
  V8TestingScope scope;
  ScriptValue value(scope.GetIsolate(),
                    v8::Number::New(scope.GetIsolate(), 1234));

  TEST_TOV8("1234", value);
}

TEST(ToV8Test, stringVectors) {
  V8TestingScope scope;
  Vector<String> string_vector;
  string_vector.push_back("foo");
  string_vector.push_back("bar");
  TEST_TOV8("foo,bar", string_vector);

  Vector<AtomicString> atomic_string_vector;
  atomic_string_vector.push_back("quux");
  atomic_string_vector.push_back("bar");
  TEST_TOV8("quux,bar", atomic_string_vector);
}

TEST(ToV8Test, basicTypeVectors) {
  V8TestingScope scope;
  Vector<int32_t> int32_vector;
  int32_vector.push_back(42);
  int32_vector.push_back(23);
  TEST_TOV8("42,23", int32_vector);

  Vector<int64_t> int64_vector;
  int64_vector.push_back(31773);
  int64_vector.push_back(404);
  TEST_TOV8("31773,404", int64_vector);

  Vector<uint32_t> uint32_vector;
  uint32_vector.push_back(1);
  uint32_vector.push_back(2);
  TEST_TOV8("1,2", uint32_vector);

  Vector<uint64_t> uint64_vector;
  uint64_vector.push_back(1001);
  uint64_vector.push_back(2002);
  TEST_TOV8("1001,2002", uint64_vector);

  Vector<float> float_vector;
  float_vector.push_back(0.125);
  float_vector.push_back(1.);
  TEST_TOV8("0.125,1", float_vector);

  Vector<double> double_vector;
  double_vector.push_back(2.3);
  double_vector.push_back(4.2);
  TEST_TOV8("2.3,4.2", double_vector);

  Vector<bool> bool_vector;
  bool_vector.push_back(true);
  bool_vector.push_back(true);
  bool_vector.push_back(false);
  TEST_TOV8("true,true,false", bool_vector);
}

TEST(ToV8Test, pairVector) {
  V8TestingScope scope;
  Vector<std::pair<String, int>> pair_vector;
  pair_vector.push_back(std::make_pair("one", 1));
  pair_vector.push_back(std::make_pair("two", 2));
  TEST_TOV8("[object Object]", pair_vector);
  v8::Local<v8::Context> context = scope.GetScriptState()->GetContext();
  v8::Local<v8::Object> result =
      ToV8(pair_vector, context->Global(), scope.GetIsolate())
          ->ToObject(context)
          .ToLocalChecked();
  v8::Local<v8::Value> one =
      result->Get(context, V8String(scope.GetIsolate(), "one"))
          .ToLocalChecked();
  EXPECT_EQ(1, one->NumberValue(context).FromJust());
  v8::Local<v8::Value> two =
      result->Get(context, V8String(scope.GetIsolate(), "two"))
          .ToLocalChecked();
  EXPECT_EQ(2, two->NumberValue(context).FromJust());
}

TEST(ToV8Test, pairHeapVector) {
  V8TestingScope scope;
  HeapVector<std::pair<String, Member<GarbageCollectedScriptWrappable>>>
      pair_heap_vector;
  pair_heap_vector.push_back(std::make_pair(
      "one", MakeGarbageCollected<GarbageCollectedScriptWrappable>("foo")));
  pair_heap_vector.push_back(std::make_pair(
      "two", MakeGarbageCollected<GarbageCollectedScriptWrappable>("bar")));
  TEST_TOV8("[object Object]", pair_heap_vector);
  v8::Local<v8::Context> context = scope.GetScriptState()->GetContext();
  v8::Local<v8::Object> result =
      ToV8(pair_heap_vector, context->Global(), scope.GetIsolate())
          ->ToObject(context)
          .ToLocalChecked();
  v8::Local<v8::Value> one =
      result->Get(context, V8String(scope.GetIsolate(), "one"))
          .ToLocalChecked();
  EXPECT_TRUE(one->IsObject());
  EXPECT_EQ(String("foo"),
            ToCoreString(one->ToString(context).ToLocalChecked()));
  v8::Local<v8::Value> two =
      result->Get(context, V8String(scope.GetIsolate(), "two"))
          .ToLocalChecked();
  EXPECT_TRUE(two->IsObject());
  EXPECT_EQ(String("bar"),
            ToCoreString(two->ToString(context).ToLocalChecked()));
}

TEST(ToV8Test, stringVectorVector) {
  V8TestingScope scope;

  Vector<String> string_vector1;
  string_vector1.push_back("foo");
  string_vector1.push_back("bar");
  Vector<String> string_vector2;
  string_vector2.push_back("quux");

  Vector<Vector<String>> compound_vector;
  compound_vector.push_back(string_vector1);
  compound_vector.push_back(string_vector2);

  EXPECT_EQ(2U, compound_vector.size());
  TEST_TOV8("foo,bar,quux", compound_vector);

  v8::Local<v8::Context> context = scope.GetScriptState()->GetContext();
  v8::Local<v8::Object> result =
      ToV8(compound_vector, context->Global(), scope.GetIsolate())
          ->ToObject(context)
          .ToLocalChecked();
  v8::Local<v8::Value> vector1 = result->Get(context, 0).ToLocalChecked();
  EXPECT_TRUE(vector1->IsArray());
  EXPECT_EQ(2U, vector1.As<v8::Array>()->Length());
  v8::Local<v8::Value> vector2 = result->Get(context, 1).ToLocalChecked();
  EXPECT_TRUE(vector2->IsArray());
  EXPECT_EQ(1U, vector2.As<v8::Array>()->Length());
}

TEST(ToV8Test, heapVector) {
  V8TestingScope scope;
  HeapVector<Member<GarbageCollectedScriptWrappable>> v;
  v.push_back(MakeGarbageCollected<GarbageCollectedScriptWrappable>("hoge"));
  v.push_back(MakeGarbageCollected<GarbageCollectedScriptWrappable>("fuga"));
  v.push_back(nullptr);

  TEST_TOV8("hoge,fuga,", v);
}

TEST(ToV8Test, withScriptState) {
  V8TestingScope scope;
  ScriptValue value(scope.GetIsolate(),
                    v8::Number::New(scope.GetIsolate(), 1234.0));

  v8::Local<v8::Value> actual = ToV8(value, scope.GetScriptState());
  EXPECT_FALSE(actual.IsEmpty());

  double actual_as_number = actual.As<v8::Number>()->Value();
  EXPECT_EQ(1234.0, actual_as_number);
}

TEST(ToV8Test, nullableDouble) {
  V8TestingScope scope;
  v8::Local<v8::Object> global = scope.GetContext()->Global();
  v8::Isolate* isolate = scope.GetIsolate();
  {
    v8::Local<v8::Value> actual =
        ToV8(base::Optional<double>(42.0), global, isolate);
    ASSERT_TRUE(actual->IsNumber());
    EXPECT_EQ(42.0, actual.As<v8::Number>()->Value());
  }
  {
    v8::Local<v8::Value> actual =
        ToV8(base::Optional<double>(), global, isolate);
    EXPECT_TRUE(actual->IsNull());
  }
}

}  // namespace

}  // namespace blink

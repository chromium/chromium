// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_align_setting.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_create_html_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_file_formdata_usvstring.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/testing/garbage_collected_script_wrappable.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/dictionary_base.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

#define TEST_TOV8_TRAITS(scope, IDLType, expected, value) \
  TestToV8Traits<IDLType>(scope, expected, value, __FILE__, __LINE__)

template <typename IDLType, typename T>
void TestToV8Traits(const V8TestingScope& scope,
                    const String& expected,
                    T value,
                    const char* path,
                    int line_number) {
  v8::Local<v8::Value> actual =
      ToV8Traits<IDLType>::ToV8(scope.GetScriptState(), value);
  String actual_string =
      ToCoreString(scope.GetIsolate(),
                   actual->ToString(scope.GetContext()).ToLocalChecked());
  if (expected != actual_string) {
    ADD_FAILURE_AT(path, line_number)
        << "ToV8 returns an incorrect value.\n  Actual: "
        << actual_string.Utf8() << "\nExpected: " << expected;
    return;
  }
}

TEST(ToV8TraitsTest, Any) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  ScriptValue value(scope.GetIsolate(),
                    v8::Number::New(scope.GetIsolate(), 1234.0));
  v8::Local<v8::Value> actual1 =
      ToV8Traits<IDLAny>::ToV8(scope.GetScriptState(), value);
  double actual_as_number1 = actual1.As<v8::Number>()->Value();
  EXPECT_EQ(1234.0, actual_as_number1);

  v8::Local<v8::Value> actual2 =
      ToV8Traits<IDLAny>::ToV8(scope.GetScriptState(), actual1);
  EXPECT_EQ(actual1, actual2);
}

TEST(ToV8TraitsTest, Boolean) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLBoolean, "true", true);
  TEST_TOV8_TRAITS(scope, IDLBoolean, "false", false);
}

TEST(ToV8TraitsTest, BigInt) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  uint64_t words[5];

  // 0
  TEST_TOV8_TRAITS(
      scope, IDLBigint, "0",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 0, 0, words)
                 .ToLocalChecked()));
  // +/- 1
  words[0] = 1;
  TEST_TOV8_TRAITS(
      scope, IDLBigint, "1",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 0, 1, words)
                 .ToLocalChecked()));
  TEST_TOV8_TRAITS(
      scope, IDLBigint, "-1",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 1, 1, words)
                 .ToLocalChecked()));

  // +/- 2^64
  words[0] = 0;
  words[1] = 1;
  TEST_TOV8_TRAITS(
      scope, IDLBigint, "18446744073709551616",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 0, 2, words)
                 .ToLocalChecked()));
  TEST_TOV8_TRAITS(
      scope, IDLBigint, "-18446744073709551616",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 1, 2, words)
                 .ToLocalChecked()));

  // +/- 2^128
  words[0] = 0;
  words[1] = 0;
  words[2] = 1;
  TEST_TOV8_TRAITS(
      scope, IDLBigint, "340282366920938463463374607431768211456",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 0, 3, words)
                 .ToLocalChecked()));
  TEST_TOV8_TRAITS(
      scope, IDLBigint, "-340282366920938463463374607431768211456",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 1, 3, words)
                 .ToLocalChecked()));

  // +/- 2^320 - 1
  uint64_t max = std::numeric_limits<uint64_t>::max();
  for (int i = 0; i < 5; i++) {
    words[i] = max;
  }
  TEST_TOV8_TRAITS(
      scope, IDLBigint,
      "213598703592091008239502170616955211460270452235665276994704160782221972"
      "5780640550022962086936575",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 0, 5, words)
                 .ToLocalChecked()));
  TEST_TOV8_TRAITS(
      scope, IDLBigint,
      "-21359870359209100823950217061695521146027045223566527699470416078222197"
      "25780640550022962086936575",
      BigInt(v8::BigInt::NewFromWords(scope.GetContext(), 1, 5, words)
                 .ToLocalChecked()));
}

TEST(ToV8TraitsTest, Integer) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  // Test type matching
  // Integer
  TEST_TOV8_TRAITS(scope, IDLByte, "0", static_cast<int8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLByte, "1", static_cast<int8_t>(1));
  TEST_TOV8_TRAITS(scope, IDLByte, "-2", static_cast<int8_t>(-2));
  TEST_TOV8_TRAITS(scope, IDLShort, "0", static_cast<int16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLong, "0", static_cast<int32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongLong, "0", static_cast<int64_t>(0));
  TEST_TOV8_TRAITS(scope, IDLOctet, "0", static_cast<uint8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedShort, "0", static_cast<uint16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLong, "0", static_cast<uint32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongLong, "0", static_cast<uint64_t>(0));
  // [Clamp] Integer
  TEST_TOV8_TRAITS(scope, IDLByteClamp, "0", static_cast<int8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLShortClamp, "0", static_cast<int16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongClamp, "0", static_cast<int32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongLongClamp, "0", static_cast<int64_t>(0));
  TEST_TOV8_TRAITS(scope, IDLOctetClamp, "0", static_cast<uint8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedShortClamp, "0", static_cast<uint16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongClamp, "0", static_cast<uint32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongLongClamp, "0",
                   static_cast<uint64_t>(0));
  // [EnforceRange] Integer
  TEST_TOV8_TRAITS(scope, IDLByteEnforceRange, "0", static_cast<int8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLShortEnforceRange, "0", static_cast<int16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongEnforceRange, "0", static_cast<int32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLLongLongEnforceRange, "0",
                   static_cast<int64_t>(0));
  TEST_TOV8_TRAITS(scope, IDLOctetEnforceRange, "0", static_cast<uint8_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedShortEnforceRange, "0",
                   static_cast<uint16_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongEnforceRange, "0",
                   static_cast<uint32_t>(0));
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongLongEnforceRange, "0",
                   static_cast<uint64_t>(0));

  // Test the maximum and the minimum integer in the range
  TEST_TOV8_TRAITS(scope, IDLLong, "-2147483648",
                   std::numeric_limits<int32_t>::min());
  TEST_TOV8_TRAITS(scope, IDLLong, "2147483647",
                   std::numeric_limits<int32_t>::max());
  TEST_TOV8_TRAITS(scope, IDLUnsignedLong, "4294967295",
                   std::numeric_limits<uint32_t>::max());

  // v8::Number can represent exact numbers in [-(2^53-1), 2^53-1].
  TEST_TOV8_TRAITS(scope, IDLLongLong, "-9007199254740991",
                   static_cast<int64_t>(-9007199254740991));  // -(2^53-1)
  TEST_TOV8_TRAITS(scope, IDLLongLong, "9007199254740991",
                   static_cast<int64_t>(9007199254740991));  // 2^53-1
  TEST_TOV8_TRAITS(scope, IDLUnsignedLongLong, "9007199254740991",
                   static_cast<uint64_t>(9007199254740991));  // 2^53-1
}

TEST(ToV8TraitsTest, FloatAndDouble) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLFloat, "0.5", static_cast<float>(0.5));
  TEST_TOV8_TRAITS(scope, IDLUnrestrictedFloat, "-0.5",
                   static_cast<float>(-0.5));
  TEST_TOV8_TRAITS(scope, IDLDouble, "0.5", static_cast<double>(0.5));
  TEST_TOV8_TRAITS(scope, IDLUnrestrictedDouble, "-0.5",
                   static_cast<double>(-0.5));
  TEST_TOV8_TRAITS(scope, IDLUnrestrictedDouble, "NaN",
                   std::numeric_limits<double>::quiet_NaN());
  TEST_TOV8_TRAITS(scope, IDLUnrestrictedDouble, "Infinity",
                   std::numeric_limits<double>::infinity());
  TEST_TOV8_TRAITS(scope, IDLUnrestrictedDouble, "-Infinity",
                   -std::numeric_limits<double>::infinity());
}

TEST(ToV8TraitsTest, String) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  const String string("string");
  const char* const charptr_string = "charptrString";
  // ByteString
  TEST_TOV8_TRAITS(scope, IDLByteString, "string", string);
  TEST_TOV8_TRAITS(scope, IDLByteString, "charptrString", charptr_string);
  // DOMString
  TEST_TOV8_TRAITS(scope, IDLString, "string", string);
  TEST_TOV8_TRAITS(scope, IDLString, "charptrString", charptr_string);
  TEST_TOV8_TRAITS(scope, IDLStringLegacyNullToEmptyString, "string", string);
  TEST_TOV8_TRAITS(scope, IDLStringLegacyNullToEmptyString, "charptrString",
                   charptr_string);
  // USVString
  TEST_TOV8_TRAITS(scope, IDLUSVString, "string", string);
  TEST_TOV8_TRAITS(scope, IDLUSVString, "charptrString", charptr_string);
  // [StringContext=TrustedHTML] DOMString
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedHTML, "string", string);
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedHTML, "charptrString",
                   charptr_string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringLegacyNullToEmptyStringStringContextTrustedHTML,
                   "string", string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringLegacyNullToEmptyStringStringContextTrustedHTML,
                   "charptrString", charptr_string);
  // [StringContext=TrustedScript] DOMString
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedScript, "string",
                   string);
  TEST_TOV8_TRAITS(scope, IDLStringStringContextTrustedScript, "charptrString",
                   charptr_string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringLegacyNullToEmptyStringStringContextTrustedScript,
                   "string", string);
  TEST_TOV8_TRAITS(scope,
                   IDLStringLegacyNullToEmptyStringStringContextTrustedScript,
                   "charptrString", charptr_string);
  // [StringContext=TrustedScriptURL] USVString
  TEST_TOV8_TRAITS(scope, IDLUSVStringStringContextTrustedScriptURL, "string",
                   string);
  TEST_TOV8_TRAITS(scope, IDLUSVStringStringContextTrustedScriptURL,
                   "charptrString", charptr_string);
}

TEST(ToV8TraitsTest, EmptyString) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  const String empty_string("");
  TEST_TOV8_TRAITS(scope, IDLString, "", empty_string);
  const char* const empty = "";
  TEST_TOV8_TRAITS(scope, IDLString, "", empty);
}

TEST(ToV8TraitsTest, Object) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  Vector<String> string_vector;
  string_vector.push_back("hello");
  string_vector.push_back("world");
  ScriptValue value(scope.GetIsolate(),
                    ToV8Traits<IDLSequence<IDLString>>::ToV8(
                        scope.GetScriptState(), string_vector));
  TEST_TOV8_TRAITS(scope, IDLObject, "hello,world", value);
  v8::Local<v8::Value> actual =
      ToV8Traits<IDLObject>::ToV8(scope.GetScriptState(), value);
  EXPECT_TRUE(actual->IsObject());
}

TEST(ToV8TraitsTest, Promise) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  auto promise = ToResolvedUndefinedPromise(scope.GetScriptState());
  TEST_TOV8_TRAITS(scope, IDLPromise<IDLUndefined>, "[object Promise]",
                   promise);
}

TEST(ToV8TraitsTest, NotShared) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  auto not_shared = NotShared<DOMUint8Array>(DOMUint8Array::Create(2));
  not_shared->Data()[0] = static_cast<uint8_t>(0);
  not_shared->Data()[1] = static_cast<uint8_t>(255);
  TEST_TOV8_TRAITS(scope, NotShared<DOMUint8Array>, "0,255", not_shared);
}

TEST(ToV8TraitsTest, MaybeShared) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  auto maybe_shared = MaybeShared<DOMInt8Array>(DOMInt8Array::Create(3));
  maybe_shared->Data()[0] = static_cast<int8_t>(-128);
  maybe_shared->Data()[1] = static_cast<int8_t>(0);
  maybe_shared->Data()[2] = static_cast<int8_t>(127);
  TEST_TOV8_TRAITS(scope, MaybeShared<DOMInt8Array>, "-128,0,127",
                   maybe_shared);
}

TEST(ToV8TraitsTest, Vector) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  Vector<String> string_vector;
  string_vector.push_back("foo");
  string_vector.push_back("bar");
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLString>, "foo,bar", string_vector);
}

TEST(ToV8TraitsTest, HeapVector) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  HeapVector<Member<GarbageCollectedScriptWrappable>> heap_vector;
  heap_vector.push_back(
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("hoge"));
  heap_vector.push_back(
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("fuga"));
  TEST_TOV8_TRAITS(scope, IDLSequence<GarbageCollectedScriptWrappable>,
                   "hoge,fuga", heap_vector);

  const HeapVector<Member<GarbageCollectedScriptWrappable>>*
      const_garbage_collected_heap_vector = &heap_vector;
  TEST_TOV8_TRAITS(scope, IDLSequence<GarbageCollectedScriptWrappable>,
                   "hoge,fuga", *const_garbage_collected_heap_vector);
}

TEST(ToV8TraitsTest, BasicIDLTypeVectors) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;

  Vector<int32_t> int32_vector;
  int32_vector.push_back(42);
  int32_vector.push_back(23);
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLLong>, "42,23", int32_vector);

  Vector<int64_t> int64_vector;
  int64_vector.push_back(31773);
  int64_vector.push_back(404);
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLLongLong>, "31773,404", int64_vector);

  Vector<uint32_t> uint32_vector;
  uint32_vector.push_back(1);
  uint32_vector.push_back(2);
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLUnsignedLong>, "1,2", uint32_vector);

  Vector<uint64_t> uint64_vector;
  uint64_vector.push_back(1001);
  uint64_vector.push_back(2002);
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLUnsignedLongLong>, "1001,2002",
                   uint64_vector);

  Vector<float> float_vector;
  float_vector.push_back(0.125);
  float_vector.push_back(1.);
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLFloat>, "0.125,1", float_vector);

  Vector<double> double_vector;
  double_vector.push_back(2.3);
  double_vector.push_back(4.2);
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLDouble>, "2.3,4.2", double_vector);

  Vector<bool> bool_vector;
  bool_vector.push_back(true);
  bool_vector.push_back(true);
  bool_vector.push_back(false);
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLBoolean>, "true,true,false",
                   bool_vector);
}

TEST(ToV8TraitsTest, StringVectorVector) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;

  Vector<String> string_vector1;
  string_vector1.push_back("foo");
  string_vector1.push_back("bar");
  Vector<String> string_vector2;
  string_vector2.push_back("quux");

  Vector<Vector<String>> compound_vector;
  compound_vector.push_back(string_vector1);
  compound_vector.push_back(string_vector2);

  EXPECT_EQ(2U, compound_vector.size());
  TEST_TOV8_TRAITS(scope, IDLSequence<IDLSequence<IDLString>>, "foo,bar,quux",
                   compound_vector);

  v8::Local<v8::Value> actual =
      ToV8Traits<IDLSequence<IDLSequence<IDLString>>>::ToV8(
          scope.GetScriptState(), compound_vector);
  v8::Local<v8::Object> result =
      actual->ToObject(scope.GetContext()).ToLocalChecked();
  v8::Local<v8::Value> vector1 =
      result->Get(scope.GetContext(), 0).ToLocalChecked();
  EXPECT_TRUE(vector1->IsArray());
  EXPECT_EQ(2U, vector1.As<v8::Array>()->Length());
  v8::Local<v8::Value> vector2 =
      result->Get(scope.GetContext(), 1).ToLocalChecked();
  EXPECT_TRUE(vector2->IsArray());
  EXPECT_EQ(1U, vector2.As<v8::Array>()->Length());
}

TEST(ToV8TraitsTest, ArrayAndSequence) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  DOMPointInit* dom_point_init1 = DOMPointInit::Create();
  dom_point_init1->setW(1.0);
  DOMPointInit* dom_point_init2 = DOMPointInit::Create();
  dom_point_init2->setW(2.0);
  DOMPointInit* dom_point_init3 = DOMPointInit::Create();
  dom_point_init3->setW(3.0);
  HeapVector<Member<DOMPointInit>> dom_point_init_vector;
  dom_point_init_vector.push_back(dom_point_init1);
  dom_point_init_vector.push_back(dom_point_init2);
  v8::Local<v8::Value> v8_dom_point_init3 =
      ToV8Traits<DOMPointInit>::ToV8(scope.GetScriptState(), dom_point_init3);

  // Frozen array
  TEST_TOV8_TRAITS(scope, IDLArray<DOMPointInit>,
                   "[object Object],[object Object]", dom_point_init_vector);
  v8::Local<v8::Value> v8_frozen_array =
      ToV8Traits<IDLArray<DOMPointInit>>::ToV8(scope.GetScriptState(),
                                               dom_point_init_vector);

  bool is_value_set;
  ASSERT_TRUE(v8_frozen_array.As<v8::Object>()
                  ->Set(scope.GetContext(), 0, v8_dom_point_init3)
                  .To(&is_value_set));
  ASSERT_TRUE(is_value_set);
  v8::Local<v8::Value> element_of_frozen_array =
      v8_frozen_array.As<v8::Object>()
          ->Get(scope.GetContext(), 0)
          .ToLocalChecked();
  // An element of a frozen array cannot be changed.
  EXPECT_NE(element_of_frozen_array, v8_dom_point_init3);

  // Sequence
  TEST_TOV8_TRAITS(scope, IDLSequence<DOMPointInit>,
                   "[object Object],[object Object]", dom_point_init_vector);
  v8::Local<v8::Value> v8_sequence =
      ToV8Traits<IDLSequence<DOMPointInit>>::ToV8(scope.GetScriptState(),
                                                  dom_point_init_vector);
  ASSERT_TRUE(v8_sequence.As<v8::Object>()
                  ->Set(scope.GetContext(), 0, v8_dom_point_init3)
                  .To(&is_value_set));
  ASSERT_TRUE(is_value_set);
  v8::Local<v8::Value> element_of_sequence =
      v8_sequence.As<v8::Object>()->Get(scope.GetContext(), 0).ToLocalChecked();
  // An element of a sequence can be changed.
  EXPECT_EQ(element_of_sequence, v8_dom_point_init3);
}

TEST(ToV8TraitsTest, PairVector) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  Vector<std::pair<String, int8_t>> pair_vector;
  pair_vector.push_back(std::make_pair("one", 1));
  pair_vector.push_back(std::make_pair("two", 2));
  using ByteRecord = IDLRecord<IDLString, IDLByte>;
  TEST_TOV8_TRAITS(scope, ByteRecord, "[object Object]", pair_vector);
  v8::Local<v8::Value> actual =
      ToV8Traits<ByteRecord>::ToV8(scope.GetScriptState(), pair_vector);
  v8::Local<v8::Object> result =
      actual->ToObject(scope.GetContext()).ToLocalChecked();
  v8::Local<v8::Value> one =
      result->Get(scope.GetContext(), V8String(scope.GetIsolate(), "one"))
          .ToLocalChecked();
  EXPECT_EQ(1, one->NumberValue(scope.GetContext()).FromJust());
  v8::Local<v8::Value> two =
      result->Get(scope.GetContext(), V8String(scope.GetIsolate(), "two"))
          .ToLocalChecked();
  EXPECT_EQ(2, two->NumberValue(scope.GetContext()).FromJust());
}

TEST(ToV8TraitsTest, PairHeapVector) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  HeapVector<std::pair<String, Member<GarbageCollectedScriptWrappable>>>
      pair_heap_vector;
  pair_heap_vector.push_back(std::make_pair(
      "one", MakeGarbageCollected<GarbageCollectedScriptWrappable>("foo")));
  pair_heap_vector.push_back(std::make_pair(
      "two", MakeGarbageCollected<GarbageCollectedScriptWrappable>("bar")));
  using HeapRecord = IDLRecord<IDLString, GarbageCollectedScriptWrappable>;
  TEST_TOV8_TRAITS(scope, HeapRecord, "[object Object]", pair_heap_vector);
  v8::Local<v8::Value> actual =
      ToV8Traits<HeapRecord>::ToV8(scope.GetScriptState(), pair_heap_vector);
  v8::Local<v8::Object> result =
      actual->ToObject(scope.GetContext()).ToLocalChecked();
  v8::Local<v8::Value> one =
      result->Get(scope.GetContext(), V8String(scope.GetIsolate(), "one"))
          .ToLocalChecked();
  EXPECT_TRUE(one->IsObject());
  EXPECT_EQ(String("foo"),
            ToCoreString(scope.GetIsolate(),
                         one->ToString(scope.GetContext()).ToLocalChecked()));
  v8::Local<v8::Value> two =
      result->Get(scope.GetContext(), V8String(scope.GetIsolate(), "two"))
          .ToLocalChecked();
  EXPECT_TRUE(two->IsObject());
  EXPECT_EQ(String("bar"),
            ToCoreString(scope.GetIsolate(),
                         two->ToString(scope.GetContext()).ToLocalChecked()));
}

TEST(ToV8TraitsTest, NullStringInputForNoneNullableType) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  const String null_string;
  TEST_TOV8_TRAITS(scope, IDLString, "", null_string);
  const char* const null = nullptr;
  TEST_TOV8_TRAITS(scope, IDLString, "", null);
}

TEST(ToV8TraitsTest, Nullable) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  // Nullable Boolean
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLBoolean>, "null", std::nullopt);
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLBoolean>, "true", true);
  // Nullable Integer
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLByte>, "null", std::nullopt);
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLUnsignedLong>, "0",
                   std::optional<uint32_t>(0));
  // Nullable Float
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLFloat>, "null",
                   std::optional<float>());
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLFloat>, "0.5",
                   std::optional<float>(0.5));
  // Nullable Double
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLDouble>, "null",
                   std::optional<double>());
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLDouble>, "3.14",
                   std::optional<double>(3.14));
  // Nullable DOMHighResTimeStamp
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLDOMHighResTimeStamp>, "null",
                   std::optional<base::Time>());
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLDOMHighResTimeStamp>, "123.456",
                   std::optional<base::Time>(
                       base::Time::FromMillisecondsSinceUnixEpoch(123.456)));
}

TEST(ToV8TraitsTest, NullableString) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLString>, "null", String());
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLString>, "string", String("string"));
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLString>, "", String(""));
  const char* const null = nullptr;
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLString>, "null", null);
  const char* const charptr_string = "charptrString";
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLString>, "charptrString",
                   charptr_string);
  const char* const charptr_empty_string = "";
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLString>, "", charptr_empty_string);
}

TEST(ToV8TraitsTest, NullableObject) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(
      scope, IDLNullable<IDLObject>, "null",
      ScriptValue(scope.GetIsolate(), v8::Null(scope.GetIsolate())));

  Vector<uint8_t> uint8_vector;
  uint8_vector.push_back(static_cast<uint8_t>(0));
  uint8_vector.push_back(static_cast<uint8_t>(255));
  ScriptValue value(scope.GetIsolate(),
                    ToV8Traits<IDLNullable<IDLSequence<IDLOctet>>>::ToV8(
                        scope.GetScriptState(), uint8_vector));
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLObject>, "0,255", value);
  v8::Local<v8::Value> actual =
      ToV8Traits<IDLNullable<IDLObject>>::ToV8(scope.GetScriptState(), value);
  EXPECT_TRUE(actual->IsObject());
}

TEST(ToV8TraitsTest, NullableScriptWrappable) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLNullable<EventTarget>, "null", nullptr);
  EventTarget* event_target = EventTarget::Create(scope.GetScriptState());
  TEST_TOV8_TRAITS(scope, IDLNullable<EventTarget>, "[object EventTarget]",
                   event_target);
}

TEST(ToV8TraitsTest, NullableDictionary) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  // bindings::DictionaryBase
  TEST_TOV8_TRAITS(scope, IDLNullable<bindings::DictionaryBase>, "null",
                   nullptr);
  DOMPointInit* dom_point_init = DOMPointInit::Create();
  TEST_TOV8_TRAITS(scope, IDLNullable<DOMPointInit>, "null", nullptr);
  TEST_TOV8_TRAITS(scope, IDLNullable<DOMPointInit>, "[object Object]",
                   dom_point_init);
}

TEST(ToV8TraitsTest, NullableCallbackFunction) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLNullable<V8CreateHTMLCallback>, "null", nullptr);
  V8CreateHTMLCallback* v8_create_html_callback =
      V8CreateHTMLCallback::Create(scope.GetContext()->Global());
  TEST_TOV8_TRAITS(scope, IDLNullable<V8CreateHTMLCallback>, "[object Window]",
                   v8_create_html_callback);
}

TEST(ToV8TraitsTest, NullableCallbackInterface) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLNullable<V8CreateHTMLCallback>, "null", nullptr);
  V8EventListener* v8_event_listener =
      V8EventListener::Create(scope.GetContext()->Global());
  TEST_TOV8_TRAITS(scope, IDLNullable<V8EventListener>, "[object Window]",
                   v8_event_listener);
}

TEST(ToV8TraitsTest, NullableEnumeration) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLNullable<V8AlignSetting>, "null", std::nullopt);
  const std::optional<V8AlignSetting> v8_align_setting =
      V8AlignSetting::Create("start");
  TEST_TOV8_TRAITS(scope, IDLNullable<V8AlignSetting>, "start",
                   v8_align_setting);
}

TEST(ToV8TraitsTest, NullableArray) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLArray<DOMPointInit>>, "null",
                   std::nullopt);

  DOMPointInit* dom_point_init1 = DOMPointInit::Create();
  DOMPointInit* dom_point_init2 = DOMPointInit::Create();
  HeapVector<Member<DOMPointInit>> dom_point_init_vector;
  dom_point_init_vector.push_back(dom_point_init1);
  dom_point_init_vector.push_back(dom_point_init2);
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLArray<DOMPointInit>>,
                   "[object Object],[object Object]", dom_point_init_vector);
}

TEST(ToV8TraitsTest, NullableDate) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLNullable<IDLDate>, "null", std::nullopt);

  base::Time expected_date;
  EXPECT_TRUE(
      base::Time::FromString("Fri, 01 Jan 2021 00:00:00 GMT", &expected_date));
  v8::Local<v8::Value> result = ToV8Traits<IDLNullable<IDLDate>>::ToV8(
      scope.GetScriptState(), std::optional<base::Time>(expected_date));
  String actual_string =
      ToCoreString(scope.GetIsolate(),
                   result->ToString(scope.GetContext()).ToLocalChecked());
  base::Time actual_date;
  EXPECT_TRUE(
      base::Time::FromString(actual_string.Ascii().c_str(), &actual_date));
  EXPECT_EQ(expected_date, actual_date);
}

TEST(ToV8TraitsTest, Union) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  const auto* usv_string =
      MakeGarbageCollected<V8UnionFileOrFormDataOrUSVString>(
          "https://example.com/");
  TEST_TOV8_TRAITS(scope, V8UnionFileOrFormDataOrUSVString,
                   "https://example.com/", usv_string);
}

TEST(ToV8TraitsTest, NullableUnion) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLNullable<V8UnionFileOrFormDataOrUSVString>, "null",
                   nullptr);
  const auto* usv_string =
      MakeGarbageCollected<V8UnionFileOrFormDataOrUSVString>(
          "http://example.com/");
  TEST_TOV8_TRAITS(scope, IDLNullable<V8UnionFileOrFormDataOrUSVString>,
                   "http://example.com/", usv_string);
}

TEST(ToV8TraitsTest, Optional) {
  test::TaskEnvironment task_environment;
  const V8TestingScope scope;
  TEST_TOV8_TRAITS(scope, IDLOptional<DOMPointInit>, "undefined", nullptr);
  DOMPointInit* dom_point_init = DOMPointInit::Create();
  TEST_TOV8_TRAITS(scope, IDLOptional<DOMPointInit>, "[object Object]",
                   dom_point_init);

  TEST_TOV8_TRAITS(scope, IDLOptional<IDLAny>, "undefined", ScriptValue());
  ScriptValue value(scope.GetIsolate(),
                    v8::Number::New(scope.GetIsolate(), 3.14));
  TEST_TOV8_TRAITS(scope, IDLOptional<IDLAny>, "3.14", value);
}

}  // namespace

}  // namespace blink

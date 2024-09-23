/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/testing/file_backed_blob_factory_test_helper.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

v8::Local<v8::Object> EvaluateScriptAsObject(V8TestingScope& scope,
                                             const char* source) {
  v8::Local<v8::Script> script =
      v8::Script::Compile(scope.GetContext(),
                          V8String(scope.GetIsolate(), source))
          .ToLocalChecked();
  return script->Run(scope.GetContext()).ToLocalChecked().As<v8::Object>();
}

std::unique_ptr<IDBKey> ScriptToKey(V8TestingScope& scope, const char* source) {
  NonThrowableExceptionState exception_state;
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> context = scope.GetContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, V8String(isolate, source)).ToLocalChecked();
  v8::Local<v8::Value> value = script->Run(context).ToLocalChecked();
  return CreateIDBKeyFromValue(isolate, value, exception_state);
}

std::unique_ptr<IDBKey> CheckKeyFromValueAndKeyPathInternal(
    v8::Isolate* isolate,
    const ScriptValue& value,
    const String& key_path) {
  IDBKeyPath idb_key_path(key_path);
  EXPECT_TRUE(idb_key_path.IsValid());

  NonThrowableExceptionState exception_state;
  return CreateIDBKeyFromValueAndKeyPath(isolate, value.V8Value(), idb_key_path,
                                         exception_state);
}

void CheckKeyPathNullValue(v8::Isolate* isolate,
                           const ScriptValue& value,
                           const String& key_path) {
  ASSERT_FALSE(CheckKeyFromValueAndKeyPathInternal(isolate, value, key_path));
}

bool InjectKey(ScriptState* script_state,
               IDBKey* key,
               ScriptValue& value,
               const String& key_path) {
  IDBKeyPath idb_key_path(key_path);
  EXPECT_TRUE(idb_key_path.IsValid());
  ScriptValue key_value(script_state->GetIsolate(), key->ToV8(script_state));
  return InjectV8KeyIntoV8Value(script_state->GetIsolate(), key_value.V8Value(),
                                value.V8Value(), idb_key_path);
}

void CheckInjection(ScriptState* script_state,
                    IDBKey* key,
                    ScriptValue& value,
                    const String& key_path) {
  bool result = InjectKey(script_state, key, value, key_path);
  ASSERT_TRUE(result);
  std::unique_ptr<IDBKey> extracted_key = CheckKeyFromValueAndKeyPathInternal(
      script_state->GetIsolate(), value, key_path);
  EXPECT_TRUE(key->IsEqual(extracted_key.get()));
}

void CheckInjectionIgnored(ScriptState* script_state,
                           IDBKey* key,
                           ScriptValue& value,
                           const String& key_path) {
  bool result = InjectKey(script_state, key, value, key_path);
  ASSERT_TRUE(result);
  std::unique_ptr<IDBKey> extracted_key = CheckKeyFromValueAndKeyPathInternal(
      script_state->GetIsolate(), value, key_path);
  EXPECT_FALSE(key->IsEqual(extracted_key.get()));
}

void CheckInjectionDisallowed(ScriptState* script_state,
                              ScriptValue& value,
                              const String& key_path) {
  const IDBKeyPath idb_key_path(key_path);
  ASSERT_TRUE(idb_key_path.IsValid());
  EXPECT_FALSE(CanInjectIDBKeyIntoScriptValue(script_state->GetIsolate(), value,
                                              idb_key_path));
}

void CheckKeyPathStringValue(v8::Isolate* isolate,
                             const ScriptValue& value,
                             const String& key_path,
                             const String& expected) {
  std::unique_ptr<IDBKey> idb_key =
      CheckKeyFromValueAndKeyPathInternal(isolate, value, key_path);
  ASSERT_TRUE(idb_key);
  ASSERT_EQ(mojom::IDBKeyType::String, idb_key->GetType());
  ASSERT_TRUE(expected == idb_key->GetString());
}

void CheckKeyPathNumberValue(v8::Isolate* isolate,
                             const ScriptValue& value,
                             const String& key_path,
                             int expected) {
  std::unique_ptr<IDBKey> idb_key =
      CheckKeyFromValueAndKeyPathInternal(isolate, value, key_path);
  ASSERT_TRUE(idb_key);
  ASSERT_EQ(mojom::IDBKeyType::Number, idb_key->GetType());
  ASSERT_TRUE(expected == idb_key->Number());
}

// Compare a key against an array of keys. Supports keys with "holes" (keys of
// type None), so IDBKey::Compare() can't be used directly.
void CheckArrayKey(const IDBKey* key, const IDBKey::KeyArray& expected) {
  EXPECT_EQ(mojom::IDBKeyType::Array, key->GetType());
  const IDBKey::KeyArray& array = key->Array();
  EXPECT_EQ(expected.size(), array.size());
  for (wtf_size_t i = 0; i < array.size(); ++i) {
    EXPECT_EQ(array[i]->GetType(), expected[i]->GetType());
    if (array[i]->GetType() != mojom::IDBKeyType::None) {
      EXPECT_EQ(0, expected[i]->Compare(array[i].get()));
    }
  }
}

// SerializedScriptValue header format offsets are inferred from the Blink and
// V8 serialization code. The code below DCHECKs that
constexpr static size_t kSSVHeaderBlinkVersionTagOffset = 0;
constexpr static size_t kSSVHeaderBlinkVersionOffset = 1;
constexpr static size_t kSSVHeaderV8VersionTagOffset = 15;
// constexpr static size_t kSSVHeaderV8VersionOffset = 16;

// Follows the same steps as the IndexedDB value serialization code.
void SerializeV8Value(v8::Local<v8::Value> value,
                      v8::Isolate* isolate,
                      Vector<char>* wire_bytes) {
  NonThrowableExceptionState non_throwable_exception_state;

  SerializedScriptValue::SerializeOptions options;
  scoped_refptr<SerializedScriptValue> serialized_value =
      SerializedScriptValue::Serialize(isolate, value, options,
                                       non_throwable_exception_state);

  DCHECK(wire_bytes->empty());
  wire_bytes->AppendSpan(serialized_value->GetWireData());

  // Sanity check that the serialization header has not changed, as the tests
  // that use this method rely on the header format.
  //
  // The cast from char* to unsigned char* is necessary to avoid VS2015 warning
  // C4309 (truncation of constant value). This happens because VersionTag is
  // 0xFF.
  const unsigned char* wire_data =
      reinterpret_cast<unsigned char*>(wire_bytes->data());
  ASSERT_EQ(static_cast<unsigned char>(kVersionTag),
            wire_data[kSSVHeaderBlinkVersionTagOffset]);
  ASSERT_EQ(
      static_cast<unsigned char>(SerializedScriptValue::kWireFormatVersion),
      wire_data[kSSVHeaderBlinkVersionOffset]);

  ASSERT_EQ(static_cast<unsigned char>(kVersionTag),
            wire_data[kSSVHeaderV8VersionTagOffset]);
  // TODO(jbroman): Use the compile-time constant for V8 data format version.
  // ASSERT_EQ(v8::ValueSerializer::GetCurrentDataFormatVersion(),
  //           wire_data[kSSVHeaderV8VersionOffset]);
}

std::unique_ptr<IDBValue> CreateIDBValue(v8::Isolate* isolate,
                                         Vector<char>&& wire_bytes,
                                         double primary_key,
                                         const String& key_path) {
  auto value =
      std::make_unique<IDBValue>(std::move(wire_bytes), Vector<WebBlobInfo>());
  value->SetInjectedPrimaryKey(IDBKey::CreateNumber(primary_key),
                               IDBKeyPath(key_path));

  value->SetIsolate(isolate);
  return value;
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyStringValue) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: "zoo" }
  ScriptValue script_value = V8ObjectBuilder(scope.GetScriptState())
                                 .AddString("foo", "zoo")
                                 .GetScriptValue();
  CheckKeyPathStringValue(isolate, script_value, "foo", "zoo");
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

}  // namespace

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyNumberValue) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: 456 }
  ScriptValue script_value = V8ObjectBuilder(scope.GetScriptState())
                                 .AddNumber("foo", 456)
                                 .GetScriptValue();
  CheckKeyPathNumberValue(isolate, script_value, "foo", 456);
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

TEST(IDBKeyFromValueAndKeyPathTest, FileLastModifiedDateUseCounterTest) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  FileBackedBlobFactoryTestHelper file_factory_helper(
      scope.GetExecutionContext());
  File* file =
      MakeGarbageCollected<File>(scope.GetExecutionContext(), "/native/path");
  file_factory_helper.FlushForTesting();
  v8::Local<v8::Value> wrapper =
      ToV8Traits<File>::ToV8(scope.GetScriptState(), file);

  IDBKeyPath idb_key_path("lastModifiedDate");
  ASSERT_TRUE(idb_key_path.IsValid());

  NonThrowableExceptionState exception_state;
  ASSERT_TRUE(CreateIDBKeyFromValueAndKeyPath(scope.GetIsolate(), wrapper,
                                              idb_key_path, exception_state));
  ASSERT_TRUE(scope.GetDocument().IsUseCounted(
      WebFeature::kIndexedDBFileLastModifiedDate));
}

TEST(IDBKeyFromValueAndKeyPathTest, SubProperty) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  // object = { foo: { bar: "zee" } }
  ScriptValue script_value =
      V8ObjectBuilder(script_state)
          .Add("foo", V8ObjectBuilder(script_state).AddString("bar", "zee"))
          .GetScriptValue();
  CheckKeyPathStringValue(isolate, script_value, "foo.bar", "zee");
  CheckKeyPathNullValue(isolate, script_value, "bar");
}

TEST(IDBKeyFromValue, Number) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto key = ScriptToKey(scope, "42.0");
  EXPECT_EQ(key->GetType(), mojom::IDBKeyType::Number);
  EXPECT_EQ(key->Number(), 42);

  EXPECT_FALSE(ScriptToKey(scope, "NaN")->IsValid());
}

TEST(IDBKeyFromValue, Date) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto key = ScriptToKey(scope, "new Date(123)");
  EXPECT_EQ(key->GetType(), mojom::IDBKeyType::Date);
  EXPECT_EQ(key->Date(), 123);

  EXPECT_FALSE(ScriptToKey(scope, "new Date(NaN)")->IsValid());
  EXPECT_FALSE(ScriptToKey(scope, "new Date(Infinity)")->IsValid());
}

TEST(IDBKeyFromValue, String) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto key = ScriptToKey(scope, "'abc'");
  EXPECT_EQ(key->GetType(), mojom::IDBKeyType::String);
  EXPECT_EQ(key->GetString(), "abc");
}

TEST(IDBKeyFromValue, Binary) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  // Key which is an ArrayBuffer.
  {
    auto key = ScriptToKey(scope, "new ArrayBuffer(3)");
    EXPECT_EQ(key->GetType(), mojom::IDBKeyType::Binary);
    EXPECT_EQ(key->Binary()->data.size(), 3UL);
  }

  // Key which is a TypedArray view on an ArrayBuffer.
  {
    auto key = ScriptToKey(scope, "new Uint8Array([0,1,2])");
    EXPECT_EQ(key->GetType(), mojom::IDBKeyType::Binary);
    EXPECT_EQ(key->Binary()->data.size(), 3UL);
  }
}

TEST(IDBKeyFromValue, InvalidSimpleKeyTypes) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const char* cases[] = {
      "true", "false", "null", "undefined", "{}", "(function(){})", "/regex/",
  };

  for (const char* expr : cases)
    EXPECT_FALSE(ScriptToKey(scope, expr)->IsValid());
}

TEST(IDBKeyFromValue, SimpleArrays) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  {
    auto key = ScriptToKey(scope, "[]");
    EXPECT_EQ(key->GetType(), mojom::IDBKeyType::Array);
    EXPECT_EQ(key->Array().size(), 0UL);
  }

  {
    auto key = ScriptToKey(scope, "[0, 'abc']");
    EXPECT_EQ(key->GetType(), mojom::IDBKeyType::Array);

    const IDBKey::KeyArray& array = key->Array();
    EXPECT_EQ(array.size(), 2UL);
    EXPECT_EQ(array[0]->GetType(), mojom::IDBKeyType::Number);
    EXPECT_EQ(array[1]->GetType(), mojom::IDBKeyType::String);
  }
}

TEST(IDBKeyFromValue, NestedArray) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto key = ScriptToKey(scope, "[0, ['xyz', Infinity], 'abc']");
  EXPECT_EQ(key->GetType(), mojom::IDBKeyType::Array);

  const IDBKey::KeyArray& array = key->Array();
  EXPECT_EQ(array.size(), 3UL);
  EXPECT_EQ(array[0]->GetType(), mojom::IDBKeyType::Number);
  EXPECT_EQ(array[1]->GetType(), mojom::IDBKeyType::Array);
  EXPECT_EQ(array[1]->Array().size(), 2UL);
  EXPECT_EQ(array[1]->Array()[0]->GetType(), mojom::IDBKeyType::String);
  EXPECT_EQ(array[1]->Array()[1]->GetType(), mojom::IDBKeyType::Number);
  EXPECT_EQ(array[2]->GetType(), mojom::IDBKeyType::String);
}

TEST(IDBKeyFromValue, CircularArray) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto key = ScriptToKey(scope,
                         "(() => {"
                         "  const a = [];"
                         "  a.push(a);"
                         "  return a;"
                         "})()");
  EXPECT_FALSE(key->IsValid());
}

TEST(IDBKeyFromValue, DeepArray) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto key = ScriptToKey(scope,
                         "(() => {"
                         "  let a = [];"
                         "  for (let i = 0; i < 10000; ++i) { a.push(a); }"
                         "  return a;"
                         "})()");
  EXPECT_FALSE(key->IsValid());
}

TEST(IDBKeyFromValue, SparseArray) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto key = ScriptToKey(scope, "[,1]");
  EXPECT_FALSE(key->IsValid());

  // Ridiculously large sparse array - ensure we check before allocating.
  key = ScriptToKey(scope, "Object.assign([], {length: 2e9})");
  EXPECT_FALSE(key->IsValid());

  // Large sparse arrays as subkeys - ensure we check while recursing.
  key = ScriptToKey(scope, "[Object.assign([], {length: 2e9})]");
  EXPECT_FALSE(key->IsValid());
}

TEST(IDBKeyFromValue, ShrinkingArray) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto key = ScriptToKey(
      scope,
      "(() => {"
      "  const a = [0, 1, 2];"
      "  Object.defineProperty(a, 1, {get: () => { a.length = 2; return 1; }});"
      "  return a;"
      "})()");
  EXPECT_FALSE(key->IsValid());
}

TEST(IDBKeyFromValue, Exceptions) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const char* cases[] = {
      // Detached ArrayBuffer.
      "(() => {"
      "  const a = new ArrayBuffer(3);"
      "  postMessage(a, '*', [a]);"
      "  return a;"
      "})()",

      // Detached ArrayBuffer view.
      "(() => {"
      "  const a = new Uint8Array([0,1,2]);"
      "  postMessage(a.buffer, '*', [a.buffer]);"
      "  return a;"
      "})()",

      // Value is an array with a getter that throws.
      "(()=>{"
      "  const a = [0, 1, 2];"
      "  Object.defineProperty(a, 1, {get: () => { throw Error(); }});"
      "  return a;"
      "})()",

      // Value is an array containing an array with a getter that throws.
      "(()=>{"
      "  const a = [0, 1, 2];"
      "  Object.defineProperty(a, 1, {get: () => { throw Error(); }});"
      "  return ['x', a, 'z'];"
      "})()",

      // Array with unconvertable item
      "(() => {"
      "  const a = new ArrayBuffer(3);"
      "  postMessage(a, '*', [a]);"
      "  return [a];"
      "})()",
  };

  for (const char* source : cases) {
    DummyExceptionStateForTesting exception_state;
    auto key = CreateIDBKeyFromValue(scope.GetIsolate(),
                                     EvaluateScriptAsObject(scope, source),
                                     exception_state);
    EXPECT_FALSE(key->IsValid());
    EXPECT_TRUE(exception_state.HadException());
  }
}

TEST(IDBKeyFromValueAndKeyPathTest, Exceptions) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Local<v8::Value> value = EvaluateScriptAsObject(
      scope, "({id:1, get throws() { throw Error(); }})");
  {
    // Key path references a property that throws.
    DummyExceptionStateForTesting exception_state;
    EXPECT_FALSE(CreateIDBKeyFromValueAndKeyPath(
        scope.GetIsolate(), value, IDBKeyPath("throws"), exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    // Compound key path references a property that throws.
    DummyExceptionStateForTesting exception_state;
    EXPECT_FALSE(CreateIDBKeyFromValueAndKeyPath(
        scope.GetIsolate(), value, IDBKeyPath(Vector<String>{"id", "throws"}),
        exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    // Compound key path references a property that throws, index case.
    DummyExceptionStateForTesting exception_state;
    EXPECT_FALSE(CreateIDBKeyFromValueAndKeyPaths(
        scope.GetIsolate(), value,
        /*store_key_path=*/IDBKeyPath("id"),
        /*index_key_path=*/IDBKeyPath(Vector<String>{"id", "throws"}),
        exception_state));
    EXPECT_TRUE(exception_state.HadException());
  }
}

TEST(IDBKeyFromValueAndKeyPathsTest, IndexKeys) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();
  NonThrowableExceptionState exception_state;

  // object = { foo: { bar: "zee" }, bad: null }
  v8::Local<v8::Value> value =
      V8ObjectBuilder(script_state)
          .Add("foo", V8ObjectBuilder(script_state).AddString("bar", "zee"))
          .AddNull("bad")
          .V8Value();

  // Index key path member matches store key path.
  std::unique_ptr<IDBKey> key = CreateIDBKeyFromValueAndKeyPaths(
      isolate, value,
      /*store_key_path=*/IDBKeyPath("id"),
      /*index_key_path=*/IDBKeyPath(Vector<String>{"id", "foo.bar"}),
      exception_state);
  IDBKey::KeyArray expected;
  expected.emplace_back(IDBKey::CreateNone());
  expected.emplace_back(IDBKey::CreateString("zee"));
  CheckArrayKey(key.get(), expected);

  // Index key path member matches, but there are unmatched members too.
  EXPECT_FALSE(CreateIDBKeyFromValueAndKeyPaths(
      isolate, value,
      /*store_key_path=*/IDBKeyPath("id"),
      /*index_key_path=*/IDBKeyPath(Vector<String>{"id", "foo.bar", "nope"}),
      exception_state));

  // Index key path member matches, but there are invalid subkeys too.
  EXPECT_FALSE(
      CreateIDBKeyFromValueAndKeyPaths(
          isolate, value,
          /*store_key_path=*/IDBKeyPath("id"),
          /*index_key_path=*/IDBKeyPath(Vector<String>{"id", "foo.bar", "bad"}),
          exception_state)
          ->IsValid());

  // Index key path member does not match store key path.
  EXPECT_FALSE(CreateIDBKeyFromValueAndKeyPaths(
      isolate, value,
      /*store_key_path=*/IDBKeyPath("id"),
      /*index_key_path=*/IDBKeyPath(Vector<String>{"id2", "foo.bar"}),
      exception_state));

  // Index key path is not array, matches store key path.
  EXPECT_FALSE(CreateIDBKeyFromValueAndKeyPaths(
      isolate, value,
      /*store_key_path=*/IDBKeyPath("id"),
      /*index_key_path=*/IDBKeyPath("id"), exception_state));
}

TEST(InjectIDBKeyTest, ImplicitValues) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  {
    v8::Local<v8::String> string = V8String(isolate, "string");
    ScriptValue value = ScriptValue(scope.GetIsolate(), string);
    std::unique_ptr<IDBKey> idb_key = IDBKey::CreateNumber(123);
    CheckInjectionIgnored(scope.GetScriptState(), idb_key.get(), value,
                          "length");
  }
  {
    v8::Local<v8::Array> array = v8::Array::New(isolate);
    ScriptValue value = ScriptValue(scope.GetIsolate(), array);
    std::unique_ptr<IDBKey> idb_key = IDBKey::CreateNumber(456);
    CheckInjectionIgnored(scope.GetScriptState(), idb_key.get(), value,
                          "length");
  }
}

TEST(InjectIDBKeyTest, TopLevelPropertyStringValue) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  // object = { foo: "zoo" }
  ScriptValue script_object = V8ObjectBuilder(scope.GetScriptState())
                                  .AddString("foo", "zoo")
                                  .GetScriptValue();
  std::unique_ptr<IDBKey> idb_string_key = IDBKey::CreateString("myNewKey");
  CheckInjection(scope.GetScriptState(), idb_string_key.get(), script_object,
                 "bar");
  std::unique_ptr<IDBKey> idb_number_key = IDBKey::CreateNumber(1234);
  CheckInjection(scope.GetScriptState(), idb_number_key.get(), script_object,
                 "bar");

  CheckInjectionDisallowed(scope.GetScriptState(), script_object, "foo.bar");
}

TEST(InjectIDBKeyTest, SubProperty) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // object = { foo: { bar: "zee" } }
  ScriptValue script_object =
      V8ObjectBuilder(script_state)
          .Add("foo", V8ObjectBuilder(script_state).AddString("bar", "zee"))
          .GetScriptValue();

  std::unique_ptr<IDBKey> idb_string_key = IDBKey::CreateString("myNewKey");
  CheckInjection(scope.GetScriptState(), idb_string_key.get(), script_object,
                 "foo.baz");
  std::unique_ptr<IDBKey> idb_number_key = IDBKey::CreateNumber(789);
  CheckInjection(scope.GetScriptState(), idb_number_key.get(), script_object,
                 "foo.baz");
  std::unique_ptr<IDBKey> idb_date_key = IDBKey::CreateDate(4567);
  CheckInjection(scope.GetScriptState(), idb_date_key.get(), script_object,
                 "foo.baz");
  CheckInjection(scope.GetScriptState(), idb_date_key.get(), script_object,
                 "bar");
  std::unique_ptr<IDBKey> idb_array_key =
      IDBKey::CreateArray(IDBKey::KeyArray());
  CheckInjection(scope.GetScriptState(), idb_array_key.get(), script_object,
                 "foo.baz");
  CheckInjection(scope.GetScriptState(), idb_array_key.get(), script_object,
                 "bar");

  CheckInjectionDisallowed(scope.GetScriptState(), script_object,
                           "foo.bar.baz");
  std::unique_ptr<IDBKey> idb_zoo_key = IDBKey::CreateString("zoo");
  CheckInjection(scope.GetScriptState(), idb_zoo_key.get(), script_object,
                 "foo.xyz.foo");
}

TEST(DeserializeIDBValueTest, CurrentVersions) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  Vector<char> object_bytes;
  v8::Local<v8::Object> empty_object = v8::Object::New(isolate);
  SerializeV8Value(empty_object, isolate, &object_bytes);
  std::unique_ptr<IDBValue> idb_value =
      CreateIDBValue(isolate, std::move(object_bytes), 42.0, "foo");

  v8::Local<v8::Value> v8_value =
      DeserializeIDBValue(scope.GetScriptState(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());

  ASSERT_TRUE(v8_value->IsObject());
  v8::Local<v8::Object> v8_value_object = v8_value.As<v8::Object>();
  v8::Local<v8::Value> v8_number_value =
      v8_value_object->Get(scope.GetContext(), V8AtomicString(isolate, "foo"))
          .ToLocalChecked();
  ASSERT_TRUE(v8_number_value->IsNumber());
  v8::Local<v8::Number> v8_number = v8_number_value.As<v8::Number>();
  EXPECT_EQ(v8_number->Value(), 42.0);
}

TEST(DeserializeIDBValueTest, FutureV8Version) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Pretend that the object was serialized by a future version of V8.
  Vector<char> object_bytes;
  v8::Local<v8::Object> empty_object = v8::Object::New(isolate);
  SerializeV8Value(empty_object, isolate, &object_bytes);
  object_bytes[kSSVHeaderV8VersionTagOffset] += 1;

  // The call sequence below mimics IndexedDB's usage pattern when attempting to
  // read a value in an object store with a key generator and a key path, but
  // the serialized value uses a newer format version.
  //
  // http://crbug.com/703704 has a reproduction for this test's circumstances.
  std::unique_ptr<IDBValue> idb_value =
      CreateIDBValue(isolate, std::move(object_bytes), 42.0, "foo");

  v8::Local<v8::Value> v8_value =
      DeserializeIDBValue(scope.GetScriptState(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());
  EXPECT_TRUE(v8_value->IsNull());
}

TEST(DeserializeIDBValueTest, InjectionIntoNonObject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Simulate a storage corruption where an object is read back as a number.
  // This test uses a one-segment key path.
  Vector<char> object_bytes;
  v8::Local<v8::Number> number = v8::Number::New(isolate, 42.0);
  SerializeV8Value(number, isolate, &object_bytes);
  std::unique_ptr<IDBValue> idb_value =
      CreateIDBValue(isolate, std::move(object_bytes), 42.0, "foo");

  v8::Local<v8::Value> v8_value =
      DeserializeIDBValue(scope.GetScriptState(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(v8_value->IsNumber());
  v8::Local<v8::Number> v8_number = v8_value.As<v8::Number>();
  EXPECT_EQ(v8_number->Value(), 42.0);
}

TEST(DeserializeIDBValueTest, NestedInjectionIntoNonObject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Simulate a storage corruption where an object is read back as a number.
  // This test uses a multiple-segment key path.
  Vector<char> object_bytes;
  v8::Local<v8::Number> number = v8::Number::New(isolate, 42.0);
  SerializeV8Value(number, isolate, &object_bytes);
  std::unique_ptr<IDBValue> idb_value =
      CreateIDBValue(isolate, std::move(object_bytes), 42.0, "foo.bar");

  v8::Local<v8::Value> v8_value =
      DeserializeIDBValue(scope.GetScriptState(), idb_value.get());
  EXPECT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(v8_value->IsNumber());
  v8::Local<v8::Number> v8_number = v8_value.As<v8::Number>();
  EXPECT_EQ(v8_number->Value(), 42.0);
}

}  // namespace blink

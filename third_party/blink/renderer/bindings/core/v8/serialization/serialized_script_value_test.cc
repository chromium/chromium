// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_data.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/testing/file_backed_blob_factory_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

TEST(SerializedScriptValueTest, WireFormatRoundTrip) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  v8::Local<v8::Value> v8OriginalTrue = v8::True(scope.GetIsolate());
  scoped_refptr<SerializedScriptValue> sourceSerializedScriptValue =
      SerializedScriptValue::Serialize(
          scope.GetIsolate(), v8OriginalTrue,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);

  base::span<const uint8_t> wire_data =
      sourceSerializedScriptValue->GetWireData();

  scoped_refptr<SerializedScriptValue> serializedScriptValue =
      SerializedScriptValue::Create(wire_data);
  v8::Local<v8::Value> deserialized =
      serializedScriptValue->Deserialize(scope.GetIsolate());
  EXPECT_TRUE(deserialized->IsTrue());
}

TEST(SerializedScriptValueTest, WireFormatVersion17NoByteSwapping) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const uint8_t data[] = {0xFF, 0x11, 0xFF, 0x0D, 0x54, 0x00};
  scoped_refptr<SerializedScriptValue> serializedScriptValue =
      SerializedScriptValue::Create(data);
  v8::Local<v8::Value> deserialized =
      serializedScriptValue->Deserialize(scope.GetIsolate());
  EXPECT_TRUE(deserialized->IsTrue());
}

TEST(SerializedScriptValueTest, WireFormatVersion16ByteSwapping) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  // Using UChar instead of uint8_t to get ntohs() byte swapping.
  const UChar data[] = {0xFF10, 0xFF0D, 0x5400};
  scoped_refptr<SerializedScriptValue> serializedScriptValue =
      SerializedScriptValue::Create(base::as_bytes(base::make_span(data)));
  v8::Local<v8::Value> deserialized =
      serializedScriptValue->Deserialize(scope.GetIsolate());
  EXPECT_TRUE(deserialized->IsTrue());
}

TEST(SerializedScriptValueTest, WireFormatVersion13ByteSwapping) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  // Using UChar instead of uint8_t to get ntohs() byte swapping.
  const UChar data[] = {0xFF0D, 0x5400};
  scoped_refptr<SerializedScriptValue> serializedScriptValue =
      SerializedScriptValue::Create(base::as_bytes(base::make_span(data)));
  v8::Local<v8::Value> deserialized =
      serializedScriptValue->Deserialize(scope.GetIsolate());
  EXPECT_TRUE(deserialized->IsTrue());
}

TEST(SerializedScriptValueTest, WireFormatVersion0ByteSwapping) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  // Using UChar instead of uint8_t to get ntohs() byte swapping.
  const UChar data[] = {0x5400};
  scoped_refptr<SerializedScriptValue> serializedScriptValue =
      SerializedScriptValue::Create(base::as_bytes(base::make_span(data)));
  v8::Local<v8::Value> deserialized =
      serializedScriptValue->Deserialize(scope.GetIsolate());
  EXPECT_TRUE(deserialized->IsTrue());
}

TEST(SerializedScriptValueTest, WireFormatVersion0ImageData) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Using UChar instead of uint8_t to get ntohs() byte swapping.
  //
  // This builds the smallest possible ImageData whose first data byte is 0xFF,
  // as follows.
  //
  // width = 127, encoded as 0xFF 0x00 (degenerate varint)
  // height = 1, encoded as 0x01 (varint)
  // pixelLength = 508 (127 * 1 * 4), encoded as 0xFC 0x03 (varint)
  // pixel data = 508 bytes, all zero
  Vector<UChar> data;
  data.push_back(0x23FF);
  data.push_back(0x001);
  data.push_back(0xFC03);
  data.resize(257);  // (508 pixel data + 6 header bytes) / 2

  scoped_refptr<SerializedScriptValue> serializedScriptValue =
      SerializedScriptValue::Create(base::as_bytes(base::make_span(data)));
  v8::Local<v8::Value> deserialized =
      serializedScriptValue->Deserialize(isolate);
  ASSERT_TRUE(deserialized->IsObject());
  v8::Local<v8::Object> deserializedObject = deserialized.As<v8::Object>();
  ImageData* imageData = V8ImageData::ToWrappable(isolate, deserializedObject);
  ASSERT_NE(imageData, nullptr);
  EXPECT_EQ(imageData->width(), 127);
  EXPECT_EQ(imageData->height(), 1);
}

TEST(SerializedScriptValueTest, UserSelectedFile) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FileBackedBlobFactoryTestHelper file_factory_helper(
      scope.GetExecutionContext());
  String file_path = test::BlinkRootDir() +
                     "/renderer/bindings/core/v8/serialization/"
                     "serialized_script_value_test.cc";
  auto* original_file =
      MakeGarbageCollected<File>(scope.GetExecutionContext(), file_path);
  file_factory_helper.FlushForTesting();
  ASSERT_TRUE(original_file->HasBackingFile());
  ASSERT_EQ(File::kIsUserVisible, original_file->GetUserVisibility());
  ASSERT_EQ(file_path, original_file->GetPath());

  v8::Local<v8::Value> v8_original_file =
      ToV8Traits<File>::ToV8(scope.GetScriptState(), original_file);
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          scope.GetIsolate(), v8_original_file,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);
  v8::Local<v8::Value> v8_file =
      serialized_script_value->Deserialize(scope.GetIsolate());

  File* file = V8File::ToWrappable(scope.GetIsolate(), v8_file);
  ASSERT_NE(file, nullptr);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsUserVisible, file->GetUserVisibility());
  EXPECT_EQ(file_path, file->GetPath());
}

TEST(SerializedScriptValueTest, FileConstructorFile) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<BlobDataHandle> blob_data_handle = BlobDataHandle::Create();
  auto* original_file = MakeGarbageCollected<File>(
      "hello.txt", base::Time::FromMillisecondsSinceUnixEpoch(12345678.0),
      blob_data_handle);
  ASSERT_FALSE(original_file->HasBackingFile());
  ASSERT_EQ(File::kIsNotUserVisible, original_file->GetUserVisibility());
  ASSERT_EQ("hello.txt", original_file->name());

  v8::Local<v8::Value> v8_original_file =
      ToV8Traits<File>::ToV8(scope.GetScriptState(), original_file);
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          scope.GetIsolate(), v8_original_file,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);
  v8::Local<v8::Value> v8_file =
      serialized_script_value->Deserialize(scope.GetIsolate());

  File* file = V8File::ToWrappable(scope.GetIsolate(), v8_file);
  ASSERT_NE(file, nullptr);
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_EQ(File::kIsNotUserVisible, file->GetUserVisibility());
  EXPECT_EQ("hello.txt", file->name());
}

}  // namespace blink

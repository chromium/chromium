// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/message_payload.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/containers/span.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/MessagePayloadJni_jni.h"

namespace content {
namespace {

TEST(MessagePayloadTest, SelfTest_String) {
  std::u16string string = u"Hello";

  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(string));
  EXPECT_EQ(string, std::get<std::u16string>(generated_message));
}

TEST(MessagePayloadTest, SelfTest_ArrayBuffer) {
  std::vector<uint8_t> data(200, 0XFF);
  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(
          blink::WebMessageArrayBufferPayload::CreateForTesting(data)));
  const auto& array_buffer =
      std::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
          generated_message);
  EXPECT_EQ(data.size(), array_buffer->GetLength());
  EXPECT_FALSE(array_buffer->GetAsSpanIfPossible());
  std::vector<uint8_t> copied_data(data.size());
  array_buffer->CopyInto(base::span(copied_data));
  EXPECT_EQ(data, copied_data);

  // Encode the message and decode it again. This time the ArrayBuffer should be
  // stored in Java ByteArray, which does not support |GetArrayBuffer|.
  auto generated_message2 = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(generated_message));
  const auto& array_buffer2 =
      std::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
          generated_message2);
  EXPECT_EQ(data.size(), array_buffer2->GetLength());
  copied_data.clear();
  copied_data.resize(data.size());
  array_buffer->CopyInto(base::span(copied_data));
  EXPECT_EQ(data, copied_data);
}

TEST(MessagePayloadTest, SelfTest_ArrayBufferEmpty) {
  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(
          blink::WebMessageArrayBufferPayload::CreateForTesting(
              std::vector<uint8_t>())));
  EXPECT_EQ(std::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
                generated_message)
                ->GetLength(),
            0u);
}

TEST(MessagePayloadTest, SelfTest_SharedArrayBuffer) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78,
                                     0x9a, 0xbc, 0xde, 0xf0};
  auto java_message =
      Java_MessagePayloadJni_createFromSharedArrayBufferForTesting(
          env, base::android::ToJavaByteArray(env, data));
  auto generated_message =
      android::ConvertToWebMessagePayloadFromJava(java_message);
  ASSERT_TRUE(std::holds_alternative<base::UnsafeSharedMemoryRegion>(
      generated_message));
  const auto& region =
      std::get<base::UnsafeSharedMemoryRegion>(generated_message);
  ASSERT_TRUE(region.IsValid());
  // Android ASharedMemory rounds sizes up to the page size, so the region size
  // is at least the requested buffer length.
  EXPECT_GE(region.GetSize(), data.size());
  base::WritableSharedMemoryMapping mapping = region.MapAt(0, data.size());
  ASSERT_TRUE(mapping.IsValid());
  EXPECT_EQ(mapping.GetMemoryAsSpan<const uint8_t>(), base::span(data));
}

}  // namespace
}  // namespace content

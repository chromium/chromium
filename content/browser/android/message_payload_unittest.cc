// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/message_payload.h"

#include <cstddef>
#include <memory>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content {
namespace {

TEST(MessagePayloadTest, SelfTest_String) {
  std::u16string string = u"Hello";

  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(string));
  EXPECT_EQ(blink::WebMessagePayload(string), generated_message);
}

TEST(MessagePayloadTest, SelfTest_ArrayBuffer) {
  std::vector<uint8_t> data(200, 0XFF);
  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(
          blink::WebMessageArrayBufferPayload::CreateForTesting(data)));
  const auto& array_buffer =
      absl::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
          generated_message);
  EXPECT_EQ(data.size(), array_buffer->GetLength());
  EXPECT_FALSE(array_buffer->GetAsSpanIfPossible());
  std::vector<uint8_t> copied_data(data.size());
  array_buffer->CopyInto(base::make_span(copied_data));
  EXPECT_EQ(data, copied_data);

  // Encode the message and decode it again. This time the ArrayBuffer should be
  // stored in Java ByteArray, which does not support |GetArrayBuffer|.
  auto generated_message2 = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(generated_message));
  const auto& array_buffer2 =
      absl::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
          generated_message2);
  EXPECT_EQ(data.size(), array_buffer2->GetLength());
  copied_data.clear();
  copied_data.resize(data.size());
  array_buffer->CopyInto(base::make_span(copied_data));
  EXPECT_EQ(data, copied_data);
}

TEST(MessagePayloadTest, SelfTest_ArrayBufferEmpty) {
  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(
          blink::WebMessageArrayBufferPayload::CreateForTesting(
              std::vector<uint8_t>())));
  EXPECT_EQ(absl::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
                generated_message)
                ->GetLength(),
            0u);
}

}  // namespace
}  // namespace content

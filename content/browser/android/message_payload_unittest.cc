// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/message_payload.h"
#include <cstddef>
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
      android::ConvertWebMessagePayloadToJava(data));
  EXPECT_EQ(blink::WebMessagePayload(data), generated_message);
}

TEST(MessagePayloadTest, SelfTest_ArrayBufferEmpty) {
  std::vector<uint8_t> data;
  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(data));
  EXPECT_EQ(blink::WebMessagePayload(data), generated_message);
}

}  // namespace
}  // namespace content

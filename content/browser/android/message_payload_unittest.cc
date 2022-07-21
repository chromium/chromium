// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/message_payload.h"
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content {
namespace {

TEST(MessagePayloadTest, SelfTest_String) {
  std::u16string string = u"Hello";
  blink::TransferableMessage message = blink::EncodeWebMessagePayload(string);

  auto generated_message =
      android::CreateTransferableMessageFromJavaMessagePayload(
          android::CreateJavaMessagePayload(message));
  EXPECT_EQ(message.encoded_message.size(),
            generated_message.encoded_message.size());
  for (size_t i = 0; i != message.encoded_message.size(); ++i) {
    EXPECT_EQ(message.encoded_message[i], generated_message.encoded_message[i]);
  }
}

TEST(MessagePayloadTest, SelfTest_InvalidString) {
  blink::TransferableMessage message;
  // Construct invalid message.
  message.owned_encoded_message = {0x1, 0x2, 0x3};
  message.encoded_message = message.owned_encoded_message;
  auto java_msg = android::CreateJavaMessagePayload(message);
  EXPECT_TRUE(java_msg.is_null());
}

}  // namespace
}  // namespace content

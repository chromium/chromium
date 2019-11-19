// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_message_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {
namespace {

const char kPushMessageData[] = "Push Message valid data string.";

TEST(PushMessageDataTest, ValidPayload) {
  // Create a WebString with the test message, then create a
  // PushMessageData from that.
  WebString s(blink::WebString::FromUTF8(kPushMessageData));
  PushMessageData* data = PushMessageData::Create(s);

  ASSERT_NE(data, nullptr);
  EXPECT_EQ(kPushMessageData, data->text());
}

TEST(PushMessageDataTest, ValidEmptyPayload) {
  // Create a WebString with a valid but empty test message, then create
  // a PushMessageData from that.
  WebString s("");
  PushMessageData* data = PushMessageData::Create(s);

  ASSERT_NE(data, nullptr);
  EXPECT_EQ("", data->text().Utf8());
}

TEST(PushMessageDataTest, NullPayload) {
  // Create a PushMessageData with a null payload.
  WebString s;
  PushMessageData* data = PushMessageData::Create(s);

  EXPECT_EQ(data, nullptr);
}

}  // anonymous namespace
}  // namespace blink

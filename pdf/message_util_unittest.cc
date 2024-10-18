// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/message_util.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

TEST(PrepareReplyMessageTest, BasicReply) {
  base::Value::Dict message;
  message.Set("type", "typeBasic");
  message.Set("messageId", "messageIdBasic");

  base::Value::Dict reply = PrepareReplyMessage(message);

  const std::string* reply_type = reply.FindString("type");
  ASSERT_TRUE(reply_type);
  EXPECT_EQ("typeBasicReply", *reply_type);

  const std::string* reply_message_id = reply.FindString("messageId");
  ASSERT_TRUE(reply_message_id);
  EXPECT_EQ("messageIdBasic", *reply_message_id);
}

}  // namespace chrome_pdf

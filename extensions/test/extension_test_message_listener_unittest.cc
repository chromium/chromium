// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/extension_test_message_listener.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

const char kFormat[] = "[\"%s\"]";
const char kTestMessage[] = "test message";
const char kTestMessage2[] = "test message 2";
const char kFailureMessage[] = "failure";

using ExtensionTestMessageListenerUnittest = ApiUnitTest;

TEST_F(ExtensionTestMessageListenerUnittest, BasicTestExtensionMessageTest) {
  // A basic test of sending a message and ensuring the listener is satisfied.
  {
    ExtensionTestMessageListener listener(kTestMessage);
    EXPECT_FALSE(listener.was_satisfied());
    RunFunction(base::MakeRefCounted<TestSendMessageFunction>(),
                base::StringPrintf(kFormat, kTestMessage));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_EQ(kTestMessage, listener.message());
  }

  // Test that we can receive an arbitrary message.
  {
    ExtensionTestMessageListener listener;  // won't reply
    EXPECT_FALSE(listener.was_satisfied());
    RunFunction(base::MakeRefCounted<TestSendMessageFunction>(),
                base::StringPrintf(kFormat, kTestMessage2));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_EQ(kTestMessage2, listener.message());
  }

  // Test that we can set the listener to be reused, and send/receive multiple
  // messages.
  {
    ExtensionTestMessageListener listener;  // won't reply
    EXPECT_FALSE(listener.was_satisfied());
    RunFunction(base::MakeRefCounted<TestSendMessageFunction>(),
                base::StringPrintf(kFormat, kTestMessage));
    EXPECT_EQ(kTestMessage, listener.message());
    EXPECT_TRUE(listener.was_satisfied());
    listener.Reset();
    EXPECT_FALSE(listener.was_satisfied());
    EXPECT_TRUE(listener.message().empty());
    RunFunction(base::MakeRefCounted<TestSendMessageFunction>(),
                base::StringPrintf(kFormat, kTestMessage2));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_EQ(kTestMessage2, listener.message());
  }

  // Test that we can listen for two explicit messages: a success, and a
  // failure.
  {
    ExtensionTestMessageListener listener(kTestMessage);
    listener.set_failure_message(kFailureMessage);
    RunFunction(base::MakeRefCounted<TestSendMessageFunction>(),
                base::StringPrintf(kFormat, kTestMessage));
    EXPECT_TRUE(listener.WaitUntilSatisfied());  // succeeds
    EXPECT_EQ(kTestMessage, listener.message());
    listener.Reset();
    RunFunction(base::MakeRefCounted<TestSendMessageFunction>(),
                base::StringPrintf(kFormat, kFailureMessage));
    EXPECT_FALSE(listener.WaitUntilSatisfied());  // fails
    EXPECT_EQ(kFailureMessage, listener.message());
  }
}

}  // namespace
}  // namespace extensions

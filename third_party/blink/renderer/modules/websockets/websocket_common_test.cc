// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "third_party/blink/renderer/modules/websockets/websocket_common.h"

#include <string.h>

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Connect() and CloseInternal() are very thoroughly tested by DOMWebSocket unit
// tests, so the rests aren't duplicated here.

// This test also indirectly tests IsValidSubprotocolCharacter.
TEST(WebSocketCommonTest, IsValidSubprotocolString) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(WebSocketCommon::IsValidSubprotocolString("Helloworld!!"));
  EXPECT_FALSE(WebSocketCommon::IsValidSubprotocolString("Hello, world!!"));
  EXPECT_FALSE(WebSocketCommon::IsValidSubprotocolString(String()));
  EXPECT_FALSE(WebSocketCommon::IsValidSubprotocolString(""));

  const String valid_characters(
      "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`"
      "abcdefghijklmnopqrstuvwxyz|~");
  for (wtf_size_t i = 0; i < valid_characters.length(); ++i) {
    EXPECT_TRUE(WebSocketCommon::IsValidSubprotocolString(
        valid_characters.Substring(i, 1u)));
  }
  for (size_t i = 0; i < 256; ++i) {
    LChar to_check = static_cast<LChar>(i);
    if (valid_characters.find(to_check) != kNotFound) {
      continue;
    }
    String s(base::span_from_ref(to_check));
    EXPECT_FALSE(WebSocketCommon::IsValidSubprotocolString(s));
  }
}

TEST(WebSocketCommonTest, EncodeSubprotocolString) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ("\\\\\\u0009\\u000D\\uFE0F ~hello\\u000A",
            WebSocketCommon::EncodeSubprotocolString(u"\\\t\r\uFE0F ~hello\n"));
}

TEST(WebSocketCommonTest, JoinStrings) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ("", WebSocketCommon::JoinStrings({}, ","));
  EXPECT_EQ("ab", WebSocketCommon::JoinStrings({"ab"}, ","));
  EXPECT_EQ("ab,c", WebSocketCommon::JoinStrings({"ab", "c"}, ","));
  EXPECT_EQ("a\r\nbcd\r\nef",
            WebSocketCommon::JoinStrings({"a", "bcd", "ef"}, "\r\n"));
  EXPECT_EQ("|||", WebSocketCommon::JoinStrings({"|", "|"}, "|"));
  // Non-ASCII strings are not required to work.
}

}  // namespace

}  // namespace blink

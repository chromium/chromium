// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_common.h"

#include <string.h>

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Connect() and CloseInternal() are very thoroughly tested by DOMWebSocket unit
// tests, so the rests aren't duplicated here.

// This test also indirectly tests IsValidSubprotocolCharacter.
TEST(WebSocketCommonTest, IsValidSubprotocolString) {
  EXPECT_TRUE(WebSocketCommon::IsValidSubprotocolString("Helloworld!!"));
  EXPECT_FALSE(WebSocketCommon::IsValidSubprotocolString("Hello, world!!"));
  EXPECT_FALSE(WebSocketCommon::IsValidSubprotocolString(String()));
  EXPECT_FALSE(WebSocketCommon::IsValidSubprotocolString(""));

  const char kValidCharacters[] =
      "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`"
      "abcdefghijklmnopqrstuvwxyz|~";
  size_t length = strlen(kValidCharacters);
  for (size_t i = 0; i < length; ++i) {
    String s(kValidCharacters + i, 1u);
    EXPECT_TRUE(WebSocketCommon::IsValidSubprotocolString(s));
  }
  for (size_t i = 0; i < 256; ++i) {
    if (std::find(kValidCharacters, kValidCharacters + length,
                  static_cast<char>(i)) != kValidCharacters + length) {
      continue;
    }
    char to_check = char{i};
    String s(&to_check, 1u);
    EXPECT_FALSE(WebSocketCommon::IsValidSubprotocolString(s));
  }
}

TEST(WebSocketCommonTest, EncodeSubprotocolString) {
  EXPECT_EQ("\\\\\\u0009\\u000D\\uFE0F ~hello\\u000A",
            WebSocketCommon::EncodeSubprotocolString(u"\\\t\r\uFE0F ~hello\n"));
}

TEST(WebSocketCommonTest, JoinStrings) {
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

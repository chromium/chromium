// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_challenge.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Test the example challenge from the RFC6455.
TEST(WebSocketHandshakeChallengeTest, RFC6455) {
  const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
  std::string accept = ComputeSecWebSocketAccept(key);
  EXPECT_EQ("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", accept);
}

}  // namespace

}  // namespace net

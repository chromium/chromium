// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_errors.h"

#include "net/base/net_errors.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {
namespace {

// Confirm that the principle classes of errors are converted correctly. We
// don't exhaustively test every error code, as it would be long, repetitive,
// and add little value.
TEST(WebSocketErrorToNetErrorTest, ResultsAreCorrect) {
  EXPECT_THAT(WebSocketErrorToNetError(kWebSocketNormalClosure), IsOk());
  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            WebSocketErrorToNetError(kWebSocketErrorProtocolError));
  EXPECT_EQ(ERR_MSG_TOO_BIG,
            WebSocketErrorToNetError(kWebSocketErrorMessageTooBig));
  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            WebSocketErrorToNetError(kWebSocketErrorNoStatusReceived));
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR,
            WebSocketErrorToNetError(kWebSocketErrorTlsHandshake));
}

}  // namespace
}  // namespace net

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/websocket_utils.h"

#include <optional>
#include <string>
#include <vector>

#include "net/base/isolation_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

net::IsolationInfo CreateIsolationInfo(
    net::IsolationInfo::RequestType request_type) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  return net::IsolationInfo::Create(request_type, origin, origin,
                                    net::SiteForCookies::FromOrigin(origin));
}

TEST(WebSocketUtilsTest, ValidParameters) {
  auto isolation_info =
      CreateIsolationInfo(net::IsolationInfo::RequestType::kOther);

  EXPECT_EQ(std::nullopt, VerifyWebSocketConnectParameters(
                              GURL("ws://example.com/"), {}, isolation_info));

  EXPECT_EQ(std::nullopt,
            VerifyWebSocketConnectParameters(GURL("wss://example.com/"),
                                             {"chat"}, isolation_info));

  EXPECT_EQ(std::nullopt, VerifyWebSocketConnectParameters(
                              GURL("wss://example.com/"), {"chat", "superchat"},
                              isolation_info));
}

TEST(WebSocketUtilsTest, InvalidIsolationInfo) {
  EXPECT_EQ(
      "WebSocket's IsolationInfo::RequestType must be kOther",
      VerifyWebSocketConnectParameters(
          GURL("ws://example.com/"), {},
          CreateIsolationInfo(net::IsolationInfo::RequestType::kMainFrame)));

  EXPECT_EQ(
      "WebSocket's IsolationInfo::RequestType must be kOther",
      VerifyWebSocketConnectParameters(
          GURL("ws://example.com/"), {},
          CreateIsolationInfo(net::IsolationInfo::RequestType::kSubFrame)));
}

TEST(WebSocketUtilsTest, InvalidScheme) {
  auto isolation_info =
      CreateIsolationInfo(net::IsolationInfo::RequestType::kOther);

  const std::string expected_error = "Invalid scheme.";
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("http://example.com/"), {},
                                             isolation_info));
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("https://example.com/"), {},
                                             isolation_info));
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("ftp://example.com/"), {},
                                             isolation_info));
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("data:text/plain,Hello"), {},
                                             isolation_info));
}

TEST(WebSocketUtilsTest, InvalidProtocols) {
  auto isolation_info =
      CreateIsolationInfo(net::IsolationInfo::RequestType::kOther);

  const std::string expected_error = "Invalid protocols.";

  // Duplicates
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("ws://example.com/"),
                                             {"chat", "chat"}, isolation_info));

  // Empty string
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("ws://example.com/"), {""},
                                             isolation_info));

  // Invalid character: space
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("ws://example.com/"),
                                             {"chat room"}, isolation_info));

  // Invalid character: separator
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("ws://example.com/"),
                                             {"chat,room"}, isolation_info));

  // Invalid character: control character
  EXPECT_EQ(expected_error,
            VerifyWebSocketConnectParameters(GURL("ws://example.com/"),
                                             {"chat\x01"}, isolation_info));
}

}  // namespace
}  // namespace network

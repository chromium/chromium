// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/server/web_socket_encoder.h"

#include <stddef.h>

#include "base/strings/strcat.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace server {

TEST(WebSocketEncoderHandshakeTest, EmptyRequestShouldBeRejected) {
  net::WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server =
      WebSocketEncoder::CreateServer("", &params);

  EXPECT_FALSE(server);
}

TEST(WebSocketEncoderHandshakeTest,
     CreateServerWithoutClientMaxWindowBitsParameter) {
  net::WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server =
      WebSocketEncoder::CreateServer("permessage-deflate", &params);

  ASSERT_TRUE(server);
  EXPECT_TRUE(server->deflate_enabled());
  EXPECT_EQ("permessage-deflate", params.AsExtension().ToString());
}

TEST(WebSocketEncoderHandshakeTest,
     CreateServerWithServerNoContextTakeoverParameter) {
  net::WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server = WebSocketEncoder::CreateServer(
      "permessage-deflate; server_no_context_takeover", &params);
  ASSERT_TRUE(server);
  EXPECT_TRUE(server->deflate_enabled());
  EXPECT_EQ("permessage-deflate; server_no_context_takeover",
            params.AsExtension().ToString());
}

TEST(WebSocketEncoderHandshakeTest, FirstExtensionShouldBeChosen) {
  net::WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server = WebSocketEncoder::CreateServer(
      "permessage-deflate; server_no_context_takeover,"
      "permessage-deflate; server_max_window_bits=15",
      &params);

  ASSERT_TRUE(server);
  EXPECT_TRUE(server->deflate_enabled());
  EXPECT_EQ("permessage-deflate; server_no_context_takeover",
            params.AsExtension().ToString());
}

TEST(WebSocketEncoderHandshakeTest, FirstValidExtensionShouldBeChosen) {
  net::WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server = WebSocketEncoder::CreateServer(
      "permessage-deflate; Xserver_no_context_takeover,"
      "permessage-deflate; server_max_window_bits=15",
      &params);

  ASSERT_TRUE(server);
  EXPECT_TRUE(server->deflate_enabled());
  EXPECT_EQ("permessage-deflate; server_max_window_bits=15",
            params.AsExtension().ToString());
}

TEST(WebSocketEncoderHandshakeTest, AllExtensionsAreUnknownOrMalformed) {
  net::WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server =
      WebSocketEncoder::CreateServer("unknown, permessage-deflate; x", &params);

  ASSERT_TRUE(server);
  EXPECT_FALSE(server->deflate_enabled());
}

class WebSocketEncoderTest : public testing::Test {
 public:
  WebSocketEncoderTest() = default;

  void SetUp() override {
    std::string response_extensions;
    server_ = WebSocketEncoder::CreateServer();
    EXPECT_EQ(std::string(), response_extensions);
    client_ = WebSocketEncoder::CreateClient("");
  }

 protected:
  std::unique_ptr<WebSocketEncoder> server_;
  std::unique_ptr<WebSocketEncoder> client_;
};

class WebSocketEncoderCompressionTest : public WebSocketEncoderTest {
 public:
  WebSocketEncoderCompressionTest() = default;

  void SetUp() override {
    net::WebSocketDeflateParameters params;
    server_ = WebSocketEncoder::CreateServer(
        "permessage-deflate; client_max_window_bits", &params);
    ASSERT_TRUE(server_);
    EXPECT_TRUE(server_->deflate_enabled());
    EXPECT_EQ("permessage-deflate; client_max_window_bits=15",
              params.AsExtension().ToString());
    client_ = WebSocketEncoder::CreateClient(params.AsExtension().ToString());
  }
};

TEST_F(WebSocketEncoderTest, DeflateDisabledEncoder) {
  std::unique_ptr<WebSocketEncoder> server = WebSocketEncoder::CreateServer();
  std::unique_ptr<WebSocketEncoder> client = WebSocketEncoder::CreateClient("");

  ASSERT_TRUE(server);
  ASSERT_TRUE(client);

  EXPECT_FALSE(server->deflate_enabled());
  EXPECT_FALSE(client->deflate_enabled());
}

TEST_F(WebSocketEncoderTest, ClientToServer) {
  std::string frame("ClientToServer");
  int mask = 123456;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  client_->EncodeFrame(frame, mask, &encoded);
  EXPECT_EQ(WebSocket::FRAME_OK,
            server_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ("ClientToServer", decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  std::string partial = encoded.substr(0, encoded.length() - 2);
  EXPECT_EQ(WebSocket::FRAME_INCOMPLETE,
            server_->DecodeFrame(partial, &bytes_consumed, &decoded));

  std::string extra = encoded + "more stuff";
  EXPECT_EQ(WebSocket::FRAME_OK,
            server_->DecodeFrame(extra, &bytes_consumed, &decoded));
  EXPECT_EQ("ClientToServer", decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  EXPECT_EQ(
      WebSocket::FRAME_ERROR,
      server_->DecodeFrame(std::string("abcde"), &bytes_consumed, &decoded));
}

TEST_F(WebSocketEncoderTest, ServerToClient) {
  std::string frame("ServerToClient");
  int mask = 0;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  server_->EncodeFrame(frame, mask, &encoded);
  EXPECT_EQ(WebSocket::FRAME_OK,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ("ServerToClient", decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  std::string partial = encoded.substr(0, encoded.length() - 2);
  EXPECT_EQ(WebSocket::FRAME_INCOMPLETE,
            client_->DecodeFrame(partial, &bytes_consumed, &decoded));

  std::string extra = encoded + "more stuff";
  EXPECT_EQ(WebSocket::FRAME_OK,
            client_->DecodeFrame(extra, &bytes_consumed, &decoded));
  EXPECT_EQ("ServerToClient", decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  EXPECT_EQ(
      WebSocket::FRAME_ERROR,
      client_->DecodeFrame(std::string("abcde"), &bytes_consumed, &decoded));
}

TEST_F(WebSocketEncoderCompressionTest, ClientToServer) {
  std::string frame("CompressionCompressionCompressionCompression");
  int mask = 654321;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  client_->EncodeFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocket::FRAME_OK,
            server_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, ServerToClient) {
  std::string frame("CompressionCompressionCompressionCompression");
  int mask = 0;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  server_->EncodeFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocket::FRAME_OK,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, LongFrame) {
  int length = 1000000;
  std::string temp;
  temp.reserve(length);
  for (int i = 0; i < length; ++i)
    temp += static_cast<char>('a' + (i % 26));

  std::string frame;
  frame.reserve(length);
  for (int i = 0; i < length; ++i) {
    int64_t j = i;
    frame += temp.data()[(j * j) % length];
  }

  int mask = 0;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  server_->EncodeFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocket::FRAME_OK,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);
}

}  // namespace server

}  // namespace network
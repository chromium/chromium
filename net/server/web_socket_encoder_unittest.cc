// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/web_socket_encoder.h"

#include <stddef.h>

#include "base/strings/strcat.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_extension.h"
#include "net/websockets/websocket_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(WebSocketEncoderHandshakeTest, EmptyRequestShouldBeRejected) {
  WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server =
      WebSocketEncoder::CreateServer("", &params);

  EXPECT_FALSE(server);
}

TEST(WebSocketEncoderHandshakeTest,
     CreateServerWithoutClientMaxWindowBitsParameter) {
  WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server =
      WebSocketEncoder::CreateServer("permessage-deflate", &params);

  ASSERT_TRUE(server);
  EXPECT_TRUE(server->deflate_enabled());
  EXPECT_EQ("permessage-deflate", params.AsExtension().ToString());
}

TEST(WebSocketEncoderHandshakeTest,
     CreateServerWithServerNoContextTakeoverParameter) {
  WebSocketDeflateParameters params;
  std::unique_ptr<WebSocketEncoder> server = WebSocketEncoder::CreateServer(
      "permessage-deflate; server_no_context_takeover", &params);
  ASSERT_TRUE(server);
  EXPECT_TRUE(server->deflate_enabled());
  EXPECT_EQ("permessage-deflate; server_no_context_takeover",
            params.AsExtension().ToString());
}

TEST(WebSocketEncoderHandshakeTest, FirstExtensionShouldBeChosen) {
  WebSocketDeflateParameters params;
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
  WebSocketDeflateParameters params;
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
  WebSocketDeflateParameters params;
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

  // Generate deflated and continuous frames from original text.
  // The length of `original_text` must be longer than 4*partitions.
  std::vector<std::string> GenerateFragmentedFrames(std::string original_text,
                                                    int mask,
                                                    int partitions,
                                                    bool compressed) {
    constexpr uint8_t kFinalBit = 0x80;
    constexpr uint8_t kReserved1Bit = 0x40;
    constexpr uint8_t kMaskBit = 0x80;

    // A frame consists of 3 or 2 parts: header, (mask) and payload.
    // The first two bytes of `encoded` are the header of the frame.
    // If there is a mask, the four bytes of the mask is inserted after the
    // header. Finally, message contents come.
    std::string encoded;
    int num_mask_header;
    char mask_key_bit;
    std::string mask_bytes;

    if (mask == 0) {
      server_->EncodeTextFrame(original_text, mask, &encoded);
      num_mask_header = 0;
      mask_key_bit = 0;
    } else {
      client_->EncodeTextFrame(original_text, mask, &encoded);
      num_mask_header = 4;
      mask_key_bit = kMaskBit;
      mask_bytes = encoded.substr(2, 4);
    }
    int divide_length =
        (static_cast<int>(encoded.length()) - 2 - num_mask_header) / partitions;
    divide_length -= divide_length % 4;
    std::vector<std::string> encoded_frames(partitions);
    std::string payload;
    std::string header;

    for (int i = 0; i < partitions; ++i) {
      char first_byte = 0;
      if (i == 0)
        first_byte |= WebSocketFrameHeader::OpCodeEnum::kOpCodeText;
      else
        first_byte |= WebSocketFrameHeader::OpCodeEnum::kOpCodeContinuation;
      if (i == partitions - 1)
        first_byte |= kFinalBit;
      if (compressed)
        first_byte |= kReserved1Bit;

      const int position = 2 + num_mask_header + i * divide_length;
      const int length =
          i < partitions - 1 ? divide_length : encoded.length() - position;
      payload = encoded.substr(position, length);
      header = {first_byte, static_cast<char>(payload.length() | mask_key_bit)};
      encoded_frames[i] += header + mask_bytes + payload;
    }

    return encoded_frames;
  }

 protected:
  std::unique_ptr<WebSocketEncoder> server_;
  std::unique_ptr<WebSocketEncoder> client_;
};

class WebSocketEncoderCompressionTest : public WebSocketEncoderTest {
 public:
  WebSocketEncoderCompressionTest() : WebSocketEncoderTest() {}

  void SetUp() override {
    WebSocketDeflateParameters params;
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

  client_->EncodeTextFrame(frame, mask, &encoded);
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            server_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ("ClientToServer", decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  std::string partial = encoded.substr(0, encoded.length() - 2);
  EXPECT_EQ(WebSocketParseResult::FRAME_INCOMPLETE,
            server_->DecodeFrame(partial, &bytes_consumed, &decoded));

  std::string extra = encoded + "more stuff";
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            server_->DecodeFrame(extra, &bytes_consumed, &decoded));
  EXPECT_EQ("ClientToServer", decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  EXPECT_EQ(
      WebSocketParseResult::FRAME_ERROR,
      server_->DecodeFrame(std::string("abcde"), &bytes_consumed, &decoded));
}

TEST_F(WebSocketEncoderTest, ServerToClient) {
  std::string frame("ServerToClient");
  int mask = 0;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  server_->EncodeTextFrame(frame, mask, &encoded);
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ("ServerToClient", decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  std::string partial = encoded.substr(0, encoded.length() - 2);
  EXPECT_EQ(WebSocketParseResult::FRAME_INCOMPLETE,
            client_->DecodeFrame(partial, &bytes_consumed, &decoded));

  std::string extra = encoded + "more stuff";
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            client_->DecodeFrame(extra, &bytes_consumed, &decoded));
  EXPECT_EQ("ServerToClient", decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);

  EXPECT_EQ(
      WebSocketParseResult::FRAME_ERROR,
      client_->DecodeFrame(std::string("abcde"), &bytes_consumed, &decoded));
}

TEST_F(WebSocketEncoderTest, DecodeFragmentedMessageClientToServerDivided2) {
  const std::string kOriginalText = "abcdefghijklmnop";
  constexpr int kMask = 123456;
  constexpr bool kCompressed = false;
  constexpr int kPartitions = 2;
  ASSERT_GT(static_cast<int>(kOriginalText.length()), 4 * kPartitions);
  std::vector<std::string> encoded_frames =
      GenerateFragmentedFrames(kOriginalText, kMask, kPartitions, kCompressed);
  ASSERT_EQ(kPartitions, static_cast<int>(encoded_frames.size()));

  const std::string& kEncodedFirstFrame = encoded_frames[0];
  const std::string& kEncodedLastFrame = encoded_frames[1];

  int bytes_consumed;
  std::string decoded;

  // kEncodedFirstFrame -> kEncodedLastFrame
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      server_->DecodeFrame(kEncodedFirstFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedFirstFrame.length()), bytes_consumed);
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            server_->DecodeFrame(kEncodedLastFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("abcdefghijklmnop", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedLastFrame.length()), bytes_consumed);
}

TEST_F(WebSocketEncoderTest, DecodeFragmentedMessageClientToServerDivided3) {
  const std::string kOriginalText = "abcdefghijklmnop";
  constexpr int kMask = 123456;
  constexpr bool kCompressed = false;
  constexpr int kPartitions = 3;
  ASSERT_GT(static_cast<int>(kOriginalText.length()), 4 * kPartitions);
  std::vector<std::string> encoded_frames =
      GenerateFragmentedFrames(kOriginalText, kMask, kPartitions, kCompressed);
  ASSERT_EQ(kPartitions, static_cast<int>(encoded_frames.size()));

  const std::string& kEncodedFirstFrame = encoded_frames[0];
  const std::string& kEncodedSecondFrame = encoded_frames[1];
  const std::string& kEncodedLastFrame = encoded_frames[2];

  int bytes_consumed;
  std::string decoded;

  // kEncodedFirstFrame -> kEncodedSecondFrame -> kEncodedLastFrame
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      server_->DecodeFrame(kEncodedFirstFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedFirstFrame.length()), bytes_consumed);
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      server_->DecodeFrame(kEncodedSecondFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedSecondFrame.length()), bytes_consumed);
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            server_->DecodeFrame(kEncodedLastFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("abcdefghijklmnop", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedLastFrame.length()), bytes_consumed);
}

TEST_F(WebSocketEncoderTest, DecodeFragmentedMessageServerToClientDivided2) {
  const std::string kOriginalText = "abcdefghijklmnop";
  constexpr int kMask = 0;
  constexpr bool kCompressed = false;

  constexpr int kPartitions = 2;
  ASSERT_GT(static_cast<int>(kOriginalText.length()), 4 * kPartitions);
  std::vector<std::string> encoded_frames =
      GenerateFragmentedFrames(kOriginalText, kMask, kPartitions, kCompressed);
  ASSERT_EQ(kPartitions, static_cast<int>(encoded_frames.size()));

  const std::string& kEncodedFirstFrame = encoded_frames[0];
  const std::string& kEncodedLastFrame = encoded_frames[1];

  int bytes_consumed;
  std::string decoded;

  // kEncodedFirstFrame -> kEncodedLastFrame
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      client_->DecodeFrame(kEncodedFirstFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedFirstFrame.length()), bytes_consumed);
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            client_->DecodeFrame(kEncodedLastFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("abcdefghijklmnop", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedLastFrame.length()), bytes_consumed);
}

TEST_F(WebSocketEncoderTest, DecodeFragmentedMessageServerToClientDivided3) {
  const std::string kOriginalText = "abcdefghijklmnop";
  constexpr int kMask = 0;
  constexpr bool kCompressed = false;

  constexpr int kPartitions = 3;
  ASSERT_GT(static_cast<int>(kOriginalText.length()), 4 * kPartitions);
  std::vector<std::string> encoded_frames =
      GenerateFragmentedFrames(kOriginalText, kMask, kPartitions, kCompressed);
  ASSERT_EQ(kPartitions, static_cast<int>(encoded_frames.size()));

  const std::string& kEncodedFirstFrame = encoded_frames[0];
  const std::string& kEncodedSecondFrame = encoded_frames[1];
  const std::string& kEncodedLastFrame = encoded_frames[2];

  int bytes_consumed;
  std::string decoded;

  // kEncodedFirstFrame -> kEncodedSecondFrame -> kEncodedLastFrame
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      client_->DecodeFrame(kEncodedFirstFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedFirstFrame.length()), bytes_consumed);
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      client_->DecodeFrame(kEncodedSecondFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedSecondFrame.length()), bytes_consumed);
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            client_->DecodeFrame(kEncodedLastFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("abcdefghijklmnop", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedLastFrame.length()), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, ClientToServer) {
  std::string frame("CompressionCompressionCompressionCompression");
  int mask = 654321;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  client_->EncodeTextFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
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

  server_->EncodeTextFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, LongFrame) {
  int length = 1000000;
  std::string temp;
  temp.reserve(length);
  for (int i = 0; i < length; ++i)
    temp += (char)('a' + (i % 26));

  std::string frame;
  frame.reserve(length);
  for (int i = 0; i < length; ++i) {
    int64_t j = i;
    frame += temp[(j * j) % length];
  }

  int mask = 0;
  std::string encoded;
  int bytes_consumed;
  std::string decoded;

  server_->EncodeTextFrame(frame, mask, &encoded);
  EXPECT_LT(encoded.length(), frame.length());
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            client_->DecodeFrame(encoded, &bytes_consumed, &decoded));
  EXPECT_EQ(frame, decoded);
  EXPECT_EQ((int)encoded.length(), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, DecodeFragmentedMessageClientToServer) {
  const std::string kOriginalText = "abcdefghijklmnop";
  constexpr int kMask = 123456;

  constexpr int kPartitions = 3;
  constexpr bool kCompressed = true;
  ASSERT_GT(static_cast<int>(kOriginalText.length()), 4 * kPartitions);
  std::vector<std::string> encoded_frames =
      GenerateFragmentedFrames(kOriginalText, kMask, kPartitions, kCompressed);
  ASSERT_EQ(kPartitions, static_cast<int>(encoded_frames.size()));

  const std::string& kEncodedFirstFrame = encoded_frames[0];
  const std::string& kEncodedSecondFrame = encoded_frames[1];
  const std::string& kEncodedLastFrame = encoded_frames[2];

  int bytes_consumed;
  std::string decoded;

  // kEncodedFirstFrame -> kEncodedSecondFrame -> kEncodedLastFrame
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      server_->DecodeFrame(kEncodedFirstFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedFirstFrame.length()), bytes_consumed);
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      server_->DecodeFrame(kEncodedSecondFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedSecondFrame.length()), bytes_consumed);
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            server_->DecodeFrame(kEncodedLastFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("abcdefghijklmnop", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedLastFrame.length()), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, DecodeFragmentedMessageServerToClient) {
  const std::string kOriginalText = "abcdefghijklmnop";
  constexpr int kMask = 0;

  constexpr int kPartitions = 3;
  constexpr bool kCompressed = true;
  ASSERT_GT(static_cast<int>(kOriginalText.length()), 4 * kPartitions);
  std::vector<std::string> encoded_frames =
      GenerateFragmentedFrames(kOriginalText, kMask, kPartitions, kCompressed);
  ASSERT_EQ(kPartitions, static_cast<int>(encoded_frames.size()));

  const std::string& kEncodedFirstFrame = encoded_frames[0];
  const std::string& kEncodedSecondFrame = encoded_frames[1];
  const std::string& kEncodedLastFrame = encoded_frames[2];

  int bytes_consumed;
  std::string decoded;

  // kEncodedFirstFrame -> kEncodedSecondFrame -> kEncodedLastFrame
  decoded.clear();
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      client_->DecodeFrame(kEncodedFirstFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedFirstFrame.length()), bytes_consumed);
  EXPECT_EQ(
      WebSocketParseResult::FRAME_OK_MIDDLE,
      client_->DecodeFrame(kEncodedSecondFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedSecondFrame.length()), bytes_consumed);
  EXPECT_EQ(WebSocketParseResult::FRAME_OK_FINAL,
            client_->DecodeFrame(kEncodedLastFrame, &bytes_consumed, &decoded));
  EXPECT_EQ("abcdefghijklmnop", decoded);
  EXPECT_EQ(static_cast<int>(kEncodedLastFrame.length()), bytes_consumed);
}

TEST_F(WebSocketEncoderCompressionTest, CheckPongFrameNotCompressed) {
  constexpr uint8_t kReserved1Bit = 0x40;
  const std::string kOriginalText = "abcdefghijklmnop";
  constexpr int kMask = 0;
  std::string encoded;

  server_->EncodePongFrame(kOriginalText, kMask, &encoded);
  EXPECT_FALSE(encoded[1] & kReserved1Bit);
  EXPECT_EQ(kOriginalText, encoded.substr(2));
}

TEST_F(WebSocketEncoderCompressionTest, CheckCloseFrameNotCompressed) {
  constexpr uint8_t kReserved1Bit = 0x40;
  const std::string kOriginalText = "\x03\xe8";
  constexpr int kMask = 0;
  std::string encoded;

  server_->EncodeCloseFrame(kOriginalText, kMask, &encoded);
  EXPECT_FALSE(encoded[1] & kReserved1Bit);
  EXPECT_EQ(kOriginalText, encoded.substr(2));
}

}  // namespace net

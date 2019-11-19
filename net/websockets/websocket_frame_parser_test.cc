// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame_parser.h"

#include <stdint.h>
#include <algorithm>
#include <vector>

#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kHello[] = "Hello, world!";
const uint64_t kHelloLength = base::size(kHello) - 1;
const char kHelloFrame[] = "\x81\x0DHello, world!";
const uint64_t kHelloFrameLength = base::size(kHelloFrame) - 1;
const char kMaskedHelloFrame[] =
    "\x81\x8D\xDE\xAD\xBE\xEF"
    "\x96\xC8\xD2\x83\xB1\x81\x9E\x98\xB1\xDF\xD2\x8B\xFF";
const uint64_t kMaskedHelloFrameLength = base::size(kMaskedHelloFrame) - 1;

struct FrameHeaderTestCase {
  const char* frame_header;
  size_t frame_header_length;
  uint64_t frame_length;
  WebSocketError error_code;
};

const FrameHeaderTestCase kFrameHeaderTests[] = {
  { "\x81\x00", 2, UINT64_C(0), kWebSocketNormalClosure },
  { "\x81\x7D", 2, UINT64_C(125), kWebSocketNormalClosure },
  { "\x81\x7E\x00\x7E", 4, UINT64_C(126), kWebSocketNormalClosure },
  { "\x81\x7E\xFF\xFF", 4, UINT64_C(0xFFFF), kWebSocketNormalClosure },
  { "\x81\x7F\x00\x00\x00\x00\x00\x01\x00\x00", 10, UINT64_C(0x10000),
    kWebSocketNormalClosure },
  { "\x81\x7F\x00\x00\x00\x00\x7F\xFF\xFF\xFF", 10, UINT64_C(0x7FFFFFFF),
    kWebSocketNormalClosure },
  { "\x81\x7F\x00\x00\x00\x00\x80\x00\x00\x00", 10, UINT64_C(0x80000000),
    kWebSocketErrorMessageTooBig },
  { "\x81\x7F\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 10,
    UINT64_C(0x7FFFFFFFFFFFFFFF), kWebSocketErrorMessageTooBig }
};
const int kNumFrameHeaderTests = base::size(kFrameHeaderTests);

TEST(WebSocketFrameParserTest, DecodeNormalFrame) {
  WebSocketFrameParser parser;

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
  EXPECT_TRUE(parser.Decode(kHelloFrame, kHelloFrameLength, &frames));
  EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
  ASSERT_EQ(1u, frames.size());
  WebSocketFrameChunk* frame = frames[0].get();
  ASSERT_TRUE(frame != nullptr);
  const WebSocketFrameHeader* header = frame->header.get();
  EXPECT_TRUE(header != nullptr);
  if (header) {
    EXPECT_TRUE(header->final);
    EXPECT_FALSE(header->reserved1);
    EXPECT_FALSE(header->reserved2);
    EXPECT_FALSE(header->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header->opcode);
    EXPECT_FALSE(header->masked);
    EXPECT_EQ(kHelloLength, header->payload_length);
  }
  EXPECT_TRUE(frame->final_chunk);

  ASSERT_EQ(static_cast<size_t>(kHelloLength), frame->payload.size());
  EXPECT_TRUE(std::equal(kHello, kHello + kHelloLength, frame->payload.data()));
}

TEST(WebSocketFrameParserTest, DecodeMaskedFrame) {
  WebSocketFrameParser parser;

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
  EXPECT_TRUE(
      parser.Decode(kMaskedHelloFrame, kMaskedHelloFrameLength, &frames));
  EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
  ASSERT_EQ(1u, frames.size());
  WebSocketFrameChunk* frame = frames[0].get();
  ASSERT_TRUE(frame != nullptr);
  const WebSocketFrameHeader* header = frame->header.get();
  EXPECT_TRUE(header != nullptr);
  if (header) {
    EXPECT_TRUE(header->final);
    EXPECT_FALSE(header->reserved1);
    EXPECT_FALSE(header->reserved2);
    EXPECT_FALSE(header->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header->opcode);
    EXPECT_TRUE(header->masked);
    EXPECT_EQ(kHelloLength, header->payload_length);
  }
  EXPECT_TRUE(frame->final_chunk);

  ASSERT_EQ(static_cast<size_t>(kHelloLength), frame->payload.size());

  std::string payload(frame->payload.data(), frame->payload.size());
  MaskWebSocketFramePayload(header->masking_key, 0, &payload[0],
                            payload.size());
  EXPECT_EQ(payload, kHello);
}

TEST(WebSocketFrameParserTest, DecodeManyFrames) {
  struct Input {
    const char* frame;
    size_t frame_length;
    const char* expected_payload;
    size_t expected_payload_length;
  };
  static const Input kInputs[] = {
    // Each |frame| data is split into two string literals because C++ lexers
    // consume unlimited number of hex characters in a hex character escape
    // (e.g. "\x05F" is not treated as { '\x5', 'F', '\0' } but as
    // { '\x5F', '\0' }).
    { "\x81\x05" "First", 7, "First", 5 },
    { "\x81\x06" "Second", 8, "Second", 6 },
    { "\x81\x05" "Third", 7, "Third", 5 },
    { "\x81\x06" "Fourth", 8, "Fourth", 6 },
    { "\x81\x05" "Fifth", 7, "Fifth", 5 },
    { "\x81\x05" "Sixth", 7, "Sixth", 5 },
    { "\x81\x07" "Seventh", 9, "Seventh", 7 },
    { "\x81\x06" "Eighth", 8, "Eighth", 6 },
    { "\x81\x05" "Ninth", 7, "Ninth", 5 },
    { "\x81\x05" "Tenth", 7, "Tenth", 5 }
  };
  static const int kNumInputs = base::size(kInputs);

  std::vector<char> input;
  // Concatenate all frames.
  for (int i = 0; i < kNumInputs; ++i) {
    input.insert(input.end(),
                 kInputs[i].frame,
                 kInputs[i].frame + kInputs[i].frame_length);
  }

  WebSocketFrameParser parser;

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
  EXPECT_TRUE(parser.Decode(input.data(), input.size(), &frames));
  EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
  ASSERT_EQ(static_cast<size_t>(kNumInputs), frames.size());

  for (int i = 0; i < kNumInputs; ++i) {
    WebSocketFrameChunk* frame = frames[i].get();
    EXPECT_TRUE(frame != nullptr);
    if (!frame)
      continue;
    EXPECT_TRUE(frame->final_chunk);
    ASSERT_EQ(kInputs[i].expected_payload_length,
              static_cast<uint64_t>(frame->payload.size()));
    EXPECT_TRUE(std::equal(
        kInputs[i].expected_payload,
        kInputs[i].expected_payload + kInputs[i].expected_payload_length,
        frame->payload.data()));

    const WebSocketFrameHeader* header = frame->header.get();
    EXPECT_TRUE(header != nullptr);
    if (!header)
      continue;
    EXPECT_TRUE(header->final);
    EXPECT_FALSE(header->reserved1);
    EXPECT_FALSE(header->reserved2);
    EXPECT_FALSE(header->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header->opcode);
    EXPECT_FALSE(header->masked);
    EXPECT_EQ(kInputs[i].expected_payload_length, header->payload_length);
  }
}

TEST(WebSocketFrameParserTest, DecodePartialFrame) {
  static const size_t kFrameHeaderSize = 2;

  for (size_t cutting_pos = 0; cutting_pos < kHelloLength; ++cutting_pos) {
    std::vector<char> input1(kHelloFrame,
                             kHelloFrame + kFrameHeaderSize + cutting_pos);
    std::vector<char> input2(kHelloFrame + input1.size(),
                             kHelloFrame + kHelloFrameLength);

    std::vector<char> expected1(kHello, kHello + cutting_pos);
    std::vector<char> expected2(kHello + cutting_pos, kHello + kHelloLength);

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames1;
    EXPECT_TRUE(parser.Decode(&input1.front(), input1.size(), &frames1));
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_EQ(1u, frames1.size());
    if (frames1.size() != 1u)
      continue;
    WebSocketFrameChunk* frame1 = frames1[0].get();
    EXPECT_TRUE(frame1 != nullptr);
    if (!frame1)
      continue;
    EXPECT_FALSE(frame1->final_chunk);
    if (expected1.size() == 0) {
      EXPECT_EQ(nullptr, frame1->payload.data());
    } else {
      ASSERT_EQ(cutting_pos, static_cast<size_t>(frame1->payload.size()));
      EXPECT_TRUE(std::equal(expected1.begin(), expected1.end(),
                             frame1->payload.data()));
    }
    const WebSocketFrameHeader* header1 = frame1->header.get();
    EXPECT_TRUE(header1 != nullptr);
    if (!header1)
      continue;
    EXPECT_TRUE(header1->final);
    EXPECT_FALSE(header1->reserved1);
    EXPECT_FALSE(header1->reserved2);
    EXPECT_FALSE(header1->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header1->opcode);
    EXPECT_FALSE(header1->masked);
    EXPECT_EQ(kHelloLength, header1->payload_length);

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames2;
    EXPECT_TRUE(parser.Decode(&input2.front(), input2.size(), &frames2));
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_EQ(1u, frames2.size());
    if (frames2.size() != 1u)
      continue;
    WebSocketFrameChunk* frame2 = frames2[0].get();
    EXPECT_TRUE(frame2 != nullptr);
    if (!frame2)
      continue;
    EXPECT_TRUE(frame2->final_chunk);
    if (expected2.size() == 0) {
      EXPECT_EQ(nullptr, frame1->payload.data());
    } else {
      ASSERT_EQ(expected2.size(),
                static_cast<uint64_t>(frame2->payload.size()));
      EXPECT_TRUE(std::equal(expected2.begin(), expected2.end(),
                             frame2->payload.data()));
    }
    const WebSocketFrameHeader* header2 = frame2->header.get();
    EXPECT_TRUE(header2 == nullptr);
  }
}

TEST(WebSocketFrameParserTest, DecodePartialMaskedFrame) {
  static const size_t kFrameHeaderSize = 6;

  for (size_t cutting_pos = 0; cutting_pos < kHelloLength; ++cutting_pos) {
    std::vector<char> input1(
        kMaskedHelloFrame, kMaskedHelloFrame + kFrameHeaderSize + cutting_pos);
    std::vector<char> input2(kMaskedHelloFrame + input1.size(),
                             kMaskedHelloFrame + kMaskedHelloFrameLength);

    std::vector<char> expected1(kHello, kHello + cutting_pos);
    std::vector<char> expected2(kHello + cutting_pos, kHello + kHelloLength);

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames1;
    EXPECT_TRUE(parser.Decode(&input1.front(), input1.size(), &frames1));
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_EQ(1u, frames1.size());
    if (frames1.size() != 1u)
      continue;
    WebSocketFrameChunk* frame1 = frames1[0].get();
    EXPECT_TRUE(frame1 != nullptr);
    if (!frame1)
      continue;
    EXPECT_FALSE(frame1->final_chunk);
    const WebSocketFrameHeader* header1 = frame1->header.get();
    EXPECT_TRUE(header1 != nullptr);
    if (!header1)
      continue;
    if (expected1.size() == 0) {
      EXPECT_EQ(nullptr, frame1->payload.data());
    } else {
      ASSERT_EQ(expected1.size(),
                static_cast<uint64_t>(frame1->payload.size()));
      std::vector<char> payload1(
          frame1->payload.data(),
          frame1->payload.data() + frame1->payload.size());
      MaskWebSocketFramePayload(header1->masking_key, 0, &payload1[0],
                                payload1.size());
      EXPECT_EQ(expected1, payload1);
    }
    EXPECT_TRUE(header1->final);
    EXPECT_FALSE(header1->reserved1);
    EXPECT_FALSE(header1->reserved2);
    EXPECT_FALSE(header1->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header1->opcode);
    EXPECT_TRUE(header1->masked);
    EXPECT_EQ(kHelloLength, header1->payload_length);

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames2;
    EXPECT_TRUE(parser.Decode(&input2.front(), input2.size(), &frames2));
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_EQ(1u, frames2.size());
    if (frames2.size() != 1u)
      continue;
    WebSocketFrameChunk* frame2 = frames2[0].get();
    EXPECT_TRUE(frame2 != nullptr);
    if (!frame2)
      continue;
    EXPECT_TRUE(frame2->final_chunk);
    if (expected2.size() == 0) {
      EXPECT_EQ(nullptr, frame2->payload.data());
    } else {
      ASSERT_EQ(expected2.size(),
                static_cast<uint64_t>(frame2->payload.size()));
      std::vector<char> payload2(
          frame2->payload.data(),
          frame2->payload.data() + frame2->payload.size());
      MaskWebSocketFramePayload(header1->masking_key, cutting_pos, &payload2[0],
                                payload2.size());
      EXPECT_EQ(expected2, payload2);
    }
    const WebSocketFrameHeader* header2 = frame2->header.get();
    EXPECT_TRUE(header2 == nullptr);
  }
}

TEST(WebSocketFrameParserTest, DecodeFramesOfVariousLengths) {
  for (int i = 0; i < kNumFrameHeaderTests; ++i) {
    const char* frame_header = kFrameHeaderTests[i].frame_header;
    size_t frame_header_length = kFrameHeaderTests[i].frame_header_length;
    uint64_t frame_length = kFrameHeaderTests[i].frame_length;

    std::vector<char> input(frame_header, frame_header + frame_header_length);
    // Limit the payload size not to flood the console on failure.
    static const uint64_t kMaxPayloadSize = 200;
    uint64_t input_payload_size = std::min(frame_length, kMaxPayloadSize);
    input.insert(input.end(), input_payload_size, 'a');

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_EQ(kFrameHeaderTests[i].error_code == kWebSocketNormalClosure,
              parser.Decode(input.data(), input.size(), &frames));
    EXPECT_EQ(kFrameHeaderTests[i].error_code, parser.websocket_error());
    if (kFrameHeaderTests[i].error_code != kWebSocketNormalClosure) {
      EXPECT_EQ(0u, frames.size());
    } else {
      EXPECT_EQ(1u, frames.size());
    }
    if (frames.size() != 1u)
      continue;
    WebSocketFrameChunk* frame = frames[0].get();
    EXPECT_TRUE(frame != nullptr);
    if (!frame)
      continue;
    if (frame_length == input_payload_size) {
      EXPECT_TRUE(frame->final_chunk);
    } else {
      EXPECT_FALSE(frame->final_chunk);
    }
    std::vector<char> expected_payload(input_payload_size, 'a');
    if (expected_payload.size() == 0) {
      EXPECT_EQ(nullptr, frame->payload.data());
    } else {
      ASSERT_EQ(expected_payload.size(),
                static_cast<uint64_t>(frame->payload.size()));
      EXPECT_TRUE(std::equal(expected_payload.begin(), expected_payload.end(),
                             frame->payload.data()));
    }
    const WebSocketFrameHeader* header = frame->header.get();
    EXPECT_TRUE(header != nullptr);
    if (!header)
      continue;
    EXPECT_TRUE(header->final);
    EXPECT_FALSE(header->reserved1);
    EXPECT_FALSE(header->reserved2);
    EXPECT_FALSE(header->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header->opcode);
    EXPECT_FALSE(header->masked);
    EXPECT_EQ(frame_length, header->payload_length);
  }
}

TEST(WebSocketFrameParserTest, DecodePartialHeader) {
  for (int i = 0; i < kNumFrameHeaderTests; ++i) {
    const char* frame_header = kFrameHeaderTests[i].frame_header;
    size_t frame_header_length = kFrameHeaderTests[i].frame_header_length;
    uint64_t frame_length = kFrameHeaderTests[i].frame_length;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    // Feed each byte to the parser to see if the parser behaves correctly
    // when it receives partial frame header.
    size_t last_byte_offset = frame_header_length - 1;
    for (size_t j = 0; j < frame_header_length; ++j) {
      bool failed =
          kFrameHeaderTests[i].error_code != kWebSocketNormalClosure &&
          j == last_byte_offset;
      EXPECT_EQ(!failed, parser.Decode(frame_header + j, 1, &frames));
      if (failed) {
        EXPECT_EQ(kFrameHeaderTests[i].error_code, parser.websocket_error());
      } else {
        EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
      }
      if (kFrameHeaderTests[i].error_code == kWebSocketNormalClosure &&
          j == last_byte_offset) {
        EXPECT_EQ(1u, frames.size()) << "i=" << i << ", j=" << j;
      } else {
        EXPECT_EQ(0u, frames.size()) << "i=" << i << ", j=" << j;
      }
    }
    if (frames.size() != 1u)
      continue;
    WebSocketFrameChunk* frame = frames[0].get();
    EXPECT_TRUE(frame != nullptr);
    if (!frame)
      continue;
    if (frame_length == 0u) {
      EXPECT_TRUE(frame->final_chunk);
    } else {
      EXPECT_FALSE(frame->final_chunk);
    }
    EXPECT_EQ(nullptr, frame->payload.data());
    const WebSocketFrameHeader* header = frame->header.get();
    EXPECT_TRUE(header != nullptr);
    if (!header)
      continue;
    EXPECT_TRUE(header->final);
    EXPECT_FALSE(header->reserved1);
    EXPECT_FALSE(header->reserved2);
    EXPECT_FALSE(header->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header->opcode);
    EXPECT_FALSE(header->masked);
    EXPECT_EQ(frame_length, header->payload_length);
  }
}

TEST(WebSocketFrameParserTest, InvalidLengthEncoding) {
  struct TestCase {
    const char* frame_header;
    size_t frame_header_length;
  };
  static const TestCase kTests[] = {
    // For frames with two-byte extended length field, the payload length
    // should be 126 (0x7E) bytes or more.
    { "\x81\x7E\x00\x00", 4 },
    { "\x81\x7E\x00\x7D", 4 },
    // For frames with eight-byte extended length field, the payload length
    // should be 0x10000 bytes or more.
    { "\x81\x7F\x00\x00\x00\x00\x00\x00\x00\x00", 10 },
    { "\x81\x7E\x00\x00\x00\x00\x00\x00\xFF\xFF", 10 },
  };
  static const int kNumTests = base::size(kTests);

  for (int i = 0; i < kNumTests; ++i) {
    const char* frame_header = kTests[i].frame_header;
    size_t frame_header_length = kTests[i].frame_header_length;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_FALSE(parser.Decode(frame_header, frame_header_length, &frames));
    EXPECT_EQ(kWebSocketErrorProtocolError, parser.websocket_error());
    EXPECT_EQ(0u, frames.size());

    // Once the parser has failed, it no longer accepts any input (even if
    // the input is empty).
    EXPECT_FALSE(parser.Decode("", 0, &frames));
    EXPECT_EQ(kWebSocketErrorProtocolError, parser.websocket_error());
    EXPECT_EQ(0u, frames.size());
  }
}

TEST(WebSocketFrameParserTest, FrameTypes) {
  struct TestCase {
    const char* frame_header;
    size_t frame_header_length;
    WebSocketFrameHeader::OpCode opcode;
  };
  static const TestCase kTests[] = {
    { "\x80\x00", 2, WebSocketFrameHeader::kOpCodeContinuation },
    { "\x81\x00", 2, WebSocketFrameHeader::kOpCodeText },
    { "\x82\x00", 2, WebSocketFrameHeader::kOpCodeBinary },
    { "\x88\x00", 2, WebSocketFrameHeader::kOpCodeClose },
    { "\x89\x00", 2, WebSocketFrameHeader::kOpCodePing },
    { "\x8A\x00", 2, WebSocketFrameHeader::kOpCodePong },
    // These are undefined opcodes, but the parser needs to be able to parse
    // them anyway.
    { "\x83\x00", 2, 0x3 },
    { "\x84\x00", 2, 0x4 },
    { "\x85\x00", 2, 0x5 },
    { "\x86\x00", 2, 0x6 },
    { "\x87\x00", 2, 0x7 },
    { "\x8B\x00", 2, 0xB },
    { "\x8C\x00", 2, 0xC },
    { "\x8D\x00", 2, 0xD },
    { "\x8E\x00", 2, 0xE },
    { "\x8F\x00", 2, 0xF }
  };
  static const int kNumTests = base::size(kTests);

  for (int i = 0; i < kNumTests; ++i) {
    const char* frame_header = kTests[i].frame_header;
    size_t frame_header_length = kTests[i].frame_header_length;
    WebSocketFrameHeader::OpCode opcode = kTests[i].opcode;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_TRUE(parser.Decode(frame_header, frame_header_length, &frames));
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_EQ(1u, frames.size());
    if (frames.size() != 1u)
      continue;
    WebSocketFrameChunk* frame = frames[0].get();
    EXPECT_TRUE(frame != nullptr);
    if (!frame)
      continue;
    EXPECT_TRUE(frame->final_chunk);
    EXPECT_EQ(nullptr, frame->payload.data());
    const WebSocketFrameHeader* header = frame->header.get();
    EXPECT_TRUE(header != nullptr);
    if (!header)
      continue;
    EXPECT_TRUE(header->final);
    EXPECT_FALSE(header->reserved1);
    EXPECT_FALSE(header->reserved2);
    EXPECT_FALSE(header->reserved3);
    EXPECT_EQ(opcode, header->opcode);
    EXPECT_FALSE(header->masked);
    EXPECT_EQ(0u, header->payload_length);
  }
}

TEST(WebSocketFrameParserTest, FinalBitAndReservedBits) {
  struct TestCase {
    const char* frame_header;
    size_t frame_header_length;
    bool final;
    bool reserved1;
    bool reserved2;
    bool reserved3;
  };
  static const TestCase kTests[] = {
    { "\x81\x00", 2, true, false, false, false },
    { "\x01\x00", 2, false, false, false, false },
    { "\xC1\x00", 2, true, true, false, false },
    { "\xA1\x00", 2, true, false, true, false },
    { "\x91\x00", 2, true, false, false, true },
    { "\x71\x00", 2, false, true, true, true },
    { "\xF1\x00", 2, true, true, true, true }
  };
  static const int kNumTests = base::size(kTests);

  for (int i = 0; i < kNumTests; ++i) {
    const char* frame_header = kTests[i].frame_header;
    size_t frame_header_length = kTests[i].frame_header_length;
    bool final = kTests[i].final;
    bool reserved1 = kTests[i].reserved1;
    bool reserved2 = kTests[i].reserved2;
    bool reserved3 = kTests[i].reserved3;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_TRUE(parser.Decode(frame_header, frame_header_length, &frames));
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_EQ(1u, frames.size());
    if (frames.size() != 1u)
      continue;
    WebSocketFrameChunk* frame = frames[0].get();
    EXPECT_TRUE(frame != nullptr);
    if (!frame)
      continue;
    EXPECT_TRUE(frame->final_chunk);
    EXPECT_EQ(nullptr, frame->payload.data());
    const WebSocketFrameHeader* header = frame->header.get();
    EXPECT_TRUE(header != nullptr);
    if (!header)
      continue;
    EXPECT_EQ(final, header->final);
    EXPECT_EQ(reserved1, header->reserved1);
    EXPECT_EQ(reserved2, header->reserved2);
    EXPECT_EQ(reserved3, header->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header->opcode);
    EXPECT_FALSE(header->masked);
    EXPECT_EQ(0u, header->payload_length);
  }
}

}  // Unnamed namespace

}  // namespace net

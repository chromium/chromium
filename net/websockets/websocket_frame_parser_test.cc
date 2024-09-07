// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/websockets/websocket_frame_parser.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "net/websockets/websocket_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

constexpr char kHello[] = "Hello, world!";
constexpr uint64_t kHelloLength = std::size(kHello) - 1;
constexpr char kHelloFrame[] = "\x81\x0DHello, world!";
constexpr char kMaskedHelloFrame[] =
    "\x81\x8D\xDE\xAD\xBE\xEF"
    "\x96\xC8\xD2\x83\xB1\x81\x9E\x98\xB1\xDF\xD2\x8B\xFF";
constexpr uint64_t kMaskedHelloFrameLength = std::size(kMaskedHelloFrame) - 1;

struct FrameHeaderTestCase {
  const std::string_view frame_header;
  uint64_t frame_length;
  WebSocketError error_code;
};

constexpr FrameHeaderTestCase kFrameHeaderTests[] = {
    {{"\x81\x00", 2}, UINT64_C(0), kWebSocketNormalClosure},
    {{"\x81\x7D", 2}, UINT64_C(125), kWebSocketNormalClosure},
    {{"\x81\x7E\x00\x7E", 4}, UINT64_C(126), kWebSocketNormalClosure},
    {{"\x81\x7E\xFF\xFF", 4}, UINT64_C(0xFFFF), kWebSocketNormalClosure},
    {{"\x81\x7F\x00\x00\x00\x00\x00\x01\x00\x00", 10},
     UINT64_C(0x10000),
     kWebSocketNormalClosure},
    {{"\x81\x7F\x00\x00\x00\x00\x7F\xFF\xFF\xFF", 10},
     UINT64_C(0x7FFFFFFF),
     kWebSocketNormalClosure},
    {{"\x81\x7F\x00\x00\x00\x00\x80\x00\x00\x00", 10},
     UINT64_C(0x80000000),
     kWebSocketErrorMessageTooBig},
    {{"\x81\x7F\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 10},
     UINT64_C(0x7FFFFFFFFFFFFFFF),
     kWebSocketErrorMessageTooBig}};
constexpr int kNumFrameHeaderTests = std::size(kFrameHeaderTests);

TEST(WebSocketFrameParserTest, DecodeNormalFrame) {
  WebSocketFrameParser parser;

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
  EXPECT_TRUE(
      parser.Decode(base::byte_span_from_cstring(kHelloFrame), &frames));
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
      parser.Decode(base::byte_span_from_cstring(kMaskedHelloFrame), &frames));
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
  MaskWebSocketFramePayload(header->masking_key, 0,
                            base::as_writable_byte_span(payload));
  EXPECT_EQ(payload, kHello);
}

TEST(WebSocketFrameParserTest, DecodeManyFrames) {
  struct Input {
    const char* frame;
    size_t frame_length;
    const char* expected_payload;
    size_t expected_payload_length;
  };
  static constexpr Input kInputs[] = {
      // Each |frame| data is split into two string literals because C++ lexers
      // consume unlimited number of hex characters in a hex character escape
      // (e.g. "\x05F" is not treated as { '\x5', 'F', '\0' } but as
      // { '\x5F', '\0' }).
      {"\x81\x05"
       "First",
       7, "First", 5},
      {"\x81\x06"
       "Second",
       8, "Second", 6},
      {"\x81\x05"
       "Third",
       7, "Third", 5},
      {"\x81\x06"
       "Fourth",
       8, "Fourth", 6},
      {"\x81\x05"
       "Fifth",
       7, "Fifth", 5},
      {"\x81\x05"
       "Sixth",
       7, "Sixth", 5},
      {"\x81\x07"
       "Seventh",
       9, "Seventh", 7},
      {"\x81\x06"
       "Eighth",
       8, "Eighth", 6},
      {"\x81\x05"
       "Ninth",
       7, "Ninth", 5},
      {"\x81\x05"
       "Tenth",
       7, "Tenth", 5}};
  static constexpr int kNumInputs = std::size(kInputs);

  std::vector<char> input;
  // Concatenate all frames.
  for (const auto& data : kInputs) {
    input.insert(input.end(), data.frame, data.frame + data.frame_length);
  }

  WebSocketFrameParser parser;

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
  EXPECT_TRUE(parser.Decode(base::as_byte_span(input), &frames));
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
  static constexpr size_t kFrameHeaderSize = 2;

  for (size_t cutting_pos = 0; cutting_pos < kHelloLength; ++cutting_pos) {
    base::span<const uint8_t> input1 =
        base::byte_span_from_cstring(kHelloFrame)
            .first(kFrameHeaderSize + cutting_pos);
    base::span<const uint8_t> input2 =
        base::byte_span_from_cstring(kHelloFrame).subspan(input1.size());

    std::vector<char> expected1(kHello, kHello + cutting_pos);
    std::vector<char> expected2(kHello + cutting_pos, kHello + kHelloLength);

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames1;
    EXPECT_TRUE(parser.Decode(input1, &frames1));
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
    EXPECT_TRUE(parser.Decode(input2, &frames2));
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
  static constexpr size_t kFrameHeaderSize = 6;

  for (size_t cutting_pos = 0; cutting_pos < kHelloLength; ++cutting_pos) {
    std::vector<char> input1(
        kMaskedHelloFrame, kMaskedHelloFrame + kFrameHeaderSize + cutting_pos);
    std::vector<char> input2(kMaskedHelloFrame + input1.size(),
                             kMaskedHelloFrame + kMaskedHelloFrameLength);

    std::vector<char> expected1(kHello, kHello + cutting_pos);
    std::vector<char> expected2(kHello + cutting_pos, kHello + kHelloLength);

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames1;
    EXPECT_TRUE(parser.Decode(base::as_byte_span(input1), &frames1));
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
      MaskWebSocketFramePayload(header1->masking_key, 0,
                                base::as_writable_byte_span(payload1));
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
    EXPECT_TRUE(parser.Decode(base::as_byte_span(input2), &frames2));
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
      MaskWebSocketFramePayload(header1->masking_key, cutting_pos,
                                base::as_writable_byte_span(payload2));
      EXPECT_EQ(expected2, payload2);
    }
    const WebSocketFrameHeader* header2 = frame2->header.get();
    EXPECT_TRUE(header2 == nullptr);
  }
}

TEST(WebSocketFrameParserTest, DecodeFramesOfVariousLengths) {
  for (const auto& test : kFrameHeaderTests) {
    const std::string_view frame_header = test.frame_header;
    uint64_t frame_length = test.frame_length;

    std::vector<char> input(frame_header.begin(), frame_header.end());
    // Limit the payload size not to flood the console on failure.
    static constexpr uint64_t kMaxPayloadSize = 200;
    uint64_t input_payload_size = std::min(frame_length, kMaxPayloadSize);
    input.insert(input.end(), input_payload_size, 'a');

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_EQ(test.error_code == kWebSocketNormalClosure,
              parser.Decode(base::as_byte_span(input), &frames));
    EXPECT_EQ(test.error_code, parser.websocket_error());
    if (test.error_code != kWebSocketNormalClosure) {
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
    const std::string_view frame_header = kFrameHeaderTests[i].frame_header;
    size_t frame_header_length = frame_header.length();
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
      EXPECT_EQ(!failed,
                parser.Decode(base::as_byte_span(frame_header).subspan(j, 1u),
                              &frames));
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
    const std::string_view frame_header;
  };
  static constexpr TestCase kTests[] = {
      // For frames with two-byte extended length field, the payload length
      // should be 126 (0x7E) bytes or more.
      {{"\x81\x7E\x00\x00", 4}},
      {{"\x81\x7E\x00\x7D", 4}},
      // For frames with eight-byte extended length field, the payload length
      // should be 0x10000 bytes or more.
      {{"\x81\x7F\x00\x00\x00\x00\x00\x00\x00\x00", 10}},
      {{"\x81\x7E\x00\x00\x00\x00\x00\x00\xFF\xFF", 10}},
  };

  for (const auto& test : kTests) {
    const std::string_view frame_header = test.frame_header;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_FALSE(parser.Decode(base::as_byte_span(frame_header), &frames));
    EXPECT_EQ(kWebSocketErrorProtocolError, parser.websocket_error());
    EXPECT_EQ(0u, frames.size());

    // Once the parser has failed, it no longer accepts any input (even if
    // the input is empty).
    EXPECT_FALSE(parser.Decode(base::byte_span_from_cstring(""), &frames));
    EXPECT_EQ(kWebSocketErrorProtocolError, parser.websocket_error());
    EXPECT_EQ(0u, frames.size());
  }
}

TEST(WebSocketFrameParserTest, FrameTypes) {
  struct TestCase {
    const std::string_view frame_header;
    WebSocketFrameHeader::OpCode opcode;
  };
  static constexpr TestCase kTests[] = {
      {{"\x80\x00", 2}, WebSocketFrameHeader::kOpCodeContinuation},
      {{"\x81\x00", 2}, WebSocketFrameHeader::kOpCodeText},
      {{"\x82\x00", 2}, WebSocketFrameHeader::kOpCodeBinary},
      {{"\x88\x00", 2}, WebSocketFrameHeader::kOpCodeClose},
      {{"\x89\x00", 2}, WebSocketFrameHeader::kOpCodePing},
      {{"\x8A\x00", 2}, WebSocketFrameHeader::kOpCodePong},
      // These are undefined opcodes, but the parser needs to be able to parse
      // them anyway.
      {{"\x83\x00", 2}, 0x3},
      {{"\x84\x00", 2}, 0x4},
      {{"\x85\x00", 2}, 0x5},
      {{"\x86\x00", 2}, 0x6},
      {{"\x87\x00", 2}, 0x7},
      {{"\x8B\x00", 2}, 0xB},
      {{"\x8C\x00", 2}, 0xC},
      {{"\x8D\x00", 2}, 0xD},
      {{"\x8E\x00", 2}, 0xE},
      {{"\x8F\x00", 2}, 0xF}};

  for (const auto& test : kTests) {
    const std::string_view frame_header = test.frame_header;
    WebSocketFrameHeader::OpCode opcode = test.opcode;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_TRUE(parser.Decode(base::as_byte_span(frame_header), &frames));
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
    const std::string_view frame_header;
    bool final;
    bool reserved1;
    bool reserved2;
    bool reserved3;
  };
  static constexpr TestCase kTests[] = {
      {{"\x81\x00", 2}, true, false, false, false},
      {{"\x01\x00", 2}, false, false, false, false},
      {{"\xC1\x00", 2}, true, true, false, false},
      {{"\xA1\x00", 2}, true, false, true, false},
      {{"\x91\x00", 2}, true, false, false, true},
      {{"\x71\x00", 2}, false, true, true, true},
      {{"\xF1\x00", 2}, true, true, true, true}};

  for (const auto& test : kTests) {
    const std::string_view frame_header = test.frame_header;
    bool final = test.final;
    bool reserved1 = test.reserved1;
    bool reserved2 = test.reserved2;
    bool reserved3 = test.reserved3;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_TRUE(parser.Decode(base::as_byte_span(frame_header), &frames));
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

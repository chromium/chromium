// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame_parser.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "net/websockets/websocket_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

constexpr auto kHello = base::span_from_cstring("Hello, world!");
constexpr size_t kHelloLength = kHello.size();
constexpr char kHelloFrame[] = "\x81\x0DHello, world!";
constexpr char kMaskedHelloFrame[] =
    "\x81\x8D\xDE\xAD\xBE\xEF"
    "\x96\xC8\xD2\x83\xB1\x81\x9E\x98\xB1\xDF\xD2\x8B\xFF";

std::vector<uint8_t> ConvertToUint8Vector(std::string_view input) {
  return base::ToVector(input, [](char c) { return static_cast<uint8_t>(c); });
}

struct FrameHeaderTestCase {
  const std::string_view frame_header;
  uint64_t frame_length;
  WebSocketError error_code;
};

constexpr auto kFrameHeaderTests = std::to_array<FrameHeaderTestCase>({
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
     kWebSocketErrorMessageTooBig},
});
constexpr int kNumFrameHeaderTests = std::size(kFrameHeaderTests);

TEST(WebSocketFrameParserTest, DecodeNormalFrame) {
  WebSocketFrameParser parser;

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;

  auto frame_data = ConvertToUint8Vector(kHelloFrame);
  EXPECT_TRUE(parser.Decode(frame_data, &frames));
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

  ASSERT_EQ(kHelloLength, frame->payload.size());
  EXPECT_EQ(kHello, frame->payload);
}

TEST(WebSocketFrameParserTest, DecodeMaskedFrame) {
  WebSocketFrameParser parser;

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;

  auto frame_data = ConvertToUint8Vector(kMaskedHelloFrame);
  EXPECT_TRUE(parser.Decode(frame_data, &frames));
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

  ASSERT_EQ(kHelloLength, frame->payload.size());

  std::string payload(frame->payload.begin(), frame->payload.end());
  MaskWebSocketFramePayload(header->masking_key, 0,
                            base::as_writable_byte_span(payload));
  EXPECT_EQ(base::span(payload), kHello);
}

TEST(WebSocketFrameParserTest, DecodeManyFrames) {
  struct Input {
    base::span<const char> frame;
    base::span<const char> expected_payload;
  };
  constexpr static const auto kInputs = std::to_array<Input>({
      // Each |frame| data is split into two string literals because C++ lexers
      // consume unlimited number of hex characters in a hex character escape
      // (e.g. "\x05F" is not treated as { '\x5', 'F', '\0' } but as
      // { '\x5F', '\0' }).
      {base::span_from_cstring("\x81\x05"
                               "First"),
       base::span_from_cstring("First")},
      {base::span_from_cstring("\x81\x06"
                               "Second"),
       base::span_from_cstring("Second")},
      {base::span_from_cstring("\x81\x05"
                               "Third"),
       base::span_from_cstring("Third")},
      {base::span_from_cstring("\x81\x06"
                               "Fourth"),
       base::span_from_cstring("Fourth")},
      {base::span_from_cstring("\x81\x05"
                               "Fifth"),
       base::span_from_cstring("Fifth")},
      {base::span_from_cstring("\x81\x05"
                               "Sixth"),
       base::span_from_cstring("Sixth")},
      {base::span_from_cstring("\x81\x07"
                               "Seventh"),
       base::span_from_cstring("Seventh")},
      {base::span_from_cstring("\x81\x06"
                               "Eighth"),
       base::span_from_cstring("Eighth")},
      {base::span_from_cstring("\x81\x05"
                               "Ninth"),
       base::span_from_cstring("Ninth")},
      {base::span_from_cstring("\x81\x05"
                               "Tenth"),
       base::span_from_cstring("Tenth")},
  });
  static constexpr int kNumInputs = std::size(kInputs);

  std::vector<uint8_t> input;
  // Concatenate all frames.
  for (const auto& data : kInputs) {
    input.insert(input.end(), data.frame.begin(), data.frame.end());
  }

  WebSocketFrameParser parser;

  std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
  EXPECT_TRUE(parser.Decode(input, &frames));
  EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
  ASSERT_EQ(static_cast<size_t>(kNumInputs), frames.size());

  for (int i = 0; i < kNumInputs; ++i) {
    WebSocketFrameChunk* frame = frames[i].get();
    EXPECT_TRUE(frame != nullptr);
    if (!frame)
      continue;
    EXPECT_TRUE(frame->final_chunk);
    ASSERT_EQ(kInputs[i].expected_payload.size(),
              static_cast<uint64_t>(frame->payload.size()));
    EXPECT_EQ(kInputs[i].expected_payload, frame->payload);

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
    EXPECT_EQ(kInputs[i].expected_payload.size(), header->payload_length);
  }
}

TEST(WebSocketFrameParserTest, DecodePartialFrame) {
  static constexpr size_t kFrameHeaderSize = 2;

  std::vector<uint8_t> hello_frame_data = ConvertToUint8Vector(kHelloFrame);

  for (size_t cutting_pos = 0; cutting_pos < kHelloLength; ++cutting_pos) {
    auto [input1, input2] =
        base::span(hello_frame_data).split_at(kFrameHeaderSize + cutting_pos);

    auto [expected1, expected2] = kHello.split_at(cutting_pos);

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
      ASSERT_EQ(cutting_pos, frame1->payload.size());
      EXPECT_EQ(expected1, frame1->payload);
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
      EXPECT_EQ(nullptr, frame2->payload.data());
    } else {
      ASSERT_EQ(expected2.size(), frame2->payload.size());
      EXPECT_EQ(expected2, frame2->payload);
    }
    const WebSocketFrameHeader* header2 = frame2->header.get();
    EXPECT_TRUE(header2 == nullptr);
  }
}

TEST(WebSocketFrameParserTest, DecodePartialMaskedFrame) {
  static constexpr size_t kFrameHeaderSize = 6;

  std::vector<uint8_t> masked_hello_frame_data =
      ConvertToUint8Vector(kMaskedHelloFrame);

  for (size_t cutting_pos = 0; cutting_pos < kHelloLength; ++cutting_pos) {
    auto [input1, input2] = base::span(masked_hello_frame_data)
                                .split_at(kFrameHeaderSize + cutting_pos);

    auto [expected1, expected2] = kHello.split_at(cutting_pos);

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
    const WebSocketFrameHeader* header1 = frame1->header.get();
    EXPECT_TRUE(header1 != nullptr);
    if (!header1)
      continue;
    if (expected1.size() == 0) {
      EXPECT_EQ(nullptr, frame1->payload.data());
    } else {
      ASSERT_EQ(expected1.size(), frame1->payload.size());
      std::vector<char> payload1(frame1->payload.begin(),
                                 frame1->payload.end());
      MaskWebSocketFramePayload(header1->masking_key, 0,
                                base::as_writable_byte_span(payload1));
      EXPECT_EQ(expected1, base::span(payload1));
    }
    EXPECT_TRUE(header1->final);
    EXPECT_FALSE(header1->reserved1);
    EXPECT_FALSE(header1->reserved2);
    EXPECT_FALSE(header1->reserved3);
    EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, header1->opcode);
    EXPECT_TRUE(header1->masked);
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
      EXPECT_EQ(nullptr, frame2->payload.data());
    } else {
      ASSERT_EQ(expected2.size(), frame2->payload.size());
      std::vector<char> payload2(frame2->payload.begin(),
                                 frame2->payload.end());
      MaskWebSocketFramePayload(header1->masking_key, cutting_pos,
                                base::as_writable_byte_span(payload2));
      EXPECT_EQ(expected2, base::span(payload2));
    }
    const WebSocketFrameHeader* header2 = frame2->header.get();
    EXPECT_TRUE(header2 == nullptr);
  }
}

TEST(WebSocketFrameParserTest, DecodeFramesOfVariousLengths) {
  for (const auto& test : kFrameHeaderTests) {
    auto frame_header = ConvertToUint8Vector(test.frame_header);
    uint64_t frame_length = test.frame_length;

    std::vector<uint8_t> input(frame_header);
    static constexpr uint64_t kMaxPayloadSize = 200;
    uint64_t input_payload_size = std::min(frame_length, kMaxPayloadSize);
    input.insert(input.end(), input_payload_size, 'a');

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_EQ(test.error_code == kWebSocketNormalClosure,
              parser.Decode(input, &frames));
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
    auto frame_header = ConvertToUint8Vector(kFrameHeaderTests[i].frame_header);
    size_t frame_header_length = frame_header.size();
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
      EXPECT_EQ(!failed, parser.Decode(base::span(frame_header).subspan(j, 1u),
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
    auto frame_header = ConvertToUint8Vector(test.frame_header);

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_EQ(kWebSocketNormalClosure, parser.websocket_error());
    EXPECT_FALSE(parser.Decode(frame_header, &frames));
    EXPECT_EQ(kWebSocketErrorProtocolError, parser.websocket_error());
    EXPECT_EQ(0u, frames.size());

    std::vector<uint8_t> empty_frame_data;
    EXPECT_FALSE(parser.Decode(empty_frame_data, &frames));
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
      {{"\x83\x00", 2}, WebSocketFrameHeader::kOpCodeDataUnused3},
      {{"\x84\x00", 2}, WebSocketFrameHeader::kOpCodeDataUnused4},
      {{"\x85\x00", 2}, WebSocketFrameHeader::kOpCodeDataUnused5},
      {{"\x86\x00", 2}, WebSocketFrameHeader::kOpCodeDataUnused6},
      {{"\x87\x00", 2}, WebSocketFrameHeader::kOpCodeDataUnused7},
      {{"\x88\x00", 2}, WebSocketFrameHeader::kOpCodeClose},
      {{"\x89\x00", 2}, WebSocketFrameHeader::kOpCodePing},
      {{"\x8A\x00", 2}, WebSocketFrameHeader::kOpCodePong},
      {{"\x8B\x00", 2}, WebSocketFrameHeader::kOpCodeControlUnusedB},
      {{"\x8C\x00", 2}, WebSocketFrameHeader::kOpCodeControlUnusedC},
      {{"\x8D\x00", 2}, WebSocketFrameHeader::kOpCodeControlUnusedD},
      {{"\x8E\x00", 2}, WebSocketFrameHeader::kOpCodeControlUnusedE},
      {{"\x8F\x00", 2}, WebSocketFrameHeader::kOpCodeControlUnusedF},
  };

  for (const auto& test : kTests) {
    auto frame_header = ConvertToUint8Vector(test.frame_header);

    WebSocketFrameHeader::OpCode opcode = test.opcode;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_TRUE(parser.Decode(frame_header, &frames));
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
    auto frame_header = ConvertToUint8Vector(test.frame_header);

    bool final = test.final;
    bool reserved1 = test.reserved1;
    bool reserved2 = test.reserved2;
    bool reserved3 = test.reserved3;

    WebSocketFrameParser parser;

    std::vector<std::unique_ptr<WebSocketFrameChunk>> frames;
    EXPECT_TRUE(parser.Decode(frame_header, &frames));
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

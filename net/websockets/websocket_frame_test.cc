// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/websockets/websocket_frame.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/ranges/algorithm.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(WebSocketFrameHeaderTest, FrameLengths) {
  struct TestCase {
    const std::string_view frame_header;
    uint64_t frame_length;
  };
  static constexpr TestCase kTests[] = {
      {{"\x81\x00", 2}, UINT64_C(0)},
      {{"\x81\x7D", 2}, UINT64_C(125)},
      {{"\x81\x7E\x00\x7E", 4}, UINT64_C(126)},
      {{"\x81\x7E\xFF\xFF", 4}, UINT64_C(0xFFFF)},
      {{"\x81\x7F\x00\x00\x00\x00\x00\x01\x00\x00", 10}, UINT64_C(0x10000)},
      {{"\x81\x7F\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 10},
       UINT64_C(0x7FFFFFFFFFFFFFFF)}};

  for (const auto& test : kTests) {
    WebSocketFrameHeader header(WebSocketFrameHeader::kOpCodeText);
    header.final = true;
    header.payload_length = test.frame_length;

    std::vector<char> expected_output(test.frame_header.begin(),
                                      test.frame_header.end());
    std::vector<char> output(expected_output.size());
    EXPECT_EQ(static_cast<int>(expected_output.size()),
              WriteWebSocketFrameHeader(header, nullptr,
                                        base::as_writable_byte_span(output)));
    EXPECT_EQ(expected_output, output);
  }
}

TEST(WebSocketFrameHeaderTest, FrameLengthsWithMasking) {
  static constexpr std::string_view kMaskingKey = "\xDE\xAD\xBE\xEF";
  static_assert(kMaskingKey.size() == WebSocketFrameHeader::kMaskingKeyLength,
                "incorrect masking key size");

  struct TestCase {
    const std::string_view frame_header;
    uint64_t frame_length;
  };
  static constexpr TestCase kTests[] = {
      {{"\x81\x80\xDE\xAD\xBE\xEF", 6}, UINT64_C(0)},
      {{"\x81\xFD\xDE\xAD\xBE\xEF", 6}, UINT64_C(125)},
      {{"\x81\xFE\x00\x7E\xDE\xAD\xBE\xEF", 8}, UINT64_C(126)},
      {{"\x81\xFE\xFF\xFF\xDE\xAD\xBE\xEF", 8}, UINT64_C(0xFFFF)},
      {{"\x81\xFF\x00\x00\x00\x00\x00\x01\x00\x00\xDE\xAD\xBE\xEF", 14},
       UINT64_C(0x10000)},
      {{"\x81\xFF\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xDE\xAD\xBE\xEF", 14},
       UINT64_C(0x7FFFFFFFFFFFFFFF)}};

  WebSocketMaskingKey masking_key;
  base::as_writable_byte_span(masking_key.key)
      .copy_from(base::as_byte_span(kMaskingKey));

  for (const auto& test : kTests) {
    WebSocketFrameHeader header(WebSocketFrameHeader::kOpCodeText);
    header.final = true;
    header.masked = true;
    header.payload_length = test.frame_length;

    std::vector<char> expected_output(test.frame_header.begin(),
                                      test.frame_header.end());
    std::vector<char> output(expected_output.size());
    EXPECT_EQ(static_cast<int>(expected_output.size()),
              WriteWebSocketFrameHeader(header, &masking_key,
                                        base::as_writable_byte_span(output)));
    EXPECT_EQ(expected_output, output);
  }
}

TEST(WebSocketFrameHeaderTest, FrameOpCodes) {
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
      // These are undefined opcodes, but the builder should accept them anyway.
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
    WebSocketFrameHeader header(test.opcode);
    header.final = true;
    header.payload_length = 0;

    std::vector<char> expected_output(test.frame_header.begin(),
                                      test.frame_header.end());
    std::vector<char> output(expected_output.size());
    EXPECT_EQ(static_cast<int>(expected_output.size()),
              WriteWebSocketFrameHeader(header, nullptr,
                                        base::as_writable_byte_span(output)));
    EXPECT_EQ(expected_output, output);
  }
}

TEST(WebSocketFrameHeaderTest, FinalBitAndReservedBits) {
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
    WebSocketFrameHeader header(WebSocketFrameHeader::kOpCodeText);
    header.final = test.final;
    header.reserved1 = test.reserved1;
    header.reserved2 = test.reserved2;
    header.reserved3 = test.reserved3;
    header.payload_length = 0;

    std::vector<char> expected_output(test.frame_header.begin(),
                                      test.frame_header.end());
    std::vector<char> output(expected_output.size());
    EXPECT_EQ(static_cast<int>(expected_output.size()),
              WriteWebSocketFrameHeader(header, nullptr,
                                        base::as_writable_byte_span(output)));
    EXPECT_EQ(expected_output, output);
  }
}

TEST(WebSocketFrameHeaderTest, InsufficientBufferSize) {
  struct TestCase {
    uint64_t payload_length;
    bool masked;
    size_t expected_header_size;
  };
  static constexpr TestCase kTests[] = {
      {UINT64_C(0), false, 2u},
      {UINT64_C(125), false, 2u},
      {UINT64_C(126), false, 4u},
      {UINT64_C(0xFFFF), false, 4u},
      {UINT64_C(0x10000), false, 10u},
      {UINT64_C(0x7FFFFFFFFFFFFFFF), false, 10u},
      {UINT64_C(0), true, 6u},
      {UINT64_C(125), true, 6u},
      {UINT64_C(126), true, 8u},
      {UINT64_C(0xFFFF), true, 8u},
      {UINT64_C(0x10000), true, 14u},
      {UINT64_C(0x7FFFFFFFFFFFFFFF), true, 14u}};

  for (const auto& test : kTests) {
    WebSocketFrameHeader header(WebSocketFrameHeader::kOpCodeText);
    header.final = true;
    header.opcode = WebSocketFrameHeader::kOpCodeText;
    header.masked = test.masked;
    header.payload_length = test.payload_length;

    std::array<uint8_t, 14> dummy_buffer;
    // Set an insufficient size to |buffer_size|.
    EXPECT_EQ(
        ERR_INVALID_ARGUMENT,
        WriteWebSocketFrameHeader(
            header, nullptr,
            base::span(dummy_buffer).first(test.expected_header_size - 1)));
  }
}

TEST(WebSocketFrameTest, MaskPayload) {
  struct TestCase {
    const std::string_view masking_key;
    uint64_t frame_offset;
    const char* input;
    const char* output;
    size_t data_length;
  };
  static constexpr TestCase kTests[] = {
      {"\xDE\xAD\xBE\xEF", 0, "FooBar", "\x98\xC2\xD1\xAD\xBF\xDF", 6},
      {"\xDE\xAD\xBE\xEF", 1, "FooBar", "\xEB\xD1\x80\x9C\xCC\xCC", 6},
      {"\xDE\xAD\xBE\xEF", 2, "FooBar", "\xF8\x80\xB1\xEF\xDF\x9D", 6},
      {"\xDE\xAD\xBE\xEF", 3, "FooBar", "\xA9\xB1\xC2\xFC\x8E\xAC", 6},
      {"\xDE\xAD\xBE\xEF", 4, "FooBar", "\x98\xC2\xD1\xAD\xBF\xDF", 6},
      {"\xDE\xAD\xBE\xEF", 42, "FooBar", "\xF8\x80\xB1\xEF\xDF\x9D", 6},
      {"\xDE\xAD\xBE\xEF", 0, "", "", 0},
      {"\xDE\xAD\xBE\xEF", 0, "\xDE\xAD\xBE\xEF", "\x00\x00\x00\x00", 4},
      {"\xDE\xAD\xBE\xEF", 0, "\x00\x00\x00\x00", "\xDE\xAD\xBE\xEF", 4},
      {{"\x00\x00\x00\x00", WebSocketFrameHeader::kMaskingKeyLength},
       0,
       "FooBar",
       "FooBar",
       6},
      {"\xFF\xFF\xFF\xFF", 0, "FooBar", "\xB9\x90\x90\xBD\x9E\x8D", 6},
  };

  for (const auto& test : kTests) {
    WebSocketMaskingKey masking_key;
    base::as_writable_byte_span(masking_key.key)
        .copy_from(base::as_byte_span(test.masking_key));
    std::vector<char> frame_data(test.input, test.input + test.data_length);
    std::vector<char> expected_output(test.output,
                                      test.output + test.data_length);
    MaskWebSocketFramePayload(masking_key, test.frame_offset,
                              base::as_writable_byte_span(frame_data));
    EXPECT_EQ(expected_output, frame_data);
  }
}

// Check that all combinations of alignment, frame offset and chunk size work
// correctly for MaskWebSocketFramePayload(). This is mainly used to ensure that
// vectorisation optimisations don't break anything. We could take a "white box"
// approach and only test the edge cases, but since the exhaustive "black box"
// approach runs in acceptable time, we don't have to take the risk of being
// clever.
//
// This brute-force approach runs in O(N^3) time where N is the size of the
// maximum vector size we want to test again. This might need reconsidering if
// MaskWebSocketFramePayload() is ever optimised for a dedicated vector
// architecture.
TEST(WebSocketFrameTest, MaskPayloadAlignment) {
  // This reflects what might be implemented in the future, rather than
  // the current implementation. FMA3 and FMA4 support 256-bit vector ops.
  static constexpr size_t kMaxVectorSizeInBits = 256;
  static constexpr size_t kMaxVectorSize = kMaxVectorSizeInBits / 8;
  static constexpr size_t kMaxVectorAlignment = kMaxVectorSize;
  static constexpr size_t kMaskingKeyLength =
      WebSocketFrameHeader::kMaskingKeyLength;
  static constexpr size_t kScratchBufferSize =
      kMaxVectorAlignment + kMaxVectorSize * 2;
  static constexpr std::string_view kTestMask = "\xd2\xba\x5a\xbe";
  // We use 786 bits of random input to reduce the risk of correlated errors.
  static constexpr char kTestInput[] = {
      "\x3d\x77\x1d\x1b\x19\x8c\x48\xa3\x19\x6d\xf7\xcc\x39\xe7\x57\x0b"
      "\x69\x8c\xda\x4b\xfc\xac\x2c\xd3\x49\x96\x6e\x8a\x7b\x5a\x32\x76"
      "\xd0\x11\x43\xa0\x89\xfc\x76\x2b\x10\x2f\x4c\x7b\x4f\xa6\xdd\xe4"
      "\xfc\x8e\xd8\x72\xcf\x7e\x37\xcd\x31\xcd\xc1\xc0\x89\x0c\xa7\x4c"
      "\xda\xa8\x4b\x75\xa1\xcb\xa9\x77\x19\x4d\x6e\xdf\xc8\x08\x1c\xb6"
      "\x6d\xfb\x38\x04\x44\xd5\xba\x57\x9f\x76\xb0\x2e\x07\x91\xe6\xa8"};
  static constexpr size_t kTestInputSize = std::size(kTestInput) - 1;
  static constexpr char kTestOutput[] = {
      "\xef\xcd\x47\xa5\xcb\x36\x12\x1d\xcb\xd7\xad\x72\xeb\x5d\x0d\xb5"
      "\xbb\x36\x80\xf5\x2e\x16\x76\x6d\x9b\x2c\x34\x34\xa9\xe0\x68\xc8"
      "\x02\xab\x19\x1e\x5b\x46\x2c\x95\xc2\x95\x16\xc5\x9d\x1c\x87\x5a"
      "\x2e\x34\x82\xcc\x1d\xc4\x6d\x73\xe3\x77\x9b\x7e\x5b\xb6\xfd\xf2"
      "\x08\x12\x11\xcb\x73\x71\xf3\xc9\xcb\xf7\x34\x61\x1a\xb2\x46\x08"
      "\xbf\x41\x62\xba\x96\x6f\xe0\xe9\x4d\xcc\xea\x90\xd5\x2b\xbc\x16"};
  static_assert(std::size(kTestInput) == std::size(kTestOutput),
                "output and input arrays should have the same length");
  std::unique_ptr<char, base::AlignedFreeDeleter> scratch(static_cast<char*>(
      base::AlignedAlloc(kScratchBufferSize, kMaxVectorAlignment)));
  WebSocketMaskingKey masking_key;
  base::as_writable_byte_span(masking_key.key)
      .copy_from(base::as_byte_span(kTestMask));
  for (size_t frame_offset = 0; frame_offset < kMaskingKeyLength;
       ++frame_offset) {
    for (size_t alignment = 0; alignment < kMaxVectorAlignment; ++alignment) {
      char* const aligned_scratch = scratch.get() + alignment;
      const size_t aligned_len = std::min(kScratchBufferSize - alignment,
                                          kTestInputSize - frame_offset);
      for (size_t chunk_size = 1; chunk_size < kMaxVectorSize; ++chunk_size) {
        memcpy(aligned_scratch, kTestInput + frame_offset, aligned_len);
        for (size_t chunk_start = 0; chunk_start < aligned_len;
             chunk_start += chunk_size) {
          const size_t this_chunk_size =
              std::min(chunk_size, aligned_len - chunk_start);
          MaskWebSocketFramePayload(
              masking_key, frame_offset + chunk_start,
              base::as_writable_bytes(base::make_span(
                  aligned_scratch + chunk_start, this_chunk_size)));
        }
        // Stop the test if it fails, since we don't want to spew thousands of
        // failures.
        ASSERT_TRUE(std::equal(aligned_scratch,
                               aligned_scratch + aligned_len,
                               kTestOutput + frame_offset))
            << "Output failed to match for frame_offset=" << frame_offset
            << ", alignment=" << alignment << ", chunk_size=" << chunk_size;
      }
    }
  }
}

// "IsKnownDataOpCode" is currently implemented in an "obviously correct"
// manner, but we test is anyway in case it changes to a more complex
// implementation in future.
TEST(WebSocketFrameHeaderTest, IsKnownDataOpCode) {
  // Make the test less verbose.
  typedef WebSocketFrameHeader Frame;

  // Known opcode, is used for data frames
  EXPECT_TRUE(Frame::IsKnownDataOpCode(Frame::kOpCodeContinuation));
  EXPECT_TRUE(Frame::IsKnownDataOpCode(Frame::kOpCodeText));
  EXPECT_TRUE(Frame::IsKnownDataOpCode(Frame::kOpCodeBinary));

  // Known opcode, is used for control frames
  EXPECT_FALSE(Frame::IsKnownDataOpCode(Frame::kOpCodeClose));
  EXPECT_FALSE(Frame::IsKnownDataOpCode(Frame::kOpCodePing));
  EXPECT_FALSE(Frame::IsKnownDataOpCode(Frame::kOpCodePong));

  // Check that unused opcodes return false
  EXPECT_FALSE(Frame::IsKnownDataOpCode(Frame::kOpCodeDataUnused));
  EXPECT_FALSE(Frame::IsKnownDataOpCode(Frame::kOpCodeControlUnused));

  // Check that opcodes with the 4 bit set return false
  EXPECT_FALSE(Frame::IsKnownDataOpCode(0x6));
  EXPECT_FALSE(Frame::IsKnownDataOpCode(0xF));

  // Check that out-of-range opcodes return false
  EXPECT_FALSE(Frame::IsKnownDataOpCode(-1));
  EXPECT_FALSE(Frame::IsKnownDataOpCode(0xFF));
}

// "IsKnownControlOpCode" is implemented in an "obviously correct" manner but
// might be optimised in future.
TEST(WebSocketFrameHeaderTest, IsKnownControlOpCode) {
  // Make the test less verbose.
  typedef WebSocketFrameHeader Frame;

  // Known opcode, is used for data frames
  EXPECT_FALSE(Frame::IsKnownControlOpCode(Frame::kOpCodeContinuation));
  EXPECT_FALSE(Frame::IsKnownControlOpCode(Frame::kOpCodeText));
  EXPECT_FALSE(Frame::IsKnownControlOpCode(Frame::kOpCodeBinary));

  // Known opcode, is used for control frames
  EXPECT_TRUE(Frame::IsKnownControlOpCode(Frame::kOpCodeClose));
  EXPECT_TRUE(Frame::IsKnownControlOpCode(Frame::kOpCodePing));
  EXPECT_TRUE(Frame::IsKnownControlOpCode(Frame::kOpCodePong));

  // Check that unused opcodes return false
  EXPECT_FALSE(Frame::IsKnownControlOpCode(Frame::kOpCodeDataUnused));
  EXPECT_FALSE(Frame::IsKnownControlOpCode(Frame::kOpCodeControlUnused));

  // Check that opcodes with the 4 bit set return false
  EXPECT_FALSE(Frame::IsKnownControlOpCode(0x6));
  EXPECT_FALSE(Frame::IsKnownControlOpCode(0xF));

  // Check that out-of-range opcodes return false
  EXPECT_FALSE(Frame::IsKnownControlOpCode(-1));
  EXPECT_FALSE(Frame::IsKnownControlOpCode(0xFF));
}

}  // namespace

}  // namespace net

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/http2/hpack/varint/hpack_varint_decoder.h"

// Test HpackVarintDecoder against hardcoded data.

#include <stddef.h>

#include "base/logging.h"
#include "net/third_party/http2/platform/api/http2_arraysize.h"
#include "net/third_party/http2/platform/api/http2_string_piece.h"
#include "net/third_party/http2/platform/api/http2_string_utils.h"
#include "net/third_party/http2/tools/random_decoder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;

namespace http2 {
namespace test {
namespace {

class HpackVarintDecoderTest : public RandomDecoderTest,
                               public ::testing::WithParamInterface<
                                   ::testing::tuple<uint8_t, const char*>> {
 protected:
  HpackVarintDecoderTest()
      : high_bits_(::testing::get<0>(GetParam())),
        suffix_(Http2HexDecode(::testing::get<1>(GetParam()))),
        prefix_length_(0) {}

  void DecodeExpectSuccess(Http2StringPiece data,
                           uint32_t prefix_length,
                           uint32_t expected_value) {
    Validator validator = [expected_value, this](
                              const DecodeBuffer& db,
                              DecodeStatus status) -> AssertionResult {
      VERIFY_EQ(expected_value, decoder_.value())
          << "Value doesn't match expected: " << decoder_.value()
          << " != " << expected_value;
      return AssertionSuccess();
    };

    // First validate that decoding is done and that we've advanced the cursor
    // the expected amount.
    validator = ValidateDoneAndOffset(/* offset = */ data.size(), validator);

    EXPECT_TRUE(Decode(data, prefix_length, validator));

    EXPECT_EQ(expected_value, decoder_.value());
  }

  void DecodeExpectError(Http2StringPiece data, uint32_t prefix_length) {
    Validator validator = [](const DecodeBuffer& db,
                             DecodeStatus status) -> AssertionResult {
      VERIFY_EQ(DecodeStatus::kDecodeError, status);
      return AssertionSuccess();
    };

    EXPECT_TRUE(Decode(data, prefix_length, validator));
  }

 private:
  AssertionResult Decode(Http2StringPiece data,
                         uint32_t prefix_length,
                         const Validator validator) {
    prefix_length_ = prefix_length;

    // Copy |data| so that it can be modified.
    Http2String data_copy(data);

    // Bits of the first byte not part of the prefix should be ignored.
    uint8_t high_bits_mask = 0b11111111 << prefix_length_;
    data_copy[0] |= (high_bits_mask & high_bits_);

    // Extra bytes appended to the input should be ignored.
    data_copy.append(suffix_);

    DecodeBuffer b(data_copy);

    // StartDecoding, above, requires the DecodeBuffer be non-empty so that it
    // can call Start with the prefix byte.
    bool return_non_zero_on_first = true;

    return DecodeAndValidateSeveralWays(&b, return_non_zero_on_first,
                                        validator);
  }

  DecodeStatus StartDecoding(DecodeBuffer* b) override {
    CHECK_LT(0u, b->Remaining());
    uint8_t prefix = b->DecodeUInt8();
    return decoder_.Start(prefix, prefix_length_, b);
  }

  DecodeStatus ResumeDecoding(DecodeBuffer* b) override {
    return decoder_.Resume(b);
  }

  // Bits of the first byte not part of the prefix.
  const uint8_t high_bits_;
  // Extra bytes appended to the input.
  const Http2String suffix_;

  HpackVarintDecoder decoder_;
  uint8_t prefix_length_;
};

INSTANTIATE_TEST_CASE_P(
    HpackVarintDecoderTest,
    HpackVarintDecoderTest,
    ::testing::Combine(
        // Bits of the first byte not part of the prefix should be ignored.
        ::testing::Values(0b00000000, 0b11111111, 0b10101010),
        // Extra bytes appended to the input should be ignored.
        ::testing::Values("", "00", "666f6f")));

struct {
  const char* data;
  uint32_t prefix_length;
  uint32_t expected_value;
} kSuccessTestData[] = {
    // Zero value with different prefix lengths.
    {"00", 3, 0},
    {"00", 4, 0},
    {"00", 5, 0},
    {"00", 6, 0},
    {"00", 7, 0},
    // Small values that fit in the prefix.
    {"06", 3, 6},
    {"0d", 4, 13},
    {"10", 5, 16},
    {"29", 6, 41},
    {"56", 7, 86},
    // Values of 2^n-1, which have an all-zero extension byte.
    {"0700", 3, 7},
    {"0f00", 4, 15},
    {"1f00", 5, 31},
    {"3f00", 6, 63},
    {"7f00", 7, 127},
    // Values of 2^n-1, plus one extra byte of padding.
    {"078000", 3, 7},
    {"0f8000", 4, 15},
    {"1f8000", 5, 31},
    {"3f8000", 6, 63},
    {"7f8000", 7, 127},
    // Values requiring one extension byte.
    {"0760", 3, 103},
    {"0f2a", 4, 57},
    {"1f7f", 5, 158},
    {"3f02", 6, 65},
    {"7f49", 7, 200},
    // Values requiring one extension byte, plus one byte of padding.
    {"07e000", 3, 103},
    {"0faa00", 4, 57},
    {"1fff00", 5, 158},
    {"3f8200", 6, 65},
    {"7fc900", 7, 200},
    // Values requiring one extension byte, plus two bytes of padding.
    {"07e08000", 3, 103},
    {"0faa8000", 4, 57},
    {"1fff8000", 5, 158},
    {"3f828000", 6, 65},
    {"7fc98000", 7, 200},
    // Values requiring one extension byte, plus the maximum amount of padding.
    {"07e080808000", 3, 103},
    {"0faa80808000", 4, 57},
    {"1fff80808000", 5, 158},
    {"3f8280808000", 6, 65},
    {"7fc980808000", 7, 200},
    // Values requiring two extension bytes.
    {"07b260", 3, 12345},
    {"0f8a2a", 4, 5401},
    {"1fa87f", 5, 16327},
    {"3fd002", 6, 399},
    {"7fff49", 7, 9598},
    // Values requiring two extension bytes, plus one byte of padding.
    {"07b2e000", 3, 12345},
    {"0f8aaa00", 4, 5401},
    {"1fa8ff00", 5, 16327},
    {"3fd08200", 6, 399},
    {"7fffc900", 7, 9598},
    // Values requiring two extension bytes, plus the maximum amount of padding.
    {"07b2e0808000", 3, 12345},
    {"0f8aaa808000", 4, 5401},
    {"1fa8ff808000", 5, 16327},
    {"3fd082808000", 6, 399},
    {"7fffc9808000", 7, 9598},
    // Values requiring three extension bytes.
    {"078ab260", 3, 1579281},
    {"0fc18a2a", 4, 689488},
    {"1fada87f", 5, 2085964},
    {"3fa0d002", 6, 43103},
    {"7ffeff49", 7, 1212541},
    // Values requiring three extension bytes, plus one byte of padding.
    {"078ab2e000", 3, 1579281},
    {"0fc18aaa00", 4, 689488},
    {"1fada8ff00", 5, 2085964},
    {"3fa0d08200", 6, 43103},
    {"7ffeffc900", 7, 1212541},
    // Values requiring four extension bytes.
    {"079f8ab260", 3, 202147110},
    {"0fa2c18a2a", 4, 88252593},
    {"1fd0ada87f", 5, 266999535},
    {"3ff9a0d002", 6, 5509304},
    {"7f9efeff49", 7, 155189149},
    // Values requiring four extension bytes, plus one byte of padding.
    {"079f8ab2e000", 3, 202147110},
    {"0fa2c18aaa00", 4, 88252593},
    {"1fd0ada8ff00", 5, 266999535},
    {"3ff9a0d08200", 6, 5509304},
    {"7f9efeffc900", 7, 155189149},
    // Examples from RFC7541 C.1.
    {"0a", 5, 10},
    {"1f9a0a", 5, 1337},
};

TEST_P(HpackVarintDecoderTest, Success) {
  for (size_t i = 0; i < HTTP2_ARRAYSIZE(kSuccessTestData); ++i) {
    DecodeExpectSuccess(Http2HexDecode(kSuccessTestData[i].data),
                        kSuccessTestData[i].prefix_length,
                        kSuccessTestData[i].expected_value);
  }
}

// HpackVarintDecoder allows at most five extension bytes,
// and fifth extension byte must be zero.
struct {
  const char* data;
  uint32_t prefix_length;
} kErrorTestData[] = {
    // Maximum number of extension bytes but last byte is non-zero.
    {"078080808001", 3},
    {"0f8080808001", 4},
    {"1f8080808001", 5},
    {"3f8080808001", 6},
    {"7f8080808001", 7},
    // Too many extension bytes, all 0s (except for extension bit in each byte).
    {"078080808080", 3},
    {"0f8080808080", 4},
    {"1f8080808080", 5},
    {"3f8080808080", 6},
    {"7f8080808080", 7},
    // Too many extension bytes, all 1s.
    {"07ffffffffff", 3},
    {"0fffffffffff", 4},
    {"1fffffffffff", 5},
    {"3fffffffffff", 6},
    {"7fffffffffff", 7},
};

TEST_P(HpackVarintDecoderTest, Error) {
  for (size_t i = 0; i < HTTP2_ARRAYSIZE(kErrorTestData); ++i) {
    DecodeExpectError(Http2HexDecode(kErrorTestData[i].data),
                      kErrorTestData[i].prefix_length);
  }
}

}  // namespace
}  // namespace test
}  // namespace http2

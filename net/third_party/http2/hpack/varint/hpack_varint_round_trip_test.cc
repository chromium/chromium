// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/http2/hpack/varint/hpack_varint_decoder.h"

// Test HpackVarintDecoder against data encoded via HpackBlockBuilder,
// which uses HpackVarintEncoder under the hood.

#include <stddef.h>

#include <iterator>
#include <set>
#include <vector>

#include "base/logging.h"
#include "net/third_party/http2/hpack/tools/hpack_block_builder.h"
#include "net/third_party/http2/platform/api/http2_string_piece.h"
#include "net/third_party/http2/platform/api/http2_string_utils.h"
#include "net/third_party/http2/tools/random_decoder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;

namespace http2 {
namespace test {
namespace {

// Returns the highest value with the specified number of extension bytes
// and the specified prefix length (bits).
uint64_t HiValueOfExtensionBytes(uint32_t extension_bytes,
                                 uint32_t prefix_length) {
  return (1 << prefix_length) - 2 +
         (extension_bytes == 0 ? 0 : (1LLU << (extension_bytes * 7)));
}

class HpackVarintRoundTripTest : public RandomDecoderTest {
 protected:
  DecodeStatus StartDecoding(DecodeBuffer* b) override {
    CHECK_LT(0u, b->Remaining());
    uint8_t prefix = b->DecodeUInt8();
    return decoder_.Start(prefix, prefix_length_, b);
  }

  DecodeStatus ResumeDecoding(DecodeBuffer* b) override {
    return decoder_.Resume(b);
  }

  void DecodeSeveralWays(uint32_t expected_value, uint32_t expected_offset) {
    // The validator is called after each of the several times that the input
    // DecodeBuffer is decoded, each with a different segmentation of the input.
    // Validate that decoder_.value() matches the expected value.
    Validator validator = [expected_value, this](
                              const DecodeBuffer& db,
                              DecodeStatus status) -> AssertionResult {
      if (decoder_.value() != expected_value) {
        return AssertionFailure()
               << "Value doesn't match expected: " << decoder_.value()
               << " != " << expected_value;
      }
      return AssertionSuccess();
    };

    // First validate that decoding is done and that we've advanced the cursor
    // the expected amount.
    validator = ValidateDoneAndOffset(expected_offset, validator);

    // StartDecoding, above, requires the DecodeBuffer be non-empty so that it
    // can call Start with the prefix byte.
    bool return_non_zero_on_first = true;

    DecodeBuffer b(buffer_);
    EXPECT_TRUE(
        DecodeAndValidateSeveralWays(&b, return_non_zero_on_first, validator));

    EXPECT_EQ(expected_value, decoder_.value());
    EXPECT_EQ(expected_offset, b.Offset());
  }

  void EncodeNoRandom(uint32_t value, uint8_t prefix_length) {
    DCHECK_LE(3, prefix_length);
    DCHECK_LE(prefix_length, 7);
    prefix_length_ = prefix_length;

    HpackBlockBuilder bb;
    bb.AppendHighBitsAndVarint(0, prefix_length_, value);
    buffer_ = bb.buffer();
    ASSERT_LT(0u, buffer_.size());

    const uint8_t prefix_mask = (1 << prefix_length_) - 1;
    ASSERT_EQ(buffer_[0], buffer_[0] & prefix_mask);
  }

  void Encode(uint32_t value, uint8_t prefix_length) {
    EncodeNoRandom(value, prefix_length);
    // Add some random bits to the prefix (the first byte) above the mask.
    uint8_t prefix = buffer_[0];
    buffer_[0] = prefix | (Random().Rand8() << prefix_length);
    const uint8_t prefix_mask = (1 << prefix_length_) - 1;
    ASSERT_EQ(prefix, buffer_[0] & prefix_mask);
  }

  // This is really a test of HpackBlockBuilder, making sure that the input to
  // HpackVarintDecoder is as expected, which also acts as confirmation that
  // my thinking about the encodings being used by the tests, i.e. cover the
  // range desired.
  void ValidateEncoding(uint32_t value,
                        uint32_t minimum,
                        uint32_t maximum,
                        size_t expected_bytes) {
    ASSERT_EQ(expected_bytes, buffer_.size());
    if (expected_bytes > 1) {
      const uint8_t prefix_mask = (1 << prefix_length_) - 1;
      EXPECT_EQ(prefix_mask, buffer_[0] & prefix_mask);
      size_t last = expected_bytes - 1;
      for (size_t ndx = 1; ndx < last; ++ndx) {
        // Before the last extension byte, we expect the high-bit set.
        uint8_t byte = buffer_[ndx];
        if (value == minimum) {
          EXPECT_EQ(0x80, byte) << "ndx=" << ndx;
        } else if (value == maximum) {
          EXPECT_EQ(0xff, byte) << "ndx=" << ndx;
        } else {
          EXPECT_EQ(0x80, byte & 0x80) << "ndx=" << ndx;
        }
      }
      // The last extension byte should not have the high-bit set.
      uint8_t byte = buffer_[last];
      if (value == minimum) {
        if (expected_bytes == 2) {
          EXPECT_EQ(0x00, byte);
        } else {
          EXPECT_EQ(0x01, byte);
        }
      } else if (value == maximum) {
        EXPECT_EQ(0x7f, byte);
      } else {
        EXPECT_EQ(0x00, byte & 0x80);
      }
    } else {
      const uint8_t prefix_mask = (1 << prefix_length_) - 1;
      EXPECT_EQ(value, static_cast<uint32_t>(buffer_[0] & prefix_mask));
      EXPECT_LT(value, prefix_mask);
    }
  }

  void EncodeAndDecodeValues(const std::set<uint32_t>& values,
                             uint8_t prefix_length,
                             size_t expected_bytes) {
    CHECK(!values.empty());
    const uint32_t minimum = *values.begin();
    const uint32_t maximum = *values.rbegin();
    for (const uint32_t value : values) {
      Encode(value, prefix_length);  // Sets prefix_buffer_

      std::stringstream ss;
      ss << "value=" << value << " (0x" << std::hex << value
         << "), prefix_length=" << prefix_length
         << ", expected_bytes=" << expected_bytes << "\n"
         << Http2HexDump(buffer_);
      Http2String msg(ss.str());

      if (value == minimum) {
        LOG(INFO) << "Checking minimum; " << msg;
      } else if (value == maximum) {
        LOG(INFO) << "Checking maximum; " << msg;
      }

      SCOPED_TRACE(msg);
      ValidateEncoding(value, minimum, maximum, expected_bytes);
      DecodeSeveralWays(value, expected_bytes);

      // Append some random data to the end of buffer_ and repeat. That random
      // data should be ignored.
      buffer_.append(Random().RandString(1 + Random().Uniform(10)));
      DecodeSeveralWays(value, expected_bytes);

      // If possible, add extension bytes that don't change the value.
      if (1 < expected_bytes) {
        buffer_.resize(expected_bytes);
        for (uint8_t total_bytes = expected_bytes + 1; total_bytes <= 6;
             ++total_bytes) {
          // Mark the current last byte as not being the last one.
          EXPECT_EQ(0x00, 0x80 & buffer_.back());
          buffer_.back() |= 0x80;
          buffer_.push_back('\0');
          DecodeSeveralWays(value, total_bytes);
        }
      }
    }
  }

  void EncodeAndDecodeValuesInRange(uint32_t start,
                                    uint32_t range,
                                    uint8_t prefix_length,
                                    size_t expected_bytes) {
    const uint8_t prefix_mask = (1 << prefix_length) - 1;
    const uint32_t beyond = start + range;

    LOG(INFO) << "############################################################";
    LOG(INFO) << "prefix_length=" << static_cast<int>(prefix_length);
    LOG(INFO) << "prefix_mask=" << std::hex << static_cast<int>(prefix_mask);
    LOG(INFO) << "start=" << start << " (" << std::hex << start << ")";
    LOG(INFO) << "range=" << range << " (" << std::hex << range << ")";
    LOG(INFO) << "beyond=" << beyond << " (" << std::hex << beyond << ")";
    LOG(INFO) << "expected_bytes=" << expected_bytes;

    // Confirm the claim that beyond requires more bytes.
    Encode(beyond, prefix_length);
    EXPECT_EQ(expected_bytes + 1, buffer_.size()) << Http2HexDump(buffer_);

    std::set<uint32_t> values;
    if (range < 200) {
      // Select all values in the range.
      for (uint32_t offset = 0; offset < range; ++offset) {
        values.insert(start + offset);
      }
    } else {
      // Select some values in this range, including the minimum and maximum
      // values that require exactly |expected_bytes| extension bytes.
      values.insert({start, start + 1, beyond - 2, beyond - 1});
      while (values.size() < 100) {
        values.insert(start + Random().Uniform(range));
      }
    }

    EncodeAndDecodeValues(values, prefix_length, expected_bytes);
  }

  HpackVarintDecoder decoder_;
  Http2String buffer_;
  uint8_t prefix_length_ = 0;
};

// To help me and future debuggers of varint encodings, this LOGs out the
// transition points where a new extension byte is added.
TEST_F(HpackVarintRoundTripTest, Encode) {
  for (int prefix_length = 3; prefix_length <= 7; ++prefix_length) {
    const uint32_t a = (1 << prefix_length) - 1;
    const uint32_t b = a + 128;
    const uint32_t c = b + (127 << 7);
    const uint32_t d = c + (127 << 14);
    const uint32_t e = d + (127 << 21);

    LOG(INFO) << "############################################################";
    LOG(INFO) << "prefix_length=" << prefix_length << "   a=" << a
              << "   b=" << b << "   c=" << c;

    EXPECT_EQ(a - 1, HiValueOfExtensionBytes(0, prefix_length));
    EXPECT_EQ(b - 1, HiValueOfExtensionBytes(1, prefix_length));
    EXPECT_EQ(c - 1, HiValueOfExtensionBytes(2, prefix_length));
    EXPECT_EQ(d - 1, HiValueOfExtensionBytes(3, prefix_length));
    EXPECT_EQ(e - 1, HiValueOfExtensionBytes(4, prefix_length));

    std::vector<uint32_t> values = {
        0,     1,                       // Force line break.
        a - 2, a - 1, a, a + 1, a + 2,  // Force line break.
        b - 2, b - 1, b, b + 1, b + 2,  // Force line break.
        c - 2, c - 1, c, c + 1, c + 2,  // Force line break.
        d - 2, d - 1, d, d + 1, d + 2,  // Force line break.
        e - 2, e - 1, e, e + 1, e + 2   // Force line break.
    };

    for (uint32_t value : values) {
      EncodeNoRandom(value, prefix_length);
      Http2String dump = Http2HexDump(buffer_);
      LOG(INFO) << Http2StringPrintf("%10u %0#10x ", value, value)
                << Http2HexDump(buffer_).substr(7);
    }
  }
}

TEST_F(HpackVarintRoundTripTest, FromSpec1337) {
  DecodeBuffer b(Http2StringPiece("\x1f\x9a\x0a"));
  uint32_t prefix_length = 5;
  uint8_t p = b.DecodeUInt8();
  EXPECT_EQ(1u, b.Offset());
  EXPECT_EQ(DecodeStatus::kDecodeDone, decoder_.Start(p, prefix_length, &b));
  EXPECT_EQ(3u, b.Offset());
  EXPECT_EQ(1337u, decoder_.value());

  EncodeNoRandom(1337, prefix_length);
  EXPECT_EQ(3u, buffer_.size());
  EXPECT_EQ('\x1f', buffer_[0]);
  EXPECT_EQ('\x9A', buffer_[1]);
  EXPECT_EQ('\x0a', buffer_[2]);
}

// Test all the values that fit into the prefix (one less than the mask).
TEST_F(HpackVarintRoundTripTest, ValidatePrefixOnly) {
  for (int prefix_length = 3; prefix_length <= 7; ++prefix_length) {
    const uint8_t prefix_mask = (1 << prefix_length) - 1;
    EncodeAndDecodeValuesInRange(0, prefix_mask, prefix_length, 1);
  }
}

// Test all values that require exactly 1 extension byte.
TEST_F(HpackVarintRoundTripTest, ValidateOneExtensionByte) {
  for (int prefix_length = 3; prefix_length <= 7; ++prefix_length) {
    const uint32_t start = (1 << prefix_length) - 1;
    EncodeAndDecodeValuesInRange(start, 128, prefix_length, 2);
  }
}

// Test *some* values that require exactly 2 extension bytes.
TEST_F(HpackVarintRoundTripTest, ValidateTwoExtensionBytes) {
  for (int prefix_length = 3; prefix_length <= 7; ++prefix_length) {
    const uint8_t prefix_mask = (1 << prefix_length) - 1;
    const uint32_t start = prefix_mask + 128;
    const uint32_t range = 127 << 7;

    EncodeAndDecodeValuesInRange(start, range, prefix_length, 3);
  }
}

// Test *some* values that require 3 extension bytes.
TEST_F(HpackVarintRoundTripTest, ValidateThreeExtensionBytes) {
  for (int prefix_length = 3; prefix_length <= 7; ++prefix_length) {
    const uint8_t prefix_mask = (1 << prefix_length) - 1;
    const uint32_t start = prefix_mask + 128 + (127 << 7);
    const uint32_t range = 127 << 14;

    EncodeAndDecodeValuesInRange(start, range, prefix_length, 4);
  }
}

// Test *some* values that require 4 extension bytes.
TEST_F(HpackVarintRoundTripTest, ValidateFourExtensionBytes) {
  for (int prefix_length = 3; prefix_length <= 7; ++prefix_length) {
    const uint8_t prefix_mask = (1 << prefix_length) - 1;
    const uint32_t start = prefix_mask + 128 + (127 << 7) + (127 << 14);
    const uint32_t range = 127 << 21;

    EncodeAndDecodeValuesInRange(start, range, prefix_length, 5);
  }
}

// Test the value one larger than the largest that can be decoded.
TEST_F(HpackVarintRoundTripTest, ValueTooLarge) {
  for (prefix_length_ = 3; prefix_length_ <= 7; ++prefix_length_) {
    const uint64_t too_large = (1 << 28) + (1 << prefix_length_) - 1;
    const uint32_t expected_offset = 6;
    HpackBlockBuilder bb;
    bb.AppendHighBitsAndVarint(0, prefix_length_, too_large);
    buffer_ = bb.buffer();

    // The validator is called after each of the several times that the input
    // DecodeBuffer is decoded, each with a different segmentation of the input.
    // Validate that decoder_.value() matches the expected value.
    bool validated = false;
    Validator validator = [&validated, expected_offset](
                              const DecodeBuffer& db,
                              DecodeStatus status) -> AssertionResult {
      validated = true;
      VERIFY_EQ(DecodeStatus::kDecodeError, status);
      VERIFY_EQ(expected_offset, db.Offset());
      return AssertionSuccess();
    };

    // StartDecoding, above, requires the DecodeBuffer be non-empty so that it
    // can call Start with the prefix byte.
    bool return_non_zero_on_first = true;
    DecodeBuffer b(buffer_);
    EXPECT_TRUE(
        DecodeAndValidateSeveralWays(&b, return_non_zero_on_first, validator));
    EXPECT_EQ(expected_offset, b.Offset());
    EXPECT_TRUE(validated);
  }
}

}  // namespace
}  // namespace test
}  // namespace http2
